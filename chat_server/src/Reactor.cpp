#include "Reactor.h"
#include <arpa/inet.h>
#include <sys/socket.h>
#include <iostream>
#include <unistd.h>
#include <string.h>
#include <error.h>

using namespace std;

Reactor::Reactor(int serverPort): serverPort(serverPort){
    // todo : 필요한 초기 설정
}

Reactor::~Reactor(){
    // todo : 리소스 정리
}

/// @brief Reactor 초기 설정 : Server socket 설정
void Reactor::init(){
    serverSock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if(serverSock < 0){
        cerr <<  "socket() failed: " << strerror(errno) << endl;
        // todo : socket 연결 실패 처리
        return;
    }

    struct sockaddr_in sin;
    memset(&sin, 0, sizeof(sin));
    sin.sin_family = AF_INET;
    sin.sin_port = htons(serverPort);
    sin.sin_addr.s_addr = INADDR_ANY;

    if(bind(serverSock, (struct sockaddr *)&sin, sizeof(sin)) < 0){
        cerr << "bind() failed: " << strerror(errno) << endl;
        // todo : bind 연결 실패 처리
        return;
    }

    if(listen(serverSock, 10) < 0){
        cerr << "listen() failed: " << strerror(errno) << endl;
        // todo : listen 실패처리
        return;
    }
}

/// @brief Server 실행
void Reactor::run(){

    // 정상적으로 서버 연결이 완료된 경우
    cout << "Server Started..." << endl;
    
    while (true)
    {
        // todo : I/O multiplexing으로 연결 받기
    }

}
