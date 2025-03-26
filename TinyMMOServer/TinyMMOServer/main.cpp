///------------------------------------------------------------------------------------------------
///  main.cpp
///  TinyMMOServer
///
///  Created by Alex Koukoulas on 25/04/2024
///------------------------------------------------------------------------------------------------

#include <algorithm>
#include <atomic>
#include <arpa/inet.h>
#include <chrono>
#include <fstream>
#include <netinet/in.h>
#include <sys/socket.h>
#include <mutex>
#include <thread>
#include <unistd.h>
#include <unordered_set>
#include <unordered_map>
#include <vector>

#include "net_common/NetworkMessages.h"
#include "net_common/SerializableNetworkObjects.h"
#include "net_common/Version.h"
#include "util/Date.h"
#include "util/FileUtils.h"
#include "util/Json.h"
#include "util/Logging.h"
#include "util/MathUtils.h"
#include "util/StringUtils.h"

///------------------------------------------------------------------------------------------------

static constexpr int PORT = 8070;
static constexpr int MAX_INCOMING_MSG_BUFFER_SIZE = 8192;
static std::atomic<long long> sPlayerIdCounter = 1;

///------------------------------------------------------------------------------------------------

void UpdateTableLoop()
{
    while (true)
    {
    }
}

///------------------------------------------------------------------------------------------------

void SendMessageToClient(nlohmann::json& messageJson, networking::MessageType messageType, const int clientSocket)
{
    networking::PopulateMessageHeader(messageJson, messageType);
    
    std::string messageString = messageJson.dump();
    if (send(clientSocket, messageString.c_str(), messageString.size(), 0) == -1)
    {
        logging::Log(logging::LogType::ERROR, "Error sending message to client");
        close(clientSocket);
        return;
    }
}

///------------------------------------------------------------------------------------------------

void OnClientLoginRequestMessage(const nlohmann::json& json, const int clientSocket)
{
    networking::LoginResponse loginResponse = {};
    loginResponse.allowed = true;
    loginResponse.playerId = sPlayerIdCounter++;
    
    auto loginResponseJson = loginResponse.SerializeToJson();
    SendMessageToClient(loginResponseJson, networking::MessageType::SC_LOGIN_RESPONSE, clientSocket);
}

///------------------------------------------------------------------------------------------------

void OnClientSpinRequestMessage(const nlohmann::json& json, const int clientSocket)
{
    networking::SpinResponse spinResponse = {};
    
    std::random_device rd;
    std::mt19937 g(rd());
    
    spinResponse.spinId = g();
    auto spinResponseJson = spinResponse.SerializeToJson();
    SendMessageToClient(spinResponseJson, networking::MessageType::SC_SPIN_RESPONSE, clientSocket);
}

///------------------------------------------------------------------------------------------------

void HandleClient(int clientSocket)
{
    std::string jsonMessage;

    while (true)
    {
        char buffer[MAX_INCOMING_MSG_BUFFER_SIZE];
        ssize_t bytesReceived = recv(clientSocket, buffer, sizeof(buffer), 0);
        if (bytesReceived == -1)
        {
            logging::Log(logging::LogType::ERROR, "recv() failed: Message too large");
            break;
        }
        else if (bytesReceived == 0)
        {
            // Connection closed by client
            break;
        } 
        else
        {
            jsonMessage.append(buffer, bytesReceived);
            if (jsonMessage.find('\0') != std::string::npos)
            {
                // Null character found, indicating end of JSON message
                break;
            }
        }
    }

    if (!jsonMessage.empty())
    {
        //logging::Log(logging::LogType::INFO, "Received message from client: %s", jsonMessage.c_str());

        // Parse JSON
        try
        {
            nlohmann::json receivedJson = nlohmann::json::parse(jsonMessage);
            
            if (receivedJson.count("version"))
            {
                const auto& clientLibVersion = receivedJson.at("version").get<std::string>();
                const auto& currentLibVersion = std::string(NET_COMMON_VERSION);
                if (clientLibVersion != currentLibVersion)
                {
                    logging::Log(logging::LogType::ERROR, "Client/Server version mismatch: Client at %s, Server at %s", clientLibVersion.c_str(), currentLibVersion.c_str());
                }
            }
            
            if (networking::IsMessageOfType(receivedJson, networking::MessageType::CS_LOGIN_REQUEST))
            {
                OnClientLoginRequestMessage(receivedJson, clientSocket);
            }
            else if (networking::IsMessageOfType(receivedJson, networking::MessageType::CS_SPIN_REQUEST))
            {
                OnClientSpinRequestMessage(receivedJson, clientSocket);
            }
        }
        catch (const std::exception& e)
        {
            logging::Log(logging::LogType::ERROR, "Error parsing JSON: %s", e.what());
        }
    }

    close(clientSocket);
}

///------------------------------------------------------------------------------------------------

int main(int argc, char* argv[])
{
    logging::Log(logging::LogType::INFO, "Initializing server from CWD: %s", argv[0]);
    
    int serverSocket, clientSocket;
    struct sockaddr_in serverAddr, clientAddr;
    socklen_t clientAddrLen = sizeof(clientAddr);

    // Create socket
    if ((serverSocket = socket(AF_INET, SOCK_STREAM, 0)) == 0)
    {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    // Bind socket
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons(PORT); // Change port as needed

    if (bind(serverSocket, (struct sockaddr *)&serverAddr, sizeof(serverAddr)) < 0)
    {
        logging::Log(logging::LogType::ERROR, "bind() failed: probably already in use");
        exit(EXIT_FAILURE);
    }

    // Listen
    if (listen(serverSocket, 3) < 0)
    {
        logging::Log(logging::LogType::ERROR, "listen() failed");
        exit(EXIT_FAILURE);
    }
    
    logging::Log(logging::LogType::INFO, "Listening for connections on port: %d", PORT);
    
    // Table Update Loop
    std::thread(UpdateTableLoop).detach();
    
    // Accept and handle incoming connections
    while (true)
    {
        if ((clientSocket = accept(serverSocket, (struct sockaddr *)&clientAddr, &clientAddrLen)) < 0)
        {
            continue;
        }

        std::thread(HandleClient, clientSocket).detach();
    }
    
    return 0;
}

///------------------------------------------------------------------------------------------------
