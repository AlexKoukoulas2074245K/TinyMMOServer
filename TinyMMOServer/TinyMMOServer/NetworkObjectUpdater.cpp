///------------------------------------------------------------------------------------------------
///  NetworkObjectUpdater.cpp
///  TinyMMOServer
///
///  Created by Alex Koukoulas on 17/02/2026
///------------------------------------------------------------------------------------------------

#include "NetworkObjectUpdater.h"
#include "MapDataRepository.h"
#include "events/EventSystem.h"

///------------------------------------------------------------------------------------------------

static const float AGGRO_RANGE = network::MAP_TILE_SIZE * 4.0f;
static const float NPC_LOITERING_TIMER_SECS = 5.0f;
static const float NPC_ATTACK_ANIMATION_TIMER_SECS = 0.5f;
static const float NPC_PATH_RECALCULATION_SECS = 0.05f;

///------------------------------------------------------------------------------------------------

NetworkObjectUpdater::NetworkObjectUpdater(MapDataRepository& mapDataRepository)
    : mMapDataRepository(mapDataRepository)
{
    events::EventSystem::GetInstance().RegisterForEvent<events::ObjectDestroyedEvent>(this, &NetworkObjectUpdater::OnObjectDestroyedEvent);
}

///------------------------------------------------------------------------------------------------

bool NetworkObjectUpdater::DoesObjectHavePath(const network::objectId_t objectId) const
{
    return mPathController.DoesObjectHavePath(objectId);
}

///------------------------------------------------------------------------------------------------

std::queue<glm::vec3>& NetworkObjectUpdater::GetPath(const network::objectId_t objectId)
{
    return mPathController.GetPath(objectId);
}

///------------------------------------------------------------------------------------------------

const std::queue<glm::vec3>& NetworkObjectUpdater::GetPath(const network::objectId_t objectId) const
{
    return mPathController.GetPath(objectId);
}

///------------------------------------------------------------------------------------------------

void NetworkObjectUpdater::OnObjectDestroyedEvent(const events::ObjectDestroyedEvent &objectDestroyedEvent)
{
    mPathController.ClearObjectPath(objectDestroyedEvent.mObjectId);
    mNPCToTargetEntries.erase(objectDestroyedEvent.mObjectId);
    for (auto iter = mNPCToTargetEntries.begin(); iter != mNPCToTargetEntries.end();)
    {
        if (iter->second.mTargetObjectId == objectDestroyedEvent.mObjectId)
        {
            iter = mNPCToTargetEntries.erase(iter);
        }
        else
        {
            iter++;
        }
    }
}

///------------------------------------------------------------------------------------------------

void NetworkObjectUpdater::PerformPreUpdateSetup(const std::unordered_map<network::objectId_t, network::ObjectData>& objectData)
{
    mPathController.Update();

    mTickObjectData = &objectData;
    mObjectIdsPerMap.clear();
    for (const auto& [objectId, data]: objectData)
    {
        mObjectIdsPerMap[strutils::StringId(network::GetCurrentMapString(data))].push_back(objectId);
    }
}

///------------------------------------------------------------------------------------------------

void NetworkObjectUpdater::UpdateNetworkObject(network::ObjectData& objectData, const float dtMillis)
{
    switch (objectData.objectType)
    {
        case network::ObjectType::ATTACK:
        {
            UpdateAttack(objectData, dtMillis);
        } break;
        
        case network::ObjectType::NPC:
        {
            UpdateNPC(objectData, dtMillis);
        } break;

        default:
            break;
    }
}

///------------------------------------------------------------------------------------------------

