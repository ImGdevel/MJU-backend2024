#include <iostream>
#include <thread>
#include <queue>
#include <vector>
#include <mutex>
#include <condition_variable>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <cstring>
#include <sys/select.h>
#include <nlohmann/json.hpp>
#include "message.pb.h"  
#include <signal.h>
#include <unordered_set>

using namespace mju;
using namespace std;
using json = nlohmann::json;
using ProtoMessage = google::protobuf::Message;

#define MAX_CLIENTS 5

#define JSON 1
#define PROTOBUF 0

class Client;
class Room;

atomic<bool> isRunning(true);

mutex workerQueueMutex, processingMutex;
mutex userMutex, roomMutex;
condition_variable workerQueueCV;
queue<int> workerQueue;

unordered_map<int, Client> clientSessions; 
unordered_set<int> processingSockets;

unordered_map<int, Room> chatRooms;
static int maxRoomId = 1;


static int WORKER = 4;
static int FORMAT = JSON; 
static int PORT = 10114;


class Client{
public:
    Client(){}

    Client(const string& name){
        this->name = name;
        enterRoom = 0;
    }

    Client(const Client& other) {
        this->name = other.name;
        this->enterRoom = other.enterRoom;  
    }

    bool isClientInAnyRoom(){
        return enterRoom > 0;
    }

    string name;
    int enterRoom;
};

class Room{
public:
    Room(){}

    Room(int id, string title) : id(id) , title(title){}

    int id;
    string title;
    unordered_set<int> joinedUser;
};



// 서버 종료
void serverSutDownHandler(int sig) {
    isRunning.store(false);
    workerQueueCV.notify_all();
}

// 연결 종료한 클라이언트
void disconnectedClient(int closed_socket){
    {
        unique_lock<mutex> lock(workerQueueMutex);
        clientSessions.erase(closed_socket);
    }
    close(closed_socket);
}


void send_message(int client_socket, const string& serialized_data) {
    uint16_t msg_length = htons(serialized_data.size());
    char length_buffer[2];
    memcpy(length_buffer, &msg_length, sizeof(msg_length));

    if (send(client_socket, length_buffer, sizeof(length_buffer), 0) != sizeof(length_buffer)) {
        perror("Failed to send message length");
        return;
    }

    if (send(client_socket, serialized_data.data(), serialized_data.size(), 0) != static_cast<ssize_t>(serialized_data.size())) {
        perror("Failed to send message data");
        return;
    }

    printf("Recv [C->S:총길이=%d바이트] %x(메시지크기) %s\n", msg_length, msg_length, serialized_data.c_str());

}

string receive_message(int client_socket) {
    char length_buffer[2];
    ssize_t length_received = recv(client_socket, length_buffer, sizeof(length_buffer), MSG_WAITALL);

    if (length_received != sizeof(length_buffer)) {
        if (length_received == 0) {
            cerr << "Client disconnected" << endl;
            disconnectedClient(client_socket);
            return "";
        } else {
            perror("Failed to receive message length");
        }
        return "";
    }


    uint16_t msg_length;
    memcpy(&msg_length, length_buffer, sizeof(msg_length));
    msg_length = ntohs(msg_length);
    string serialized_data(msg_length, 0);

    if(msg_length == 0){
        cout << "메시지 없음" << endl;
        return "";
    }

    ssize_t data_received = recv(client_socket, &serialized_data[0], msg_length, MSG_WAITALL);

    while (data_received < msg_length && isRunning.load()) {
        ssize_t more_data = recv(client_socket, &serialized_data[data_received], msg_length - data_received, 0);
        if (more_data <= 0) {
            perror("Failed to receive complete message data");
            return "";
        }
        data_received += more_data;
    }

    printf("Recv [C->S:총길이=%d바이트] %x(메시지크기) %s\n", msg_length, msg_length, serialized_data.c_str());
    return serialized_data;
}

////////
/// process
////////

