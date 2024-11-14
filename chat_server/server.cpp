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

#define JSON 1
#define PROTOBUF 0

class Client;
class Room;

atomic<bool> isServerRunning(true);

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
static int SERVER_PORT = 10114;

class Client {
public:
    Client() {}

    Client(const string& name) : name(name), enterRoomId(0) {}

    Client(const Client& other) {
        this->name = other.name;
        this->enterRoomId = other.enterRoomId;  
    }

    bool isClientInAnyRoom() const {
        return enterRoomId > 0;
    }

    string name;
    int enterRoomId;
};

class Room {
public:
    Room() {}

    Room(int id, const string& title) : id(id), title(title) {}

    int id;
    string title;
    unordered_set<int> joinedUsers;
};


void disconnectClient(int);
void serverShutdownHandler(int);

// 메시지 전송
void sendMessage(int clientSocket, const string& serializedData) {

    uint16_t msgLength = htons(serializedData.size());
    char lengthBuffer[2];
    memcpy(lengthBuffer, &msgLength, sizeof(msgLength));

    if (send(clientSocket, lengthBuffer, sizeof(lengthBuffer), 0) != sizeof(lengthBuffer)) {
        perror("Failed to send message length");
        return;
    }

    if (send(clientSocket, serializedData.data(), serializedData.size(), 0) != static_cast<ssize_t>(serializedData.size())) {
        perror("Failed to send message data");
        return;
    }

    printf("Recv [C->S:총길이=%d바이트] %x(메시지크기) %s\n", msgLength, msgLength, serializedData.c_str());
}

// 메시지 수신
string receiveMessage(int clientSocket) {

    char lengthBuffer[2];
    ssize_t lengthReceived = recv(clientSocket, lengthBuffer, sizeof(lengthBuffer), MSG_WAITALL);

    if (lengthReceived != sizeof(lengthBuffer)) {
        if (lengthReceived == 0) {
            perror("Client disconnected ");
            disconnectClient(clientSocket);
            return "";
        } else {
            perror("Failed to receive message length");
        }
        return "";
    }

    uint16_t msgLength;
    memcpy(&msgLength, lengthBuffer, sizeof(msgLength));
    msgLength = ntohs(msgLength);
    string serializedData(msgLength, 0);

    if (msgLength == 0) {
        return "";
    }

    ssize_t dataReceived = recv(clientSocket, &serializedData[0], msgLength, MSG_WAITALL);

    while (dataReceived < msgLength && isServerRunning.load()) {
        ssize_t moreData = recv(clientSocket, &serializedData[dataReceived], msgLength - dataReceived, 0);
        if (moreData <= 0) {
            perror("Failed to receive complete message data");
            return "";
        }
        dataReceived += moreData;
    }

    printf("Recv [C->S:총길이=%d바이트] %x(메시지크기) %s\n", msgLength, msgLength, serializedData.c_str());
    return serializedData;
}

////////
/// process 로직
////////

// 클라이언트 이름 변경
void changeNameProcess(int clientId, const string& newName) {
    {
        unique_lock<mutex> look(userMutex);
        if (clientSessions.find(clientId) != clientSessions.end()) {
            clientSessions[clientId].name = newName;
        }
    }
}

// 채팅방 생성
void createRoomProcess(int clientId, const string& roomTitle) {
    int roomId = maxRoomId;
    {
        unique_lock<mutex> look(roomMutex);
        int roomId = maxRoomId;
        chatRooms[roomId] = Room(roomId, roomTitle);
        chatRooms[roomId].joinedUsers.insert(clientId);
        maxRoomId++;
    }
    {
        unique_lock<mutex> look(userMutex);
        clientSessions[clientId].enterRoomId = roomId;
    }
}

// 채팅방 입장
void joinRoomProcess(int clientId, int roomId) {
    {
        unique_lock<mutex> look(roomMutex);
        if (chatRooms.find(roomId) == chatRooms.end()) {
            return;
        }
        chatRooms[roomId].joinedUsers.insert(clientId);
    }
    {
        unique_lock<mutex> look(userMutex);
        clientSessions[clientId].enterRoomId = roomId;
    }
}