void NetworkObjectUpdater::UpdateAttack(network::ObjectData& objectData, const float dtMillis)
{
    objectData.position += objectData.velocity * dtMillis;
    
    auto currentMap = strutils::StringId(GetCurrentMapString(objectData));
    auto mapPosition = mMapDataRepository.GetMapMetaData().at(currentMap).mMapPosition;

    auto navmap = mMapDataRepository.GetNavmaps().at(currentMap);
    if (objectData.attackType == network::AttackType::PROJECTILE && navmap.GetNavmapTileAt(navmap.GetNavmapCoord(objectData.position, mapPosition, network::MAP_GAME_SCALE)) == network::NavmapTileType::SOLID)
    {
        events::EventSystem::GetInstance().DispatchEvent<events::NetworkObjectCollisionEvent>(objectData.objectId);
    }
    
    CheckForMapChange(objectData, mMapDataRepository.GetMapMetaData().at(currentMap));
}

///------------------------------------------------------------------------------------------------

void NetworkObjectUpdater::UpdateNPC(network::ObjectData& objectData, const float dtMillis)
{
    auto currentMap = strutils::StringId(GetCurrentMapString(objectData));
    auto mapPosition = mMapDataRepository.GetMapMetaData().at(currentMap).mMapPosition;
    auto& navmap = mMapDataRepository.GetNavmaps().at(currentMap);
    objectData.velocity = glm::vec3(0.0f);

   
    switch (objectData.objectState)
    {
        // Not chasing any player
        case network::ObjectState::IDLE:
        {
            if (mPathController.DoesObjectHavePath(objectData.objectId))
            {
                UpdateNPCPath(objectData, dtMillis, mapPosition, navmap);
            }
            else
            {
                auto newTargetId = FindValidTarget(objectData, dtMillis, currentMap, mapPosition, navmap);
                if (newTargetId)
                {
                    mNPCToTargetEntries[objectData.objectId] = {newTargetId, NPC_PATH_RECALCULATION_SECS};
                    
                    assert(mTickObjectData->count(newTargetId));
                    const auto& otherObjectData = mTickObjectData->at(newTargetId);
                    
                    // Face target
                    auto direction = glm::normalize(glm::vec3(otherObjectData.position.x, otherObjectData.position.y, objectData.position.z) - objectData.position);
                    objectData.facingDirection = network::VecToFacingDirection(direction);
                    
                    // Fire Aggro event
                    events::EventSystem::GetInstance().DispatchEvent<events::NPCAggroEvent>(objectData.objectId, otherObjectData.objectId);
                    
                    // Find Path to target
                    mPathController.FindPath(objectData, otherObjectData, mapPosition, navmap);
                }
                else if (objectData.actionTimer < 0)
                {
                    // This is a loitering around timer;
                    objectData.actionTimer = NPC_LOITERING_TIMER_SECS;
                    auto mapCoord = navmap.GetNavmapCoord(objectData.position, mapPosition, network::MAP_GAME_SCALE);
                    auto nextDirection = static_cast<network::FacingDirection>(math::RandomInt(0, 7));
                    glm::vec3 toNextPosition;
                    
                    switch (nextDirection)
                    {
                        case network::FacingDirection::SOUTH: toNextPosition = glm::vec3(0.0f, -1.0f, 0.0f); mapCoord.y++; break;
                        case network::FacingDirection::NORTH: toNextPosition = glm::vec3(0.0f, 1.0f, 0.0f); mapCoord.y--; break;
                        case network::FacingDirection::WEST:  toNextPosition = glm::vec3(-1.0f, 0.0f, 0.0f); mapCoord.x--; break;
                        case network::FacingDirection::EAST:  toNextPosition = glm::vec3(1.0f, 0.0f, 0.0f); mapCoord.x++; break;
                        case network::FacingDirection::NORTH_WEST: toNextPosition = glm::vec3(-1.0f, 1.0f, 0.0f); mapCoord.x--; mapCoord.y--; break;
                        case network::FacingDirection::NORTH_EAST: toNextPosition = glm::vec3(1.0f, 1.0f, 0.0f); mapCoord.x++; mapCoord.y--; break;
                        case network::FacingDirection::SOUTH_WEST: toNextPosition = glm::vec3(-1.0f, -1.0f, 0.0f); mapCoord.x--; mapCoord.y++; break;
                        case network::FacingDirection::SOUTH_EAST: toNextPosition = glm::vec3(1.0f, -1.0f, 0.0f); mapCoord.x++; mapCoord.y++; break;
                    }
                    
                    if (navmap.GetNavmapTileAt(mapCoord) == network::NavmapTileType::WALKABLE)
                    {
                        auto targetPosition = navmap.GetMapPositionFromNavmapCoord(mapCoord, mapPosition, network::MAP_GAME_SCALE, objectData.position.z);
                        mPathController.SetObjectTargetPosition(objectData.objectId, targetPosition);
                        
                        objectData.facingDirection = nextDirection;
                    }
                }
            }
        } break;
            
        // Chasing player
        case network::ObjectState::RUNNING:
        {
            objectData.objectState = network::ObjectState::IDLE;
        } break;

        case network::ObjectState::MELEE_ATTACK:
        {
            if (objectData.actionTimer < 0)
            {
                if (!mNPCToTargetEntries.count(objectData.objectId))
                {
                    objectData.objectState = network::ObjectState::IDLE;
                }
                else if (!network::CollidersIntersect(mTickObjectData->at(mNPCToTargetEntries.at(objectData.objectId).mTargetObjectId), objectData))
                {
                    objectData.objectState = network::ObjectState::IDLE;
                    mPathController.FindPath(objectData, mTickObjectData->at(mNPCToTargetEntries.at(objectData.objectId).mTargetObjectId), mapPosition, navmap);
                }
                else
                {
                    events::EventSystem::GetInstance().DispatchEvent<events::NPCAttackEvent>(objectData.objectId, network::AttackType::MELEE, network::ProjectileType::NONE);
                    objectData.actionTimer = NPC_ATTACK_ANIMATION_TIMER_SECS;
                }
            }
        } break;
        
        default: break;
    }
    
    objectData.actionTimer -= dtMillis / 1000.0f;
}

