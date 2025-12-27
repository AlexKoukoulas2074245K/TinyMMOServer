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
#include <enet/enet.h>
#include <sys/socket.h>
#include <mutex>
#include <thread>
#include <unistd.h>
#include <unordered_set>
#include <unordered_map>
#include <vector>

#include "net_common/NetworkMessages.h"
#include "net_common/Version.h"

#include "util/Date.h"
#include "util/FileUtils.h"
#include "util/Json.h"
#include "util/Logging.h"
#include "util/MathUtils.h"
#include "util/StringUtils.h"


///------------------------------------------------------------------------------------------------

struct PlayerState
{
    glm::vec2 position{};
};

int main()
{
    enet_initialize();
    atexit(enet_deinitialize);

    ENetAddress address{};
    address.host = ENET_HOST_ANY;
    address.port = 7777;

    ENetHost* server = enet_host_create(
        &address,
        32,     // max clients
        2,      // channels
        0, 0
    );
    
    if (!server)
    {
        logging::Log(logging::LogType::ERROR, "Failed to create ENet host!");
        return EXIT_FAILURE;
    }
    
    std::unordered_map<ENetPeer*, uint32_t> peerToPlayerId;
    std::unordered_map<uint32_t, PlayerState> players;

    uint32_t nextPlayerId = 1;

    logging::Log(logging::LogType::INFO, "Server running on port 7777");

    ENetEvent event;
    const float tickRate = 20.0f;
    const float tickInterval = 1.0f / tickRate;
    double lastTick = enet_time_get() / 1000.0;

    while (true)
    {
        while (enet_host_service(server, &event, 1) > 0)
        {
            switch (event.type)
            {
                case ENET_EVENT_TYPE_CONNECT:
                {
                    uint32_t id = nextPlayerId++;
                    peerToPlayerId[event.peer] = id;
                    players[id] = {};
                    
                    logging::Log(logging::LogType::INFO, "Player %d connected", id);
                    break;
                }

                case ENET_EVENT_TYPE_RECEIVE:
                {
                    auto* data = event.packet->data;
                    auto type = static_cast<MessageType>(data[0]);
                    
                    auto messageValidity = GetMessageVersionValidity(data);
                    if (messageValidity == MessageVersionValidityEnum::VALID)
                    {
                        uint32_t playerId = peerToPlayerId[event.peer];

                        if (type == MessageType::MoveMessage)
                        {
                            auto* msg = reinterpret_cast<MoveMessage*>(data);
                            players[playerId].position = msg->position;
                        }
                        else if (type == MessageType::AttackMessage)
                        {
                            logging::Log(logging::LogType::INFO, "Player %d attacked", playerId);
                        }
                        else if (type == MessageType::QuestCompleteMessage)
                        {
                            auto* msg = reinterpret_cast<QuestCompleteMessage*>(data);
                            logging::Log(logging::LogType::INFO, "Player %d completed quest %d", playerId, msg->questId);
                        }
                    }
                    else
                    {
                        logging::Log(logging::LogType::ERROR, "Invalid incoming message: %s", GetMessageVersionValidityString(messageValidity));
                    }
                    

                    enet_packet_destroy(event.packet);
                    break;
                }

                case ENET_EVENT_TYPE_DISCONNECT:
                {
                    uint32_t id = peerToPlayerId[event.peer];
                    players.erase(id);
                    peerToPlayerId.erase(event.peer);
                    logging::Log(logging::LogType::INFO, "Player %d disconnected.", id);
                    break;
                }

                default: break;
            }
        }

        double now = enet_time_get() / 1000.0;
        if (now - lastTick >= tickInterval)
        {
            lastTick = now;

            // Broadcast snapshots
            for (auto& [playerId, state] : players)
            {
                SnapshotMessage snap{};
                snap.playerId = playerId;
                snap.position = state.position;

                ENetPacket* packet = enet_packet_create(
                    &snap,
                    sizeof(snap),
                    0 // UNRELIABLE
                );

                enet_host_broadcast(server, 0, packet);
            }
        }
    }

    enet_host_destroy(server);
}

///------------------------------------------------------------------------------------------------
