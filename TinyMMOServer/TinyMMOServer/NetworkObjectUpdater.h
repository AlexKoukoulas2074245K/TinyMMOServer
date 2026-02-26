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
class NetworkObjectUpdater final: public events::IListener
{
public:
    NetworkObjectUpdater(MapDataRepository& mapDataRepository);
    
    bool DoesObjectHavePath(const network::objectId_t objectId) const;
    std::queue<glm::vec3>& GetPath(const network::objectId_t objectId);
    const std::queue<glm::vec3>& GetPath(const network::objectId_t objectId) const;
    
    void OnObjectDestroyedEvent(const events::ObjectDestroyedEvent& objectDestroyedEvent);
    void PerformPreUpdateSetup(const std::unordered_map<network::objectId_t, network::ObjectData>& objectData);
    void UpdateNetworkObject(network::ObjectData& objectData, const float dtMillis);

private:
    void UpdateAttack(network::ObjectData& objectData, const float dtMillis);
    void UpdateNPC(network::ObjectData& objectData, const float dtMillis);
    void UpdateNPCPath(network::ObjectData& objectData, const float dtMillis, const glm::vec2& mapPosition, const network::Navmap& navmap);
    network::objectId_t FindValidTarget(network::ObjectData& objectData, const float dtMillis, const strutils::StringId& currentMap, const glm::vec2& mapPosition, const network::Navmap& navmap);
    bool CheckForMapChange(network::ObjectData& objectData, const MapMetaData& currentMapMetaData);

private:
    struct NpcTargetEntry
    {
        network::objectId_t mTargetObjectId;
        float mPathRecalculationTimer;
    };

private:
    MapDataRepository& mMapDataRepository;
    PathController mPathController;
    
    std::unordered_map<strutils::StringId, std::vector<network::objectId_t>, strutils::StringIdHasher> mObjectIdsPerMap;
    std::unordered_map<network::objectId_t, NpcTargetEntry> mNPCToTargetEntries;
    const std::unordered_map<network::objectId_t, network::ObjectData>*  mTickObjectData;
};

///------------------------------------------------------------------------------------------------

#endif /* NetworkObjectUpdater_h */
