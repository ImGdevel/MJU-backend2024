#include <iostream>
#include <condition_variable>
#include <thread>
#include <atomic>
#include <queue>
#include <vector>
#include <arpa/inet.h>
#include <string.h>
#include <set>
#include <unistd.h>
#include <stdexcept>
#include <unordered_map>
#include <nlohmann/json.hpp>
using json = nlohmann::json;

using namespace std;

#define BUF 65536
#define PORT 10114
#define THREAD_COUNT 2

atomic<bool> quitWorker(false);
atomic<bool> runServer(true);


vector<thread> workerPool;
queue<int> taskQueue;
mutex taskQueueMutex;
condition_variable taskCV;


string msgType = "json";



void sendMessage(int socket,  string& message) {

    uint16_t dataSize = htons(message.length());
    vector<char> sendBuffer(sizeof(dataSize) + message.length());

    memcpy(sendBuffer.data(), &dataSize, sizeof(dataSize));
    memcpy(sendBuffer.data() + sizeof(dataSize), message.c_str(), message.length());

    int sendSize = send(socket, sendBuffer.data(), sendBuffer.size(), MSG_NOSIGNAL);
    if (sendSize < 0) {
        cerr << "Send() failed: " << strerror(errno) << endl;
    } else {
        cout << "Sent: " << sendSize << " bytes" << endl;
    }
}



string receiveMessage(int socket){
    uint16_t recvDataSize = 0;
    char buffer[1024]; 

    int recvByte = recv(socket, &recvDataSize, sizeof(recvDataSize), 0);
    if(recvByte <= 0){
        // todo : 실패 처리 (데이터를 받기 실패했거나 클라이언트 종료)
    }
    recvDataSize = htons(recvDataSize);
    
    int totalReceived = 0;
    while (totalReceived < recvDataSize) {
        recvByte = recv(socket, buffer + totalReceived, recvDataSize - totalReceived, 0);
        if (recvByte <= 0) {
            // todo : 실패 처리 (데이터를 받기 실패했거나 클라이언트 종료)
        }
        totalReceived += recvByte;
    }

    string recivedData(buffer, recvDataSize);
    cout << "Recived Data: " << recivedData << " size(" << recvDataSize  << ")"<< endl;
    return recivedData;
}


// 클라이언트 세션 관리
class Clinet{
public:
    Clinet(string ip, int port): name(ip), ip(ip), port(port), connect(true), enterChatroom(-1) {}  
    
    void setName(string name){
        this->name = name;
    }

    int getEnterChatroom(){
        return enterChatroom;
    }

    string getName(){
        return name;
    }

private:
    string name;
    string ip;
    int port;
    bool connect;
    int enterChatroom;
};

map<int, Clinet*> clinetSessions;


class Room {
public:
    Room(int roomId, const string& title) : roomId(roomId), title(title) {}

    bool joinChatRoom(int sock) {
        return members.insert(sock).second;
    }

    bool leaveChatRoom(int sock) {
        return members.erase(sock) > 0;
    }

    set<int> getJoinedClients() const {
        return members;
    }

    string getTitle() const {
        return title;
    }

private:
    int roomId;
    string title;
    set<int> members;
};

class ChatRoomManager {
public:
    static ChatRoomManager& getInstance() {
        static ChatRoomManager instance;
        return instance;
    }

    Room& createChatroom(const string& title) {
        lastRoomId++;
        rooms.emplace(lastRoomId, Room(lastRoomId, title));
        return rooms.at(lastRoomId);
    }

    bool joinChatroom(int roomId, int client) {
        Room* room = getChatroom(roomId);
        return room && room->joinChatRoom(client);
    }

    bool leaveChatroom(int roomId, int client) {
        Room* room = getChatroom(roomId);
        return room && room->leaveChatRoom(client);
    }

    Room* getChatroom(int roomId) {
        auto it = rooms.find(roomId);
        return (it != rooms.end()) ? &(it->second) : nullptr;
    }

    bool isChatroomExists(int roomId) const {
        return rooms.find(roomId) != rooms.end();
    }

private:
    ChatRoomManager() : lastRoomId(0) {}

    unordered_map<int, Room> rooms;
    int lastRoomId;
};

// utils JsonFomat으로 만듬
string makeJsonFormat(string& message){
    json data = {
        { "type" , "SCSystemMessage"},
        { "text", message }
    };
    return data.dump();
}


