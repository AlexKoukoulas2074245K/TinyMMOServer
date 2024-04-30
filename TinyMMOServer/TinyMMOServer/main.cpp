///------------------------------------------------------------------------------------------------
///  main.cpp
///  TinyMMOServer
///
///  Created by Alex Koukoulas on 25/04/2024
///------------------------------------------------------------------------------------------------

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <mutex>
#include <thread>
#include <unistd.h>
#include <vector>

#include "net_common/NetworkMessages.h"
#include "net_common/SerializableNetworkObjects.h"
#include "util/Date.h"
#include "util/Json.h"
#include "util/Logging.h"
#include "util/MathUtils.h"
#include "util/NameGenerator.h"
#include "util/StringUtils.h"

///------------------------------------------------------------------------------------------------

static constexpr int WORLD_UPDATE_TARGET_INTERVAL_MILLIS = 16;
static constexpr int PLAYER_KICK_INTERVAL_SECS = 20;
static constexpr int PORT = 8070;
static constexpr int MAX_INCOMING_MSG_BUFFER_SIZE = 4096;

///------------------------------------------------------------------------------------------------

struct ServerPlayerData
{
    networking::PlayerData mPlayerData;
    std::chrono::time_point<std::chrono::high_resolution_clock> mLastHeartbeatTimePoint;
};

///------------------------------------------------------------------------------------------------

static std::mutex sWorldMutex;
static std::vector<ServerPlayerData> sWorldData;

///------------------------------------------------------------------------------------------------

void WorldUpdateLoop()
{
    auto lastUpdateTimePoint = std::chrono::high_resolution_clock::now();
    
    while(true)
    {
        auto now = std::chrono::high_resolution_clock::now();
        if (std::chrono::duration_cast<std::chrono::milliseconds>(now - lastUpdateTimePoint).count() > WORLD_UPDATE_TARGET_INTERVAL_MILLIS)
        {
            // Update world
            std::lock_guard<std::mutex> worldGuard(sWorldMutex);
            
            for (auto iter = sWorldData.begin(); iter != sWorldData.end(); )
            {
                // DC Check
                auto& playerData = *iter;
                if (std::chrono::duration_cast<std::chrono::seconds>(now - playerData.mLastHeartbeatTimePoint).count() > PLAYER_KICK_INTERVAL_SECS)
                {
                    logging::Log(logging::LogType::INFO, "Kicking player %s due to inactivity (new player count %d)", playerData.mPlayerData.playerName.GetString().c_str(), sWorldData.size() - 1);
                    iter = sWorldData.erase(iter);
                }
                else
                {
                    //playerData.mPlayerPosition += playerData.mPlayerVelocity;
                    iter++;
                }
            }
            
            lastUpdateTimePoint = std::chrono::high_resolution_clock::now();
        }
    }
}

///------------------------------------------------------------------------------------------------

void UpdateWorldPlayerEntries(ServerPlayerData&& playerData)
{
    std::lock_guard<std::mutex> worldGuard(sWorldMutex);
    
    auto playerIter = std::find_if(sWorldData.begin(), sWorldData.end(), [&](ServerPlayerData& playerEntry){ return playerEntry.mPlayerData.playerName == strutils::StringId(playerData.mPlayerData.playerName); });
    if (playerIter != sWorldData.end())
    {
        *playerIter = std::move(playerData);
    }
    else
    {
        logging::Log(logging::LogType::INFO, "Creating entry for player %s (new player count %d)", playerData.mPlayerData.playerName.GetString().c_str(), sWorldData.size() + 1);
        sWorldData.insert(playerIter, std::move(playerData));
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

void OnClientPlayerStateMessage(const nlohmann::json& json, const int clientSocket)
{
    networking::PlayerData incomingPlayerData;
    incomingPlayerData.DeserializeFromJson(json);
    
    if (!incomingPlayerData.playerName.isEmpty())
    {
        UpdateWorldPlayerEntries(ServerPlayerData{incomingPlayerData, std::chrono::high_resolution_clock::now() });
    }
    
    nlohmann::json worldStateJson;
    nlohmann::json playerDataJsonArray;
    for (const auto& serverPlayerDataEntry: sWorldData)
    {
        auto serverPlayerDataCopy = serverPlayerDataEntry;
        serverPlayerDataCopy.mPlayerData.isLocal = serverPlayerDataCopy.mPlayerData.playerName == incomingPlayerData.playerName;
        playerDataJsonArray.push_back(serverPlayerDataCopy.mPlayerData.SerializeToJson());
    }
    
    worldStateJson[networking::PlayerData::ObjectCollectionHeader()] = playerDataJsonArray;
    
    SendMessageToClient(worldStateJson, networking::MessageType::SC_PLAYER_STATE_RESPONSE, clientSocket);
}

///------------------------------------------------------------------------------------------------

void OnClientLoginRequestMessage(const nlohmann::json& json, const int clientSocket)
{
    std::lock_guard<std::mutex> worldGuard(sWorldMutex);
    
    networking::LoginResponse loginResponse;
    loginResponse.playerPosition = glm::vec3(math::RandomFloat(-0.3f, 0.3f), math::RandomFloat(-0.15f, 0.15f), 0.1f);
    loginResponse.color = math::RandomFloat(0.0f, 1.0f);
    loginResponse.allowed = true;
    loginResponse.playerName = strutils::StringId(GenerateName());
    
    if (!sWorldData.empty())
    {
        while (std::find_if(sWorldData.begin(), sWorldData.end(), [&](ServerPlayerData& playerEntry){ return playerEntry.mPlayerData.playerName == loginResponse.playerName; }) != sWorldData.end())
        {
            loginResponse.playerName = strutils::StringId(GenerateName());
        }
    }
    
    ServerPlayerData placeHolderData;
    placeHolderData.mPlayerData.playerPosition = loginResponse.playerPosition;
    placeHolderData.mPlayerData.color = loginResponse.color;
    placeHolderData.mPlayerData.playerName = loginResponse.playerName;
    placeHolderData.mLastHeartbeatTimePoint = std::chrono::high_resolution_clock::now();
    
    sWorldData.push_back(placeHolderData);
    
    auto loginResponseJson = loginResponse.SerializeToJson();
    SendMessageToClient(loginResponseJson, networking::MessageType::SC_REQUEST_LOGIN_RESPONSE, clientSocket);
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
            
            if (networking::IsMessageOfType(receivedJson, networking::MessageType::CS_PLAYER_STATE))
            {
                OnClientPlayerStateMessage(receivedJson, clientSocket);
            }
            else if (networking::IsMessageOfType(receivedJson, networking::MessageType::CS_REQUEST_LOGIN))
            {
                OnClientLoginRequestMessage(receivedJson, clientSocket);
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
    
    // World Update Loop
    std::thread(WorldUpdateLoop).detach();
    
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
