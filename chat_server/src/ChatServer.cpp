#include "ChatServer.h"

ChatServer::ChatServer(int port) : serverPort(port),  reactor(new Reactor()){
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

