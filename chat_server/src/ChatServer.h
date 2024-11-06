#ifndef CHAT_SERVER
#define CHAT_SERVER

class Reactor;
class WorkerPool;

class ChatServer{
public:
    ChatServer(int port, int thread);
    ~ChatServer();

    void run();

private:
    Reactor* reactor;
    WorkerPool* workerPool;
    int serverPort;
};

#endif