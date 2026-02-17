///------------------------------------------------------------------------------------------------
///  PathController.cpp
///  TinyMMOServer
///
///  Created by Alex Koukoulas on 17/02/2026
///------------------------------------------------------------------------------------------------

#include "PathController.h"

///------------------------------------------------------------------------------------------------

bool PathController::DoesObjectHavePath(const network::objectId_t objectId) const
{
    return mPaths.count(objectId) != 0;
}

///------------------------------------------------------------------------------------------------

std::queue<glm::vec3>& PathController::GetPath(const network::objectId_t objectId)
{
    return mPaths.at(objectId);
}

///------------------------------------------------------------------------------------------------

void PathController::ClearObjectPath(const network::objectId_t objectId)
{
    mPaths.erase(objectId);
}

///------------------------------------------------------------------------------------------------

void PathController::AddTargetPositionToPath(const network::objectId_t objectId, const glm::vec3& target)
{
    mPaths[objectId].push(target);
}

///------------------------------------------------------------------------------------------------

void PathController::SetObjectTargetPosition(const network::objectId_t objectId, const glm::vec3& target)
{
    ClearObjectPath(objectId);
    mPaths[objectId].push(target);
}

///------------------------------------------------------------------------------------------------
