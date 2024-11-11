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

#define PORT 10114
#define MAX_CLIENTS 5

std::mutex queueMutex;
std::condition_variable cv;
std::queue<int> clientQueue; // 클라이언트 요청을 위한 공유 큐


#include <nlohmann/json.hpp>  // 새로운 JSON 라이브러리
#include "message.pb.h"             // Protobuf 메시지 정의 헤더

using namespace mju;
using namespace std;
using json = nlohmann::json;


/*
// 전송할 메시지를 클라이언트로 송신하는 메서드
void send_message(int client_socket, const google::protobuf::Message& message, bool use_json = false) {
    std::string serialized_data;
    std::string message_str;

    // JSON 포맷을 사용할지 Protobuf 포맷을 사용할지 결정
    if (use_json) {
        nlohmann::json json_message = {
            {"type", message.GetTypeName()},  // 메시지 타입 필드
            {"data", message.DebugString()}   // 메시지 데이터 필드
        };
        serialized_data = json_message.dump();
        message_str = serialized_data;
    } else {
        serialized_data = message.SerializeAsString();
        message_str = message.ShortDebugString();
    }

    uint16_t msg_length = htons(serialized_data.size()); // 메시지 크기를 big-endian 형식으로 변환
    char length_buffer[2];
    std::memcpy(length_buffer, &msg_length, sizeof(msg_length));

    // 길이 정보 + 메시지 데이터를 클라이언트에 전송
    if (send(client_socket, length_buffer, sizeof(length_buffer), 0) != sizeof(length_buffer)) {
        perror("Failed to send message length");
        return;
    }
    if (send(client_socket, serialized_data.data(), serialized_data.size(), 0) != static_cast<ssize_t>(serialized_data.size())) {
        perror("Failed to send message data");
        return;
    }

    std::cout << "[S->C: Message sent] Length: " << serialized_data.size() << " bytes, Data: " << message_str << std::endl;
}

// 클라이언트로부터 메시지를 수신하는 메서드
bool receive_message(int client_socket, google::protobuf::Message& message, bool use_json = false) {
    char length_buffer[2];
    
    // 길이 정보를 수신
    ssize_t length_received = recv(client_socket, length_buffer, sizeof(length_buffer), MSG_WAITALL);
    if (length_received != sizeof(length_buffer)) {
        if (length_received == 0) {
            std::cerr << "Client disconnected" << std::endl;
            return false;
        } else {
            perror("Failed to receive message length");
            return false;
        }
    }

    uint16_t msg_length;
    std::memcpy(&msg_length, length_buffer, sizeof(msg_length));
    msg_length = ntohs(msg_length);

    // 메시지 본문 수신
    std::string serialized_data(msg_length, 0);
    ssize_t data_received = recv(client_socket, &serialized_data[0], msg_length, MSG_WAITALL);
    if (data_received != msg_length) {
        perror("Failed to receive message data");
        return false;
    }

    // 수신된 메시지 처리
    if (use_json) {
        nlohmann::json json_message;
        try {
            json_message = nlohmann::json::parse(serialized_data);
        } catch (const nlohmann::json::parse_error& e) {
            std::cerr << "JSON parse error: " << e.what() << std::endl;
            return false;
        }

        // 필요한 경우 JSON 메시지에서 특정 필드를 확인
        std::cout << "[C->S: JSON Message received] Data: " << json_message.dump(4) << std::endl;
        // 여기서 JSON 메시지를 기반으로 처리 로직을 작성

    } else {
        if (!message.ParseFromString(serialized_data)) {
            std::cerr << "Protobuf parse error" << std::endl;
            return false;
        }
        std::cout << "[C->S: Protobuf Message received] Data: " << message.ShortDebugString() << std::endl;
        // Protobuf 메시지에서 특정 필드를 확인하여 처리 로직 작성
    }

    return true;
}

void send_json_dummy_data(int client_socket) {
    // 더미 JSON 데이터 생성
    nlohmann::json json_message = {
        {"type", "SCSystemMessage"},
        {"text", "MESSAGE"}
    };

    // JSON 문자열로 직렬화
    std::string serialized_data = json_message.dump();

    // 메시지 길이 계산 및 전송
    uint16_t msg_length = htons(serialized_data.size());
    char length_buffer[2];
    std::memcpy(length_buffer, &msg_length, sizeof(msg_length));

    // 길이 정보 + 메시지 데이터를 클라이언트에 전송
    if (send(client_socket, length_buffer, sizeof(length_buffer), 0) != sizeof(length_buffer)) {
        perror("Failed to send message length");
        return;
    }
    if (send(client_socket, serialized_data.data(), serialized_data.size(), 0) != static_cast<ssize_t>(serialized_data.size())) {
        perror("Failed to send message data");
        return;
    }

    std::cout << "[S->C: JSON Dummy Data sent] Length: " << serialized_data.size() << " bytes, Data: " << json_message.dump(4) << std::endl;
}
*/


