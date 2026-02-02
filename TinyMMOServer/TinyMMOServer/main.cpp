///------------------------------------------------------------------------------------------------
///  main.cpp
///  TinyMMOServer
///
///  Created by Alex Koukoulas on 25/04/2024
///------------------------------------------------------------------------------------------------

#include <fstream>
#include <unordered_map>
#include <vector>

#include "net_common/NetworkMessages.h"
#include "net_common/Version.h"

#include "util/Date.h"
#include "util/FileUtils.h"
#include "util/Logging.h"
#include "util/MathUtils.h"
#include "util/StringUtils.h"

///------------------------------------------------------------------------------------------------

using namespace network;

///------------------------------------------------------------------------------------------------

static const float PROJECTILE_TTL = 3.0f;
static const float PLAYER_BASE_SPEED = 0.0003f;
static const float PROJECTILE_SPEED = 0.001f;

static const std::string STARTING_ZONE = "forest_1";

///------------------------------------------------------------------------------------------------

// Assumes relevant object types have been set
void SetColliderData(ObjectData& objectData)
{
    switch (objectData.objectType)
    {
        case network::ObjectType::PLAYER:
        {
            objectData.colliderData.colliderType = network::ColliderType::RECTANGLE;
            objectData.colliderData.colliderRelativeDimentions = glm::vec2(0.5f, 0.8f);
        } break;
        
        case network::ObjectType::ATTACK:
        {
            switch (objectData.attackType)
            {
                case network::AttackType::PROJECTILE:
                {
                    objectData.colliderData.colliderType = network::ColliderType::CIRCLE;
                    objectData.colliderData.colliderRelativeDimentions = glm::vec2(1.0f);
                } break;

                case network::AttackType::NONE:
                    break;
                case network::AttackType::MELEE:
                    break;
            } break;
        } break;
            
        case network::ObjectType::NPC:
            break;
        case network::ObjectType::STATIC:
            break;
    }
}