// 클라이언트 이름 변경
void changeNameProcess(int client_id, string new_name){
    {
        unique_lock<mutex> look(userMutex);
        if(clientSessions.find(client_id) != clientSessions.end() ){
            clientSessions[client_id].name = new_name;
        }
    }
}

// 채팅방 생성
void createRoomProcess(int client_id, string room_title){
    {
        unique_lock<mutex> look(roomMutex);
        int room_id = maxRoomId;
        chatRooms[room_id] = Room(room_id, room_title) ;
        chatRooms[room_id].joinedUser.insert(client_id);
        clientSessions[client_id].enterRoom = room_id;
        maxRoomId++;
    }
}

// 채팅방 입장
void joinRoomProcess(int clinet_id, int roomId){
    {
        unique_lock<mutex> look(roomMutex);
        if(chatRooms.find(roomId) == chatRooms.end()){
            return;
        }
        chatRooms[roomId].joinedUser.insert(clinet_id);
    }
    clientSessions[clinet_id].enterRoom = roomId;
}

// 채팅방 퇴장
void leaveRoomProcess(int clinet_id, int roomId){
    {
        unique_lock<mutex> look(roomMutex);
        if(chatRooms.find(roomId) == chatRooms.end()){
            return;
        }
        chatRooms[roomId].joinedUser.erase(clinet_id);
    }
    clientSessions[clinet_id].enterRoom = 0;
}


////////////////
/// Handelr Json
////////////////

void notifyRoomMembers(int, int, const string);

// 클라이언트 이름 변경
void handleMessageCSName(int client_sock, json& json_message){
    cout << "Name Change!\n";

    Client client = clientSessions[client_sock];
    string original_name = client.name;
    string new_name = json_message["name"];

    changeNameProcess(client_sock, new_name);

    json json_format;
    json_format["type"] = "SCSystemMessage";
    json_format["text"] =  "이름이 " + original_name + "에서 " +  new_name + " 으로 변경되었습니다.";

    send_message(client_sock, json_format.dump(4));
    
    if(client.isClientInAnyRoom()){
        notifyRoomMembers(client.enterRoom, client_sock, json_format.dump(4));
    }
}


// 채팅방 목록 전송 (복사를 허용한다. 포인터 접근하다가 메모리 헤제되면 골치 아파진다)
void handleMessageCSRooms(int client_sock, json& json_message){
    json json_format;
    json_format["type"] = "SCRoomsResult";
    json_format["rooms"] = json::array();

    for (auto [_, room] : chatRooms) {
        json room_form = {
            {"roomId", room.id},
            {"title", room.title},
            {"members", json::array()}
        };
        unordered_map<int, Client> clients;
        copy(clientSessions.begin(), clientSessions.end(), inserter(clients, clients.end()));
        for(int member_sock : room.joinedUser){
            string member_name = clients[member_sock].name;
            room_form["members"].push_back(member_name);
        }
        json_format["rooms"].push_back(room_form);
    }

    send_message(client_sock, json_format.dump(4));
}

// 새로운 방 생성
void handleMessageCSCreateRoom(int client_sock, json& json_message){
    string title = json_message["title"];
    
    createRoomProcess(client_sock, title);

    json json_format;
    json_format["type"] = "SCSystemMessage";
    json_format["text"] =  "방제[" + title + "] 방에 입장했습니다.";
    send_message(client_sock, json_format.dump(4));
}

// 채팅방 참가
void handleMessageCSJoinRoom(int client_sock, json& json_message){
    json json_format;
    json_format["type"] = "SCSystemMessage";

    int roomId = json_message["roomId"];
    Client client = clientSessions[client_sock];
    
    if(client.isClientInAnyRoom()){
        json_format["text"] =  "대화 방에 있을 때는 다른 방에 들어갈 수 없습니다.";
        send_message(client_sock, json_format.dump(4));
        return;
    }
    if(chatRooms.find(roomId) == chatRooms.end()){
        json_format["text"] =  "대화방이 존재하지 않습니다.";
        send_message(client_sock, json_format.dump(4));
        return;
    }
    string roomTitle = chatRooms[roomId].title;
    joinRoomProcess(client_sock, roomId);
    
    json_format["text"] =  "방제[" + roomTitle + "] 방에 입장했습니다.";
    send_message(client_sock, json_format.dump(4));
    
    json_format["text"] =  "[" + client.name + "] 님이 입장했습니다.";
    notifyRoomMembers(roomId, client_sock, json_format.dump(4));
}



