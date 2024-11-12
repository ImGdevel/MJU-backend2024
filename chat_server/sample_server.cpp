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

#define MAX_CLIENTS 5

#define JSON 1
#define PROTOBUF 0

class Clinet;
class Room;

atomic<bool> isRunning(true);

mutex queueMutex, processingMutex;
mutex userMutex, roomMutex;
condition_variable cv;
queue<int> clientQueue;

unordered_map<int, Clinet> clientSessions; 
unordered_set<int> processingSockets;

unordered_map<int, Room> chatRooms;

static int WORKER = 4;
static int FORMAT = JSON; 
static int PORT = 10114;


class Clinet{
public:
    Clinet(){
        cout << "불리면 안됨";
    }

    Clinet(const string& name){
        this->name = name;
        enterRoom = 0;
    }

    Clinet(const Clinet& other) {
        this->name = other.name;
        this->enterRoom = other.enterRoom;  
    }
    string name;
    int enterRoom;
};

class Room{
public:
    int id;
    string title;
    unordered_set<int> joinedUser;
};





void serverSutDownHandler(int sig) {
    isRunning.store(false);
    cv.notify_all();
}

// 연결 종료한 클라이언트
void disconnectedClinet(int sock){
    {
        unique_lock<mutex> lock(queueMutex);
        clientSessions.erase(sock);
    }
    close(sock);
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
            disconnectedClinet(client_socket);
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

////////////////
/// Handelr Json
////////////////

// 클라이언트 이름 변경
void handleMessageCSName(int clinet_sock, json& json_message){
    cout << "Name Change!\n";

    string original_name = clientSessions[clinet_sock].name;
    string name = json_message["name"];
    {
        unique_lock<mutex> look(userMutex);
        if(clientSessions.find(clinet_sock) != clientSessions.end() ){
            clientSessions[clinet_sock] = name;
        }
    }

    json json_format;
    json_format["type"] = "SCSystemMessage";
    json_format["name"] =  "이름이 " + original_name + "에서" +  name + " 으로 변경되었습니다.";

    send_message(clinet_sock, json_format.dump(4));
    
    // 만약 방에 있다면? 모든 유저에게 메시지 전달
    // notifyRoomMembers()
}


// 채팅방 목록 전송 (복사를 허용한다. 포인터 접근하다가 괜히 사라지면 골치 아파진다)
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
        unordered_map<int, Clinet> clinets;
        copy(clientSessions.begin(), clientSessions.end(), inserter(clinets, clinets.end()));
        for(int member_sock : room.joinedUser){
            string member_name = clinets[member_sock].name;
            room_form["members"].push_back(member_name);
        }
        json_format["rooms"].push_back(room_form);
    }

    send_message(client_sock, json_format.dump(4));
}

// 새로운 방 생성
void handleMessageCSCreateRoom(int client_sock, json& json_message){
    string title = json_message["title"];
    cout << "Create New Rooms!\n";
    


}

// 채팅방 참가
void handleMessageCSJoinRoom(int client_sock, json& json_message){
    int roomId = stoi((string)json_message["roomId"]);
    cout << "Room Join!\n";
    // todo: 방에 추가

}



// 방 나가기
void handleMessageCSLeaveRoom(int client_sock, json& json_message){
    cout << "Room Leave!\n";
    // todo: 방에서 삭제

}

// 채팅
void handleMessageCSChat(int client_sock, json& json_message){
    cout << "Chat!\n";


}

// 서버 종료
void handleMessageCSShutdown(int client_sock, json& json_message){
    cout << "Shutdown!\n";
    serverSutDownHandler(0);   
}

