///------------------------------------------------------------------------------------------------
///  main.cpp
///  TinyMMOServer
///
///  Created by Alex Koukoulas on 25/04/2024
///------------------------------------------------------------------------------------------------

#include <algorithm>
#include <atomic>
#include <arpa/inet.h>
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
#include "util/Date.h"
#include "util/FileUtils.h"
#include "util/Json.h"
#include "util/Logging.h"
#include "util/MathUtils.h"
#include "util/NameGenerator.h"
#include "util/StringUtils.h"
#include "util/lodepng.h"

///------------------------------------------------------------------------------------------------

using WorldTranslationsType = std::unordered_map<std::string, std::string>;

static constexpr int PORT = 8070;
static constexpr int MAX_INCOMING_MSG_BUFFER_SIZE = 8192;
static std::atomic<long long> sWorldObjectIdCounter = 1;
static std::vector<std::string> sDictionarySupportedLanguages;
static std::vector<WorldTranslationsType> sDictionaryWordTranslations;

///------------------------------------------------------------------------------------------------

void UpdateLoop()
{
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
    loginResponse.playerId = sWorldObjectIdCounter++;
    loginResponse.allowed = true;
    loginResponse.playerName = strutils::StringId(GenerateName());

    auto loginResponseJson = loginResponse.SerializeToJson();
    SendMessageToClient(loginResponseJson, networking::MessageType::SC_LOGIN_RESPONSE, clientSocket);
}

///------------------------------------------------------------------------------------------------

void OnClientWordRequestMessage(const nlohmann::json& json, const int clientSocket)
{
    networking::WordRequest wordRequest = {};
    wordRequest.DeserializeFromJson(json);
    
    networking::WordResponse wordResponse = {};
    if (std::find(sDictionarySupportedLanguages.cbegin(), sDictionarySupportedLanguages.cend(), wordRequest.sourceLanguge) != sDictionarySupportedLanguages.cend() &&
        std::find(sDictionarySupportedLanguages.cbegin(), sDictionarySupportedLanguages.cend(), wordRequest.targetLanguage) != sDictionarySupportedLanguages.cend())
    {
        wordResponse.allowed = true;
        
        auto wordFamilyIndex = math::RandomInt(0, static_cast<int>(sDictionaryWordTranslations.size())/4 - 1);
        auto sourceWordIndexOffset = math::RandomInt(0, 3);
        
        const auto& wordTranslations = sDictionaryWordTranslations.at(wordFamilyIndex * 4 + sourceWordIndexOffset);
        wordResponse.sourceWord = wordTranslations.at(wordRequest.sourceLanguge);
        wordResponse.choices.push_back(wordTranslations.at(wordRequest.targetLanguage));
        
        for (auto i = 0; i < 4; ++i)
        {
            if (i != sourceWordIndexOffset)
            {
                wordResponse.choices.push_back(sDictionaryWordTranslations.at(wordFamilyIndex * 4 + i).at(wordRequest.targetLanguage));
            }
        }
        
        std::random_device rd;
        std::mt19937 g(rd());
        std::shuffle(wordResponse.choices.begin(), wordResponse.choices.end(), g);
    }
    else
    {
        wordResponse.allowed = false;
    }
    auto wordResponseJson = wordResponse.SerializeToJson();
    SendMessageToClient(wordResponseJson, networking::MessageType::SC_WORD_RESPONSE, clientSocket);
}


///------------------------------------------------------------------------------------------------

void OnClientGetSupportedLanguagesRequestMessage(const nlohmann::json& json, const int clientSocket)
{
    networking::GetSupportedLanguagesResponse response = {};
    response.supportedLanguages = sDictionarySupportedLanguages;
    auto responseJson = response.SerializeToJson();
    SendMessageToClient(responseJson, networking::MessageType::SC_GET_SUPPORTED_LANGUAGES_RESPONSE, clientSocket);
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
            
            if (networking::IsMessageOfType(receivedJson, networking::MessageType::CS_LOGIN_REQUEST))
            {
                OnClientLoginRequestMessage(receivedJson, clientSocket);
            }
            else if (networking::IsMessageOfType(receivedJson, networking::MessageType::CS_WORD_REQUEST))
            {
                OnClientWordRequestMessage(receivedJson, clientSocket);
            }
            else if (networking::IsMessageOfType(receivedJson, networking::MessageType::CS_GET_SUPPORTED_LANGUAGES_REQUEST))
            {
                OnClientGetSupportedLanguagesRequestMessage(receivedJson, clientSocket);
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

void LoadWordsDictionary(const std::string& assetsDirectory)
{
    std::ifstream dataFile(assetsDirectory + "/data/words.csv");
    
    sDictionarySupportedLanguages.clear();
    sDictionaryWordTranslations.clear();
    
    if (dataFile.is_open())
    {
        std::string csvLine;
        std::getline(dataFile, csvLine);
        
        // Load supported languages
        sDictionarySupportedLanguages = strutils::StringSplit(csvLine, ',');
        
        // Load word translations
        while (std::getline(dataFile, csvLine))
        {
            auto words = strutils::StringSplit(csvLine, ',');
            assert(words.size() == sDictionarySupportedLanguages.size());
            
            WorldTranslationsType translations;
            for (auto i = 0; i < words.size(); ++i)
            {
                translations[sDictionarySupportedLanguages[i]] = words[i];
            }
            sDictionaryWordTranslations.push_back(translations);
        }
    }
    
    logging::Log(logging::LogType::INFO, "Loaded Word data for %lu entries.", sDictionaryWordTranslations.size());
}

///------------------------------------------------------------------------------------------------

int main(int argc, char* argv[])
{
    if (argc < 2)
    {
        logging::Log(logging::LogType::INFO, "Asset Directory not provided");
        exit(EXIT_FAILURE);
    }
    
    logging::Log(logging::LogType::INFO, "Initializing server from CWD: %s", argv[0]);
    logging::Log(logging::LogType::INFO, "Asset Directory: %s", argv[1]);
    
    LoadWordsDictionary(argv[1]);
    
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
    std::thread(UpdateLoop).detach();
    
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