// 채팅방 퇴장
void leaveRoomProcess(int clientId, int roomId) {
    {
        unique_lock<mutex> look(roomMutex);
        if (chatRooms.find(roomId) == chatRooms.end()) {
            return;
        }
        chatRooms[roomId].joinedUsers.erase(clientId);
    }
    {
        unique_lock<mutex> look(userMutex);
        clientSessions[clientId].enterRoomId = 0;
    }
}



//////////////
// 원래는 MessageHandler 인터페이스를 만들고 
// 타입에 따라 Json OR Proto Handler를 선택하는 방식으로 설계할 목적이었으나.
// (즉 Strategy Pattern)
// 단일 파일 버전에서는 임시 방편으로 그냥 아래와 같이 구현하였다.
//////////////

////////////////
/// Json 핸들러
////////////////

// 채팅방 사용자가 있을 때 알림 전송
void sendToRoomMembers(int roomId, int senderSock, const string& message){

    if(chatRooms.find(roomId) != chatRooms.end()){
        Room room = chatRooms[roomId];
        unordered_set<int> userList = room.joinedUsers;
        for(int userSocket : userList){
            if(userSocket != senderSock){
                sendMessage(userSocket, message);
            }
        }
    }
}

// JSON 클라이언트 이름 변경
void handleMessageCSName(int clientSock, string& requestMessage){

    json jsonData = json::parse(requestMessage);

    Client client = clientSessions[clientSock];
    string originalName = client.name;
    string newName = jsonData["name"];

    changeNameProcess(clientSock, newName);

    json jsonFormat;
    jsonFormat["type"] = "SCSystemMessage";
    jsonFormat["text"] = "이름이 " + originalName + "에서 " + newName + "으로 변경되었습니다.";

    sendMessage(clientSock, jsonFormat.dump());
    
    if(client.isClientInAnyRoom()){
        sendToRoomMembers(client.enterRoomId, clientSock, jsonFormat.dump());
    }
}

// JSON 채팅방 목록 전송
void handleMessageCSRooms(int clientSock, string& requestMessage){

    json jsonFormat;
    jsonFormat["type"] = "SCRoomsResult";
    jsonFormat["rooms"] = json::array();

    for (auto [_, room] : chatRooms) {
        json roomForm;
        roomForm["roomId"] = room.id;
        roomForm["title"] = room.title;
        roomForm["members"] = json::array();

        unordered_map<int, Client> clients;
        copy(clientSessions.begin(), clientSessions.end(), inserter(clients, clients.end()));
        for(int memberSock : room.joinedUsers){
            string memberName = clients[memberSock].name;
            roomForm["members"].push_back(memberName);
        }
        jsonFormat["rooms"].push_back(roomForm);
    }

    sendMessage(clientSock, jsonFormat.dump());
    
}

// JSON 새로운 방 생성
void handleMessageCSCreateRoom(int clientSock, string& requestMessage){

    json jsonData = json::parse(requestMessage);
    string title = jsonData["title"];

    json jsonFormat;
    jsonFormat["type"] = "SCSystemMessage";

    Client client = clientSessions[clientSock];
    if(client.isClientInAnyRoom()){
        jsonFormat["text"] = "대화 방에 있을 때는 방을 개설 할 수 없습니다.";
        sendMessage(clientSock, jsonFormat.dump());
        return;
    }

    createRoomProcess(clientSock, title);
    
    jsonFormat["text"] = "방제[" + title + "] 방에 입장했습니다.";
    sendMessage(clientSock, jsonFormat.dump());

}

// JSON 채팅방 참가
void handleMessageCSJoinRoom(int clientSock, string& requestMessage){

    json jsonData = json::parse(requestMessage);
    int roomId = jsonData["roomId"];
    Client client = clientSessions[clientSock];
    
    json jsonFormat;
    jsonFormat["type"] = "SCSystemMessage";
    
    if(client.isClientInAnyRoom()){
        jsonFormat["text"] = "대화 방에 있을 때는 다른 방에 들어갈 수 없습니다.";
        sendMessage(clientSock, jsonFormat.dump());
        return;
    }
    if(chatRooms.find(roomId) == chatRooms.end()){
        jsonFormat["text"] = "대화방이 존재하지 않습니다.";
        sendMessage(clientSock, jsonFormat.dump());
        return;
    }
    string roomTitle = chatRooms[roomId].title;
    joinRoomProcess(clientSock, roomId);
    
    jsonFormat["text"] = "방제[" + roomTitle + "] 방에 입장했습니다.";
    sendMessage(clientSock, jsonFormat.dump());
    
    jsonFormat["text"] = "[" + client.name + "] 님이 입장했습니다.";
    sendToRoomMembers(roomId, clientSock, jsonFormat.dump());
}

