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

int verbosity = 1;

void sendMessage(int socket,  string& message);
string receiveMessage(int socket);

//////////////////////////
// Clinet
/////////////////////////

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

    Clinet* findClientSession(int sock){
        return clinetSessions[sock];
    }

    bool isSessionExists(int sock){
        if(clinetSessions.find(sock) == clinetSessions.end()){
            cerr << "Not Found Client Session: " << sock << endl;
            throw runtime_error("세션 없음");
        }
    }

    map<int, Clinet*> getClientSessions(){
        return clinetSessions;
    }

private:
    ClientSession() {}

    map<int, Clinet*> clinetSessions;
};


//////////////////////////
// ChatRoom
/////////////////////////

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
        vector<json> memberList;
        for (int memberId : members) {
            Clinet* member = clientSession.findClientSession(memberId);
            if (member) {
                memberList.push_back(member->getName());
            }
        }
        return nlohmann::json{
            {"roomId", to_string(getRoomId())},
            {"title", getTitle()},
            {"members", memberList}
        };
    }

private:
    int roomId;
    string title;
    unordered_set<int> members;
    ClientSession& clientSession = ClientSession::getInstance();
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


//////////////////////////
// handler
/////////////////////////

class EventHandler{
public:
    EventHandler() {

        // 이벤트 헨틀러 등록 (this method help by GPT)
        messageHandler = {
            {"CSName", [this](int sock, string& message) { handleMessageCSName(sock, message); }},
            {"CSRooms", [this](int sock, string& message) { handleMessageCSRooms(sock, message); }},
            {"CSCreateRoom", [this](int sock, string& message) { handleMessageCSCreateRoom(sock, message); }},
            {"CSJoinRoom", [this](int sock, string& message) { handleMessageCSJoinRoom(sock, message); }},
            {"CSLeaveRoom", [this](int sock, string& message) { handleMessageCSLeaveRoom(sock, message); }},
            {"CSChat", [this](int sock, string& message) { handleMessageCSChat(sock, message); }},
            {"CSShutdown", [this](int sock, string& message) { handleMessageCSShutdown(sock, message); }}
        };
    }

    void handleMessage(int clinetSock, string& message) {
        json jsonData = json::parse(message);
        string messageType = jsonData["type"];

        if (messageHandler.find(messageType) != messageHandler.end()) {
            messageHandler[messageType](clinetSock, message);
        } else {
            cerr << "No handler found for message type: " << messageType << endl;
        }
    }



    // 클라이언트 이름 변경
    void handleMessageCSName(int sock, string& message){
        json data = json::parse(message);
        string name = data["name"];

        Clinet* clinet = clientSession.findClientSession(sock);
        int roomId = clinet->getEnterChatroom();
        clinet->setName(name);
            
        string systemMessage = "이름이 " +  name + " 으로 변경되었습니다.";
        sendSystemMessage(sock , systemMessage);
        if(roomId >= 0){
            notifyRoomMembers(roomId, sock, systemMessage);
        }
    }

    // 채팅방 목록 전송
    void handleMessageCSRooms(int sock, string& message){
        sendRoomInfomation(sock, chatRoomManager);
    }

    // 새로운 방 생성
    void handleMessageCSCreateRoom(int sock, string& message){
        json data = json::parse(message);
        string title = data["title"];
        
        Room chatRoom = chatRoomManager.createChatRoom(title);
        Clinet* clinet = clientSession.findClientSession(sock);
        int roomId = chatRoom.getRoomId();
        
        chatRoomManager.joinChatRoom(roomId, sock);
        clinet->setEnterChatroom(roomId);
        
        string systemMessage = "방제[" + title + "] 방에 입장했습니다.";
        sendSystemMessage(sock, systemMessage);
    }

