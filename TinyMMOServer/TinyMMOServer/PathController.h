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
#include "util/ThreadSafeQueue.h"

#include <atomic>
#include <unordered_map>
#include <queue>
#include <thread>

///------------------------------------------------------------------------------------------------

namespace network { class Navmap; }
class PathController final
{
public:
    PathController() = default;
    
    bool DoesObjectHavePath(const network::objectId_t objectId) const;
    std::queue<glm::vec3>& GetPath(const network::objectId_t objectId);
    const std::queue<glm::vec3>& GetPath(const network::objectId_t objectId) const;
    
    void Update();
    void FindPath(const network::ObjectData& sourceObjectData, const network::ObjectData& targetObjectData, const glm::vec2& mapPosition, const network::Navmap& navmap);
    void ClearObjectPath(const network::objectId_t objectId);
    void AddTargetPositionToPath(const network::objectId_t objectId, const glm::vec3& target);
    void SetObjectTargetPosition(const network::objectId_t objectId, const glm::vec3& target);
    bool IsTargetInLOS(const network::ObjectData& sourceObjectData, const network::ObjectData& targetObjectData, const network::Navmap& navmap, const glm::vec2& mapPosition, const float dtMillis);
    
private:
    class PathFindingTask
    {
    public:
        PathFindingTask(const network::objectId_t objectId, const glm::vec3& startPosition, const glm::vec3& targetPosition, const glm::vec2& mapPosition, const network::Navmap& navmap)
            : mObjectId(objectId)
            , mStartPosition(startPosition)
            , mTargetPosition(targetPosition)
            , mMapPosition(mapPosition)
            , mNavmap(navmap)
        {
        }
        
        const network::objectId_t mObjectId;
        const glm::vec3 mStartPosition;
        const glm::vec3 mTargetPosition;
        const glm::vec2 mMapPosition;
        const network::Navmap& mNavmap;
    };
    
    class PathFindingResult
    {
    public:
        PathFindingResult(const network::objectId_t objectId, const std::queue<glm::vec3>& path)
            : mObjectId(objectId)
            , mPath(path)
        {
        }
        
        const network::objectId_t mObjectId;
        const std::queue<glm::vec3> mPath;
    };
    
    class PathFindingWorker
    {
    public:
        PathFindingWorker(ThreadSafeQueue<PathFindingTask>& pathFindingTasks, ThreadSafeQueue<PathFindingResult>& pathFindingResults);
        PathFindingResult FindPath(const PathFindingTask& pathFindingTask);
        void StartWorker();
        void StopWorker();
        
    private:
        ThreadSafeQueue<PathFindingTask>& mPathFindingTasks;
        ThreadSafeQueue<PathFindingResult>& mPathFindingResults;
        std::thread mThread;
        std::atomic<bool> mStop = false;
    };

private:
    ThreadSafeQueue<PathFindingTask> mPathFindingTasks;
    ThreadSafeQueue<PathFindingResult> mPathFindingResults;
    std::unique_ptr<PathFindingWorker> mPathFindingWorker;
    std::unordered_map<network::objectId_t, std::queue<glm::vec3>> mPaths;
};

///------------------------------------------------------------------------------------------------

#endif /* PathController_h */
