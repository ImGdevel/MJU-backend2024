#ifndef REACTOR
#define REACTOR

class Reactor{
public:
    Reactor(int serverPort);
    ~Reactor();

    void init();
    void run();
    
private:

    int serverSock;
    int serverPort;
    
};

#endif