void checkSession(int sock){
    if(clinetSessions.find(sock) == clinetSessions.end()){
        // 클라이언트 세션이 존재하는 경우
        cerr << "Not Found Client Session: " << sock << endl;
        // todo : 에러 처리
        throw runtime_error("세션 없음");
    }
    
}

// 클라이언트 이름 변경
void handleMessageCSName(int sock, string& message){
    checkSession(sock);
    json data = json::parse(message);
    string name = data["name"];
    Clinet* clinet = clinetSessions[sock];
    clinet->setName(name);
    
    // todo: System Message
    string systemMessage = "이름이 " +  name + " 으로 변경되었습니다.";
    string messageSend = makeJsonFormat(systemMessage);

    if(clinet->getEnterChatroom() < 0){
        sendMessage(sock ,messageSend);
    }else{
        // 채팅 방에 들어간 경우
        // todo : 같은 채팅방에 들어간 클라이언트 객체들에게 전부 전송
    }

}

void handleMessageCSRooms(int sock, string& message){
    cout << "Rooms" << endl;
    
    
}

// 새로운 방을 만드는 이벤트
void handleMessageCSCreateRoom(int sock, string& message){
    json data = json::parse(message);
    string title = data["title"];
    
    ChatRoomManager chatRoomManager = ChatRoomManager::getInstance();
    Room chatRoom = chatRoomManager.createChatroom(title);
    chatRoom.joinChatRoom(sock);

    string log = "방제[" + title + "] 방에 입장했습니다.";
    string msg = makeJsonFormat(log);
    sendMessage(sock , msg);
}

void handleMessageCSJoinRoom(int sock, string& message){
    cout << "?" << endl;
    Clinet* clinet = clinetSessions[sock];
    if(clinet->getEnterChatroom() >= 0){
        //이미 클라이언트가 방에 입장한 상태임
        string log = "대화 방에 있을 때는 다른 방에 들어갈 수 없습니다.";
        string msg = makeJsonFormat(log);
        sendMessage(sock , msg);
        return;
    }

    cout << "??" << endl;

    json data = json::parse(message);
    int roomId = data["roomId"];
    ChatRoomManager chatRoomManager = ChatRoomManager::getInstance();

    cout << "???" << endl;
    if(chatRoomManager.isChatroomExists(roomId)){
        string log = "대화방이 존재하지 않습니다.";
        string msg = makeJsonFormat(log);
        sendMessage(sock , msg);
        return;
    }

    cout << "????" << endl;
    Room* chatRoom = chatRoomManager.getChatroom(roomId);
    chatRoomManager.joinChatroom(roomId, sock);

    cout << "?????" << endl;
    
    // todo : 방에 성공적으로 입장함
    string log = "방제[" + chatRoom->getTitle() + "] 방에 입장했습니다.";
    string msg = makeJsonFormat(log);
    sendMessage(sock , msg);

    log = "[" + clinet->getName() + "] 님이 입장했습니다.";
    msg = makeJsonFormat(log);
    for(int clinetSock : chatRoom->getJoinedClients()){
        
        if(clinetSock != sock){
            sendMessage(clinetSock, msg);
        }
    }

}

void handleMessageCSLeaveRoom(int sock, string& message){
    cout << "LeaveRoom" << endl;
}

void handleMessageCSChat(int sock, string& message){
    cout << "Chat" << endl;

    // todo : 채팅방에 들어가 있는지 확인
    Clinet* clinet = clinetSessions[sock];
    int roomId = clinet->getEnterChatroom();
    if(roomId < 0){
        //이미 클라이언트가 방에 입장한 상태임
        string log = "현재 대화방에 들어가 있지 않습니다.";
        string msg = makeJsonFormat(log);
        sendMessage(sock , msg);
        return;
    }
    
    json data = json::parse(message);
    string text = data["text"];

    // 들어 가있다면 채팅처리
    ChatRoomManager chatRoomManager = ChatRoomManager::getInstance();
    Room* chatRoom = chatRoomManager.getChatroom(roomId);
    
    string log = "(" + clinet->getName() + "): " + text;
    string jsonMsg = makeJsonFormat(log);
    for(int clinetSock : chatRoom->getJoinedClients()){
        if(clinetSock != sock){
            sendMessage(clinetSock, jsonMsg);
        }
    }
}

void handleMessageCSShutdown(int sock, string& message){
    cout << "Shutdown" << endl;
    runServer.store(false);
}


