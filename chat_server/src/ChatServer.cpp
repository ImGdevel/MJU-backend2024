#include "ChatServer.h"
#include "Reactor.h"
#include "WorkerPool.h"

#include <iostream>

using namespace std;

ChatServer::ChatServer(int port = 8080, int thread = 2) : serverPort(port), reactor(new Reactor()), workerPool(new WorkerPool(thread)) {
    // 초기화 진행
}

ChatServer::~ChatServer(){
    delete reactor;
}

void ChatServer::run(){
    // 기본적인 실행 흐름
    try {
        reactor->init(serverPort);
        reactor->run();
    }
    catch(const runtime_error& e){
        cerr << e.what() << endl;
    }
}

