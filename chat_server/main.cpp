
#include "src/ChatServer.h"
#include <iostream>


int main(int argc, char* argv[]){
    int defaultServerPort = 10114;

    ChatServer server(defaultServerPort);
    server.run();

    return 0;
}