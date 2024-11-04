#include "ChatServer.h"

ChatServer::ChatServer(int port) : serverPort(port){
    // 초기화 진행
    
}

ChatServer::~ChatServer(){
    //리소스 정리
}

void ChatServer::run(){
    // 기본적인 실행 흐름

    reactor = new Reactor(serverPort);
    reactor->init();
    reactor->run();


}

