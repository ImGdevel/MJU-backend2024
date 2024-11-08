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
#include <unordered_set>
#include <nlohmann/json.hpp>
using json = nlohmann::json;

using namespace std;

#define BUF 65536
#define PORT 10114
#define THREAD_COUNT 4

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
        throw runtime_error("recv error");
    }
    recvDataSize = htons(recvDataSize);
    
    int totalReceived = 0;
    while (totalReceived < recvDataSize) {
        recvByte = recv(socket, buffer + totalReceived, recvDataSize - totalReceived, 0);
        if (recvByte <= 0) {
            // todo : 실패 처리 (데이터를 받기 실패했거나 클라이언트 종료)
            throw runtime_error("recv error");
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
    Clinet(string ip, int port): name(ip + ", " + to_string(port)), ip(ip), port(port), enterChatroom(-1) {}  
    
    void setName(string name){
        this->name = name;
    }

    int getEnterChatroom(){
        return enterChatroom;
    }

    void setEnterChatroom(int roomId){
        this->enterChatroom = roomId;
    }

    string getName(){
        return name;
    }

private:
    string name;
    string ip;
    int port;
    int enterChatroom;
};

map<int, Clinet*> clinetSessions;


class ClientSession {
public:
    static ClientSession& getInstance() {
        static ClientSession instance;
        return instance;
    }

    void addClientSession(int sock, string& ip, int port){
        clinetSessions.insert({sock, new Clinet(ip, port)});
    }

    void deleteClientSession(int sock){
        clinetSessions.erase(sock);
    }

    Clinet* getClientSession(int sock){
        return clinetSessions[sock];
    }

    bool isSessionExists(int sock){
    if(clinetSessions.find(sock) == clinetSessions.end()){
        cerr << "Not Found Client Session: " << sock << endl;
        throw runtime_error("세션 없음");
    }
}


private:
    ClientSession() {}
    map<int, Clinet*> clinetSessions;
};

class Room {
public:
    Room(int roomId, const string& title) : roomId(roomId), title(title) {}

    bool joinChatRoom(int id) {
        return members.insert(id).second;
    }

    bool leaveChatRoom(int id) {
        return members.erase(id) > 0;
    }

    unordered_set<int> getMembers() const {
        return members;
    }

    string getTitle() const {
        return title;
    }

    int getRoomId() const {
        return roomId;
    }

    json toJson() const {
        return nlohmann::json{
            {"roomId", to_string(getRoomId())},
            {"title", getTitle()},
            {"members", vector<string>()}
        };
    }

private:
    int roomId;
    string title;
    unordered_set<int> members;
};

class ChatRoomManager {
public:
    static ChatRoomManager& getInstance() {
        static ChatRoomManager instance;
        return instance;
    }

    Room& createChatRoom(const string& title) {
        int roomId = ++lastRoomId;
        rooms.emplace_back(roomId, title);
        return rooms.back();
    }

    bool joinChatRoom(int roomId, int client) {
        int index = roomId - 1;
        if (index >= 0 && index < rooms.size()) {
            return rooms[index].joinChatRoom(client);
        }
        return false;
    }

    bool leaveChatRoom(int roomId, int client) {
        int index = roomId - 1;
        if (index >= 0 && index < rooms.size()) {
            return rooms[index].leaveChatRoom(client);
        }
        return false;
    }

    Room* getChatRoom(int roomId) {
        int index = roomId - 1;
        if (index >= 0 && index < rooms.size()) {
            return &rooms[index];
        }
        return nullptr;
    }

    bool isChatRoomExists(int roomId) const {
        int index = roomId - 1;
        return index >= 0 && index < rooms.size();
    }

    const vector<Room>& getRooms() const {
        return rooms;
    }

    json toJson() const {
        json jsonRooms = json::array();
        for (const auto& room : rooms) {
            jsonRooms.push_back(room.toJson());
        }
        return jsonRooms;
    }

private:
    ChatRoomManager() : lastRoomId(0) {}

    vector<Room> rooms; 
    int lastRoomId;
};


void checkSession(int sock){
    if(clinetSessions.find(sock) == clinetSessions.end()){
        cerr << "Not Found Client Session: " << sock << endl;
        throw runtime_error("세션 없음");
    }
}

//////////////////////////
// Message
/////////////////////////

void sendSystemMessage(int sock, string& message){
    json data = {
        { "type" , "SCSystemMessage"},
        { "text", message }
    };
    string msg = data.dump();
    sendMessage(sock , msg);
}

void sendChat(int sock, string& message){
    json data = {
        { "type" , "SCSystemMessage"},
        { "text", message }
    };
    string msg = data.dump();
    sendMessage(sock , msg);
}

void sendRoomInfomation(int sock, ChatRoomManager& roomInfo){
    json data = {
        { "type" , "SCRoomsResult"},
        { "text", roomInfo.toJson() }
    };
    string msg = data.dump();
    sendMessage(sock , msg);
}



//////////////////////////
// handleer
/////////////////////////


// 클라이언트 이름 변경
void handleMessageCSName(int sock, string& message){
    checkSession(sock);
    json data = json::parse(message);
    string name = data["name"];
    Clinet* clinet = clinetSessions[sock];
    clinet->setName(name);
    
    // todo: System Message
    string systemMessage = "이름이 " +  name + " 으로 변경되었습니다.";

    if(clinet->getEnterChatroom() < 0){
        sendSystemMessage(sock , systemMessage);
    }else{
        // 채팅 방에 들어간 경우
        // todo : 같은 채팅방에 들어간 클라이언트 객체들에게 전부 전송
    }

}

// 방 개설 목록 반환
void handleMessageCSRooms(int sock, string& message){
    ChatRoomManager& chatRoomManager = ChatRoomManager::getInstance();
    sendRoomInfomation(sock, chatRoomManager);
}

// 새로운 방을 만드는 이벤트
void handleMessageCSCreateRoom(int sock, string& message){
    json data = json::parse(message);
    string title = data["title"];
    
    ChatRoomManager& chatRoomManager = ChatRoomManager::getInstance();
    Room chatRoom = chatRoomManager.createChatRoom(title);
    chatRoom.joinChatRoom(sock);
    Clinet* clinet = clinetSessions[sock];
    clinet->setEnterChatroom(chatRoom.getRoomId());
    
    string systemMessage = "방제[" + title + "] 방에 입장했습니다.";
    sendSystemMessage(sock, systemMessage);
}

void handleMessageCSJoinRoom(int sock, string& message){
    Clinet* clinet = clinetSessions[sock];

    if(clinet->getEnterChatroom() >= 0){
        string systemMessage = "대화 방에 있을 때는 다른 방에 들어갈 수 없습니다.";
        sendSystemMessage(sock, systemMessage);
        return;
    }


    json data = json::parse(message);
    int roomId = data["roomId"];
    ChatRoomManager& chatRoomManager = ChatRoomManager::getInstance();
    if(!chatRoomManager.isChatRoomExists(roomId)){
        string systemMessage = "대화방이 존재하지 않습니다.";
        sendSystemMessage(sock, systemMessage);
        return;
    }

    Room* chatRoom = chatRoomManager.getChatRoom(roomId);
    chatRoomManager.joinChatRoom(roomId, sock);
    
    // todo : 방에 성공적으로 입장함
    string systemMessage = "방제[" + chatRoom->getTitle() + "] 방에 입장했습니다.";
    sendSystemMessage(sock, systemMessage);
    clinet->setEnterChatroom(chatRoom->getRoomId());

    systemMessage = "[" + clinet->getName() + "] 님이 입장했습니다.";
    sendSystemMessage(sock, systemMessage);
    for(int clinetSock : chatRoom->getMembers()){
        if(clinetSock != sock){
            sendMessage(clinetSock, systemMessage);
        }
    }

}

void handleMessageCSLeaveRoom(int sock, string& message){
    Clinet* clinet = clinetSessions[sock];
    int roomId = clinet->getEnterChatroom();
    if(roomId < 0){
        string systemMessage = "현재 대화방에 들어가 있지 않습니다.";
        sendSystemMessage(sock, systemMessage);
        return;
    }
    
    ChatRoomManager& chatRoomManager = ChatRoomManager::getInstance();
    Room* chatRoom = chatRoomManager.getChatRoom(roomId);
    chatRoomManager.joinChatRoom(roomId, sock);
    clinet->setEnterChatroom(-1);

    string systemMessage = "방제[" + chatRoom->getTitle() + "] 대화 방에서 퇴장했습니다.";
    sendSystemMessage(sock, systemMessage);
    
    systemMessage = "[" + clinet->getName() + "] 님이 퇴장했습니다.";

    for(int clinetSock : chatRoom->getMembers()){
        
        if(clinetSock != sock){
            sendSystemMessage(sock, systemMessage);
        }
    }
}

void handleMessageCSChat(int sock, string& message){

    // todo : 채팅방에 들어가 있는지 확인
    Clinet* clinet = clinetSessions[sock];
    int roomId = clinet->getEnterChatroom();
    if(roomId < 0){
        //이미 클라이언트가 방에 입장한 상태임
        string systemMessage = "현재 대화방에 들어가 있지 않습니다.";
        sendSystemMessage(sock, systemMessage);
        return;
    }
    
    json data = json::parse(message);
    string text = data["text"];

    // 들어가 있다면 채팅처리
    ChatRoomManager chatRoomManager = ChatRoomManager::getInstance();
    Room* chatRoom = chatRoomManager.getChatRoom(roomId);
    
    string chatText = "(" + clinet->getName() + "): " + text;
    for(int clinetSock : chatRoom->getMembers()){
        if(clinetSock != sock){
            sendChat(clinetSock, chatText);
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

        try{
            string messages = receiveMessage(taskSock);
            handleJsonMessage(taskSock, messages);
        } catch(const runtime_error& e){
            cout << "연결 종료" << endl;
            clinetSessions.erase(taskSock);
        }
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
    

    cout << "Server Stared..." << endl;

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
                cout << "Client " << clientSock << "와 통신 중..." << endl;
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