// 방 나가기
void handleMessageCSLeaveRoom(int client_sock, json& json_message){
    json json_format;
    json_format["type"] = "SCSystemMessage";

    Client client = clientSessions[client_sock];
    int roomId = client.enterRoom;
    if(!client.isClientInAnyRoom()){
        json_format["text"] =  "현재 대화방에 들어가 있지 않습니다.";
        send_message(client_sock, json_format.dump(4));
        return;
    }
    leaveRoomProcess(client_sock, roomId);
    

    json_format["text"] =  "방제[" + chatRooms[roomId].title + "] 대화 방에서 퇴장했습니다.";
    send_message(client_sock, json_format.dump(4));

    json_format["text"] =  "[" + client.name + "] 님이 퇴장했습니다.";
    notifyRoomMembers(roomId, client_sock, json_format.dump(4));
}

// 채팅
void handleMessageCSChat(int client_sock, json& json_message){
    json json_format;
    string text = json_message["text"];

    Client client = clientSessions[client_sock];
    int roomId = client.enterRoom;
    if(!client.isClientInAnyRoom()){
        json_format["type"] = "SCSystemMessage";
        json_format["text"] =  "현재 대화방에 들어가 있지 않습니다.";
        send_message(client_sock, json_format.dump(4));
        return;
    }

    json_format["type"] = "SCChat";
    json_format["text"] =  text;
    json_format["member"] =  client.name;
    notifyRoomMembers(roomId, client_sock, json_format.dump(4));
}

// 서버 종료
void handleMessageCSShutdown(int client_sock, json& json_message){
    serverSutDownHandler(0);   
}

// 
void notifyRoomMembers(int roomId, int senderSock, const string message){
    if(chatRooms.find(roomId) != chatRooms.end()){
        Room room = chatRooms[roomId];
        unordered_set<int> userList = room.joinedUser;
        for(int user_socket : userList){
            if(user_socket != senderSock){
                send_message(user_socket, message);
            }
        }
    }
}




///////
/// handler proto
///////


// 통일된 메시지 전송 메서드
void sendProtoMessage(int client_sock, Type::MessageType msg_type, const ProtoMessage& proto_msg) {
    Type type;
    type.set_type(msg_type);

    string serialized_type;
    if (type.SerializePartialToString(&serialized_type)) {
        send_message(client_sock, serialized_type);
    } else {
        cerr << "Failed to serialize Type message with type " << msg_type << endl;
        return;
    }

    string serialized_msg;
    if (proto_msg.SerializeToString(&serialized_msg)) {
        send_message(client_sock, serialized_msg);
    } else {
        cerr << "Failed to serialize data message" << endl;
    }
}


void notifyRoomMembers(int roomId, int senderSock, Type::MessageType msg_type,  const ProtoMessage& message){
    if(chatRooms.find(roomId) != chatRooms.end()){
        Room room = chatRooms[roomId];
        unordered_set<int> userList = room.joinedUser;
        for(int user_socket : userList){
            if(user_socket != senderSock){
                sendProtoMessage(user_socket, msg_type, message);
            }
        }
    }
}


