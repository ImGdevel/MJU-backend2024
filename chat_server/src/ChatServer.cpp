#include "ChatServer.h"

ChatServer::ChatServer(int port) : serverPort(port){
    // 초기화 진행
}

ChatServer::~ChatServer(){
    //리소스 정리
}

void ChatServer::run(){
    // 기본적인 실행 흐름
    cout << "Server Started..." << endl;
    cout << "Server Port : " << serverPort << endl;
    
    while (true)
    {
        // todo : Server 로직 작성

        // 1. 연결 대기 할 서버 소캣 (passive socket 설정)
        
        // 2. bind 및 litsen하며 대기
        
        // 3. I/O multiplexing으로 각 소캣 대기

        // 4. 연결이 발생하면 수신

        // 5. 수신이 발생한 소캣에 대하여 accept후 active소캣 생성

        // 6. 각 acctive 소캣을 worker에게 전달

        // 7. 각 세션 저장

        // 8. 

    }

}
