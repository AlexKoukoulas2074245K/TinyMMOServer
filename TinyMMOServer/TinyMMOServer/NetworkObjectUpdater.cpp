///------------------------------------------------------------------------------------------------
///  NetworkObjectUpdater.cpp
///  TinyMMOServer
///
///  Created by Alex Koukoulas on 17/02/2026
///------------------------------------------------------------------------------------------------

#include "NetworkObjectUpdater.h"
#include "MapDataRepository.h"
#include "events/EventSystem.h"
#include "net_common/Navmap.h"

///------------------------------------------------------------------------------------------------

static const float AGGRO_RANGE = network::MAP_TILE_SIZE * 8.0f;

///------------------------------------------------------------------------------------------------

NetworkObjectUpdater::NetworkObjectUpdater(MapDataRepository& mapDataRepository)
    : mMapDataRepository(mapDataRepository)
{
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
    // Path update
    if (mPathController.DoesObjectHavePath(objectData.objectId))
    {
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
        
        // Kill path on map change
        if (CheckForMapChange(objectData, mMapDataRepository.GetMapMetaData().at(strutils::StringId(GetCurrentMapString(objectData)))))
        {
            mPathController.ClearObjectPath(objectData.objectId);
        }
    }
    // Pathless update
    else
    {
        auto currentMap = strutils::StringId(GetCurrentMapString(objectData));
        auto mapPosition = mMapDataRepository.GetMapMetaData().at(currentMap).mMapPosition;
        auto& navmap = mMapDataRepository.GetNavmaps().at(currentMap);

        switch (objectData.objectState)
        {
            // Not chasing any player
            case network::ObjectState::IDLE:
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
                    
                    auto direction = glm::normalize(glm::vec3(otherObjectData.position.x, otherObjectData.position.y, objectData.position.z) - objectData.position);
                    objectData.facingDirection = network::VecToFacingDirection(direction);
                    objectData.objectState = network::ObjectState::RUNNING;
                    
                    events::EventSystem::GetInstance().DispatchEvent<events::NPCAggroEvent>(objectData.objectId, otherObjectData.objectId);
                    
                    mPathController.FindPath(objectData, otherObjectData, mapPosition, navmap);
                }
                
                objectData.actionTimer -= dtMillis / 1000.0f;
                if (objectData.actionTimer < 0)
                {
                    objectData.actionTimer = 5.0f;
                        
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
            } break;
                
            // Chasing player
            case network::ObjectState::RUNNING:
            {
            
            
            } break;
                
            default: break;
        }
    }
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