// JSON 방 나가기
void handleMessageCSLeaveRoom(int clientSock, string& requestMessage){

    Client client = clientSessions[clientSock];
    int roomId = client.enterRoomId;

    json jsonFormat;
    jsonFormat["type"] = "SCSystemMessage";

    if(!client.isClientInAnyRoom()){
        jsonFormat["text"] = "현재 대화방에 들어가 있지 않습니다.";
        sendMessage(clientSock, jsonFormat.dump());
        return;
    }
    leaveRoomProcess(clientSock, roomId);
    
    jsonFormat["text"] = "방제[" + chatRooms[roomId].title + "] 대화 방에서 퇴장했습니다.";
    sendMessage(clientSock, jsonFormat.dump());

    jsonFormat["text"] = "[" + client.name + "] 님이 퇴장했습니다.";
    sendToRoomMembers(roomId, clientSock, jsonFormat.dump());
}

// JSON 채팅
void handleMessageCSChat(int clientSock, string& requestMessage){

    json jsonData = json::parse(requestMessage);
    string text = jsonData["text"];
    Client client = clientSessions[clientSock];
    int roomId = client.enterRoomId;

    json jsonFormat;

    if(!client.isClientInAnyRoom()){
        jsonFormat["type"] = "SCSystemMessage";
        jsonFormat["text"] = "현재 대화방에 들어가 있지 않습니다.";
        sendMessage(clientSock, jsonFormat.dump());
        return;
    }

    jsonFormat["type"] = "SCChat";
    jsonFormat["text"] = text;
    jsonFormat["member"] = client.name;
    sendToRoomMembers(roomId, clientSock, jsonFormat.dump());
}

// JSON 서버 종료
void handleMessageCSShutdown(int clientSock, string& requestMessage){
    serverShutdownHandler(0);   
}


///////
/// ProtoBuf 핸들러
///////

// ProtoBuf 메시지 전송
void sendProtoMessage(int clientSock, Type::MessageType msgType, const ProtoMessage& protoMsg) {
    Type type;
    type.set_type(msgType);

    string serializedType;
    if (type.SerializePartialToString(&serializedType)) {
        sendMessage(clientSock, serializedType);
    } else {
        cerr << "Failed to serialize Type message with type " << msgType << endl;
        return;
    }

    string serializedMsg;
    if (protoMsg.SerializeToString(&serializedMsg)) {
        sendMessage(clientSock, serializedMsg);
    } else {
        cerr << "Failed to serialize data message" << endl;
    }
}

// 모든 사용자에게 전송
void sendToRoomMembers(int roomId, int senderSock, Type::MessageType msg_type,  const ProtoMessage& message){

    if(chatRooms.find(roomId) != chatRooms.end()){
        Room room = chatRooms[roomId];
        unordered_set<int> userList = room.joinedUsers;
        for(int user_socket : userList){
            if(user_socket != senderSock){
                sendProtoMessage(user_socket, msg_type, message);
            }
        }
    }
}

//// ** 원래는 Proto 헨들러의 이름도 Json헨들러와 이름이 같아야한다. 하지만 현제 클래스로 묶고 있지 않음으로 임시적으로 P를 붙인다.

// ProtoBuf 클라이언트 이름 변경
void handleMessageCSNameP(int clientSock, string& requestMessage){

    CSName protoFormat;
    if (!protoFormat.ParseFromString(requestMessage)) {
        perror("Not match type!");
    }
    Client client = clientSessions[clientSock];
    string originalName = client.name;
    string newName = protoFormat.name();

    changeNameProcess(clientSock, newName);

    SCSystemMessage responseMsg;
    responseMsg.set_text("이름이 " + originalName + "에서 " +  newName + " 으로 변경되었습니다.");
    sendProtoMessage(clientSock, Type::SC_SYSTEM_MESSAGE, responseMsg);

    if(client.isClientInAnyRoom()){
        sendToRoomMembers(client.enterRoomId, clientSock, Type::SC_SYSTEM_MESSAGE, responseMsg);
    }
}

