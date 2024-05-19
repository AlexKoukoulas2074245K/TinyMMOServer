///------------------------------------------------------------------------------------------------
///  Pathfinding.cpp
///  TinyMMOClient
///
///  Created by Alex Koukoulas on 19/05/2024.
///-----------------------------------------------------------------------------------------------

#include "Pathfinding.h"
#include "Logging.h"

///-----------------------------------------------------------------------------------------------

namespace pathfinding
{

///-----------------------------------------------------------------------------------------------

bool DoesObjectHaveLOSToTarget(const glm::vec3& sourceObjectPosition, const glm::vec3& targetObjectPosition, const glm::vec2& mapPosition, const float mapScale, const float sourceObjectSpeed, const float dtMillis, const networking::Navmap& navmap)
{
    const auto& directionToTarget = targetObjectPosition - sourceObjectPosition;
    auto distanceToTarget = glm::length(directionToTarget);
    
    // Already at target
    if (distanceToTarget <= 0.0f || distanceToTarget < glm::length(glm::normalize(directionToTarget) * sourceObjectSpeed * dtMillis))
    {
        return true;
    }
    
    // We ray cast and move half the object's speed till we reach the target
    auto normalizedDirectionToTarget = glm::normalize(directionToTarget);
    auto tIncrements = (sourceObjectSpeed * dtMillis)/2.0f;
    auto numTIncrements = static_cast<int>(0.9f * distanceToTarget/tIncrements);
    
    for (auto i = 0; i < numTIncrements; ++i)
    {
        auto testPosition = sourceObjectPosition + normalizedDirectionToTarget * static_cast<float>(i) * tIncrements;
        auto tileAtTestPosition = navmap.GetNavmapTileAt(navmap.GetNavmapCoord(testPosition, mapPosition, mapScale));
        if (tileAtTestPosition != networking::NavmapTileType::EMPTY)
        {
            return false;
        }
    }
    
    return true;
}

///-----------------------------------------------------------------------------------------------

}
