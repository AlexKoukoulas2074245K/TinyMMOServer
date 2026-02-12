///------------------------------------------------------------------------------------------------
///  MapDataRepository.h
///  TinyMMOServer
///
///  Created by Alex Koukoulas on 05/02/2026
///------------------------------------------------------------------------------------------------

#ifndef MapDataRepository_h
#define MapDataRepository_h

///------------------------------------------------------------------------------------------------

#include "net_common/Navmap.h"
#include "net_common/NetworkQuadtree.h"
#include "util/MathUtils.h"
#include "util/StringUtils.h"

#include <array>
#include <unordered_map>

///------------------------------------------------------------------------------------------------

enum class MapConnectionDirection
{
    NORTH = 0,
    EAST = 1,
    SOUTH = 2,
    WEST = 3,
    MAX = 4
};

using MapConnectionsType = std::array<strutils::StringId, static_cast<size_t>(MapConnectionDirection::MAX)>;

///------------------------------------------------------------------------------------------------

struct MapMetaData
{
    glm::vec2 mMapDimensions;
    glm::vec2 mMapPosition;
    MapConnectionsType mMapConnections;
};

///------------------------------------------------------------------------------------------------

class MapDataRepository final
{
public:
    MapDataRepository() = default;
    void LoadMapData(const std::string& assetsDirectory);
    
    const std::unordered_map<strutils::StringId, MapMetaData, strutils::StringIdHasher>& GetMapMetaData() const;
    const std::unordered_map<strutils::StringId, network::Navmap, strutils::StringIdHasher>& GetNavmaps() const;
    const std::unordered_map<strutils::StringId, std::unique_ptr<network::NetworkQuadtree>, strutils::StringIdHasher>& GetMapQuadtrees() const;

    const network::NetworkQuadtree& GetMapQuadtree(const strutils::StringId& mapName) const;
    network::NetworkQuadtree& GetMapQuadtree(const strutils::StringId& mapName);
    
private:
    void LoadNavmapData(const std::string& assetsDirectory);
    void LoadMapMetaData(const std::string& assetsDirectory);
    void CreateQuadtrees();
    
public:
    std::unordered_map<strutils::StringId, MapMetaData, strutils::StringIdHasher> mMapMetaData;
    std::unordered_map<strutils::StringId, network::Navmap, strutils::StringIdHasher> mNavmaps;
    std::unordered_map<strutils::StringId, std::unique_ptr<network::NetworkQuadtree>, strutils::StringIdHasher> mMapQuadtrees;
    std::unordered_map<strutils::StringId, std::vector<unsigned char>, strutils::StringIdHasher> mNavmapPixels;
};

///------------------------------------------------------------------------------------------------

#endif /* MapDataRepository_h */
