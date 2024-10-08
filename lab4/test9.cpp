#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>
#include <error.h>
#include <string.h>
#include <iostream>

using namespace std;

#define BUF 1024 * 100

int main(){

    int s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if(s < 0){
        cerr <<  "socket() failed: " << strerror(errno) << endl;
        return 1;
    }

    struct sockaddr_in sin;
    memset(&sin, 0, sizeof(sin));
    sin.sin_family = AF_INET;
    sin.sin_port = htons(10001);
    sin.sin_addr.s_addr = inet_addr("127.0.0.1");

    if(connect(s, (struct sockaddr *)&sin, sizeof(sin)) < 0){
        cerr << "connct() failed: " << strerror(errno) << endl;
        return 1;
    }
    
    char buf[BUF];
    int sendSize = send(s,buf, sizeof(buf), MSG_NOSIGNAL);
    if(sendSize < 0){
        cerr << "Send() failed : " << strerror(errno) << endl;
    }else{
        cout << "Sent: "<< sendSize << "bytes" << endl;
    }

    char recv_buf[65536];
    int recvSize, total = 0;
    while (total < sendSize) {
        recvSize = recv(s, recv_buf, sizeof(recv_buf), 0);
        if (recvSize < 0) {
            cerr << "recv() failed: " << strerror(errno) << endl;
            break;
        } else if (recvSize == 0) {
            cout << "Socket closed by server." << endl;
            break;
        } else {
            cout << "Received: " << recvSize << " bytes" << endl;
            total += recvSize;
        }
    }

    close(s);
    return 0;
}