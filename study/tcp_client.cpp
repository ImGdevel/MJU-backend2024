#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>
#include <cstring>
#include <iostream>
#include <cerrno>

using namespace std;

#define PORT 10114
#define BUF_SIZE 65536

int main() {
    int s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (s < 0) {
        cerr << "socket() failed: " << strerror(errno) << endl;
        return 1;
    }

    struct sockaddr_in sin;
    memset(&sin, 0, sizeof(sin));
    sin.sin_family = AF_INET;
    sin.sin_port = htons(PORT);
    sin.sin_addr.s_addr = inet_addr("127.0.0.1");

    if (connect(s, (struct sockaddr *)&sin, sizeof(sin)) < 0) {
        cerr << "connect() failed: " << strerror(errno) << endl;
        close(s);
        return 1;
    }

    char buf[BUF_SIZE];
    
    while (cin >> buf) {

        int sendSize = send(s, buf, strlen(buf), MSG_NOSIGNAL); 
        
        if (sendSize < 0) {
            cerr << "send() failed: " << strerror(errno) << endl;
            break;
        } else {
            cout << "Sent: " << sendSize << " bytes" << endl;
        }
        
        memset(buf, 0, sizeof(buf)); 

        int recvSize;
        while ((recvSize = recv(s, buf, sizeof(buf), 0)) > 0) {
            cout << "Received: " << recvSize << " bytes" << endl;
            memset(buf, 0, sizeof(buf)); 
        }
        
        if (recvSize < 0) {
            cerr << "recv() failed: " << strerror(errno) << endl;
            break;
        } else if (recvSize == 0) {
            cout << "Socket closed by server." << endl;
            break;
        }
    }

    close(s);
    return 0;
}