///------------------------------------------------------------------------------------------------

void NetworkObjectUpdater::UpdateNPCPath(network::ObjectData& objectData, const float dtMillis, const glm::vec2& mapPosition, const network::Navmap& navmap)
{
    auto npcToTargetIter = mNPCToTargetEntries.find(objectData.objectId);
    auto& path = mPathController.GetPath(objectData.objectId);
    auto vecToTarget = path.front() - objectData.position;
    float distance = glm::length(vecToTarget);
    float step = objectData.speed * dtMillis;
            
    if (distance > step)
    {
        objectData.velocity = glm::normalize(vecToTarget) * step;
        objectData.position += objectData.velocity;
    }
    else
    {
        objectData.position = path.front();
        objectData.velocity = glm::vec3(0.0f);
        
        path.pop();
        if (path.empty())
        {
            mPathController.ClearObjectPath(objectData.objectId);
            objectData.objectState = network::ObjectState::IDLE;
        }
    }
    objectData.facingDirection = network::VecToFacingDirection(vecToTarget);
    
    // If it is pursuing, recalculate path
    if (npcToTargetIter != mNPCToTargetEntries.end())
    {
        // Can attack
        if (objectData.actionTimer < 0.0f && mTickObjectData->count(npcToTargetIter->second.mTargetObjectId) && network::CollidersIntersect(mTickObjectData->at(npcToTargetIter->second.mTargetObjectId), objectData))
        {
            // This is now an attack animation timer
            events::EventSystem::GetInstance().DispatchEvent<events::NPCAttackEvent>(objectData.objectId, network::AttackType::MELEE, network::ProjectileType::NONE);
            objectData.actionTimer = NPC_ATTACK_ANIMATION_TIMER_SECS;
            objectData.objectState = network::ObjectState::MELEE_ATTACK;
            mPathController.ClearObjectPath(objectData.objectId);
        }
        else
        {
            npcToTargetIter->second.mPathRecalculationTimer -= dtMillis/1000.0f;
            if (npcToTargetIter->second.mPathRecalculationTimer < 0.0f)
            {
                npcToTargetIter->second.mPathRecalculationTimer += NPC_PATH_RECALCULATION_SECS;
                mPathController.FindPath(objectData, mTickObjectData->at(npcToTargetIter->second.mTargetObjectId), mapPosition, navmap);
            }
        }
    }
    
    // Kill path on map change
    if (CheckForMapChange(objectData, mMapDataRepository.GetMapMetaData().at(strutils::StringId(GetCurrentMapString(objectData)))))
    {
        mPathController.ClearObjectPath(objectData.objectId);
    }
}

