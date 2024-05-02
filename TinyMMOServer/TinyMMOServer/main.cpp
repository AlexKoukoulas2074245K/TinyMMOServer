///------------------------------------------------------------------------------------------------
///  main.cpp
///  TinyMMOServer
///
///  Created by Alex Koukoulas on 25/04/2024
///------------------------------------------------------------------------------------------------

#include <atomic>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <mutex>
#include <thread>
#include <unistd.h>
#include <vector>

#include "net_common/NetworkMessages.h"
#include "net_common/WorldObjectTypes.h"
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

static const float SHURIKEN_SPEED = 0.001f;
static const float SHURIKEN_LIFETIME_SECS = 5.0f;

///------------------------------------------------------------------------------------------------

struct ServerWorldObjectData
{
    networking::WorldObjectData mWorldObjectData;
    std::chrono::time_point<std::chrono::high_resolution_clock> mLastHeartbeatTimePoint;
};

///------------------------------------------------------------------------------------------------

static std::mutex sWorldMutex;
static std::vector<ServerWorldObjectData> sWorldObjects;
static std::atomic<int> sWorldObjectIdCounter = 1;

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
            
            for (auto iter = sWorldObjects.begin(); iter != sWorldObjects.end(); )
            {
                auto& serverWorldObjectData = *iter;
                
                switch (serverWorldObjectData.mWorldObjectData.objectType)
                {
                    case networking::OBJ_TYPE_PLAYER:
                    {
                        // Player DC check
                        if (std::chrono::duration_cast<std::chrono::seconds>(now - serverWorldObjectData.mLastHeartbeatTimePoint).count() > PLAYER_KICK_INTERVAL_SECS)
                        {
                            logging::Log(logging::LogType::INFO, "Kicking player (id %d): %s due to inactivity (new object count %d)", serverWorldObjectData.mWorldObjectData.objectId, serverWorldObjectData.mWorldObjectData.objectName.GetString().c_str(), sWorldObjects.size() - 1);
                            iter = sWorldObjects.erase(iter);
                            continue;
                        }
                    } break;
                        
                    case networking::OBJ_TYPE_NPC_SHURIKEN:
                    {
                        serverWorldObjectData.mWorldObjectData.objectPosition += serverWorldObjectData.mWorldObjectData.objectVelocity * static_cast<float>(WORLD_UPDATE_TARGET_INTERVAL_MILLIS);
                        
                        if (std::chrono::duration_cast<std::chrono::seconds>(now - serverWorldObjectData.mLastHeartbeatTimePoint).count() > SHURIKEN_LIFETIME_SECS)
                        {
                            logging::Log(logging::LogType::INFO, "Destroying shuriken (id %d): %s due to lifetime reached (new object count %d)", serverWorldObjectData.mWorldObjectData.objectId, serverWorldObjectData.mWorldObjectData.objectName.GetString().c_str(), sWorldObjects.size() - 1);
                            iter = sWorldObjects.erase(iter);
                            continue;
                        }
                    } break;
                }
                
                iter++;
            }
            
            lastUpdateTimePoint = std::chrono::high_resolution_clock::now();
        }
    }
}

///------------------------------------------------------------------------------------------------