// ProtoBuf 채팅방 리스트
void handleMessageCSRoomsP(int clientSock, string& requestMessage){

    SCRoomsResult responseMsg;

    for (const auto& [_, room] : chatRooms) {
        SCRoomsResult::RoomInfo* roomInfo = responseMsg.add_rooms();

        roomInfo->set_roomid(room.id);
        roomInfo->set_title(room.title);
        
        for (int memberSock : room.joinedUsers) {
            string memberName = clientSessions[memberSock].name; 
            roomInfo->add_members(memberName);
        }
    }

    sendProtoMessage(clientSock, Type::SC_ROOMS_RESULT, responseMsg);
}

// ProtoBuf 방 생성
void handleMessageCSCreateRoomP(int clientSock, string& requestMessage){

    CSCreateRoom protoFormat;
    if (!protoFormat.ParseFromString(requestMessage)) {
        perror("Not match type!");
    }
    string title = protoFormat.title();
    Client client = clientSessions[clientSock];

    SCSystemMessage responseMsg;
    if(client.isClientInAnyRoom()){
        responseMsg.set_text("대화 방에 있을 때는 방을 개설 할 수 없습니다.");
        sendProtoMessage(clientSock, Type::SC_SYSTEM_MESSAGE, responseMsg);
        return;
    }

    createRoomProcess(clientSock, title);

    responseMsg.set_text("방제[" + title + "] 방에 입장했습니다.");
    sendProtoMessage(clientSock, Type::SC_SYSTEM_MESSAGE, responseMsg);
}

// ProtoBuf 채팅방 참가
void handleMessageCSJoinRoomP(int clientSock, string& requestMessage){

    CSJoinRoom protoFormat;
    if (!protoFormat.ParseFromString(requestMessage)) {
        perror("Not match type!");
    }

    SCSystemMessage responseMsg;
    int roomId = protoFormat.roomid();
    Client client = clientSessions[clientSock];
    
    if(client.isClientInAnyRoom()){
        responseMsg.set_text("대화 방에 있을 때는 다른 방에 들어갈 수 없습니다.");
        sendProtoMessage(clientSock, Type::SC_SYSTEM_MESSAGE, responseMsg);
        return;
    }
    if(chatRooms.find(roomId) == chatRooms.end()){
        responseMsg.set_text("대화방이 존재하지 않습니다.");
        sendProtoMessage(clientSock, Type::SC_SYSTEM_MESSAGE, responseMsg);
        return;
    }
    string roomTitle = chatRooms[roomId].title;
    joinRoomProcess(clientSock, roomId);
    
    responseMsg.set_text("방제[" + roomTitle + "] 방에 입장했습니다.");
    sendProtoMessage(clientSock, Type::SC_SYSTEM_MESSAGE, responseMsg);
    
    responseMsg.set_text("[" + client.name + "] 님이 입장했습니다.");
    sendToRoomMembers(client.enterRoomId, clientSock, Type::SC_SYSTEM_MESSAGE, responseMsg);
}

// ProtoBuf 방 나가기
void handleMessageCSLeaveRoomP(int clientSock, string& requestMessage){

    SCSystemMessage responseMsg;
    Client client = clientSessions[clientSock];
    int roomId = client.enterRoomId;
    if (!client.isClientInAnyRoom()) {
        responseMsg.set_text("현재 대화방에 들어가 있지 않습니다.");
        sendProtoMessage(clientSock, Type::SC_SYSTEM_MESSAGE, responseMsg);
        return;
    }
    leaveRoomProcess(clientSock, roomId);

    responseMsg.set_text("방제[" + chatRooms[roomId].title + "] 대화 방에서 퇴장했습니다.");
    sendProtoMessage(clientSock, Type::SC_SYSTEM_MESSAGE, responseMsg);

    responseMsg.set_text("[" + client.name + "] 님이 퇴장했습니다.");
    sendToRoomMembers(client.enterRoomId, clientSock, Type::SC_SYSTEM_MESSAGE, responseMsg);
}