void notifyRoomMembers(int roomId, int senderSock, const string& message){
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

unordered_map<string, function<void(int, json&)>> jsonMessageHandler = {
    {"CSName", handleMessageCSName },
    {"CSRooms", handleMessageCSRooms },
    {"CSCreateRoom", handleMessageCSCreateRoom },
    {"CSJoinRoom", handleMessageCSJoinRoom },
    {"CSLeaveRoom", handleMessageCSLeaveRoom },
    {"CSChat", handleMessageCSChat },
    {"CSShutdown", handleMessageCSShutdown }
};


///////
/// handler proto
///////
void sendTypeMessage(int client_sock, Type::MessageType msg_type) {
    Type type;
    type.set_type(msg_type);

    string serialized_data;
    if (type.SerializePartialToString(&serialized_data)) {
        send_message(client_sock, serialized_data);
    } else {
        cerr << "Failed to serialize message of type " << msg_type << endl;
    }
}



void handleMessageCSNameP(int client_sock, string& proto_message){
    cout << "Name Change!\n"; 
    CSName proto_format;
    if(!proto_format.ParseFromString(proto_message)){
        perror("Not match type!");
    }
}

void handleMessageCSRoomsP(int client_sock, string& proto_message){
    cout << "Rooms!\n";
    SCRoomsResult proto_format;

    for (const auto& [_, room] : chatRooms) {
        SCRoomsResult::RoomInfo* room_info = proto_format.add_rooms();

        room_info->set_roomid(room.id);
        room_info->set_title(room.title);
        
        for (int member_sock : room.joinedUser) {
            string member_name = clientSessions[member_sock].name; 
            room_info->add_members(member_name);
        }
    }

    sendTypeMessage(client_sock, Type::SC_ROOMS_RESULT);

    string serialized_data;
    if (proto_format.SerializeToString(&serialized_data)) {
        send_message(client_sock, serialized_data); 
    } else {
        cerr << "Failed to serialize SCRoomsResult message" << endl;
    }
}

void handleMessageCSCreateRoomP(int client_sock, string& proto_message){
    cout << "Create New Rooms!\n";
    mju::CSCreateRoom proto_format;
    if(!proto_format.ParseFromString(proto_message)){
        perror("Not match type!");
    }
}

void handleMessageCSJoinRoomP(int client_sock, string& proto_message){
    cout << "Room Join!\n";
    mju::CSJoinRoom proto_format;
    if(!proto_format.ParseFromString(proto_message)){
        perror("Not match type!");
    }


}

void handleMessageCSLeaveRoomP(int client_sock, string& proto_message){
    cout << "Room Leave!\n";

    
}

void handleMessageCSChatP(int client_sock, string& proto_message){
    cout << "Chat!\n";
    mju::CSChat proto_format;
    if(!proto_format.ParseFromString(proto_message)){
        perror("Not match type!");
    }

}

void handleMessageCSShutdownP(int client_sock, string& proto_message){
    cout << "Shutdown!\n";
    serverSutDownHandler(0);  
}



unordered_map<Type::MessageType, function<void(int, string&)>> protobufMessageHandler = {
    {Type::CS_NAME, handleMessageCSNameP },
    {Type::CS_ROOMS, handleMessageCSRoomsP },
    {Type::CS_CREATE_ROOM, handleMessageCSCreateRoomP },
    {Type::CS_JOIN_ROOM, handleMessageCSJoinRoomP },
    {Type::CS_LEAVE_ROOM, handleMessageCSLeaveRoomP },
    {Type::CS_CHAT, handleMessageCSChatP },
    {Type::CS_SHUTDOWN, handleMessageCSShutdownP }
};

void jsonEventHanler(int client_socket){
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


void handleClient(int client_socket) {
    
    if (FORMAT == JSON) {
        jsonEventHanler(client_socket);
    }
    else if(FORMAT == PROTOBUF){
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
        }
            
        
    }

    /*
    if (FORMAT == JSON) {
        json json_message = {{"type", "SCSystemMessage"}, {"text", "MESSAGE"}};
        send_message(client_socket, json_message.dump());
    } else {
        Type response_type_msg;
        response_type_msg.set_type(Type::SC_SYSTEM_MESSAGE);
        send_message(client_socket, response_type_msg.SerializeAsString());

        SCSystemMessage response_msg;
        response_msg.set_text("Response from server");
        send_message(client_socket, response_msg.SerializeAsString());
    }
    */
    
    {
        unique_lock<mutex> lock(processingMutex);
        processingSockets.erase(client_socket);
    }
}

void worker() {
    while (isRunning.load()) {
        int clientSocket;
        {
            unique_lock<mutex> lock(queueMutex);
            while (clientQueue.empty()) {
                cv.wait(lock);
                if(!isRunning.load()){
                    return;
                }
            }

            clientSocket = clientQueue.front();
            clientQueue.pop();
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
                unique_lock<mutex> lock(queueMutex);
                clientSessions.insert({new_socket, Clinet(default_name)});
            }
        }

        for (auto& [client_socket, _] : clientSessions) {
            if (FD_ISSET(client_socket, &readfds)) 
            {
                unique_lock<mutex> lock(processingMutex);
                if (processingSockets.find(client_socket) == processingSockets.end()) {
                    processingSockets.insert(client_socket);
                    {
                        unique_lock<mutex> lock(queueMutex);
                        clientQueue.push(client_socket);
                    }
                    cv.notify_one();
                }
            }
        }
    }

    isRunning.store(false);
    cv.notify_all();
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