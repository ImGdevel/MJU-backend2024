#ifndef CHAT_SERVER
#define CHAT_SERVER

#include <iostream>
#include "Reactor.h"
using namespace std;

class Reactor;
class WorkerPool;

class ChatServer{
public:
    ChatServer(int port);
    ~ChatServer();

    void run();

private:
    int serverPort;
    Reactor* reactor;

};

#endif