///------------------------------------------------------------------------------------------------

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
    
    std::unordered_map<ENetPeer*, objectId_t> peerToPlayerId;
    std::unordered_map<objectId_t, ObjectData> objectDataMap;
    std::unordered_map<objectId_t, float> tempObjectTTL;
    std::vector<objectId_t> tempObjectsToRemove;

    objectId_t nextId = 2;

    objectDataMap[1] = {};
    objectDataMap[1].objectId = 1;
    objectDataMap[1].objectType = network::ObjectType::PLAYER;
    objectDataMap[1].attackType = network::AttackType::NONE;
    objectDataMap[1].projectileType = network::ProjectileType::NONE;
    objectDataMap[1].position = glm::vec3(-1.3f, -1.0f, math::RandomFloat(0.11f, 0.5f));
    objectDataMap[1].velocity = glm::vec3(0.0f);
    objectDataMap[1].currentAnimation = network::AnimationType::RUNNING;
    objectDataMap[1].facingDirection = network::FacingDirection::SOUTH;
    objectDataMap[1].speed = PLAYER_BASE_SPEED;

    SetColliderData(objectDataMap[1]);
    SetCurrentMap(objectDataMap[1], STARTING_ZONE);
    
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
                    const auto id = nextId++;
                    peerToPlayerId[event.peer] = id;
                    objectDataMap[id] = {};
                    objectDataMap[id].objectId = id;
                    objectDataMap[id].objectType = network::ObjectType::PLAYER;
                    objectDataMap[id].attackType = network::AttackType::NONE;
                    objectDataMap[id].projectileType = network::ProjectileType::NONE;
                    objectDataMap[id].position = glm::vec3( math::RandomFloat(-1.5f, -1.1f), math::RandomFloat(-1.4, -0.5f), math::RandomFloat(0.11f, 0.5f));
                    objectDataMap[id].velocity = glm::vec3(0.0f);
                    objectDataMap[id].currentAnimation = network::AnimationType::RUNNING;
                    objectDataMap[id].facingDirection = network::FacingDirection::SOUTH;
                    objectDataMap[id].speed = PLAYER_BASE_SPEED;

                    SetColliderData(objectDataMap[id]);
                    SetCurrentMap(objectDataMap[id], STARTING_ZONE);
                    
                    logging::Log(logging::LogType::INFO, "Player %d connected", id);
                    
                    PlayerConnectedMessage playerConnectedMessage = {};
                    playerConnectedMessage.objectId = id;
                    
                    SendMessage(event.peer, &playerConnectedMessage, sizeof(playerConnectedMessage), channels::RELIABLE);
                    
                    ObjectCreatedMessage objectCreatedMessage = {};
                    objectCreatedMessage.objectData = objectDataMap[id];
                    
                    BroadcastMessage(server, &objectCreatedMessage, sizeof(objectCreatedMessage), channels::RELIABLE);

                    break;
                }

                case ENET_EVENT_TYPE_RECEIVE:
                {
                    auto* data = event.packet->data;
                    auto type = static_cast<MessageType>(data[0]);
                    
                    auto messageValidity = GetMessageVersionValidity(data);
                    if (messageValidity == MessageVersionValidityEnum::VALID)
                    {
                        objectId_t playerId = peerToPlayerId[event.peer];

                        if (type == MessageType::ObjectStateUpdateMessage)
                        {
                            auto* msg = reinterpret_cast<ObjectStateUpdateMessage*>(data);
                            
                            // Make sure player updates directly only their data
                            assert(playerId == msg->objectData.objectId);
                            
                            // More checks here probably
                            objectDataMap[playerId] = msg->objectData;
                        }
                        else if (type == MessageType::AttackMessage)
                        {
                            auto* msg = reinterpret_cast<AttackMessage*>(data);
                            const auto& attackerData = objectDataMap.at(msg->attackerId);
                            
                            if (msg->attackType == network::AttackType::PROJECTILE && msg->projectileType == network::ProjectileType::FIREBALL)
                            {
                                const auto id = nextId++;
                                objectDataMap[id] = {};
                                objectDataMap[id].objectId = id;
                                objectDataMap[id].objectType = network::ObjectType::ATTACK;
                                objectDataMap[id].attackType = msg->attackType;
                                objectDataMap[id].projectileType = msg->projectileType;
                                objectDataMap[id].position = attackerData.position;
                                objectDataMap[id].position.z -= 0.001f;
                                objectDataMap[id].currentAnimation = network::AnimationType::IDLE;
                                objectDataMap[id].facingDirection = attackerData.facingDirection;
                                objectDataMap[id].speed = PROJECTILE_SPEED;
                                
                                SetColliderData(objectDataMap[id]);
                                SetCurrentMap(objectDataMap[id], GetCurrentMapString(attackerData));

                                tempObjectTTL[id] = PROJECTILE_TTL;
                                
                                switch (attackerData.facingDirection)
                                {
                                    case network::FacingDirection::NORTH_WEST: objectDataMap[id].velocity = glm::vec3(-PROJECTILE_SPEED, PROJECTILE_SPEED, 0.0f); break;
                                    case network::FacingDirection::NORTH_EAST: objectDataMap[id].velocity = glm::vec3(PROJECTILE_SPEED, PROJECTILE_SPEED, 0.0f); break;
                                    case network::FacingDirection::SOUTH_WEST: objectDataMap[id].velocity = glm::vec3(-PROJECTILE_SPEED, -PROJECTILE_SPEED, 0.0f); break;
                                    case network::FacingDirection::SOUTH_EAST: objectDataMap[id].velocity = glm::vec3(PROJECTILE_SPEED, -PROJECTILE_SPEED, 0.0f); break;
                                    case network::FacingDirection::NORTH: objectDataMap[id].velocity = glm::vec3(0.0f, PROJECTILE_SPEED, 0.0f); break;
                                    case network::FacingDirection::SOUTH: objectDataMap[id].velocity = glm::vec3(0.0f, -PROJECTILE_SPEED, 0.0f); break;
                                    case network::FacingDirection::WEST: objectDataMap[id].velocity = glm::vec3(-PROJECTILE_SPEED, 0.0f, 0.0f); break;
                                    case network::FacingDirection::EAST: objectDataMap[id].velocity = glm::vec3(PROJECTILE_SPEED, 0.0f, 0.0f); break;
                                }
                                
                                ObjectCreatedMessage objectCreatedMessage = {};
                                objectCreatedMessage.objectData = objectDataMap[id];
                                
                                BroadcastMessage(server, &objectCreatedMessage, sizeof(objectCreatedMessage), channels::RELIABLE);
                            }
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
                    objectId_t id = peerToPlayerId[event.peer];
                    objectDataMap.erase(id);
                    peerToPlayerId.erase(event.peer);
                    logging::Log(logging::LogType::INFO, "Player %d disconnected.", id);
                    
                    PlayerDisconnectedMessage playerDCed = {};
                    playerDCed.objectId = id;
                    
                    BroadcastMessage(server, &playerDCed, sizeof(playerDCed), channels::RELIABLE);
                    break;
                }

                default: break;
            }
        }
        
        double now = enet_time_get() / 1000.0;
        float dtMillis = (now - lastTick) * 1000.0f;

        if (now - lastTick >= tickInterval)
        {
            for (auto& [objectId, ttl]: tempObjectTTL)
            {
                auto& objectData = objectDataMap[objectId];
                if (objectData.objectType == network::ObjectType::ATTACK && objectData.attackType == network::AttackType::PROJECTILE)
                {
                    objectData.position += objectData.velocity * dtMillis;
                }

                ttl -= dtMillis / 1000.0f;
                if (ttl <= 0.0f)
                {
                    tempObjectsToRemove.push_back(objectId);
                }
            }

            for (auto objectIdToRemove: tempObjectsToRemove)
            {
                ObjectDestroyedMessage objectDestroyedMessage{};
                objectDestroyedMessage.objectId = objectIdToRemove;
                
                BroadcastMessage(server, &objectDestroyedMessage, sizeof(objectDestroyedMessage), channels::RELIABLE);
                
                tempObjectTTL.erase(objectIdToRemove);
                objectDataMap.erase(objectIdToRemove);
            }
            tempObjectsToRemove.clear();
            
            lastTick = now;

            // Broadcast snapshots
            for (auto& [objectId, data] : objectDataMap)
            {
                ObjectStateUpdateMessage stateUpdateMessage{};
                stateUpdateMessage.objectData = data;

                BroadcastMessage(server, &stateUpdateMessage, sizeof(stateUpdateMessage), channels::UNRELIABLE);
            }
        }
    }

    enet_host_destroy(server);
}

///------------------------------------------------------------------------------------------------