/*
클라이언트->서버가 전송하는 메시지 타입
CSName
CSRooms
CSCreateRoom = { title }
CSJoinRoom = { roomId }
CSChat = { text }
CSLeaveRoom
CSShutdown
*/


/*
서버->클라이언트 가 전송하는 타입
SCRoomsResult = { }
SCChat
SCSystemMessage
*/

unordered_map<string, function<void(int, string&)>> messageHandler = {
    {"CSName", handleMessageCSName},
    {"CSRooms", handleMessageCSRooms},
    {"CSCreateRoom", handleMessageCSCreateRoom},
    {"CSJoinRoom", handleMessageCSJoinRoom},
    {"CSLeaveRoom", handleMessageCSLeaveRoom},
    {"CSChat", handleMessageCSChat},
    {"CSShutdown", handleMessageCSShutdown},
};


void handleJsonMessage(int clinetSock, string& message) {
    json jsonData = json::parse(message);
    string messageType = jsonData["type"];
    if (messageHandler.find(messageType) != messageHandler.end()) {
        messageHandler[messageType](clinetSock, message);
    } else {
        cerr << "No handler found for message type: " << messageType << endl;
    }
}



void task() {
    while (quitWorker.load() == false) {
        int taskSock;
        {
            unique_lock<mutex> lock(taskQueueMutex);
            while (taskQueue.empty()) {
                taskCV.wait(lock);
            }
            taskSock = taskQueue.front();
            taskQueue.pop();
        }

        string messages = receiveMessage(taskSock);

        // 메시지 타입에 따라 이벤트 handler
        handleJsonMessage(taskSock, messages);
    }
}

int main() {
    quitWorker = false;

    for (int i = 0; i < THREAD_COUNT; i++) {
        workerPool.emplace_back(task);
    }

    int serverSock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (serverSock < 0) {
        throw runtime_error(string("socket failed: ") + strerror(errno));
    }

    struct sockaddr_in sin;
    memset(&sin, 0, sizeof(sin));
    sin.sin_family = AF_INET;
    sin.sin_port = htons(PORT);
    sin.sin_addr.s_addr = INADDR_ANY;

    if (bind(serverSock, (struct sockaddr *)&sin, sizeof(sin)) < 0) {
        throw runtime_error(string("bind failed: ") + strerror(errno));
    }

    if (listen(serverSock, 10) < 0) {
        throw runtime_error(string("listen failed: ") + strerror(errno));
    }

    fd_set readFd;
    int maxFd = 0;
    
    while (runServer.load()) {
        FD_ZERO(&readFd);
        FD_SET(serverSock, &readFd);
        maxFd = serverSock;

        for (auto& client : clinetSessions) {
            int clientSock = client.first;
            FD_SET(clientSock, &readFd);
            
            if (clientSock > maxFd) maxFd = clientSock;
        }
        int numReady = select(maxFd + 1, &readFd, NULL, NULL, NULL);
        if (numReady < 0) {
            cerr << "select() failed: " << strerror(errno) << endl;
            continue;
        }

        if (FD_ISSET(serverSock, &readFd)) {
            struct sockaddr_in clientSin;
            socklen_t clientSinSize = sizeof(clientSin);
            memset(&clientSin, 0, sizeof(clientSin));
            int clientSock = accept(serverSock, (struct sockaddr*)&clientSin, &clientSinSize);

            if (clientSock >= 0) {
                string clientAddr = inet_ntoa(clientSin.sin_addr);
                int clientPort = ntohs(clientSin.sin_port);
                cout << "Client Connect: Ip_" << clientAddr << " Port_" << clientPort << "\n";
                
                if(clientSock < maxFd) maxFd = clientSock;
                
                FD_SET(clientSock, &readFd);
            
                clinetSessions.insert({clientSock, new Clinet(clientAddr, clientPort)});
                
            } else {
                cerr << "accept() failed: " << strerror(errno) << endl; 
            }
        }

        for (auto& client : clinetSessions) {
            int clientSock = client.first;
            if (FD_ISSET(clientSock, &readFd)) {
                //cout << "Client " << clientSock << "와 통신 중..." << endl;
                {
                    unique_lock<mutex> lock(taskQueueMutex);
                    taskQueue.push(clientSock);
                    taskCV.notify_one();
                }
            }
        }
    }

    quitWorker.store(true);
    taskCV.notify_all();
    for (auto& t : workerPool) {
        if (t.joinable()) {
            t.join();
        }
    }

    close(serverSock);
    return 0;
}