    // 채팅방 참가
    void handleMessageCSJoinRoom(int sock, string& message){
        json data = json::parse(message);
        int roomId = data["roomId"];
        Clinet* clinet = clientSession.findClientSession(sock);
        Room* chatRoom = chatRoomManager.getChatRoom(roomId);

        if(clinet->getEnterChatroom() >= 0){
            string systemMessage = "대화 방에 있을 때는 다른 방에 들어갈 수 없습니다.";
            sendSystemMessage(sock, systemMessage);
            return;
        }
        if(!chatRoomManager.isChatRoomExists(roomId)){
            string systemMessage = "대화방이 존재하지 않습니다.";
            sendSystemMessage(sock, systemMessage);
            return;
        }

        chatRoomManager.joinChatRoom(roomId, sock);
        clinet->setEnterChatroom(chatRoom->getRoomId());

        string systemMessage = "방제[" + chatRoom->getTitle() + "] 방에 입장했습니다.";
        sendSystemMessage(sock, systemMessage);
        
        systemMessage = "[" + clinet->getName() + "] 님이 입장했습니다.";
        notifyRoomMembers(roomId, sock, systemMessage);

    }

    // 방 나가기
    void handleMessageCSLeaveRoom(int sock, string& message){
        Clinet* clinet = clientSession.findClientSession(sock);
        int roomId = clinet->getEnterChatroom();
        Room* chatRoom = chatRoomManager.getChatRoom(roomId);

        if(roomId < 0){
            string systemMessage = "현재 대화방에 들어가 있지 않습니다.";
            sendSystemMessage(sock, systemMessage);
            return;
        }
        
        chatRoomManager.joinChatRoom(roomId, sock);
        clinet->setEnterChatroom(-1);

        string systemMessage = "방제[" + chatRoom->getTitle() + "] 대화 방에서 퇴장했습니다.";
        sendSystemMessage(sock, systemMessage);
        
        systemMessage = "[" + clinet->getName() + "] 님이 퇴장했습니다.";
        notifyRoomMembers(roomId, sock, systemMessage);
    }

    // 채팅
    void handleMessageCSChat(int sock, string& message){
        json data = json::parse(message);
        string text = data["text"];

        Clinet* clinet = clientSession.findClientSession(sock);
        int roomId = clinet->getEnterChatroom();
        Room* chatRoom = chatRoomManager.getChatRoom(roomId);

        if(roomId < 0){
            string systemMessage = "현재 대화방에 들어가 있지 않습니다.";
            sendSystemMessage(sock, systemMessage);
            return;
        }
        
        for(int clinetSock : chatRoom->getMembers()){
            if(clinetSock != sock){
                sendChat(clinetSock, clinet->getName(), text);
            }
        }
    }

    // 방에 있는 모든 사용자에게 시스템 메시지 전송
    void notifyRoomMembers(int roomId, int senderSock, string message) {
        ChatRoomManager& chatRoomManager = ChatRoomManager::getInstance();
        Room* room = chatRoomManager.getChatRoom(roomId);
        for (int memberSock : room->getMembers()) {
            if (memberSock != senderSock) {
                sendSystemMessage(memberSock, message);
            }
        }
    }

    // 서버 종료
    void handleMessageCSShutdown(int sock, string& message){
        cout << "Shutdown" << endl;
        runServer.store(false);
    }

private:
    unordered_map<string, function<void(int, string&)>> messageHandler;

    ChatRoomManager& chatRoomManager = ChatRoomManager::getInstance();
    ClientSession& clientSession = ClientSession::getInstance();

    void sendSystemMessage(int sock, string& message){
        json data = {
            { "type" , "SCSystemMessage"},
            { "text", message }
        };
        string msg = data.dump();
        sendMessage(sock , msg);
    }

    void sendChat(int sock, string member , string& message){
        json data = {
            { "type" , "SCChat"},
            { "member", member },
            { "text", message }
        };
        string msg = data.dump();
        sendMessage(sock , msg);
    }

