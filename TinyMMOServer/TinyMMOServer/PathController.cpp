///------------------------------------------------------------------------------------------------
///  PathController.cpp
///  TinyMMOServer
///
///  Created by Alex Koukoulas on 17/02/2026
///------------------------------------------------------------------------------------------------

#include "PathController.h"
#include "net_common/Navmap.h"
#include "util/Logging.h"
#include <unordered_set>

///------------------------------------------------------------------------------------------------

static const bool ARTIFICIAL_ASYNC_LOADING_DELAY = false;
static constexpr int PATH_FINDING_WORKER_COUNT = 2;

///------------------------------------------------------------------------------------------------

struct Node
{
    int row, col;
    float gCost, hCost;
    Node* parent;

    Node(int r, int c, float g = std::numeric_limits<float>::max(), float h = 0, Node* p = nullptr)
        : row(r), col(c), gCost(g), hCost(h), parent(p) {}

    float fCost() const
    {
        return gCost + hCost;
    }

    bool operator>(const Node& other) const
    {
        return fCost() > other.fCost();
    }
        
    bool operator<(const Node& other) const
    {
        return fCost() < other.fCost();
    }

    bool operator==(const Node& other) const
    {
        return row == other.row && col == other.col;
    }
};

struct NodeHasher
{
    std::size_t operator()(const Node& key) const
    {
        return std::hash<int>()(key.row) ^ std::hash<int>()(key.col);
    }
};

// Custom comparator for priority queue
struct CompareNode {
    bool operator()(Node* lhs, Node* rhs) const
    {
        return lhs->fCost() > rhs->fCost();
    }
};

///------------------------------------------------------------------------------------------------

PathController::PathFindingWorker::PathFindingWorker(ThreadSafeQueue<PathFindingTask>& pathFindingTasks, ThreadSafeQueue<PathFindingResult>& pathFindingResults)
    : mPathFindingTasks(pathFindingTasks)
    , mPathFindingResults(pathFindingResults)
{
}

///------------------------------------------------------------------------------------------------

void PathController::PathFindingWorker::StopWorker()
{
    mStop = true;
}


///------------------------------------------------------------------------------------------------
PathController::PathFindingResult PathController::PathFindingWorker::FindPath(const PathFindingTask &pathFindingTask)
{
    std::priority_queue<Node*, std::vector<Node*>, CompareNode> openSet;
    std::unordered_set<Node, NodeHasher> closedSet;
    std::unordered_map<int, std::unordered_map<int, std::unique_ptr<Node>>> nodes;
    std::vector<glm::vec3> resultPathVec;
    std::queue<glm::vec3> resultPath;
    
    const auto& startCoord = pathFindingTask.mNavmap.GetNavmapCoord(pathFindingTask.mStartPosition, pathFindingTask.mMapPosition, network::MAP_GAME_SCALE);
    const auto& endCoord = pathFindingTask.mNavmap.GetNavmapCoord(pathFindingTask.mTargetPosition, pathFindingTask.mMapPosition, network::MAP_GAME_SCALE);
    
    if (startCoord == endCoord)
    {
        return PathController::PathFindingResult(pathFindingTask.mObjectId, resultPath);
    }
    
    auto beginTp = std::chrono::high_resolution_clock::now();
    
    openSet = {};
    nodes.clear();
    closedSet = {};
    
    auto HeuristicLambda = [](int x1, int y1, int x2, int y2)
    {
        return std::abs(x1 - x2) + std::abs(y1 - y2);
    };
    
    auto AddOrUpdateNode = [&](int row, int col, float gCost, Node* parent)
    {
        if (nodes[row][col] == nullptr || gCost < nodes[row][col]->gCost)
        {
            if (nodes[row][col] == nullptr)
            {
                nodes[row][col] = std::make_unique<Node>(row, col);
            }
            
            nodes[row][col]->gCost = gCost;
            nodes[row][col]->hCost = HeuristicLambda(row, col, endCoord.y, endCoord.x);
            nodes[row][col]->parent = parent;
            openSet.push(nodes[row][col].get());
        }
    };
    
    AddOrUpdateNode(startCoord.y, startCoord.x, 0, nullptr);
    
    while (!openSet.empty())
    {
        Node* current = openSet.top();
        openSet.pop();
        
        if (closedSet.find(*current) != closedSet.end())
        {
            continue;
        }
        closedSet.insert(*current);
        
        if (current->row == endCoord.y && current->col == endCoord.x)
        {
            Node* node = current;
            while (node != nullptr)
            {
                if (node->col != startCoord.x || node->row != startCoord.y)
                {
                    resultPathVec.push_back(pathFindingTask.mNavmap.GetMapPositionFromNavmapCoord(glm::ivec2(node->col, node->row), pathFindingTask.mMapPosition, network::MAP_GAME_SCALE, pathFindingTask.mStartPosition.z));
                }
                
                node = node->parent;
            }
            break;
        }

//        static const int directions[8][2] = { {0, 1}, {1, 0}, {0, -1}, {-1, 0}, {1, 1}, {-1, 1}, {1, -1}, {-1, -1} };
        static const int directions[4][2] = { {0, 1}, {1, 0}, {0, -1}, {-1, 0} };
        for (const auto& dir : directions) {
            int newRow = current->row + dir[0];
            int newCol = current->col + dir[1];

            if (newRow >= 0 && newRow < pathFindingTask.mNavmap.GetSize() && newCol >= 0 && newCol < pathFindingTask.mNavmap.GetSize() && pathFindingTask.mNavmap.GetNavmapTileAt(glm::ivec2(newCol, newRow)) == network::NavmapTileType::WALKABLE)
            {
                float newGCost = current->gCost + 1;
                AddOrUpdateNode(newRow, newCol, newGCost, current);
            }
        }
    }
    
    auto endTp = std::chrono::high_resolution_clock::now();
    auto micros = std::chrono::duration_cast<std::chrono::microseconds>(endTp - beginTp).count();
    
    if (micros > 10000)
    {
        logging::Log(logging::LogType::INFO, "Excessive Pathfinding took %d millis", micros);
    }
    
    for (auto iter = resultPathVec.rbegin(); iter != resultPathVec.rend(); ++iter)
    {
        resultPath.push(*iter);
    }
    return PathController::PathFindingResult(pathFindingTask.mObjectId, resultPath);
}

