///------------------------------------------------------------------------------------------------
///  NetworkObjectUpdater.h
///  TinyMMOServer
///
///  Created by Alex Koukoulas on 17/02/2026
///------------------------------------------------------------------------------------------------

#ifndef NetworkObjectUpdater_h
#define NetworkObjectUpdater_h

///------------------------------------------------------------------------------------------------

#include "PathController.h"
#include "events/EventSystem.h"
#include "net_common/Navmap.h"
#include "net_common/NetworkCommon.h"
#include "util/StringUtils.h"

#include <functional>
#include <unordered_map>
#include <queue>

///------------------------------------------------------------------------------------------------

class MapDataRepository;
struct MapMetaData;

namespace network
{

///------------------------------------------------------------------------------------------------

class NetworkObjectUpdater final: public events::IListener
{
public:
    NetworkObjectUpdater(MapDataRepository& mapDataRepository);
    
    bool DoesObjectHavePath(const objectId_t objectId) const;
    std::queue<glm::vec3>& GetPath(const objectId_t objectId);
    const std::queue<glm::vec3>& GetPath(const objectId_t objectId) const;
    
    void OnObjectDestroyedEvent(const events::ObjectDestroyedEvent& objectDestroyedEvent);
    void PerformPreUpdateSetup(const std::unordered_map<objectId_t, ObjectData>& objectData);
    void UpdateNetworkObject(ObjectData& objectData, const float dtMillis);
    void SetSwarmParams(const float separationDistance, const float separationWeight);
    
private:
    void UpdateAttack(ObjectData& objectData, const float dtMillis);
    void UpdateNPC(ObjectData& objectData, const float dtMillis);
    void FindPathToTarget(const ObjectData& objectData, const objectId_t targetId, const float dtMillis, const glm::vec2& mapPosition, const Navmap& navmap);
    void UpdateNPCPath(ObjectData& objectData, const float dtMillis, const glm::vec2& mapPosition, const Navmap& navmap, const strutils::StringId& currentMap);
    objectId_t FindValidTarget(ObjectData& objectData, const float dtMillis, const strutils::StringId& currentMap, const glm::vec2& mapPosition, const Navmap& navmap);
    bool CheckForMapChange(ObjectData& objectData, const MapMetaData& currentMapMetaData);
    
private:
    struct NpcTargetEntry
    {
        objectId_t mTargetObjectId;
        float mPathRecalculationTimer;
    };
    
private:
    MapDataRepository& mMapDataRepository;
    PathController mPathController;
    
    std::unordered_map<strutils::StringId, std::vector<objectId_t>, strutils::StringIdHasher> mObjectIdsPerMap;
    std::unordered_map<objectId_t, NpcTargetEntry> mNPCToTargetEntries;
    const std::unordered_map<objectId_t, ObjectData>*  mTickObjectData;
    float mSeparationDistance;
    float mSeparationWeight;
};

///------------------------------------------------------------------------------------------------

}

///------------------------------------------------------------------------------------------------

#endif /* NetworkObjectUpdater_h */