// 이름 변경
void handleMessageCSNameP(int client_sock, string& proto_message){
    cout << "Name Change!\n"; 
    CSName proto_format;
    if(!proto_format.ParseFromString(proto_message)){
        perror("Not match type!");
    }
    Client client = clientSessions[client_sock];
    string original_name = client.name;
    string new_name = proto_format.name();

    changeNameProcess(client_sock, new_name);

    SCSystemMessage response_msg;
    response_msg.set_text( "이름이 " + original_name + "에서 " +  new_name + " 으로 변경되었습니다.");
    sendProtoMessage(client_sock, Type::SC_SYSTEM_MESSAGE, response_msg);

    if(client.isClientInAnyRoom()){
        notifyRoomMembers(client.enterRoom, client_sock, Type::SC_SYSTEM_MESSAGE, response_msg);
    }
}


// 채팅방 리스트
void handleMessageCSRoomsP(int client_sock, string& proto_message){
    cout << "Rooms!\n";
    SCRoomsResult response_msg;

    for (const auto& [_, room] : chatRooms) {
        SCRoomsResult::RoomInfo* room_info = response_msg.add_rooms();

        room_info->set_roomid(room.id);
        room_info->set_title(room.title);
        
        for (int member_sock : room.joinedUser) {
            string member_name = clientSessions[member_sock].name; 
            room_info->add_members(member_name);
        }
    }

    sendProtoMessage(client_sock, Type::SC_ROOMS_RESULT, response_msg);
}

// 방 생성
void handleMessageCSCreateRoomP(int client_sock, string& proto_message){
    cout << "Create New Rooms!\n";
    mju::CSCreateRoom proto_format;
    if(!proto_format.ParseFromString(proto_message)){
        perror("Not match type!");
    }
    string title = proto_format.title();

    createRoomProcess(client_sock, title);

    mju::SCSystemMessage response_msg;
    response_msg.set_text("방제[" + title + "] 방에 입장했습니다.");
    sendProtoMessage(client_sock, Type::SC_SYSTEM_MESSAGE, response_msg);
}

void handleMessageCSJoinRoomP(int client_sock, string& proto_message){
    cout << "Room Join!\n";
    mju::CSJoinRoom proto_format;
    if(!proto_format.ParseFromString(proto_message)){
        perror("Not match type!");
    }

    mju::SCSystemMessage response_msg;
    int roomId = proto_format.roomid();
    Client client = clientSessions[client_sock];
    
    if(client.isClientInAnyRoom()){
        response_msg.set_text("대화 방에 있을 때는 다른 방에 들어갈 수 없습니다.");
        sendProtoMessage(client_sock, Type::SC_SYSTEM_MESSAGE, response_msg);
        return;
    }
    if(chatRooms.find(roomId) == chatRooms.end()){
        response_msg.set_text("대화방이 존재하지 않습니다.");
        sendProtoMessage(client_sock, Type::SC_SYSTEM_MESSAGE, response_msg);
        return;
    }
    string roomTitle = chatRooms[roomId].title;
    joinRoomProcess(client_sock, roomId);
    
    response_msg.set_text("방제[" + roomTitle + "] 방에 입장했습니다.");
    sendProtoMessage(client_sock, Type::SC_SYSTEM_MESSAGE, response_msg);
    
    response_msg.set_text("[" + client.name + "] 님이 입장했습니다.");
    notifyRoomMembers(client.enterRoom, client_sock, Type::SC_SYSTEM_MESSAGE, response_msg);
}

void handleMessageCSLeaveRoomP(int client_sock, string& proto_message){
    cout << "Room Leave!\n";

    SCSystemMessage response_msg;
    Client client = clientSessions[client_sock];
    int roomId = client.enterRoom;
    if(!client.isClientInAnyRoom()){
        response_msg.set_text( "현재 대화방에 들어가 있지 않습니다.");
        sendProtoMessage(client_sock, Type::SC_SYSTEM_MESSAGE, response_msg);
        return;
    }
    leaveRoomProcess(client_sock, roomId);
    

    response_msg.set_text("방제[" + chatRooms[roomId].title + "] 대화 방에서 퇴장했습니다.");
    sendProtoMessage(client_sock, Type::SC_SYSTEM_MESSAGE, response_msg);

    response_msg.set_text("[" + client.name + "] 님이 퇴장했습니다.");
    notifyRoomMembers(client.enterRoom, client_sock, Type::SC_SYSTEM_MESSAGE, response_msg);
}

