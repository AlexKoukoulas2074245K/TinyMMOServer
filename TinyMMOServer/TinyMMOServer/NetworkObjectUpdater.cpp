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

NetworkObjectUpdater::NetworkObjectUpdater(MapDataRepository& mapDataRepository)
    : mMapDataRepository(mapDataRepository)
{
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
            }
        }
        objectData.facingDirection = network::VecToFacingDirection(vecToTarget);
        
        // Kill path on map change
        if (CheckForMapChange(objectData, mMapDataRepository.GetMapMetaData().at(strutils::StringId(GetCurrentMapString(objectData)))))
        {
            mPathController.ClearObjectPath(objectData.objectId);
        }
    }
    else
    {
        objectData.actionTimer -= dtMillis / 1000.0f;
        if (objectData.actionTimer < 0)
        {
            objectData.actionTimer = 5.0f;
                
            auto currentMap = strutils::StringId(GetCurrentMapString(objectData));
            auto mapPosition = mMapDataRepository.GetMapMetaData().at(currentMap).mMapPosition;
            
            auto mapCoord = mMapDataRepository.GetNavmaps().at(currentMap).GetNavmapCoord(objectData.position, mapPosition, network::MAP_GAME_SCALE);
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
            
            if (mMapDataRepository.GetNavmaps().at(currentMap).GetNavmapTileAt(mapCoord) == network::NavmapTileType::WALKABLE)
            {
                auto targetPosition = mMapDataRepository.GetNavmaps().at(currentMap).GetMapPositionFromNavmapCoord(mapCoord, mapPosition, network::MAP_GAME_SCALE, objectData.position.z);
                mPathController.SetObjectTargetPosition(objectData.objectId, targetPosition);
                
                objectData.facingDirection = nextDirection;
            }
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
