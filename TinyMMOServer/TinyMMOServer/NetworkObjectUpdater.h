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
#include "net_common/NetworkCommon.h"

#include <functional>
#include <unordered_map>
#include <queue>

///------------------------------------------------------------------------------------------------

class MapDataRepository;
struct MapMetaData;
class NetworkObjectUpdater final
{
public:
    NetworkObjectUpdater(MapDataRepository& mapDataRepository);
    
    bool DoesObjectHavePath(const network::objectId_t objectId) const;
    std::queue<glm::vec3>& GetPath(const network::objectId_t objectId);
    const std::queue<glm::vec3>& GetPath(const network::objectId_t objectId) const;

    void UpdateNetworkObject(network::ObjectData& objectData, const float dtMillis);
private:
    void UpdateAttack(network::ObjectData& objectData, const float dtMillis);
    void UpdateNPC(network::ObjectData& objectData, const float dtMillis);
    bool CheckForMapChange(network::ObjectData& objectData, const MapMetaData& currentMapMetaData);

private:
    MapDataRepository& mMapDataRepository;
    PathController mPathController;

};

///------------------------------------------------------------------------------------------------

#endif /* NetworkObjectUpdater_h */
