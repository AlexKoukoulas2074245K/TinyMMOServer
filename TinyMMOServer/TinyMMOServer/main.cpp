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

#include "net_common/Card.h"
#include "net_common/NetworkMessages.h"
#include "net_common/SerializableNetworkObjects.h"
#include "util/Date.h"
#include "util/FileUtils.h"
#include "util/Json.h"
#include "util/Logging.h"
#include "util/MathUtils.h"
#include "util/StringUtils.h"

///------------------------------------------------------------------------------------------------

struct InternalTableState
{
    enum class RoundState
    {
        PLACING_BLINDS,
        DEALING_HOLE_CARDS,
        WAITING_FOR_ACTIONS_PREFLOP,
        DEALING_FLOP,
        WAITING_FOR_ACTIONS_POSTFLOP,
        DEALING_TURN,
        WAITING_FOR_ACTIONS_POSTTURN,
        DEALING_RIVER,
        WAITING_FOR_ACTIONS_POSTRIVER,
    };
    
    std::vector<std::pair<long long, std::vector<poker::Card>>> mPlayerHoleCards;
    std::vector<poker::Card> mCommunityCards;
    std::vector<poker::Card> mDeck;
    RoundState mRoundState;
    std::chrono::time_point<std::chrono::high_resolution_clock> mTableTimer;
};

///------------------------------------------------------------------------------------------------

static constexpr int PORT = 8070;
static constexpr int MAX_INCOMING_MSG_BUFFER_SIZE = 8192;
static std::atomic<long long> sPlayerIdCounter = 1;
static std::atomic<long long> sTableIdCounter = 1;
static std::mutex sGlobalTableMutex;
static std::unordered_map<long long, InternalTableState> sTableStates;

///------------------------------------------------------------------------------------------------

std::vector<poker::Card> CreateShuffledDeck()
{
    std::vector<poker::Card> deck;
    
    for (const auto& suit: { poker::CardSuit::SPADE, poker::CardSuit::HEART, poker::CardSuit::DIAMOND, poker::CardSuit::CLUB})
    {
        for (auto i = static_cast<int>(poker::CardRank::TWO); i < static_cast<int>(poker::CardRank::ACE); ++i)
        {
            deck.push_back(poker::Card(static_cast<poker::CardRank>(i), suit));
        }
    }
    
    std::random_device rd;
    std::mt19937 g(rd());
    std::shuffle(deck.begin(), deck.end(), g);
    return deck;
}

///------------------------------------------------------------------------------------------------

void UpdateTableLoop()
{
    while (true)
    {
        std::lock_guard<std::mutex> globalTableLock(sGlobalTableMutex);
        for (auto& tableStateEntry: sTableStates)
        {
            switch (tableStateEntry.second.mRoundState)
            {
                case InternalTableState::RoundState::PLACING_BLINDS:
                {
                    tableStateEntry.second.mRoundState = InternalTableState::RoundState::DEALING_HOLE_CARDS;
                } break;
                    
                case InternalTableState::RoundState::DEALING_HOLE_CARDS:
                {
                    for (auto& playerEntry: tableStateEntry.second.mPlayerHoleCards)
                    {
                        playerEntry.second.push_back(tableStateEntry.second.mDeck.back());
                        tableStateEntry.second.mDeck.pop_back();
                        playerEntry.second.push_back(tableStateEntry.second.mDeck.back());
                        tableStateEntry.second.mDeck.pop_back();
                    }
                    
                    tableStateEntry.second.mRoundState = InternalTableState::RoundState::WAITING_FOR_ACTIONS_PREFLOP;
                    tableStateEntry.second.mTableTimer = std::chrono::high_resolution_clock::now();
                } break;
                    
                case InternalTableState::RoundState::WAITING_FOR_ACTIONS_PREFLOP:
                {
                    auto secondsDelta = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::high_resolution_clock::now() - tableStateEntry.second.mTableTimer).count();
                    if (secondsDelta > 5)
                    {
                        tableStateEntry.second.mRoundState = InternalTableState::RoundState::DEALING_FLOP;
                    }
                } break;
                    
                case InternalTableState::RoundState::DEALING_FLOP:
                {
                    for (int i = 0; i < 3; ++i)
                    {
                        tableStateEntry.second.mCommunityCards.push_back(tableStateEntry.second.mDeck.back());
                        tableStateEntry.second.mDeck.pop_back();
                    }
                    
                    tableStateEntry.second.mTableTimer = std::chrono::high_resolution_clock::now();
                    tableStateEntry.second.mRoundState = InternalTableState::RoundState::WAITING_FOR_ACTIONS_POSTFLOP;
                } break;
                
                case InternalTableState::RoundState::WAITING_FOR_ACTIONS_POSTFLOP:
                {
                    auto secondsDelta = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::high_resolution_clock::now() - tableStateEntry.second.mTableTimer).count();
                    if (secondsDelta > 2)
                    {
                        tableStateEntry.second.mRoundState = InternalTableState::RoundState::DEALING_TURN;
                    }
                } break;
                    
                case InternalTableState::RoundState::DEALING_TURN:
                {
                    tableStateEntry.second.mCommunityCards.push_back(tableStateEntry.second.mDeck.back());
                    tableStateEntry.second.mDeck.pop_back();
                    
                    tableStateEntry.second.mTableTimer = std::chrono::high_resolution_clock::now();
                    tableStateEntry.second.mRoundState = InternalTableState::RoundState::WAITING_FOR_ACTIONS_POSTTURN;
                } break;
                    
                case InternalTableState::RoundState::WAITING_FOR_ACTIONS_POSTTURN:
                {
                    auto secondsDelta = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::high_resolution_clock::now() - tableStateEntry.second.mTableTimer).count();
                    if (secondsDelta > 3)
                    {
                        tableStateEntry.second.mRoundState = InternalTableState::RoundState::DEALING_RIVER;
                    }
                } break;
                    
                case InternalTableState::RoundState::DEALING_RIVER:
                {
                    tableStateEntry.second.mCommunityCards.push_back(tableStateEntry.second.mDeck.back());
                    tableStateEntry.second.mDeck.pop_back();
                    
                    tableStateEntry.second.mTableTimer = std::chrono::high_resolution_clock::now();
                    tableStateEntry.second.mRoundState = InternalTableState::RoundState::WAITING_FOR_ACTIONS_POSTRIVER;
                } break;
                    
                case InternalTableState::RoundState::WAITING_FOR_ACTIONS_POSTRIVER: break;
            }
        }
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