void UpdateWorldObjectEntries(ServerWorldObjectData&& newObjectData)
{
    std::lock_guard<std::mutex> worldGuard(sWorldMutex);
    
    auto objectIter = std::find_if(sWorldObjects.begin(), sWorldObjects.end(), [&](ServerWorldObjectData& objectEntry){ return objectEntry.mWorldObjectData.objectId == newObjectData.mWorldObjectData.objectId; });
    if (objectIter != sWorldObjects.end())
    {
        *objectIter = std::move(newObjectData);
    }
    else
    {
        sWorldObjects.insert(objectIter, std::move(newObjectData));
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
    networking::WorldObjectData incomingPlayerData = {};
    incomingPlayerData.DeserializeFromJson(json);
    
    if (incomingPlayerData.objectId != 0)
    {
        UpdateWorldObjectEntries(ServerWorldObjectData{incomingPlayerData, std::chrono::high_resolution_clock::now() });
    }
    
    nlohmann::json worldStateJson;
    nlohmann::json objectJsonArray;
    for (const auto& serverObjectDataEntry: sWorldObjects)
    {
        auto serverObjectDataCopy = serverObjectDataEntry;
        serverObjectDataCopy.mWorldObjectData.isLocal = serverObjectDataCopy.mWorldObjectData.objectId == incomingPlayerData.objectId;
        objectJsonArray.push_back(serverObjectDataCopy.mWorldObjectData.SerializeToJson());
    }
    
    worldStateJson[networking::WorldObjectData::ObjectCollectionHeader()] = objectJsonArray;
    
    SendMessageToClient(worldStateJson, networking::MessageType::SC_PLAYER_STATE_RESPONSE, clientSocket);
}

///------------------------------------------------------------------------------------------------

void OnClientLoginRequestMessage(const nlohmann::json& json, const int clientSocket)
{
    std::lock_guard<std::mutex> worldGuard(sWorldMutex);
    
    networking::LoginResponse loginResponse = {};
    loginResponse.playerId = sWorldObjectIdCounter++;
    loginResponse.playerPosition = glm::vec3(math::RandomFloat(-0.3f, -0.1f), math::RandomFloat(-0.15f, 0.15f), 0.1f);
    loginResponse.color = math::RandomFloat(0.0f, 1.0f);
    loginResponse.allowed = true;
    loginResponse.playerName = strutils::StringId(GenerateName());
    
    logging::Log(logging::LogType::INFO, "Creating entry for player (id %d): %s (new object count %d)", loginResponse.playerId, loginResponse.playerName.GetString().c_str(), sWorldObjects.size() + 1);
	    
    ServerWorldObjectData placeHolderData = {};
    placeHolderData.mWorldObjectData.objectId = loginResponse.playerId;
    placeHolderData.mWorldObjectData.objectPosition = loginResponse.playerPosition;
    placeHolderData.mWorldObjectData.color = loginResponse.color;
    placeHolderData.mWorldObjectData.objectName = loginResponse.playerName;
    placeHolderData.mWorldObjectData.objectType = networking::OBJ_TYPE_PLAYER;
    placeHolderData.mLastHeartbeatTimePoint = std::chrono::high_resolution_clock::now();
    
    sWorldObjects.push_back(placeHolderData);

    auto loginResponseJson = loginResponse.SerializeToJson();
    SendMessageToClient(loginResponseJson, networking::MessageType::SC_REQUEST_LOGIN_RESPONSE, clientSocket);
}

///------------------------------------------------------------------------------------------------

void OnClientThrowRangedWeaponMessage(const nlohmann::json& json, const int clientSocket)
{
    std::lock_guard<std::mutex> worldGuard(sWorldMutex);
    
    networking::ThrowRangedWeaponRequest throwRangedWeaponRequest = {};
    throwRangedWeaponRequest.DeserializeFromJson(json);
    
    networking::ThrowRangedWeaponResponse response = {};
    
    auto playerObjectIter = std::find_if(sWorldObjects.begin(), sWorldObjects.end(), [&](ServerWorldObjectData& objectEntry){ return objectEntry.mWorldObjectData.objectId == throwRangedWeaponRequest.playerId; });
    if (playerObjectIter == sWorldObjects.end())
    {
        response.allowed = false;
    }
    else
    {
        const auto& playerPosition = playerObjectIter->mWorldObjectData.objectPosition;
        auto weaponPosition = playerPosition;
        weaponPosition.z /= 2.0f;
        
        const auto direction = throwRangedWeaponRequest.targetPosition - playerPosition;
        
        if (glm::length(direction) <= 0.0f)
        {
            response.allowed = false;
        }
        else
        {
            ServerWorldObjectData weaponData = {};
            weaponData.mWorldObjectData.objectId = sWorldObjectIdCounter++;
            weaponData.mWorldObjectData.parentObjectId = playerObjectIter->mWorldObjectData.objectId;
            weaponData.mWorldObjectData.objectPosition = weaponPosition;
            weaponData.mWorldObjectData.objectVelocity = glm::normalize(direction) * SHURIKEN_SPEED;
            weaponData.mWorldObjectData.objectType = networking::OBJ_TYPE_NPC_SHURIKEN;
            weaponData.mLastHeartbeatTimePoint = std::chrono::high_resolution_clock::now();

            response.allowed = true;
            sWorldObjects.push_back(weaponData);
        }
    }
    
    auto throwRangedWeaponResponseJson = response.SerializeToJson();
    SendMessageToClient(throwRangedWeaponResponseJson, networking::MessageType::SC_THROW_RANGED_WEAPON_RESPONSE, clientSocket);
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
            else if (networking::IsMessageOfType(receivedJson, networking::MessageType::CS_THROW_RANGED_WEAPON))
            {
                OnClientThrowRangedWeaponMessage(receivedJson, clientSocket);
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
