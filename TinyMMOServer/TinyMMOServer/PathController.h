///------------------------------------------------------------------------------------------------
///  PathController.h
///  TinyMMOServer
///
///  Created by Alex Koukoulas on 17/02/2026
///------------------------------------------------------------------------------------------------

#ifndef PathController_h
#define PathController_h

///------------------------------------------------------------------------------------------------

#include "net_common/NetworkCommon.h"
#include "util/MathUtils.h"

#include <unordered_map>
#include <queue>

///------------------------------------------------------------------------------------------------

class PathController final
{
public:
    PathController() = default;
    
    bool DoesObjectHavePath(const network::objectId_t objectId) const;
    std::queue<glm::vec3>& GetPath(const network::objectId_t objectId);

    void ClearObjectPath(const network::objectId_t objectId);
    void AddTargetPositionToPath(const network::objectId_t objectId, const glm::vec3& target);
    void SetObjectTargetPosition(const network::objectId_t objectId, const glm::vec3& target);
    
private:
    std::unordered_map<network::objectId_t, std::queue<glm::vec3>> mPaths;
};

///------------------------------------------------------------------------------------------------

#endif /* PathController_h */
