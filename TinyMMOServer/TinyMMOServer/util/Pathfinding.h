///------------------------------------------------------------------------------------------------
///  Pathfinding.h
///  TinyMMOClient
///
///  Created by Alex Koukoulas on 19/05/2024.
///-----------------------------------------------------------------------------------------------

#ifndef Pathfinding_h
#define Pathfinding_h

///-----------------------------------------------------------------------------------------------

#include "MathUtils.h"
#include "net_common/Navmap.h"

#include <list>

///-----------------------------------------------------------------------------------------------

namespace pathfinding
{

///-----------------------------------------------------------------------------------------------

bool DoesObjectHaveLOSToTarget(const glm::vec3& sourceObjectPosition, const glm::vec3& targetObjectPosition, const glm::vec2& mapPosition, const float mapScale, const float sourceObjectSpeed, const float dtMillis, const networking::Navmap& navmap);

///-----------------------------------------------------------------------------------------------

std::list<glm::vec3> CalculateAStarPathToTarget(const glm::vec3& sourceObjectPosition, const glm::vec3& targetObjectPosition, const glm::vec2& mapPosition, const float mapScale, const networking::Navmap& navmap);

///-----------------------------------------------------------------------------------------------

}

///-----------------------------------------------------------------------------------------------

#endif /* Pathfinding_h */