void OnClientPlayRequestMessage(const nlohmann::json& json, const int clientSocket)
{
    networking::PlayResponse playResponse = {};
    playResponse.allowed = true;
    playResponse.playerId = sPlayerIdCounter;
    playResponse.tableId = sTableIdCounter;
    
    {
        std::lock_guard<std::mutex> globalTableStateLock(sGlobalTableMutex);

        sTableStates[sTableIdCounter].mDeck = CreateShuffledDeck();
        
        sTableStates[sTableIdCounter].mPlayerHoleCards.push_back(std::make_pair(sPlayerIdCounter++, std::vector<poker::Card>()));
        sTableStates[sTableIdCounter].mPlayerHoleCards.push_back(std::make_pair(sPlayerIdCounter++, std::vector<poker::Card>())); // Bot

        sTableStates[sTableIdCounter].mRoundState = InternalTableState::RoundState::PLACING_BLINDS;
        sTableIdCounter++;
    }
    
    auto playResponseJson = playResponse.SerializeToJson();
    SendMessageToClient(playResponseJson, networking::MessageType::SC_PLAY_RESPONSE, clientSocket);
}

///------------------------------------------------------------------------------------------------

void OnClientTableStateRequestMessage(const nlohmann::json& json, const int clientSocket)
{
    networking::TableStateRequest tableStateRequest = {};
    tableStateRequest.DeserializeFromJson(json);
    
    networking::TableStateResponse tableStateResponse = {};
    
    {
        std::lock_guard<std::mutex> globalTableStateLock(sGlobalTableMutex);
        auto tableIter = sTableStates.find(tableStateRequest.tableId);
        if (tableIter != sTableStates.end())
        {
            const auto& table = tableIter->second;
            
            for (int i = 0; i < table.mCommunityCards.size(); ++i)
            {
                tableStateResponse.communityCards += table.mCommunityCards[i].ToString();
                if (i != table.mCommunityCards.size() - 1)
                {
                    tableStateResponse.communityCards += ",";
                }
            }
            
            for (const auto& playerEntry: table.mPlayerHoleCards)
            {
                tableStateResponse.holeCardPlayerIds.push_back(playerEntry.first);
                
                if (playerEntry.first == tableStateRequest.playerId)
                {
                    tableStateResponse.holeCards.push_back(playerEntry.second[0].ToString() + "," + playerEntry.second[1].ToString());
                }
                else
                {
                    tableStateResponse.holeCards.push_back("0,0"); // All foreign hole cards are unknown
                }
            }
            
            switch (table.mRoundState)
            {
                case InternalTableState::RoundState::PLACING_BLINDS: tableStateResponse.roundStateName = "PLACING_BLINDS"; break;
                case InternalTableState::RoundState::DEALING_HOLE_CARDS: tableStateResponse.roundStateName = "DEALING_HOLE_CARDS"; break;
                case InternalTableState::RoundState::WAITING_FOR_ACTIONS_PREFLOP: tableStateResponse.roundStateName = "WAITING_FOR_ACTIONS_PREFLOP"; break;
                case InternalTableState::RoundState::DEALING_FLOP: tableStateResponse.roundStateName = "DEALING_FLOP"; break;
                case InternalTableState::RoundState::WAITING_FOR_ACTIONS_POSTFLOP: tableStateResponse.roundStateName = "WAITING_FOR_ACTIONS_POSTFLOP"; break;
                case InternalTableState::RoundState::DEALING_TURN: tableStateResponse.roundStateName = "DEALING_TURN"; break;
                case InternalTableState::RoundState::WAITING_FOR_ACTIONS_POSTTURN: tableStateResponse.roundStateName = "WAITING_FOR_ACTIONS_POSTTURN"; break;
                case InternalTableState::RoundState::DEALING_RIVER: tableStateResponse.roundStateName = "DEALING_RIVER"; break;
                case InternalTableState::RoundState::WAITING_FOR_ACTIONS_POSTRIVER: tableStateResponse.roundStateName = "WAITING_FOR_ACTIONS_POSTRIVER"; break;
            }
        }
    }
    
    auto tableStateResponseJson = tableStateResponse.SerializeToJson();
    SendMessageToClient(tableStateResponseJson, networking::MessageType::SC_TABLE_STATE_RESPONSE, clientSocket);
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
            
            if (networking::IsMessageOfType(receivedJson, networking::MessageType::CS_PLAY_REQUEST))
            {
                OnClientPlayRequestMessage(receivedJson, clientSocket);
            }
            else if (networking::IsMessageOfType(receivedJson, networking::MessageType::CS_TABLE_STATE_REQUEST))
            {
                OnClientTableStateRequestMessage(receivedJson, clientSocket);
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