///------------------------------------------------------------------------------------------------

network::objectId_t NetworkObjectUpdater::FindValidTarget(network::ObjectData& objectData, const float dtMillis, const strutils::StringId& currentMap, const glm::vec2& mapPosition, const network::Navmap& navmap)
{
    // Aggro check
    const auto& objectIdsInCurrentMap = mObjectIdsPerMap.at(currentMap);
    for (const auto id: objectIdsInCurrentMap)
    {
        // Identity check
        if (id == objectData.objectId)
        {
            continue;
        }
        
        // Neutrality check
        if (objectData.objectFaction == network::ObjectFaction::NEUTRAL)
        {
            continue;
        }
        
        const auto& otherObjectData = mTickObjectData->at(id);
        
        // Object type check
        if (otherObjectData.objectType != network::ObjectType::PLAYER && otherObjectData.objectType != network::ObjectType::NPC)
        {
            continue;
        }
        
        // Object faction check
        if (objectData.objectFaction == otherObjectData.objectFaction)
        {
            continue;
        }
        
        // Range check
        if (glm::distance(objectData.position, glm::vec3(otherObjectData.position.x, otherObjectData.position.y, objectData.position.z)) > AGGRO_RANGE)
        {
            continue;
        }
        
        // LOS check
        if (!mPathController.IsTargetInLOS(objectData, otherObjectData, navmap, mapPosition, dtMillis))
        {
            continue;
        }
        
        return id;
    }
    
    return 0;
}

///------------------------------------------------------------------------------------------------

bool NetworkObjectUpdater::CheckForMapChange(network::ObjectData& objectData, const MapMetaData& currentMapMetaData)
{
    strutils::StringId nextMapName;
    if (objectData.position.x > currentMapMetaData.mMapPosition.x * network::MAP_GAME_SCALE + (currentMapMetaData.mMapDimensions.x * network::MAP_GAME_SCALE)/2.0f)
    {
        nextMapName = currentMapMetaData.mMapConnections[static_cast<int>(MapConnectionDirection::EAST)];
    }
    else if (objectData.position.x < currentMapMetaData.mMapPosition.x * network::MAP_GAME_SCALE - (currentMapMetaData.mMapDimensions.x * network::MAP_GAME_SCALE)/2.0f)
    {
        nextMapName = currentMapMetaData.mMapConnections[static_cast<int>(MapConnectionDirection::WEST)];
    }
    else if (objectData.position.y > currentMapMetaData.mMapPosition.y * network::MAP_GAME_SCALE + (currentMapMetaData.mMapDimensions.y * network::MAP_GAME_SCALE)/2.0f)
    {
        nextMapName = currentMapMetaData.mMapConnections[static_cast<int>(MapConnectionDirection::NORTH)];
    }
    else if (objectData.position.y < currentMapMetaData.mMapPosition.y * network::MAP_GAME_SCALE - (currentMapMetaData.mMapDimensions.y * network::MAP_GAME_SCALE)/2.0f)
    {
        nextMapName = currentMapMetaData.mMapConnections[static_cast<int>(MapConnectionDirection::SOUTH)];
    }
    
    if (!nextMapName.isEmpty() && nextMapName.GetString() != "None")
    {
        SetCurrentMap(objectData, nextMapName.GetString());
    }
    
    return !nextMapName.isEmpty();
}

///------------------------------------------------------------------------------------------------

