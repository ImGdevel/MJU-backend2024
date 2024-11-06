#include <iostream>
#include <condition_variable>
#include <thread>
#include <atomic>
#include <queue>
#include <vector>
#include <arpa/inet.h>
#include <string.h>
#include <set>
#include <unistd.h>
#include <stdexcept>

using namespace std;

#define BUF 65536

#define PORT 10114
#define THREAD_COUNT 2

atomic<bool> quitWorker(false);
vector<thread> workerPool;
queue<int> taskQueue;
mutex taskQueueMutex;
condition_variable taskCV;

set<int> clientSocks;

void task() {
    while (quitWorker.load() == false) {
        int taskSock;
        {
            unique_lock<mutex> lock(taskQueueMutex);
            while (taskQueue.empty()) {
                taskCV.wait(lock);
            }
            taskSock = taskQueue.front();
            taskQueue.pop();
        }

        char buf[BUF];
        memset(buf, 0, sizeof(buf));
        int numRecv = recv(taskSock, buf, sizeof(buf), 0);
        if(numRecv == 0){
            cout << "Socket closed: " << taskSock << endl;
        }else if(numRecv < 0){
            cerr << "recv() failed: " << strerror(errno) << endl;
        }else{
            cout << "Recived: " << numRecv << " Bytes, clientSock " << taskSock << endl;
        }

        int sendSize = send(taskSock, buf, numRecv, MSG_NOSIGNAL);
        if(sendSize < 0){
            cerr << "Send() failed : " << strerror(errno) << endl;
        }else{
            cout << "Sent: "<< sendSize << "bytes" << endl;
        }

        std::cout << "Data: " << buf << std::endl;
    }
}

int receiveAll(int sock, char* buf, int bufSize) {
    int totalReceived = 0;
    while (totalReceived < bufSize) {
        int numRecv = recv(sock, buf + totalReceived, bufSize - totalReceived, 0);
        
        if (numRecv == 0) {
            std::cout << "Socket closed: " << sock << std::endl;
            break;
        } else if (numRecv < 0) {
            std::cerr << "recv() failed: " << strerror(errno) << std::endl;
            break;
        }

        totalReceived += numRecv;
        std::cout << "Received: " << numRecv << " bytes, clientSock " << sock << std::endl;
    }
    return totalReceived;
}

int main() {
    quitWorker = false;

    for (int i = 0; i < THREAD_COUNT; i++) {
        workerPool.emplace_back(task);
    }

    int serverSock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (serverSock < 0) {
        throw runtime_error(string("socket failed: ") + strerror(errno));
    }

    struct sockaddr_in sin;
    memset(&sin, 0, sizeof(sin));
    sin.sin_family = AF_INET;
    sin.sin_port = htons(PORT);
    sin.sin_addr.s_addr = INADDR_ANY;

    if (bind(serverSock, (struct sockaddr *)&sin, sizeof(sin)) < 0) {
        throw runtime_error(string("bind failed: ") + strerror(errno));
    }

    if (listen(serverSock, 10) < 0) {
        throw runtime_error(string("listen failed: ") + strerror(errno));
    }

    fd_set readFd;
    int maxFd = 0;
    

    while (true) {
        FD_ZERO(&readFd);
        FD_SET(serverSock, &readFd);
        maxFd = serverSock;

        for (int clientSock : clientSocks) {
            FD_SET(clientSock, &readFd);
            
            if (clientSock > maxFd) maxFd = clientSock;
        }
        cout << "event waiting" << endl;
        int numReady = select(maxFd + 1, &readFd, NULL, NULL, NULL);
        if (numReady < 0) {
            cerr << "select() failed: " << strerror(errno) << endl;
            continue;
        }

        if (FD_ISSET(serverSock, &readFd)) {
            struct sockaddr_in clientSin;
            socklen_t clientSinSize = sizeof(clientSin);
            memset(&clientSin, 0, sizeof(clientSin));
            int clientSock = accept(serverSock, (struct sockaddr*)&clientSin, &clientSinSize);

            if (clientSock >= 0) {
                string clientAddr = inet_ntoa(clientSin.sin_addr);
                int clientPort = ntohs(clientSin.sin_port);
                cout << "Client Connect: Ip_" << clientAddr << " Port_" << clientPort << "\n";
                
                if(clientSock < maxFd) maxFd = clientSock;
                
                FD_SET(clientSock, &readFd);
                clientSocks.insert(clientSock);
                
            } else {
                cerr << "accept() failed: " << strerror(errno) << endl; 
            }
        }

        for (int clientSock : clientSocks) {
            if (FD_ISSET(clientSock, &readFd)) {
                cout << "Client " << clientSock << "와 통신 중..." << endl;
                {
                    unique_lock<mutex> lock(taskQueueMutex);
                    taskQueue.push(clientSock);
                    taskCV.notify_one();
                }
            }
        }
    }

    quitWorker.store(true);
    taskCV.notify_all();
    for (auto& t : workerPool) {
        if (t.joinable()) {
            t.join();
        }
    }

    close(serverSock);
    return 0;
}
