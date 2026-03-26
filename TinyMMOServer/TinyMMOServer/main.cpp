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
#include "NetworkObjectSpawner.h"

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
    
    NetworkObjectSpawner netObjectSpawner;
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
    std::unordered_map<objectId_t, std::unordered_set<objectId_t>> attackAffectedObjectIds;
    
    std::vector<objectId_t> tempObjectsToDestroy;
    
    auto SpawnRatLambda = [&]()
    {
        auto position = mapDataRepo.GetNavmaps().at(strutils::StringId(STARTING_ZONE)).GetMapPositionFromNavmapCoord(glm::ivec2(math::RandomInt(25,30), math::RandomInt(25,30)), mapDataRepo.GetMapMetaData().at(strutils::StringId(STARTING_ZONE)).mMapPosition, MAP_GAME_SCALE, 0.5f);
        position.z = math::RandomFloat(0.4f, 0.5f);
        
        auto objectData = netObjectSpawner.NewObject()
            .SetObjectType(network::ObjectType::NPC)
            .SetPosition(position)
            .SetObjectState(network::ObjectState::IDLE)
            .SetFacingDirection(network::FacingDirection::SOUTH)
            .SetObjectFaction(network::ObjectFaction::EVIL)
            .SetCurrentHealthPoints(100)
            .SetMaxHealthPoints(100)
            .SetDamagePoints(5)
            .SetSpeed(PLAYER_BASE_SPEED)
            .SetActionTimer(3.0f)
            .SetObjectScale(0.1f)
            .SetColliderType(network::ColliderType::RECTANGLE)
            .SetColliderRelativeDimensions(glm::vec2(0.5f, 0.5f))
            .SetCurrentMap(STARTING_ZONE)
            .SetDisplayName("Evil Rat")
        .GetObjectData();
        
        pendingObjectsToSpawn[objectData.objectId] = std::make_pair(std::move(objectData), 0.0f);
    };
    
    auto ObjectDestructionCleanupLambda = [&](const objectId_t objectId)
    {
        events::EventSystem::GetInstance().DispatchEvent<events::ObjectDestroyedEvent>(objectId);
        objectDataMap.erase(objectId);
        tempObjectTTLSecs.erase(objectId);
        
        if (attackAffectedObjectIds.count(objectId))
        {
            attackAffectedObjectIds.erase(objectId);
        }
        else
        {
            for (auto& [attackId, affectedObjectIds]: attackAffectedObjectIds)
            {
                affectedObjectIds.erase(objectId);
            }
        }
    };
    
    auto DamageApplicationLambda = [&](const objectId_t defenderId, const health_t damagePoints)
    {
        auto& defenderData = objectDataMap[defenderId];
        
        CharacterDamagedMessage charDamagedMessage = {};
        charDamagedMessage.objectId = defenderId;
        charDamagedMessage.damagePoints = damagePoints;
        
        defenderData.currentHealthPoints = math::Min(defenderData.maxHealthPoints, math::Max(0LL, defenderData.currentHealthPoints - charDamagedMessage.damagePoints));
        charDamagedMessage.newHealthPoints = defenderData.currentHealthPoints;
        
        BroadcastMessage(server, &charDamagedMessage, sizeof(charDamagedMessage), channels::RELIABLE);
        
        if (defenderData.objectType == ObjectType::NPC && defenderData.currentHealthPoints <= 0)
        {
            if (std::find(tempObjectsToDestroy.begin(), tempObjectsToDestroy.end(), defenderId) == tempObjectsToDestroy.end())
            {
                tempObjectsToDestroy.push_back(defenderId);
                
                if (defenderData.objectFaction == ObjectFaction::EVIL)
                {
                    SpawnRatLambda();
                }
            }
        }
    };
    
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
    
    std::unique_ptr<events::IListener> npcAttackEventListener = eventSystem.RegisterForEvent<events::NPCAttackEvent>([&](const events::NPCAttackEvent& event)
    {
        NPCAttackMessage npcAttackMessage = {};
        npcAttackMessage.attackerId = event.mAttackerId;
        npcAttackMessage.attackType = event.mAttackType;
        npcAttackMessage.projectileType = event.mProjectileType;
        
        BroadcastMessage(server, &npcAttackMessage, sizeof(npcAttackMessage), channels::RELIABLE);
        
        DamageApplicationLambda(event.mDefenderId, objectDataMap[event.mAttackerId].damagePoints);
    });
    
    // Spawn them test rats
    for (int i = 1; i < 2; ++i)
    {
        SpawnRatLambda();
    }
    
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
                    auto objectData = netObjectSpawner.NewObject()
                        .SetObjectType(network::ObjectType::PLAYER)
                        .SetPosition(glm::vec3(-1.0f, -1.0f, math::RandomFloat(0.11f, 0.5f)))
                        .SetObjectState(network::ObjectState::RUNNING)
                        .SetFacingDirection(network::FacingDirection::SOUTH)
                        .SetObjectFaction(network::ObjectFaction::GOOD)
                        .SetCurrentHealthPoints(100)
                        .SetMaxHealthPoints(100)
                        .SetDamagePoints(0)
                        .SetSpeed(PLAYER_BASE_SPEED)
                        .SetObjectScale(0.1f)
                        .SetColliderType(network::ColliderType::RECTANGLE)
                        .SetColliderRelativeDimensions(glm::vec2(0.5f, 0.8f))
                        .SetCurrentMap(STARTING_ZONE)
                        .SetDisplayName("Player")
                    .GetObjectData();
                    
                    peerToPlayerId[event.peer] = objectData.objectId;
                    logging::Log(logging::LogType::INFO, "Player %d connected", objectData.objectId);
                    
                    PlayerConnectedMessage playerConnectedMessage = {};
                    playerConnectedMessage.objectId = objectData.objectId;
                    
                    SendMessage(event.peer, &playerConnectedMessage, sizeof(playerConnectedMessage), channels::RELIABLE);
                    
                    objectDataMap.insert(std::make_pair(objectData.objectId, objectData));
                    ObjectCreatedMessage objectCreatedMessage = {};
                    objectCreatedMessage.objectData = objectDataMap[objectData.objectId];
                    
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
                        else if (type == MessageType::DebugSetSwarmParams)
                        {
                            auto* msg = reinterpret_cast<DebugSetSwarmParams*>(data);
                            netObjectUpdater.SetSwarmParams(msg->separationDistance, msg->separationWeight);
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
                            
                            if (msg->attackType == AttackType::MELEE)
                            {
                                responseMessage.allowed = true;
                                responseMessage.chargeDurationSecs = FAST_MELEE_CHARGE_TIME_SECS;
                                
                                auto objectData = netObjectSpawner.NewObject()
                                    .SetParentObjectId(attackerData.objectId)
                                    .SetObjectType(network::ObjectType::ATTACK)
                                    .SetAttackType(msg->attackType)
                                    .SetProjectileType(msg->projectileType)
                                    .SetObjectState(network::ObjectState::IDLE)
                                    .SetFacingDirection(attackerData.facingDirection)
                                    .SetObjectFaction(attackerData.objectFaction)
                                    .SetObjectScale(0.125f)
                                    .SetPosition(attackerData.position)
                                    .SetDamagePoints(50)
                                .GetObjectData();
                                
                                switch (attackerData.facingDirection)
                                {
                                    case FacingDirection::SOUTH:
                                    {
                                        objectData.position.y -= MAP_TILE_SIZE * 0.8f;
                                    } break;
                                        
                                    case FacingDirection::NORTH:
                                    {
                                        objectData.position.y += MAP_TILE_SIZE * 0.8f;
                                    } break;
                                        
                                    case FacingDirection::WEST:
                                    {
                                        objectData.position.x -= MAP_TILE_SIZE * 0.5f;
                                    } break;
                                        
                                    case FacingDirection::EAST:
                                    {
                                        objectData.position.x += MAP_TILE_SIZE * 0.5f;
                                    } break;
                                        
                                    case FacingDirection::NORTH_WEST:
                                    {
                                        objectData.position.x -= MAP_TILE_SIZE * 0.3f;
                                        objectData.position.y += MAP_TILE_SIZE * 0.6f;
                                    } break;
                                        
                                    case FacingDirection::NORTH_EAST:
                                    {
                                        objectData.position.x += MAP_TILE_SIZE * 0.3f;
                                        objectData.position.y += MAP_TILE_SIZE * 0.6f;
                                    } break;
                                        
                                    case FacingDirection::SOUTH_WEST:
                                    {
                                        objectData.position.x -= MAP_TILE_SIZE * 0.3f;
                                        objectData.position.y -= MAP_TILE_SIZE * 0.6f;
                                    } break;
                                        
                                    case FacingDirection::SOUTH_EAST:
                                    {
                                        objectData.position.x += MAP_TILE_SIZE * 0.3f;
                                        objectData.position.y -= MAP_TILE_SIZE * 0.6f;
                                    } break;
                                }
                                
                                objectData.colliderData.colliderType = ColliderType::CIRCLE;
                                objectData.colliderData.colliderRelativeDimensions = glm::vec2(0.8f, 0.8f);
                                SetCurrentMap(objectData, GetCurrentMapString(attackerData));
                                
                                pendingObjectsToSpawn[objectData.objectId] = std::make_pair(objectData, responseMessage.chargeDurationSecs);
                                
                                // Pre-emptively add it to the tempObjects. The timer won't start going down
                                // until it is properly added to the main objectData container.
                                tempObjectTTLSecs[objectData.objectId] = FAST_MELEE_SLASH_TIME_SECS;
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
                    peerToPlayerId.erase(event.peer);
                    
                    ObjectDestructionCleanupLambda(id);

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
            
            netObjectUpdater.PerformPreUpdateSetup(objectDataMap);

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
                        tempObjectsToDestroy.push_back(objectData.objectId);
                    }
                }
                
                // Fill quadtrees
                glm::vec3 colliderDimensions(objectData.colliderData.colliderRelativeDimensions.x * objectData.objectScale, objectData.colliderData.colliderRelativeDimensions.y * objectData.objectScale, 1.0f);
                mapDataRepo.GetMapQuadtree(strutils::StringId(GetCurrentMapString(objectData))).InsertObject(objectId, objectData.position, colliderDimensions);
            }
            
            // Collision response
            for (auto& [objectId, objectData] : objectDataMap)
            {
                if (objectData.objectType == ObjectType::ATTACK)
                {
                    auto collisionCandidates = mapDataRepo.GetMapQuadtree(strutils::StringId(GetCurrentMapString(objectData))).GetCollisionCandidates(objectData);
                    for (auto candidateId: collisionCandidates)
                    {
                        // Ignore non player/npc collision candidates
                        if (objectDataMap[candidateId].objectType != ObjectType::NPC && objectDataMap[candidateId].objectType != ObjectType::PLAYER)
                        {
                            continue;
                        }
                        
                        // Ignore same faction candidates
                        if (objectDataMap[candidateId].objectFaction == objectData.objectFaction)
                        {
                            continue;
                        }
                        
                        // Ignore already affected candidates
                        if (attackAffectedObjectIds[objectId].count(candidateId))
                        {
                            continue;
                        }
                                                
                        // Ignore non colliding candidates
                        if (!CollidersIntersect(objectData, objectDataMap[candidateId]))
                        {
                            continue;
                        }
                        
                        attackAffectedObjectIds[objectId].insert(candidateId);
                        DamageApplicationLambda(candidateId, objectData.damagePoints);
                    }
                }
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
            for (auto objectIdToRemove: tempObjectsToDestroy)
            {
                ObjectDestroyedMessage objectDestroyedMessage{};
                objectDestroyedMessage.objectId = objectIdToRemove;
                
                BroadcastMessage(server, &objectDestroyedMessage, sizeof(objectDestroyedMessage), channels::RELIABLE);
                
                ObjectDestructionCleanupLambda(objectIdToRemove);
            }
            tempObjectsToDestroy.clear();
            
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
}

///------------------------------------------------------------------------------------------------