// ProtoBuf 채팅
void handleMessageCSChatP(int clientSock, string& requestMessage){

    CSChat protoFormat;
    if (!protoFormat.ParseFromString(requestMessage)) {
        perror("Not match type!");
    }

    SCSystemMessage responseMsg;
    string text = protoFormat.text();
    Client client = clientSessions[clientSock];
    int roomId = client.enterRoomId;

    if (!client.isClientInAnyRoom()) {
        responseMsg.set_text("현재 대화방에 들어가 있지 않습니다.");
        sendProtoMessage(clientSock, Type::SC_SYSTEM_MESSAGE, responseMsg);
        return;
    }

    SCChat chatMessage;
    chatMessage.set_member(client.name);
    chatMessage.set_text(text);
    
    sendToRoomMembers(client.enterRoomId, clientSock, Type::SC_CHAT, chatMessage);
}

// ProtoBuf 서버 종료
void handleMessageCSShutdownP(int clientSock, string& requestMessage){
    serverShutdownHandler(0);  
}


// 클라이언트 연결 종료
void disconnectClient(int closedSocket) {
    int roomId = clientSessions[closedSocket].enterRoomId;
    {
        unique_lock<mutex> lock(roomMutex);
        chatRooms[roomId].joinedUsers.erase(closedSocket);
    }
    {
        unique_lock<mutex> lock(userMutex);
        clientSessions.erase(closedSocket);
    }
    close(closedSocket);
}


//////
/// 메시지 핸들러
//////

