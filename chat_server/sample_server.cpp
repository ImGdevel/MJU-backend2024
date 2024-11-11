#include <iostream>
#include <thread>
#include <queue>
#include <vector>
#include <mutex>
#include <condition_variable>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <cstring>
#include <sys/select.h>
#include <nlohmann/json.hpp>  // JSON 라이브러리
#include "message.pb.h"       // Protobuf 메시지 정의 헤더

using namespace mju;
using namespace std;
using json = nlohmann::json;

#define PORT 10114
#define MAX_CLIENTS 5

mutex queueMutex;
condition_variable cv;
queue<int> clientQueue;

void send_message(int client_socket, const string& serialized_data) {
    uint16_t msg_length = htons(serialized_data.size());
    char length_buffer[2];
    memcpy(length_buffer, &msg_length, sizeof(msg_length));

    if (send(client_socket, length_buffer, sizeof(length_buffer), 0) != sizeof(length_buffer)) {
        perror("Failed to send message length");
        return;
    }

    if (send(client_socket, serialized_data.data(), serialized_data.size(), 0) != static_cast<ssize_t>(serialized_data.size())) {
        perror("Failed to send message data");
        return;
    }

    cout << "[S->C: Message sent] Length: " << serialized_data.size() << " bytes, Data: " << serialized_data << endl;
}

string receive_message(int client_socket) {
    char length_buffer[2];
    ssize_t length_received = recv(client_socket, length_buffer, sizeof(length_buffer), MSG_WAITALL);

    if (length_received != sizeof(length_buffer)) {
        if (length_received == 0) {
            cerr << "Client disconnected" << endl;
        } else {
            perror("Failed to receive message length");
        }
        return "";
    }

    uint16_t msg_length;
    memcpy(&msg_length, length_buffer, sizeof(msg_length));
    msg_length = ntohs(msg_length);

    string serialized_data(msg_length, 0);
    ssize_t data_received = recv(client_socket, &serialized_data[0], msg_length, MSG_WAITALL);

    if (data_received != msg_length) {
        perror("Failed to receive message data");
        return "";
    }

    cout << "[C->S: Message received] Length: " << serialized_data.size() << " bytes, Data: " << serialized_data << endl;
    return serialized_data;
}

void handleClient(int client_socket) {
    bool use_json = true;
    bool running = true;

    while (running) {


        if (use_json) {

            string serialized_data = receive_message(client_socket);
            if (serialized_data.empty()) break;

            try {
                json json_message = json::parse(serialized_data);
                cout << "[C->S: JSON Message received] Data: " << json_message.dump(4) << endl;
            } catch (const json::parse_error& e) {
                cerr << "JSON parse error: " << e.what() << endl;
            }
        } else {
            string serialized_data_type = receive_message(client_socket);
            if (serialized_data_type.empty()) break;

            string serialized_data_message = receive_message(client_socket);
            if (serialized_data_message.empty()) break;

            Type incoming_type_msg;
            if (!incoming_type_msg.ParseFromString(serialized_data_type)) {
                cerr << "Protobuf parse error" << endl;
            } else {
                
                switch (incoming_type_msg.type()) {
                    case Type::CS_NAME:
                        cout << "Handling CS_NAME message in Protobuf" << endl;
                        break;
                    case Type::CS_CHAT:
                        cout << "Handling CS_CHAT message in Protobuf" << endl;
                        break;
                    case Type::CS_SHUTDOWN:
                        cout << "Handling CS_SHUTDOWN message in Protobuf" << endl;
                        running = false;
                        break;
                    default:
                        cerr << "Unknown message type received" << endl;
                        break;
                }

                cout << "[C->S: Protobuf Message received] Data: " << incoming_type_msg.ShortDebugString() << endl;

            }
        }

        if (use_json) {
            json json_message = {{"type", "SCSystemMessage"}, {"text", "MESSAGE"}};
            send_message(client_socket, json_message.dump());
        } else {
            Type response_type_msg;
            response_type_msg.set_type(Type::SC_SYSTEM_MESSAGE);
            send_message(client_socket, response_type_msg.SerializeAsString());

            SCSystemMessage response_msg;
            response_msg.set_text("Response from server");
            send_message(client_socket, response_msg.SerializeAsString());
        }
    }

    close(client_socket);
    cout << "Client disconnected" << endl;
}

void worker() {
    while (true) {
        int clientSocket;
        {
            unique_lock<mutex> lock(queueMutex);
            cv.wait(lock, [] { return !clientQueue.empty(); });
            clientSocket = clientQueue.front();
            clientQueue.pop();
        }
        handleClient(clientSocket);
    }
}

int main() {
    int server_fd, new_socket;
    sockaddr_in address;
    int opt = 1;
    int addrlen = sizeof(address);

    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt))) {
        perror("setsockopt failed");
        exit(EXIT_FAILURE);
    }

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    if (bind(server_fd, (sockaddr*)&address, sizeof(address)) < 0) {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }

    if (listen(server_fd, MAX_CLIENTS) < 0) {
        perror("listen failed");
        exit(EXIT_FAILURE);
    }

    cout << "Server listening on port " << PORT << endl;

    vector<thread> workers;
    for (int i = 0; i < 3; ++i) {
        workers.emplace_back(worker);
    }

    fd_set readfds;

    while (true) {
        FD_ZERO(&readfds);
        FD_SET(server_fd, &readfds);
        int max_sd = server_fd;

        int activity = select(max_sd + 1, &readfds, NULL, NULL, NULL);
        if (activity < 0 && errno != EINTR) {
            perror("select error");
        }

        if (FD_ISSET(server_fd, &readfds)) {
            if ((new_socket = accept(server_fd, (sockaddr*)&address, (socklen_t*)&addrlen)) < 0) {
                perror("accept");
                exit(EXIT_FAILURE);
            }

            {
                lock_guard<mutex> lock(queueMutex);
                clientQueue.push(new_socket);
            }
            cv.notify_one();
        }
    }

    for (auto& worker : workers) {
        worker.join();
    }

    return 0;
}
