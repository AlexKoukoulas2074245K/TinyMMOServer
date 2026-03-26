///------------------------------------------------------------------------------------------------
///  NetworkObjectUpdater.cpp
///  TinyMMOServer
///
///  Created by Alex Koukoulas on 17/02/2026
///------------------------------------------------------------------------------------------------

#include "NetworkObjectUpdater.h"
#include "MapDataRepository.h"
#include "events/EventSystem.h"
#include "util/Logging.h"

///------------------------------------------------------------------------------------------------

namespace network
{

///------------------------------------------------------------------------------------------------

static const float AGGRO_RANGE = MAP_TILE_SIZE * 4.0f;
static const float NPC_LOITERING_TIMER_SECS = 5.0f;
static const float NPC_ATTACK_ANIMATION_TIMER_SECS = 1.0f;
static const float NPC_PATH_RECALCULATION_SECS = 0.05f;
static const float NPC_SEPARATION_DISTANCE = 0.04f;
static const float NPC_SEPARATION_WEIGHT = 0.5f;

///------------------------------------------------------------------------------------------------

NetworkObjectUpdater::NetworkObjectUpdater(MapDataRepository& mapDataRepository)
: mMapDataRepository(mapDataRepository)
, mSeparationDistance(NPC_SEPARATION_DISTANCE)
, mSeparationWeight(NPC_SEPARATION_WEIGHT)
{
    events::EventSystem::GetInstance().RegisterForEvent<events::ObjectDestroyedEvent>(this, &NetworkObjectUpdater::OnObjectDestroyedEvent);
}

///------------------------------------------------------------------------------------------------

bool NetworkObjectUpdater::DoesObjectHavePath(const objectId_t objectId) const
{
    return mPathController.DoesObjectHavePath(objectId);
}

///------------------------------------------------------------------------------------------------

std::queue<glm::vec3>& NetworkObjectUpdater::GetPath(const objectId_t objectId)
{
    return mPathController.GetPath(objectId);
}

///------------------------------------------------------------------------------------------------

const std::queue<glm::vec3>& NetworkObjectUpdater::GetPath(const objectId_t objectId) const
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

void NetworkObjectUpdater::PerformPreUpdateSetup(const std::unordered_map<objectId_t, ObjectData>& objectData)
{
    mPathController.Update();
    
    mTickObjectData = &objectData;
    mObjectIdsPerMap.clear();
    for (const auto& [objectId, data]: objectData)
    {
        mObjectIdsPerMap[strutils::StringId(GetCurrentMapString(data))].push_back(objectId);
    }
}

///------------------------------------------------------------------------------------------------

void NetworkObjectUpdater::UpdateNetworkObject(ObjectData& objectData, const float dtMillis)
{
    switch (objectData.objectType)
    {
        case ObjectType::ATTACK:
        {
            UpdateAttack(objectData, dtMillis);
        } break;
            
        case ObjectType::NPC:
        {
            UpdateNPC(objectData, dtMillis);
        } break;
            
        default:
            break;
    }
}

///------------------------------------------------------------------------------------------------

void NetworkObjectUpdater::UpdateAttack(ObjectData& objectData, const float dtMillis)
{
    objectData.position += objectData.velocity * dtMillis;
    
    auto currentMap = strutils::StringId(GetCurrentMapString(objectData));
    auto mapPosition = mMapDataRepository.GetMapMetaData().at(currentMap).mMapPosition;
    
    auto navmap = mMapDataRepository.GetNavmaps().at(currentMap);
    if (objectData.attackType == AttackType::PROJECTILE && navmap.GetNavmapTileAt(navmap.GetNavmapCoord(objectData.position, mapPosition, MAP_GAME_SCALE)) == NavmapTileType::SOLID)
    {
        events::EventSystem::GetInstance().DispatchEvent<events::NetworkObjectCollisionEvent>(objectData.objectId);
    }
    
    CheckForMapChange(objectData, mMapDataRepository.GetMapMetaData().at(currentMap));
}

///------------------------------------------------------------------------------------------------

void NetworkObjectUpdater::SetSwarmParams(const float separationDistance, const float separationWeight)
{
    mSeparationDistance = separationDistance;
    mSeparationWeight = separationWeight;
}

///------------------------------------------------------------------------------------------------

void NetworkObjectUpdater::UpdateNPC(ObjectData& objectData, const float dtMillis)
{
    auto currentMap = strutils::StringId(GetCurrentMapString(objectData));
    auto mapPosition = mMapDataRepository.GetMapMetaData().at(currentMap).mMapPosition;
    auto& navmap = mMapDataRepository.GetNavmaps().at(currentMap);
    
    if (objectData.objectState != ObjectState::IDLE || !mPathController.DoesObjectHavePath(objectData.objectId))
    {
        objectData.velocity = glm::vec3(0.0f);
    }
    
    switch (objectData.objectState)
    {
            // Not chasing any player
        case ObjectState::IDLE:
        {
            if (mPathController.DoesObjectHavePath(objectData.objectId))
            {
                UpdateNPCPath(objectData, dtMillis, mapPosition, navmap, currentMap);
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
                    objectData.facingDirection = VecToFacingDirection(direction);
                    
                    // Fire Aggro event
                    events::EventSystem::GetInstance().DispatchEvent<events::NPCAggroEvent>(objectData.objectId, otherObjectData.objectId);
                    
                    // Find Path to target
                    FindPathToTarget(objectData, newTargetId, dtMillis, mapPosition, navmap);
                }
                else if (objectData.actionTimer < 0)
                {
                    // This is a loitering around timer;
                    objectData.actionTimer = NPC_LOITERING_TIMER_SECS;
                    auto mapCoord = navmap.GetNavmapCoord(objectData.position, mapPosition, MAP_GAME_SCALE);
                    auto nextDirection = static_cast<FacingDirection>(math::RandomInt(0, 7));
                    glm::vec3 toNextPosition;
                    
                    switch (nextDirection)
                    {
                        case FacingDirection::SOUTH: toNextPosition = glm::vec3(0.0f, -1.0f, 0.0f); mapCoord.y++; break;
                        case FacingDirection::NORTH: toNextPosition = glm::vec3(0.0f, 1.0f, 0.0f); mapCoord.y--; break;
                        case FacingDirection::WEST:  toNextPosition = glm::vec3(-1.0f, 0.0f, 0.0f); mapCoord.x--; break;
                        case FacingDirection::EAST:  toNextPosition = glm::vec3(1.0f, 0.0f, 0.0f); mapCoord.x++; break;
                        case FacingDirection::NORTH_WEST: toNextPosition = glm::vec3(-1.0f, 1.0f, 0.0f); mapCoord.x--; mapCoord.y--; break;
                        case FacingDirection::NORTH_EAST: toNextPosition = glm::vec3(1.0f, 1.0f, 0.0f); mapCoord.x++; mapCoord.y--; break;
                        case FacingDirection::SOUTH_WEST: toNextPosition = glm::vec3(-1.0f, -1.0f, 0.0f); mapCoord.x--; mapCoord.y++; break;
                        case FacingDirection::SOUTH_EAST: toNextPosition = glm::vec3(1.0f, -1.0f, 0.0f); mapCoord.x++; mapCoord.y++; break;
                    }
                    
                    if (navmap.GetNavmapTileAt(mapCoord) == NavmapTileType::WALKABLE)
                    {
                        auto targetPosition = navmap.GetMapPositionFromNavmapCoord(mapCoord, mapPosition, MAP_GAME_SCALE, objectData.position.z);
                        mPathController.SetObjectTargetPosition(objectData.objectId, targetPosition);
                        
                        objectData.facingDirection = nextDirection;
                    }
                }
            }
        } break;
            
            // Chasing player
        case ObjectState::RUNNING:
        {
            objectData.objectState = ObjectState::IDLE;
        } break;
            
        case ObjectState::MELEE_ATTACK:
        {
            if (objectData.actionTimer < 0)
            {
                if (!mNPCToTargetEntries.count(objectData.objectId))
                {
                    objectData.objectState = ObjectState::IDLE;
                }
                else if (!CollidersIntersect(mTickObjectData->at(mNPCToTargetEntries.at(objectData.objectId).mTargetObjectId), objectData))
                {
                    objectData.objectState = ObjectState::IDLE;
                    FindPathToTarget(objectData, mNPCToTargetEntries.at(objectData.objectId).mTargetObjectId, dtMillis, mapPosition, navmap);
                }
                else
                {
                    events::EventSystem::GetInstance().DispatchEvent<events::NPCAttackEvent>(objectData.objectId, mNPCToTargetEntries.at(objectData.objectId).mTargetObjectId, AttackType::MELEE, ProjectileType::NONE);
                    objectData.actionTimer += NPC_ATTACK_ANIMATION_TIMER_SECS;
                }
            }
        } break;
            
        default: break;
    }
    
