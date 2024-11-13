#ifndef REACTOR
#define REACTOR

class Reactor{
public:
    Reactor();
    ~Reactor();

    void init(int port);
    void run();

    void connectClient(int sock);
    
private:

    int serverSock;
    
};

#endif
