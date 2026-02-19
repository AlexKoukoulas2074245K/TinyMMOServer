///------------------------------------------------------------------------------------------------
///  main.cpp
///  TinyMMOServer
///
///  Created by Alex Koukoulas on 25/04/2024
///------------------------------------------------------------------------------------------------

#include <fstream>
#include <unordered_map>
#include <vector>

#include "MapDataRepository.h"
#include "NetworkObjectUpdater.h"

#include "events/EventSystem.h"

#include "net_common/Navmap.h"
#include "net_common/NetworkMessages.h"
#include "net_common/Version.h"

#include "util/Logging.h"
#include "util/MathUtils.h"
#include "util/StringUtils.h"
#include "util/ThreadSafeQueue.h"

///------------------------------------------------------------------------------------------------

using namespace network;

///------------------------------------------------------------------------------------------------

//static const float PROJECTILE_TTL = 3.0f;
static const float PLAYER_BASE_SPEED = 0.0003f;
//static const float PROJECTILE_SPEED = 0.0005f;
static const float FAST_MELEE_CHARGE_TIME_SECS = 0.3f;
static const float FAST_MELEE_SLASH_TIME_SECS = 0.3f;

static const std::string STARTING_ZONE = "forest_1";

///------------------------------------------------------------------------------------------------



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
        
        case network::ObjectType::NPC:
        {
            objectData.colliderData.colliderType = network::ColliderType::RECTANGLE;
            objectData.colliderData.colliderRelativeDimentions = glm::vec2(0.8f, 0.8f);
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

                case network::AttackType::MELEE:
                {
                    objectData.colliderData.colliderType = network::ColliderType::CIRCLE;
                    objectData.colliderData.colliderRelativeDimentions = glm::vec2(0.8f, 0.8f);
                } break;
                    
                case network::AttackType::NONE:
                    break;
            } break;
        } break;
            
        case network::ObjectType::STATIC:
            break;
    }
}


///------------------------------------------------------------------------------------------------


