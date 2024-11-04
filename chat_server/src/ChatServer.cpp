#include "ChatServer.h"

ChatServer::ChatServer(int port) : reactor(new Reactor(port)){
    // 초기화 진행
}

ChatServer::~ChatServer(){
    //리소스 정리
}

void ChatServer::run(){
    // 기본적인 실행 흐름
    try {
        reactor->init();
        reactor->run();
    }
    catch(const runtime_error& e){
        cerr << e.what() << endl;
    }
}