unordered_map<string, function<void(int, string&)>> requestMessageHandler = {
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

// JSON 방식의 이벤트 처리기
void handleJsonEvent(int clientSocket){
    string serializedData = receiveMessage(clientSocket);
    if (serializedData.empty()) return;

    try {
        json requestMessage = json::parse(serializedData);
        string messageType = requestMessage["type"];
        // 메시지 헨들러
        if (requestMessageHandler.find(messageType) != requestMessageHandler.end()) {
            requestMessageHandler[messageType](clientSocket, serializedData);
        } else {
            cerr << "No handler found for message type: " << messageType << endl;
        }
    } catch (const json::parse_error& e) {
        cerr << "JSON parse error: " << e.what() << endl;
    }
}

// ProtoBuf 방식의 이벤트 처리기
void handleProtobufEvent(int clientSocket){
    string serializedDataType = receiveMessage(clientSocket);
    if (serializedDataType.empty()) return;

    Type incomingTypeMsg;
    if (!incomingTypeMsg.ParseFromString(serializedDataType)) {
        cerr << "Protobuf parse error" << endl;
    } 
    Type::MessageType messageType = incomingTypeMsg.type();
    cout << "[C->S: Protobuf Message received] Data: " << incomingTypeMsg.ShortDebugString() << endl;

    string serializedDataMessage = receiveMessage(clientSocket);

    // 메시지 헨들러
    if (protobufMessageHandler.find(messageType) != protobufMessageHandler.end()) {
        protobufMessageHandler[messageType](clientSocket, serializedDataMessage);
    } else {
        cerr << "No handler found for message type: " << messageType << endl;
    }
}


// 워커 쓰레드 함수
void worker() {
    while (isServerRunning.load()) {
        int clientSocket;
        {
            unique_lock<mutex> lock(workerQueueMutex);
            while (workerQueue.empty()) {
                workerQueueCV.wait(lock);
                if (!isServerRunning.load()) {
                    return;
                }
            }
            clientSocket = workerQueue.front();
            workerQueue.pop();
        }

        // 포멧에 따라 처리하는 헨들러가 다르다. 원래는 전략 패턴을 사용해서 구현한다.
        if (FORMAT == JSON) {
            handleJsonEvent(clientSocket); 
        }
        else if(FORMAT == PROTOBUF){
            handleProtobufEvent(clientSocket);
        }

        {
            unique_lock<mutex> lock(processingMutex);
            processingSockets.erase(clientSocket);
        }
    }
}

// 서버 강제 종료 인터럽트
void serverShutdownHandler(int sig) {
    isServerRunning.store(false);
    workerQueueCV.notify_all();
}

// 명령줄 인자를 통해 서버 매개변수 설정
void configureParameters(const int argc, char* argv[]) {
    for (int i = 1; i < argc; ++i) {
        if (strncmp(argv[i], "--port=", 7) == 0) {
            SERVER_PORT = stoi(argv[i] + 7);
        } else if (strncmp(argv[i], "--worker=", 9) == 0) {
            WORKER = stoi(argv[i] + 9); 
        } else if (strncmp(argv[i], "--format=", 9) == 0) {
            string format = argv[i] + 9;
            FORMAT = (format == "json");
        }
    }
}

int main(int argc, char* argv[]) {
    int serverSocket, clinetSocket;
    struct sockaddr_in sin;
    int opt = 1;
    int addrlen = sizeof(sin);

    configureParameters(argc, argv);

    signal(SIGINT, serverShutdownHandler);

    if ((serverSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) == 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    if (setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt))) {
        perror("setsockopt failed");
        exit(EXIT_FAILURE);
    }

    sin.sin_family = AF_INET;
    sin.sin_addr.s_addr = INADDR_ANY;
    sin.sin_port = htons(SERVER_PORT);

    if (bind(serverSocket, (struct sockaddr*)&sin, sizeof(sin)) < 0) {
        perror("bind() failed");
        exit(EXIT_FAILURE);
    }

    if (listen(serverSocket, 10) < 0) {
        perror("listen() failed");
        exit(EXIT_FAILURE);
    }

    int workerCount;
    vector<thread> workers;
    for (workerCount = 0; workerCount < WORKER; ++workerCount) {
        workers.emplace_back(worker);
    }

    string format = (FORMAT) ? "json" : "protobuf";
    cout << "Server listening on port " << SERVER_PORT << endl;
    cout << "message format : " << format << endl;
    cout << "worker : " << workerCount  << endl;

    fd_set readfds;

    while (isServerRunning.load()) {
        FD_ZERO(&readfds);
        FD_SET(serverSocket, &readfds);
        int maxSd = serverSocket;

        for (auto& [sock, _] : clientSessions) {
            FD_SET(sock, &readfds);
            maxSd = max(maxSd, sock);
        }

        struct timeval timeout = {1, 0};
        int activity = select(maxSd + 1, &readfds, NULL, NULL, &timeout);
        if (activity < 0) {
            if (errno == EINTR) continue;
            perror("select error");
            break;
        }

        if (!isServerRunning.load()) break;

        if (FD_ISSET(serverSocket, &readfds)) {
            if ((clinetSocket = accept(serverSocket, (struct sockaddr*)&sin, (socklen_t*)&addrlen)) < 0) {
                if (isServerRunning.load()) perror("accept");
                break;
            }
            
            string clientIP = inet_ntoa(sin.sin_addr);
            int clientPort = ntohs(sin.sin_port);
            cout << "Client Connect: Ip = " << clientIP << " Port = " << clientPort << "Socket = " << clinetSocket << "\n";

            string defaultName = "(" + clientIP + ", " + to_string(clientPort) + ")";
            {
                unique_lock<mutex> lock(workerQueueMutex);
                clientSessions.insert({clinetSocket, Client(defaultName)});
            }
        }

        for (auto& [clientSocket, _] : clientSessions) {
            if (FD_ISSET(clientSocket, &readfds)) 
            {
                unique_lock<mutex> lock(processingMutex);
                if (processingSockets.find(clientSocket) == processingSockets.end()) {
                    processingSockets.insert(clientSocket);
                    {
                        unique_lock<mutex> lock(workerQueueMutex);
                        workerQueue.push(clientSocket);
                    }
                    workerQueueCV.notify_one();
                }
            }
        }
    }

    isServerRunning.store(false);
    workerQueueCV.notify_all();
    for (auto& worker : workers) {
        cout << "thread finished...\n";
        worker.join();
    }
    for (auto& [clientFd, _] : clientSessions) {
        close(clientFd);
    }
    close(serverSocket);
    cout << "Server ShutDown\n";

    return 0;
}