void handleMessageCSChatP(int client_sock, string& proto_message){
    cout << "Chat!\n";
    mju::CSChat proto_format;
    if(!proto_format.ParseFromString(proto_message)){
        perror("Not match type!");
    }

    SCSystemMessage response_msg;
    string text = proto_format.text();
    Client client = clientSessions[client_sock];
    int roomId = client.enterRoom;

    if(!client.isClientInAnyRoom()){
        response_msg.set_text("현재 대화방에 들어가 있지 않습니다.");
        sendProtoMessage(client_sock, Type::SC_SYSTEM_MESSAGE, response_msg);
        return;
    }

    mju::SCChat chatMessage;
    chatMessage.set_member(client.name);
    chatMessage.set_text(text);
    
    notifyRoomMembers(client.enterRoom, client_sock, Type::SC_CHAT, chatMessage);

}

void handleMessageCSShutdownP(int client_sock, string& proto_message){
    cout << "Shutdown!\n";
    serverSutDownHandler(0);  
}


/////// 
/// handler
///////

unordered_map<string, function<void(int, json&)>> jsonMessageHandler = {
    {"CSName", handleMessageCSName },
    {"CSRooms", handleMessageCSRooms },
    {"CSCreateRoom", handleMessageCSCreateRoom },
    {"CSJoinRoom", handleMessageCSJoinRoom },
    {"CSLeaveRoom", handleMessageCSLeaveRoom },
    {"CSChat", handleMessageCSChat },
    {"CSShutdown", handleMessageCSShutdown }
};


unordered_map<Type::MessageType, function<void(int, string&)>> protobufMessageHandler = {
    {Type::CS_NAME, handleMessageCSNameP },
    {Type::CS_ROOMS, handleMessageCSRoomsP },
    {Type::CS_CREATE_ROOM, handleMessageCSCreateRoomP },
    {Type::CS_JOIN_ROOM, handleMessageCSJoinRoomP },
    {Type::CS_LEAVE_ROOM, handleMessageCSLeaveRoomP },
    {Type::CS_CHAT, handleMessageCSChatP },
    {Type::CS_SHUTDOWN, handleMessageCSShutdownP }
};

void jsonEventHandler(int client_socket){
    string serialized_data = receive_message(client_socket);
    if (serialized_data.empty()) return;

    try {
        json json_message = json::parse(serialized_data);
        
        string messageType = json_message["type"];
        if (jsonMessageHandler.find(messageType) != jsonMessageHandler.end()) {
            jsonMessageHandler[messageType](client_socket, json_message);
        } else {
            cerr << "No handler found for message type: " << messageType << endl;
        }
    } catch (const json::parse_error& e) {
        cerr << "JSON parse error: " << e.what() << endl;
    }
}

void protobufEventHandler(int client_socket){
    string serialized_data_type = receive_message(client_socket);
    if (serialized_data_type.empty()) return;

    Type incoming_type_msg;
    if (!incoming_type_msg.ParseFromString(serialized_data_type)) {
        cerr << "Protobuf parse error" << endl;
    } 
    Type::MessageType messageType = incoming_type_msg.type();
    cout << "[C->S: Protobuf Message received] Data: " << incoming_type_msg.ShortDebugString() << endl;

    string serialized_data_message = receive_message(client_socket);

    if (protobufMessageHandler.find(messageType) != protobufMessageHandler.end()) {
        protobufMessageHandler[messageType](client_socket, serialized_data_message);
    } else {
        cerr << "No handler found for message type: " << messageType << endl;
    }
}


void handleClient(int client_socket) {
    
    if (FORMAT == JSON) {
        jsonEventHandler(client_socket);
    }
    else if(FORMAT == PROTOBUF){
        protobufEventHandler(client_socket);
    }

    {
        unique_lock<mutex> lock(processingMutex);
        processingSockets.erase(client_socket);
    }
}