/*
void handleClient(int client_socket) {
    bool use_json = false;  // JSON을 기본 포맷으로 사용. 필요에 따라 변경 가능
    bool running = true;

    while (running) {
        // Protobuf 포맷에서 mju::Type 메시지를 사용하여 수신 메시지 타입 결정
        mju::Type incoming_type_msg;

        if (!receive_message(client_socket, incoming_type_msg, false)) {
            std::cerr << "Failed to receive Protobuf message" << std::endl;
            break;  // 오류 발생 시 연결 종료
        }

        // 수신된 메시지 타입에 따라 처리 로직 구현
        switch (incoming_type_msg.type()) {
            case mju::Type::CS_NAME: {
                std::cout << "Handling CS_NAME message in Protobuf" << std::endl;
                // 각 Protobuf 메시지에 맞는 처리 로직 추가
                break;
            }
            case mju::Type::CS_CHAT: {
                std::cout << "Handling CS_CHAT message in Protobuf" << std::endl;
                // 필요한 작업 수행
                break;
            }
            case mju::Type::CS_SHUTDOWN: {
                std::cout << "Handling CS_SHUTDOWN message in Protobuf" << std::endl;
                running = false;
                break;
            }
            // 추가 메시지 타입을 처리할 경우 여기서 작성
            default: {
                std::cerr << "Unknown message type received" << std::endl;
                break;
            }
        }

        // 응답을 위한 Type 메시지 전송 (타입 필드를 설정)
        mju::Type response_type_msg;
        response_type_msg.set_type(mju::Type::SC_SYSTEM_MESSAGE); // 'type' 필드를 설정
        send_message(client_socket, response_type_msg, use_json);

        // 실제 응답 메시지 생성 (mju::SCSystemMessage 사용)
        mju::SCSystemMessage response_msg;
        response_msg.set_text("Response from server");

        // 클라이언트로 응답 송신
        send_message(client_socket, response_msg, use_json);
    }

    close(client_socket);
    std::cout << "Client disconnected" << std::endl;
}
*/

// 메시지를 전송하는 메서드
void send_message(int client_socket, const std::string& serialized_data) {
    uint16_t msg_length = htons(serialized_data.size()); // 메시지 길이 big-endian 변환
    char length_buffer[2];
    std::memcpy(length_buffer, &msg_length, sizeof(msg_length));

    // 길이 정보 전송
    if (send(client_socket, length_buffer, sizeof(length_buffer), 0) != sizeof(length_buffer)) {
        perror("Failed to send message length");
        return;
    }

    // 메시지 데이터 전송
    if (send(client_socket, serialized_data.data(), serialized_data.size(), 0) != static_cast<ssize_t>(serialized_data.size())) {
        perror("Failed to send message data");
        return;
    }

    std::cout << "[S->C: Message sent] Length: " << serialized_data.size() << " bytes, Data: " << serialized_data << std::endl;
}

// 메시지를 수신하는 메서드
std::string receive_message(int client_socket) {
    char length_buffer[2];

    // 길이 정보 수신
    ssize_t length_received = recv(client_socket, length_buffer, sizeof(length_buffer), MSG_WAITALL);
    if (length_received != sizeof(length_buffer)) {
        if (length_received == 0) {
            std::cerr << "Client disconnected" << std::endl;
        } else {
            perror("Failed to receive message length");
        }
        return "";  // 오류 발생 시 빈 문자열 반환
    }

    uint16_t msg_length;
    std::memcpy(&msg_length, length_buffer, sizeof(msg_length));
    msg_length = ntohs(msg_length);

    // 메시지 본문 수신
    std::string serialized_data(msg_length, 0);
    ssize_t data_received = recv(client_socket, &serialized_data[0], msg_length, MSG_WAITALL);
    if (data_received != msg_length) {
        perror("Failed to receive message data");
        return "";  // 오류 발생 시 빈 문자열 반환
    }

    std::cout << "[C->S: Message received] Length: " << serialized_data.size() << " bytes, Data: " << serialized_data << std::endl;
    return serialized_data;
}