int main(int argc, char* argv[])
{
    if (argc >= 2)
    {
        logging::Log(logging::LogType::INFO, "Initializing server from CWD: %s", argv[0]);
        logging::Log(logging::LogType::INFO, "Asset Directory: %s", argv[1]);
    }
    else
    {
        logging::Log(logging::LogType::ERROR, "Asset Directory Not Provided");
        return -1;
    }
    
    MapDataRepository mapDataRepo;
    mapDataRepo.LoadMapData(argv[1]);
    
    NetworkObjectUpdater netObjectUpdater(mapDataRepo);

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
    std::unordered_map<objectId_t, std::pair<ObjectData, float>> pendingObjectsToSpawn;
    std::unordered_map<objectId_t, float> tempObjectTTLSecs;
    std::vector<objectId_t> tempObjectsToRemove;
    
    // Register event listeners
    auto& eventSystem = events::EventSystem::GetInstance();
    std::unique_ptr<events::IListener> collisionEventListener = eventSystem.RegisterForEvent<events::NetworkObjectCollisionEvent>([&](const events::NetworkObjectCollisionEvent& event)
    {
        // collision with geometry
        if (!event.mRhs)
        {
            tempObjectTTLSecs[event.mLhs] = 0.0f;
        }
    });
    
    objectDataMap[1] = {};
    objectDataMap[1].objectId = 1;
    objectDataMap[1].parentObjectId = 1;
    objectDataMap[1].objectType = network::ObjectType::NPC;
    objectDataMap[1].attackType = network::AttackType::NONE;
    objectDataMap[1].projectileType = network::ProjectileType::NONE;
    objectDataMap[1].position = mapDataRepo.GetNavmaps().at(strutils::StringId(STARTING_ZONE)).GetMapPositionFromNavmapCoord(glm::ivec2(32, 32), mapDataRepo.GetMapMetaData().at(strutils::StringId(STARTING_ZONE)).mMapPosition, MAP_GAME_SCALE, 0.5f);
    objectDataMap[1].velocity = glm::vec3(0.0f);
    objectDataMap[1].objectState = network::ObjectState::IDLE;
    objectDataMap[1].facingDirection = network::FacingDirection::SOUTH;
    objectDataMap[1].objectFaction = network::ObjectFaction::EVIL;
    objectDataMap[1].speed = PLAYER_BASE_SPEED/2.0f;
    objectDataMap[1].actionTimer = 3.0f;
    objectDataMap[1].objectScale = 0.1f;

    SetColliderData(objectDataMap[1]);
    SetCurrentMap(objectDataMap[1], STARTING_ZONE);
    objectId_t nextId = 2;
    
    logging::Log(logging::LogType::INFO, "Server running on port 7777");

    ENetEvent event;
    const float tickRate = 40.0f;
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
                    objectDataMap[id].parentObjectId = id;
                    objectDataMap[id].objectType = network::ObjectType::PLAYER;
                    objectDataMap[id].attackType = network::AttackType::NONE;
                    objectDataMap[id].projectileType = network::ProjectileType::NONE;
                    objectDataMap[id].position =  glm::vec3(math::RandomFloat(-1.5f, -1.1f), math::RandomFloat(-1.4, -0.6f), math::RandomFloat(0.11f, 0.5f));
                    objectDataMap[id].velocity = glm::vec3(0.0f);
                    objectDataMap[id].objectState = network::ObjectState::RUNNING;
                    objectDataMap[id].facingDirection = network::FacingDirection::SOUTH;
                    objectDataMap[id].objectFaction = network::ObjectFaction::GOOD;
                    objectDataMap[id].speed = PLAYER_BASE_SPEED;
                    objectDataMap[id].objectScale = 0.1f;

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
                        else if (type == MessageType::DebugGetQuadtreeRequestMessage)
                        {
                            auto& quadtree = mapDataRepo.GetMapQuadtree(strutils::StringId(GetCurrentMapString(objectDataMap[playerId])));
                            const auto& debugRects = quadtree.GetDebugRenderRectangles();
                            
                            DebugGetQuadtreeResponseMessage quadtreeDataResponseMessage = {};
                            quadtreeDataResponseMessage.quadtreeData.debugRectCount = debugRects.size();
                            
                            for (int i = 0; i < debugRects.size(); ++i)
                            {
                                quadtreeDataResponseMessage.quadtreeData.debugRectPositions[i] = debugRects[i].first;
                                quadtreeDataResponseMessage.quadtreeData.debugRectDimensions[i] = debugRects[i].second;
                            }

                            SendMessage(event.peer, &quadtreeDataResponseMessage, sizeof(quadtreeDataResponseMessage), channels::RELIABLE);
                        }
                        else if (type == MessageType::DebugGetObjectPathRequestMessage)
                        {
                            auto* msg = reinterpret_cast<DebugGetObjectPathRequestMessage*>(data);
                            
                            DebugGetObjectPathResponseMessage objectPathResponseMessage = {};
                            objectPathResponseMessage.objectId = msg->objectId;
                            objectPathResponseMessage.pathData.debugPathPositionsCount = 0;
                            
                            if (netObjectUpdater.DoesObjectHavePath(msg->objectId))
                            {
                                //copy of path
                                auto objectPath = netObjectUpdater.GetPath(msg->objectId);
                                objectPathResponseMessage.pathData.debugPathPositionsCount = objectPath.size();
                                
                                int debugPathIndex = 0;
                                while (!objectPath.empty())
                                {
                                    objectPathResponseMessage.pathData.debugPathPositions[debugPathIndex] = objectPath.front();
                                    objectPath.pop();
                                    debugPathIndex++;
                                }
                            }

                            SendMessage(event.peer, &objectPathResponseMessage, sizeof(objectPathResponseMessage), channels::UNRELIABLE);
                        }
                        else if (type == MessageType::CancelAttackMessage)
                        {
                            auto* msg = reinterpret_cast<CancelAttackMessage*>(data);
                            // Find and discard respective attack from parentId
                            for (auto iter = pendingObjectsToSpawn.begin(); iter != pendingObjectsToSpawn.end();)
                            {
                                auto& pendingObjectEntry = (*iter).second;
                                const auto& objectData = pendingObjectEntry.first;

                                if (msg->attackerId == objectData.parentObjectId)
                                {
                                    tempObjectTTLSecs.erase(iter->first);
                                    iter = pendingObjectsToSpawn.erase(iter);
                                }
                                else
                                {
                                    iter++;
                                }
                            }
                        }
                        else if (type == MessageType::BeginAttackRequestMessage)
                        {
                            auto* msg = reinterpret_cast<BeginAttackRequestMessage*>(data);
                            const auto& attackerData = objectDataMap.at(msg->attackerId);
                            
                            BeginAttackResponseMessage responseMessage = {};
                            responseMessage.allowed = false;
                            responseMessage.attackType = msg->attackType;
                            responseMessage.attackerId = msg->attackerId;
                            responseMessage.chargeDurationSecs = 0.0f;
                            responseMessage.projectileType = msg->projectileType;
                            
                            if (msg->attackType == network::AttackType::MELEE)
                            {
                                responseMessage.allowed = true;
                                responseMessage.chargeDurationSecs = FAST_MELEE_CHARGE_TIME_SECS;
                                
                                const auto id = nextId++;
                                ObjectData objectData = {};
                                objectData.objectId = id;
                                objectData.parentObjectId = attackerData.objectId;
                                objectData.objectType = network::ObjectType::ATTACK;
                                objectData.attackType = msg->attackType;
                                objectData.projectileType = msg->projectileType;
                                objectData.objectState = network::ObjectState::IDLE;
                                objectData.facingDirection = attackerData.facingDirection;
                                objectData.objectFaction = attackerData.objectFaction;
                                objectData.objectScale = 0.125f;
                                objectData.position = attackerData.position;
                                
                                switch (attackerData.facingDirection)
                                {
                                    case network::FacingDirection::SOUTH:
                                    {
                                        objectData.position.y -= network::MAP_TILE_SIZE * 0.8f;
                                    } break;
                                        
                                    case network::FacingDirection::NORTH:
                                    {
                                        objectData.position.y += network::MAP_TILE_SIZE * 0.8f;
                                    } break;
                                        
                                    case network::FacingDirection::WEST:
                                    {
                                        objectData.position.x -= network::MAP_TILE_SIZE * 0.5f;
                                    } break;
                                        
                                    case network::FacingDirection::EAST:
                                    {
                                        objectData.position.x += network::MAP_TILE_SIZE * 0.5f;
                                    } break;
                                        
                                    case network::FacingDirection::NORTH_WEST:
                                    {
                                        objectData.position.x -= network::MAP_TILE_SIZE * 0.3f;
                                        objectData.position.y += network::MAP_TILE_SIZE * 0.6f;
                                    } break;
                                        
                                    case network::FacingDirection::NORTH_EAST:
                                    {
                                        objectData.position.x += network::MAP_TILE_SIZE * 0.3f;
                                        objectData.position.y += network::MAP_TILE_SIZE * 0.6f;
                                    } break;
                                        
                                    case network::FacingDirection::SOUTH_WEST:
                                    {
                                        objectData.position.x -= network::MAP_TILE_SIZE * 0.3f;
                                        objectData.position.y -= network::MAP_TILE_SIZE * 0.6f;
                                    } break;
                                        
                                    case network::FacingDirection::SOUTH_EAST:
                                    {
                                        objectData.position.x += network::MAP_TILE_SIZE * 0.3f;
                                        objectData.position.y -= network::MAP_TILE_SIZE * 0.6f;
                                    } break;
                                }
                                
                                SetColliderData(objectData);
                                SetCurrentMap(objectData, GetCurrentMapString(attackerData));
                                
                                pendingObjectsToSpawn[id] = std::make_pair(objectData, responseMessage.chargeDurationSecs);
                                
                                // Pre-emptively add it to the tempObjects. The timer won't start going down
                                // until it is properly added to the main objectData container.
                                tempObjectTTLSecs[id] = FAST_MELEE_SLASH_TIME_SECS;
                            }

                            SendMessage(event.peer, &responseMessage, sizeof(responseMessage), channels::RELIABLE);
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
            // Clear Quadtrees
            for (auto& [mapName, quadtree]: mapDataRepo.GetMapQuadtrees())
            {
                quadtree->Clear();
            }
            
            // Main object update loop
            for (auto& [objectId, objectData] : objectDataMap)
            {
                netObjectUpdater.UpdateNetworkObject(objectData, dtMillis);
                
                auto iter = tempObjectTTLSecs.find(objectData.objectId);
                if (iter != tempObjectTTLSecs.cend())
                {
                    auto& ttl = iter->second;
                    ttl -= dtMillis / 1000.0f;
                    if (ttl <= 0.0f)
                    {
                        tempObjectsToRemove.push_back(objectData.objectId);
                    }
                }
                
                // Fill quadtrees
                glm::vec3 colliderDimensions(objectData.colliderData.colliderRelativeDimentions.x * objectData.objectScale, objectData.colliderData.colliderRelativeDimentions.y * objectData.objectScale, 1.0f);
                mapDataRepo.GetMapQuadtree(strutils::StringId(GetCurrentMapString(objectData))).InsertObject(objectId, objectData.position, colliderDimensions);
            }
            
            // Update and move objects ready to spawn into main object data map
            for (auto iter = pendingObjectsToSpawn.begin(); iter != pendingObjectsToSpawn.end();)
            {
                auto& pendingObjectEntry = (*iter).second;
                const auto& objectData = pendingObjectEntry.first;
                float& timeToSpawn = pendingObjectEntry.second;

                timeToSpawn -= dtMillis / 1000.0f;
                if (timeToSpawn <= 0.0f)
                {
                    objectDataMap[objectData.objectId] = objectData;
                    
                    ObjectCreatedMessage objectCreatedMessage = {};
                    objectCreatedMessage.objectData = objectData;

                    BroadcastMessage(server, &objectCreatedMessage, sizeof(objectCreatedMessage), channels::RELIABLE);
                    
                    iter = pendingObjectsToSpawn.erase(iter);
                }
                else
                {
                    iter++;
                }
            }
            
            // Remove objects whose lifetime is over
            for (auto objectIdToRemove: tempObjectsToRemove)
            {
                ObjectDestroyedMessage objectDestroyedMessage{};
                objectDestroyedMessage.objectId = objectIdToRemove;
                
                BroadcastMessage(server, &objectDestroyedMessage, sizeof(objectDestroyedMessage), channels::RELIABLE);
                
                tempObjectTTLSecs.erase(objectIdToRemove);
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