void worker() {
    while (isRunning.load()) {
        int clientSocket;
        {
            unique_lock<mutex> lock(workerQueueMutex);
            while (workerQueue.empty()) {
                workerQueueCV.wait(lock);
                if(!isRunning.load()){
                    return;
                }
            }

            clientSocket = workerQueue.front();
            workerQueue.pop();
        }
        handleClient(clientSocket);
    }
}


void configureParameters(const int argc, char* argv[]) {
    for (int i = 1; i < argc; ++i) {
        if (strncmp(argv[i], "--port=", 7) == 0) {
            PORT = stoi(argv[i] + 7);
        } else if (strncmp(argv[i], "--worker=", 9) == 0) {
            WORKER = stoi(argv[i] + 9); 
        } else if (strncmp(argv[i], "--format=", 9) == 0) {
            string format = argv[i] + 9;
            FORMAT = (format == "json");
        }
    }
}

int main(int argc, char* argv[]) {
    int server_fd, new_socket;
    sockaddr_in address;
    int opt = 1;
    int addrlen = sizeof(address);

    configureParameters(argc, argv);

    signal(SIGINT, serverSutDownHandler);

    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt))) {
        perror("setsockopt failed");
        exit(EXIT_FAILURE);
    }

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    if (bind(server_fd, (sockaddr*)&address, sizeof(address)) < 0) {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }

    if (listen(server_fd, MAX_CLIENTS) < 0) {
        perror("listen failed");
        exit(EXIT_FAILURE);
    }

    cout << "Server listening on port " << PORT << endl;

    vector<thread> workers;
    for (int i = 0; i < WORKER; ++i) {
        cout << "Thread 생성 " << i << "\n";
        workers.emplace_back(worker);
    }

    fd_set readfds;

    while (isRunning.load()) {

        FD_ZERO(&readfds);
        FD_SET(server_fd, &readfds);
        int max_sd = server_fd;

        for (auto& [sock, _] : clientSessions) {
            FD_SET(sock, &readfds);
            max_sd = max(max_sd, sock);
        }

        struct timeval timeout = {1, 0};
        int activity = select(max_sd + 1, &readfds, NULL, NULL, &timeout);
        if (activity < 0) {
            if (errno == EINTR) continue;
            perror("select error");
            break;
        }

        if (!isRunning.load()) break;

        if (FD_ISSET(server_fd, &readfds)) {
            if ((new_socket = accept(server_fd, (sockaddr*)&address, (socklen_t*)&addrlen)) < 0) {
                if (isRunning.load()) perror("accept");
                break;
            }
            
            string clientIP = inet_ntoa(address.sin_addr);
            int clientPort = ntohs(address.sin_port);
            cout << "Client Connect: Ip_" << clientIP << " Port_" << clientPort << "\n";

            string default_name = "(" + clientIP + ", " + to_string(clientPort) + ")";
            {
                unique_lock<mutex> lock(workerQueueMutex);
                clientSessions.insert({new_socket, Client(default_name)});
            }
        }

        for (auto& [client_socket, _] : clientSessions) {
            if (FD_ISSET(client_socket, &readfds)) 
            {
                unique_lock<mutex> lock(processingMutex);
                if (processingSockets.find(client_socket) == processingSockets.end()) {
                    processingSockets.insert(client_socket);
                    {
                        unique_lock<mutex> lock(workerQueueMutex);
                        workerQueue.push(client_socket);
                    }
                    workerQueueCV.notify_one();
                }
            }
        }
    }

    isRunning.store(false);
    workerQueueCV.notify_all();
    for (auto& worker : workers) {
        cout << "Thread join" << "\n";
        worker.join();
    }
    for (auto& [client_fd, _] : clientSessions) {
        close(client_fd);
    }
    close(server_fd);
    cout << "Server ShutDown";

    return 0;
}