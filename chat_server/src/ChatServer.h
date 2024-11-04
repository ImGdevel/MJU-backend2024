#ifndef CHAT_SERVER
#define CHAT_SERVER

#include <iostream>

using namespace std;

class ChatServer{
public:
    ChatServer(int port);
    ~ChatServer();

    void run();

private:
    int serverPort;
    
};

#endif