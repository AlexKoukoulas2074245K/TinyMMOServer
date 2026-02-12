///------------------------------------------------------------------------------------------------
///  MapDataRepository.cpp
///  TinyMMOServer
///
///  Created by Alex Koukoulas on 05/02/2026
///------------------------------------------------------------------------------------------------

#include "MapDataRepository.h"
#include "util/Json.h"
#include "util/FileUtils.h"
#include "util/Logging.h"
#include "util/Lodepng.h"

#include <fstream>

///------------------------------------------------------------------------------------------------

static const int NAVMAP_SIZE = 128;

///------------------------------------------------------------------------------------------------

void MapDataRepository::LoadMapData(const std::string& assetsDirectory)
{
    LoadMapMetaData(assetsDirectory);
    LoadNavmapData(assetsDirectory);
    CreateQuadtrees();
}

///------------------------------------------------------------------------------------------------

const std::unordered_map<strutils::StringId, MapMetaData, strutils::StringIdHasher>& MapDataRepository::GetMapMetaData() const
{
    return mMapMetaData;
}

///------------------------------------------------------------------------------------------------

const std::unordered_map<strutils::StringId, network::Navmap, strutils::StringIdHasher>& MapDataRepository::GetNavmaps() const
{
    return mNavmaps;
}

///------------------------------------------------------------------------------------------------

const std::unordered_map<strutils::StringId, std::unique_ptr<network::NetworkQuadtree>, strutils::StringIdHasher>& MapDataRepository::GetMapQuadtrees() const
{
    return mMapQuadtrees;
}

///------------------------------------------------------------------------------------------------

const network::NetworkQuadtree& MapDataRepository::GetMapQuadtree(const strutils::StringId& mapName) const
{
    assert(mMapQuadtrees.count(mapName));
    return *mMapQuadtrees.at(mapName);
}

///------------------------------------------------------------------------------------------------

network::NetworkQuadtree& MapDataRepository::GetMapQuadtree(const strutils::StringId& mapName)
{
    assert(mMapQuadtrees.count(mapName));
    return *mMapQuadtrees.at(mapName);
}

///------------------------------------------------------------------------------------------------

void MapDataRepository::LoadMapMetaData(const std::string& assetsDirectory)
{
    std::ifstream dataFile(assetsDirectory + "map_global_data.json");
    if (dataFile.is_open())
    {
        std::stringstream buffer;
        buffer << dataFile.rdbuf();
        
        auto globalMapDataJson = nlohmann::json::parse(buffer.str());
        
        for (auto mapTransformIter = globalMapDataJson["map_transforms"].begin(); mapTransformIter != globalMapDataJson["map_transforms"].end(); ++mapTransformIter)
        {
            auto mapFileName = mapTransformIter.key();
            auto mapName = mapTransformIter.key().substr(0, mapFileName.find(".json"));
            auto mapNameId = strutils::StringId(mapName);
            auto mapPosition = glm::vec2(mapTransformIter.value()["x"].get<float>(), mapTransformIter.value()["y"].get<float>());
            auto mapDimensions = glm::vec2(mapTransformIter.value()["width"].get<float>(), mapTransformIter.value()["height"].get<float>());
            
            MapConnectionsType mapConnections;
            auto northConnectionMapName = globalMapDataJson["map_connections"][mapFileName]["top"].get<std::string>();
            auto eastConnectionMapName = globalMapDataJson["map_connections"][mapFileName]["right"].get<std::string>();
            auto southConnectionMapName = globalMapDataJson["map_connections"][mapFileName]["bottom"].get<std::string>();
            auto westConnectionMapName = globalMapDataJson["map_connections"][mapFileName]["left"].get<std::string>();
            
            mapConnections[static_cast<int>(MapConnectionDirection::NORTH)] = strutils::StringId(northConnectionMapName.substr(0, northConnectionMapName.find(".json")));
            mapConnections[static_cast<int>(MapConnectionDirection::EAST)]  = strutils::StringId(eastConnectionMapName.substr(0, eastConnectionMapName.find(".json")));
            mapConnections[static_cast<int>(MapConnectionDirection::SOUTH)] = strutils::StringId(southConnectionMapName.substr(0, southConnectionMapName.find(".json")));
            mapConnections[static_cast<int>(MapConnectionDirection::WEST)]  = strutils::StringId(westConnectionMapName.substr(0, westConnectionMapName.find(".json")));
            
            mMapMetaData.emplace(std::make_pair(mapNameId, MapMetaData{ mapDimensions, mapPosition, std::move(mapConnections)}));
        }
    }
    
    logging::Log(logging::LogType::INFO, "Loaded MapMetaData for %lu maps.", mMapMetaData.size());
}

///------------------------------------------------------------------------------------------------

void MapDataRepository::LoadNavmapData(const std::string& assetsDirectory)
{
    auto navmapFilePaths = fileutils::GetAllFilenamesAndFolderNamesInDirectory(assetsDirectory + "navmaps/");
    
    for (const auto& navmapFileName: navmapFilePaths)
    {
        std::vector<unsigned char> rawPNG;
        std::vector<unsigned char> navmapPixels;
        
        unsigned width, height;
        lodepng::State state;

        auto error = lodepng::load_file(rawPNG, assetsDirectory + "/navmaps/" + navmapFileName);
        
        if (!error)
        {
            error = lodepng::decode(navmapPixels, width, height, state, rawPNG);
        }
        
        if(error)
        {
            logging::Log(logging::LogType::ERROR, "PNG Loading Error %d: %s", error, lodepng_error_text(error));
        }
        else
        {
            auto mapName = strutils::StringId(navmapFileName.substr(0, navmapFileName.find("_navmap.png")));
            
            mNavmapPixels.emplace(std::make_pair(mapName, navmapPixels));
            mNavmaps.emplace(std::make_pair(mapName, network::Navmap(mNavmapPixels.at(mapName).data(), NAVMAP_SIZE)));
        }
    }
    
    logging::Log(logging::LogType::INFO, "Loaded Navmap data for %lu maps.", mNavmaps.size());
}

///------------------------------------------------------------------------------------------------

void MapDataRepository::CreateQuadtrees()
{
    for (const auto& [mapName, mapMetaData]: mMapMetaData)
    {
        mMapQuadtrees[mapName] = std::make_unique<network::NetworkQuadtree>(
            glm::vec3(mapMetaData.mMapPosition.x * network::MAP_GAME_SCALE, mapMetaData.mMapPosition.y * network::MAP_GAME_SCALE, 20.0f),
            glm::vec3(mapMetaData.mMapDimensions.x * network::MAP_GAME_SCALE, mapMetaData.mMapDimensions.y * network::MAP_GAME_SCALE, 1.0f));
    }
}

///------------------------------------------------------------------------------------------------