    void sendRoomInfomation(int sock, ChatRoomManager& roomInfo){
        json data = {
            { "type" , "SCRoomsResult"},
            { "rooms", roomInfo.toJson() }
        };
        string msg = data.dump();
        sendMessage(sock , msg);
    }

};

EventHandler eventHandler = EventHandler();
ClientSession& clientSession = ClientSession::getInstance();

class ConnectionClosedException : public runtime_error {
public:
    explicit ConnectionClosedException(const string& msg) : runtime_error(msg) {}
};

void sendMessage(int socket, string& message) { // (this method help by GPT)
    uint16_t dataSize = htons(message.length());
    vector<char> sendBuffer(sizeof(dataSize) + message.length());

    memcpy(sendBuffer.data(), &dataSize, sizeof(dataSize));
    memcpy(sendBuffer.data() + sizeof(dataSize), message.c_str(), message.length());

    size_t totalSent = 0;
    size_t dataLength = sendBuffer.size();
    
    while (totalSent < dataLength) {
        int sentBytes = send(socket, sendBuffer.data() + totalSent, dataLength - totalSent, MSG_NOSIGNAL);
        if (sentBytes < 0) {
            cerr << "Send() failed: " << strerror(errno) << endl;
            return;
        }
        totalSent += sentBytes;
    }

    if (verbosity <= 1) {
        printf("Send [S->C:총길이=%d바이트] %x(메시지크기) + %s\n", dataSize, dataSize, message.c_str());
    }
}


string receiveMessage(int socket){
    uint16_t recvDataSize = 0;
    char buffer[1024]; 

    cout << "Recv..";

    int recvByte = recv(socket, &recvDataSize, sizeof(recvDataSize), 0);
    if(recvByte <= 0){
        throw ConnectionClosedException("Client connection closed or recv error");
    }
    recvDataSize = ntohs(recvDataSize);
    
    cout << recvByte << "Size = " << recvDataSize << endl;

    int totalReceived = 0;
    while (totalReceived < recvDataSize) {
        recvByte = recv(socket, buffer + totalReceived, recvDataSize - totalReceived, 0);
        if (recvByte <= 0) {
            throw ConnectionClosedException("Client connection closed or recv error");
        }
        totalReceived += recvByte;
    }
    string recivedData(buffer, recvDataSize);

    if(verbosity <= 1){
        printf("Recv [C->S:총길이=%d바이트] %x(메시지크기) + %s\n", recvDataSize, recvDataSize, recivedData.c_str());
    }

    return recivedData;
}




void task() {
    while (quitWorker.load() == false) {
        int taskSock = -1;
        {
            unique_lock<mutex> lock(taskQueueMutex);
            while (taskQueue.empty()) {
                taskCV.wait(lock);
            }
            taskSock = taskQueue.front();
            taskQueue.pop();
            cout << "Do Work Socket:" << taskSock << endl;
        }

        
        try{
            string messages = receiveMessage(taskSock);
            eventHandler.handleMessage(taskSock, messages);

        } catch (const ConnectionClosedException& e) {
            cout << "연결 종료: " << e.what() << endl;
            string dummyMsg = "{\"type\":\"CSLeaveRoom\"}";
            eventHandler.handleMessage(taskSock, dummyMsg);
            clientSession.deleteClientSession(taskSock);
        } catch (const runtime_error& e) {
            cout << "런타임 오류 발생: " << e.what() << endl;
        } catch (const exception& e) {
            cout << "예외 발생: " << e.what() << endl;
        } catch (const exception_ptr& e) {
            cout << "포인터 문제 발생"  << endl;
        }
    }
}

class Worker{
public:



private:

};


class Reactor{  
public:



private:


};

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

        for (auto& client : clientSession.getClientSessions()) {
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
            
                clientSession.addClientSession(clientSock, clientAddr, clientPort);
                
            } else {
                cerr << "accept() failed: " << strerror(errno) << endl; 
            }
        }

        for (auto& client : clientSession.getClientSessions()) {
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
