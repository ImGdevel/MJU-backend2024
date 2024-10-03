#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <string.h>
#include <unistd.h>

#include <iostream>
#include <string>

#define BUF 10000

using namespace std;

int main(){

    int s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if(s <0) return 1;

    struct sockaddr_in sin, recv_sin;
    socklen_t recv_sin_size = sizeof(recv_sin);
    memset(&sin, 0 , sizeof(sin));
    sin.sin_family = AF_INET;
    sin.sin_port = htons(10114);
    sin.sin_addr.s_addr = INADDR_ANY;

    if(bind(s, (struct sockaddr *) &sin, sizeof(sin))){
        cerr << strerror(errno) << endl;
    }

    char buf[BUF];
    memset(buf, 0 , sizeof(buf));
    cout << "Server Started..." << endl;
    int recv_size;
    while (true)
    {
        if((recv_size = recvfrom(s, buf, sizeof(buf), 0, (struct sockaddr*)&recv_sin, &recv_sin_size)) < 0){
            cerr << strerror(errno) << endl;
            return 1;
        }
        
        //확인 용
        cout << buf << endl;

        if(sendto(s, buf,  recv_size, 0, (struct sockaddr*)&recv_sin, recv_sin_size) < 0){
            cerr << strerror(errno) << endl;
            return 1;
        }
    }
    
    close(s);
    return 0;
}
