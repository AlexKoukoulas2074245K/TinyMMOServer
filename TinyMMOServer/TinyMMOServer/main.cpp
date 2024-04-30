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

#include "net_common/PlayerData.h"
#include "util/Date.h"
#include "util/Json.h"
#include "util/Logging.h"
#include "util/MathUtils.h"
#include "util/StringUtils.h"

///------------------------------------------------------------------------------------------------

static constexpr int WORLD_UPDATE_TARGET_INTERVAL_MILLIS = 16;
static constexpr int PLAYER_KICK_INTERVAL_SECS = 5;
static constexpr int PORT = 8070;
static constexpr int MAX_INCOMING_MSG_BUFFER_SIZE = 4096;

///------------------------------------------------------------------------------------------------

struct PlayerData
{
    strutils::StringId mPlayerName;
    glm::vec3 mPlayerPosition;
    glm::vec3 mPlayerVelocity;
    float mColor;
    std::chrono::time_point<std::chrono::high_resolution_clock> mLastHeartbeatTimePoint;
};

///------------------------------------------------------------------------------------------------

static std::mutex sWorldMutex;
static std::vector<PlayerData> sWorldData;

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
                    logging::Log(logging::LogType::INFO, "Kicking player %s due to inactivity (new player count %d)", playerData.mPlayerName.GetString().c_str(), sWorldData.size() - 1);
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

void UpdateWorldPlayerEntries(PlayerData&& playerData)
{
    std::lock_guard<std::mutex> worldGuard(sWorldMutex);
    
    auto playerIter = std::find_if(sWorldData.begin(), sWorldData.end(), [&](PlayerData& playerEntry){ return playerEntry.mPlayerName == strutils::StringId(playerData.mPlayerName); });
    if (playerIter != sWorldData.end())
    {
        *playerIter = std::move(playerData);
    }
    else
    {
        logging::Log(logging::LogType::INFO, "Creating entry for player %s (new player count %d)", playerData.mPlayerName.GetString().c_str(), sWorldData.size() + 1);
        sWorldData.insert(playerIter, std::move(playerData));
    }
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
            
            auto playerName = receivedJson["player_name"].get<std::string>();
            auto playerPosition = glm::vec3(
                receivedJson["player_position"]["x"].get<float>(),
                receivedJson["player_position"]["y"].get<float>(),
                receivedJson["player_position"]["z"].get<float>());
            auto playerVelocity = glm::vec3(
                receivedJson["player_velocity"]["x"].get<float>(),
                receivedJson["player_velocity"]["y"].get<float>(),
                receivedJson["player_velocity"]["z"].get<float>());
            float playerColor = receivedJson["player_color"].get<float>();
            
            UpdateWorldPlayerEntries(PlayerData{ strutils::StringId(playerName), playerPosition, playerVelocity, playerColor, std::chrono::high_resolution_clock::now() });
            
            nlohmann::json worldStateJson;
            nlohmann::json playerDataJsonArray;
            for (const auto& playerData: sWorldData)
            {
                nlohmann::json playerDataJson;
                playerDataJson["player_name"] = playerData.mPlayerName.GetString();
                playerDataJson["player_color"] = playerData.mColor;
                
                nlohmann::json playerPositionJson;
                playerPositionJson["x"] = playerData.mPlayerPosition.x;
                playerPositionJson["y"] = playerData.mPlayerPosition.y;
                playerPositionJson["z"] = playerData.mPlayerPosition.z;
                playerDataJson["player_position"] = playerPositionJson;
                
                nlohmann::json playerVelocityJson;
                playerVelocityJson["x"] = playerData.mPlayerVelocity.x;
                playerVelocityJson["y"] = playerData.mPlayerVelocity.y;
                playerVelocityJson["z"] = playerData.mPlayerVelocity.z;
                playerDataJson["player_velocity"] = playerVelocityJson;
                playerDataJson["is_local"] = playerData.mPlayerName == strutils::StringId(playerName);
                playerDataJsonArray.push_back(playerDataJson);
            }
            
            worldStateJson["player_data"] = playerDataJsonArray;
            
            std::string worldStateString = worldStateJson.dump();
            if (send(clientSocket, worldStateString.c_str(), worldStateString.size(), 0) == -1)
            {
                logging::Log(logging::LogType::ERROR, "Error sending message to client");
                close(clientSocket);
                return;
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
