#include "Reactor.h"
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/select.h>

#include <iostream>
#include <unistd.h>
#include <string.h>
#include <error.h>
#include <set>

using namespace std;

Reactor::Reactor(){
    // todo : 필요한 초기 설정
}

Reactor::~Reactor(){
    // todo : 리소스 정리
    close(serverSock);
}

/// @brief Reactor 초기 설정 : Server socket 설정
void Reactor::init(int serverPort){
    serverSock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if(serverSock < 0){
        throw runtime_error(string("socket failed: ") + strerror(errno));
    }

    struct sockaddr_in sin;
    memset(&sin, 0, sizeof(sin));
    sin.sin_family = AF_INET;
    sin.sin_port = htons(serverPort);
    sin.sin_addr.s_addr = INADDR_ANY;

    if(bind(serverSock, (struct sockaddr *)&sin, sizeof(sin)) < 0){
        throw runtime_error(string("bind failed: ") + strerror(errno));
    }

    if(listen(serverSock, 10) < 0){
        throw runtime_error(string("listen failed: ") + strerror(errno));
    }
}

/// @brief Server 실행
void Reactor::run(){
    // 정상적으로 서버 연결이 완료된 경우
    cout << "Server Started..." << endl;
    
    fd_set readFd;
    int maxFd = 0;

    set<int> clientSocks;

    while (true)
    {
        // todo : I/O multiplexing으로 연결 받기
        FD_ZERO(&readFd);
        FD_SET(serverSock, &readFd);
        maxFd = serverSock;

        for (int clientSock : clientSocks) {
            FD_SET(clientSock, &readFd);
            if (clientSock > maxFd) {
                maxFd = clientSock;
            }
        }

        int numReady = select(maxFd + 1, &readFd, NULL, NULL, NULL);

        if (numReady < 0) {
            cerr << "select() failed: " << strerror(errno) << endl;
            continue;
        } else if (numReady == 0) {
            continue;
        }

        

        if(FD_ISSET(serverSock, &readFd)){
            struct sockaddr_in clinetSin;
            socklen_t clientSinSize = sizeof(clinetSin);
            memset(&clinetSin, 0, sizeof(clinetSin));
            int clinetSock = accept(serverSock, (struct sockaddr*)& clinetSin, &clientSinSize);


            if(clinetSock >= 0){
                // 클라이언트 연결 등록
                // 클라이언트 등록
                string clientAddr = inet_ntoa(clinetSin.sin_addr);
                int clinetPort = ntohs(clinetSin.sin_port);
                cout << "Client Connect: Ip_" << clientAddr << " Port_" << clientAddr  << ")\n";

            }else {
                // todo : 연결 실패시 처리
            }
        }
    }
}

void Reactor::connectClient(int sock){
    
}