///------------------------------------------------------------------------------------------------

void PathController::PathFindingWorker::StartWorker()
{
    mThread = std::thread([&]
    {
        while(!mStop)
        {
            const auto& task = mPathFindingTasks.dequeue();
            
            if (ARTIFICIAL_ASYNC_LOADING_DELAY)
            {
                using namespace std::chrono_literals;
                std::this_thread::sleep_for(100ms);
            }

            mPathFindingResults.enqueue(FindPath(task));
        }
    });
    mThread.detach();
}

///------------------------------------------------------------------------------------------------

PathController::PathController()
{
    for (int i = 0; i < PATH_FINDING_WORKER_COUNT; ++i)
    {
        mPathFindingWorkers.emplace_back(std::make_unique<PathFindingWorker>(mPathFindingTasks, mPathFindingResults));
        mPathFindingWorkers.back()->StartWorker();
    }
}

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

const std::queue<glm::vec3>& PathController::GetPath(const network::objectId_t objectId) const
{
    return mPaths.at(objectId);
}

///------------------------------------------------------------------------------------------------

void PathController::Update()
{
    while (mPathFindingResults.size())
    {
        auto&& pathFindingResult = mPathFindingResults.dequeue();
        
        if (pathFindingResult.mPath.size() > 0)
        {
            mPaths[pathFindingResult.mObjectId] = pathFindingResult.mPath;
        }
    }
}

///------------------------------------------------------------------------------------------------

void PathController::FindPath(const network::ObjectData& sourceObjectData, const network::ObjectData& targetObjectData, const glm::vec2& mapPosition, const network::Navmap& navmap)
{
    auto targetPosition = glm::vec3(targetObjectData.position.x, targetObjectData.position.y, sourceObjectData.position.z);
    mPathFindingTasks.enqueue(PathFindingTask(sourceObjectData.objectId, sourceObjectData.position, targetPosition, mapPosition, navmap));
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

bool PathController::IsTargetInLOS(const network::ObjectData& sourceObjectData, const network::ObjectData& targetObjectData, const network::Navmap& navmap, const glm::vec2& mapPosition, const float dtMillis)
{
    auto targetPosition = glm::vec3(targetObjectData.position.x, targetObjectData.position.y, sourceObjectData.position.z);
    
    auto normalizedDirectionToTarget = glm::normalize(targetPosition - sourceObjectData.position);
    auto tIncrements = (sourceObjectData.speed * dtMillis)/2.0f;
    auto numTIncrements = static_cast<int>(glm::distance(sourceObjectData.position, targetPosition)/tIncrements);
    
    for (auto i = 0; i < numTIncrements; ++i)
    {
        auto testPosition = sourceObjectData.position + normalizedDirectionToTarget * static_cast<float>(i) * tIncrements;
        auto tileAtTestPosition = navmap.GetNavmapTileAt(navmap.GetNavmapCoord(testPosition, mapPosition, network::MAP_GAME_SCALE));
        if (tileAtTestPosition != network::NavmapTileType::WALKABLE)
        {
            return false;
        }
    }
    
    return true;
}

///------------------------------------------------------------------------------------------------