    objectData.actionTimer -= dtMillis / 1000.0f;
}

///------------------------------------------------------------------------------------------------

void NetworkObjectUpdater::FindPathToTarget(const ObjectData& objectData, const objectId_t targetId, const float dtMillis, const glm::vec2& mapPosition, const Navmap& navmap)
{
    
    if (mPathController.IsTargetInLOS(objectData, mTickObjectData->at(targetId), navmap, mapPosition, dtMillis))
    {
        const auto& targetPosition = mTickObjectData->at(targetId).position;
        mPathController.SetObjectTargetPosition(objectData.objectId, glm::vec3(targetPosition.x, targetPosition.y, objectData.position.z));
    }
    else
    {
        mPathController.FindPath(objectData, mTickObjectData->at(targetId), mapPosition, navmap);
    }
}

///------------------------------------------------------------------------------------------------

void NetworkObjectUpdater::UpdateNPCPath(ObjectData& objectData, const float dtMillis, const glm::vec2& mapPosition, const Navmap& navmap, const strutils::StringId& currentMap)
{
    auto npcToTargetIter = mNPCToTargetEntries.find(objectData.objectId);
    auto& path = mPathController.GetPath(objectData.objectId);
    auto vecToTarget = path.front() - objectData.position;
    float distance = glm::length(vecToTarget);
    float step = objectData.speed * dtMillis;
    
    if (distance > step)
    {
        const auto& collisionCandidates = mMapDataRepository.GetMapQuadtree(currentMap).GetCollisionCandidates(objectData);
        glm::vec3 separation(0.0f);
        
        for (auto collisionCandidateId: collisionCandidates)
        {
            // Don't add collision force for other non npc objects
            if (mTickObjectData->at(collisionCandidateId).objectType != ObjectType::NPC)
            {
                continue;
            }
            // Don't add collision force for players that aren't this npc's primary target
            else if (mTickObjectData->at(collisionCandidateId).objectType == ObjectType::PLAYER)
            {
                if (npcToTargetIter == mNPCToTargetEntries.end() || npcToTargetIter->second.mTargetObjectId != collisionCandidateId)
                {
                    continue;
                }
            }
            
            auto diff = objectData.position - mTickObjectData->at(collisionCandidateId).position;
            diff.z = 0.0f;
            
            auto distance = glm::length(diff);
            if (distance < mSeparationDistance)
            {
                auto normal = glm::normalize(diff);
                auto tangent = glm::vec3(-normal.y, normal.x, 0.0f);
                separation += normal * 0.5f + tangent * 0.5f;
            }
        }
        
        if (glm::dot(glm::normalize(objectData.velocity), glm::normalize(glm::normalize(vecToTarget + separation * mSeparationWeight) * step)) < 0.0f && glm::length(separation) > 0.0f)
        {
            separation = glm::vec3(0.0f);
        }
        
        objectData.velocity = glm::normalize(vecToTarget + separation * mSeparationWeight) * step;
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
            objectData.objectState = ObjectState::IDLE;
        }
    }
    objectData.facingDirection = VecToFacingDirection(vecToTarget);
    
    // If it is pursuing, recalculate path
    if (npcToTargetIter != mNPCToTargetEntries.end())
    {
        // If you end up touching your target during a path traversal, kill the path
        if (CollidersIntersect(mTickObjectData->at(mNPCToTargetEntries.at(objectData.objectId).mTargetObjectId), objectData))
        {
            mPathController.ClearObjectPath(objectData.objectId);
        }
        
        // Can attack
        if (objectData.actionTimer < 0.0f && mTickObjectData->count(npcToTargetIter->second.mTargetObjectId) && CollidersIntersect(mTickObjectData->at(npcToTargetIter->second.mTargetObjectId), objectData))
        {
            // This is now an attack animation timer
            events::EventSystem::GetInstance().DispatchEvent<events::NPCAttackEvent>(objectData.objectId, npcToTargetIter->second.mTargetObjectId, AttackType::MELEE, ProjectileType::NONE);
            objectData.actionTimer = NPC_ATTACK_ANIMATION_TIMER_SECS;
            objectData.objectState = ObjectState::MELEE_ATTACK;
            mPathController.ClearObjectPath(objectData.objectId);
        }
        else
        {
            npcToTargetIter->second.mPathRecalculationTimer -= dtMillis/1000.0f;
            if (npcToTargetIter->second.mPathRecalculationTimer < 0.0f)
            {
                npcToTargetIter->second.mPathRecalculationTimer += NPC_PATH_RECALCULATION_SECS;
                FindPathToTarget(objectData, npcToTargetIter->second.mTargetObjectId, dtMillis, mapPosition, navmap);
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

objectId_t NetworkObjectUpdater::FindValidTarget(ObjectData& objectData, const float dtMillis, const strutils::StringId& currentMap, const glm::vec2& mapPosition, const Navmap& navmap)
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
        if (objectData.objectFaction == ObjectFaction::NEUTRAL)
        {
            continue;
        }
        
        const auto& otherObjectData = mTickObjectData->at(id);
        
        // Object type check
        if (otherObjectData.objectType != ObjectType::PLAYER && otherObjectData.objectType != ObjectType::NPC)
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

bool NetworkObjectUpdater::CheckForMapChange(ObjectData& objectData, const MapMetaData& currentMapMetaData)
{
    strutils::StringId nextMapName;
    if (objectData.position.x > currentMapMetaData.mMapPosition.x * MAP_GAME_SCALE + (currentMapMetaData.mMapDimensions.x * MAP_GAME_SCALE)/2.0f)
    {
        nextMapName = currentMapMetaData.mMapConnections[static_cast<int>(MapConnectionDirection::EAST)];
    }
    else if (objectData.position.x < currentMapMetaData.mMapPosition.x * MAP_GAME_SCALE - (currentMapMetaData.mMapDimensions.x * MAP_GAME_SCALE)/2.0f)
    {
        nextMapName = currentMapMetaData.mMapConnections[static_cast<int>(MapConnectionDirection::WEST)];
    }
    else if (objectData.position.y > currentMapMetaData.mMapPosition.y * MAP_GAME_SCALE + (currentMapMetaData.mMapDimensions.y * MAP_GAME_SCALE)/2.0f)
    {
        nextMapName = currentMapMetaData.mMapConnections[static_cast<int>(MapConnectionDirection::NORTH)];
    }
    else if (objectData.position.y < currentMapMetaData.mMapPosition.y * MAP_GAME_SCALE - (currentMapMetaData.mMapDimensions.y * MAP_GAME_SCALE)/2.0f)
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

}

///------------------------------------------------------------------------------------------------
