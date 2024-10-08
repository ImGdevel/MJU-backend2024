#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>
#include <error.h>
#include <string.h>
#include <iostream>

using namespace std;

#define BUF 65536

int main(){

    int passiveSock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if(passiveSock < 0){
        cerr <<  "socket() failed: " << strerror(errno) << endl;
        return 1;
    }

    struct sockaddr_in sin;
    memset(&sin, 0, sizeof(sin));
    sin.sin_family = AF_INET;
    sin.sin_port = htons(10114);
    sin.sin_addr.s_addr = INADDR_ANY;

    if(bind(passiveSock, (struct sockaddr*)&sin, sizeof(sin))){
        cerr << "bind() failed: " << strerror(errno) << endl;
        return 1;
    }

    if(listen(passiveSock, 10) < 0){
        cerr << "listen() failed: " << strerror(errno) << endl;
        return 1;
    }
    
    cout << "Server Strated..." << endl;

    while (true)
    {
        memset(&sin, 0, sizeof(sin));
        unsigned int sin_len = sizeof(sin);
        int clientSock = accept(passiveSock, (struct sockaddr *)&sin, &sin_len);
        if(clientSock < 0){
            cerr << "accept() failed:"  << strerror(errno) << endl;
            return 1;
        }

        char buf[BUF];
        int numRecv = recv(clientSock, buf, sizeof(buf), 0);
        if(numRecv == 0){
            cout << "Socket closed: " << clientSock << endl;
        }else if(numRecv < 0){
            cerr << "recv() failed: " << strerror(errno) << endl;
        }else{
            cout << "Recived: " << numRecv << "Bytes" << clientSock << endl;
        }

        int offset = 0;
        while (offset < numRecv)
        {
            int numSend = send(clientSock, buf + offset, numRecv - offset, 0);
            if(numSend < 0){
                cerr << "send() failed: " << strerror(errno) << endl;
            }else{
                cout << "Sent: " << numSend << endl;
                offset += numSend;
            }
        }
        close(clientSock);
    }
    
    close(passiveSock);
    return 0;
}