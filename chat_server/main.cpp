
#include "src/ChatServer.h"
//#include "tempServer.cpp"
#include <iostream>


int main(int argc, char* argv[]){
    int defaultServerPort = 10114;

    ChatServer server(defaultServerPort, 2);
    server.run();

    return 0;
}