void handleClient(int client_socket) {
    bool use_json = false;  // 메시지 포맷을 지정하는 플래그
    bool running = true;

    while (running) {
        // 메시지 수신
        std::string serialized_data = receive_message(client_socket);
        if (serialized_data.empty()) break;  // 오류 발생 시 연결 종료

        // 메시지 파싱 (Protobuf 또는 JSON)
        if (use_json) {
            // JSON 파싱 로직
            try {
                nlohmann::json json_message = nlohmann::json::parse(serialized_data);
                std::cout << "[C->S: JSON Message received] Data: " << json_message.dump(4) << std::endl;

                // JSON에 따라 처리 로직 구현

            } catch (const nlohmann::json::parse_error& e) {
                std::cerr << "JSON parse error: " << e.what() << std::endl;
            }
        } else {
            // Protobuf 파싱 로직
            mju::Type incoming_type_msg;
            if (!incoming_type_msg.ParseFromString(serialized_data)) {
                std::cerr << "Protobuf parse error" << std::endl;
            } else {
                std::cout << "[C->S: Protobuf Message received] Data: " << incoming_type_msg.ShortDebugString() << std::endl;

                // Protobuf에 따른 처리 로직 구현
                switch (incoming_type_msg.type()) {
                    case mju::Type::CS_NAME:
                        std::cout << "Handling CS_NAME message in Protobuf" << std::endl;
                        break;
                    case mju::Type::CS_CHAT:
                        std::cout << "Handling CS_CHAT message in Protobuf" << std::endl;
                        break;
                    case mju::Type::CS_SHUTDOWN:
                        std::cout << "Handling CS_SHUTDOWN message in Protobuf" << std::endl;
                        running = false;
                        break;
                    default:
                        std::cerr << "Unknown message type received" << std::endl;
                        break;
                }
            }
        }

        // 응답 데이터 생성
        mju::Type response_type_msg;
        response_type_msg.set_type(mju::Type::SC_SYSTEM_MESSAGE);
        send_message(client_socket, response_type_msg.SerializeAsString());

        // 실제 응답 메시지 생성
        mju::SCSystemMessage response_msg;
        response_msg.set_text("Response from server");
        send_message(client_socket, response_msg.SerializeAsString());
    }

    close(client_socket);
    std::cout << "Client disconnected" << std::endl;
}



void worker() {
    while (true) {
        int clientSocket;
        
        // 작업 큐에서 클라이언트를 가져오기 위해 대기
        {
            std::unique_lock<std::mutex> lock(queueMutex);
            cv.wait(lock, [] { return !clientQueue.empty(); });
            clientSocket = clientQueue.front();
            clientQueue.pop();
        }

        // 클라이언트 소켓 처리
        handleClient(clientSocket);
    }
}

int main() {
    int server_fd, new_socket;
    struct sockaddr_in address;
    int opt = 1;
    int addrlen = sizeof(address);

    // 서버 소켓 생성
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    // 소켓 옵션 설정
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt))) {
        perror("setsockopt failed");
        exit(EXIT_FAILURE);
    }

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    // 바인딩
    if (bind(server_fd, (struct sockaddr*)&address, sizeof(address)) < 0) {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }

    // 리스닝
    if (listen(server_fd, MAX_CLIENTS) < 0) {
        perror("listen failed");
        exit(EXIT_FAILURE);
    }

    std::cout << "Server listening on port " << PORT << std::endl;

    // 워커 스레드 시작
    std::vector<std::thread> workers;
    for (int i = 0; i < 2; ++i) { // 2개 이상의 스레드
        workers.emplace_back(worker);
    }

    fd_set readfds;

    while (true) {
        FD_ZERO(&readfds);
        FD_SET(server_fd, &readfds);
        int max_sd = server_fd;

        // select() 호출
        int activity = select(max_sd + 1, &readfds, NULL, NULL, NULL);
        if (activity < 0 && errno != EINTR) {
            perror("select error");
        }

        // 새로운 클라이언트 요청 처리
        if (FD_ISSET(server_fd, &readfds)) {
            if ((new_socket = accept(server_fd, (struct sockaddr*)&address, (socklen_t*)&addrlen)) < 0) {
                perror("accept");
                exit(EXIT_FAILURE);
            }

            // 연결된 클라이언트 소켓을 작업 큐에 추가
            {
                std::lock_guard<std::mutex> lock(queueMutex);
                clientQueue.push(new_socket);
            }
            cv.notify_one();
        }
    }

    // 스레드 종료 대기
    for (auto& worker : workers) {
        worker.join();
    }

    return 0;
}
