///------------------------------------------------------------------------------------------------
///  Pathfinding.cpp
///  TinyMMOClient
///
///  Created by Alex Koukoulas on 19/05/2024.
///-----------------------------------------------------------------------------------------------

#include "Pathfinding.h"
#include "Logging.h"

#include <algorithm>
#include <chrono>
#include <queue>
#include <unordered_set>
#include <utility>
#include <functional>

///-----------------------------------------------------------------------------------------------

namespace pathfinding
{

///-----------------------------------------------------------------------------------------------

static const float TILE_SIZE = 0.0625f;

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
    auto numTIncrements = static_cast<int>(distanceToTarget/tIncrements);
    
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


std::priority_queue<Node*, std::vector<Node*>, CompareNode> sOpenSet;
std::unordered_set<Node, NodeHasher> closedSet;
std::unordered_map<int, std::unordered_map<int, std::unique_ptr<Node>>> sNodes;

std::list<glm::vec3> CalculateAStarPathToTarget(const glm::vec3& sourceObjectPosition, const glm::vec3& targetObjectPosition, const glm::vec2& mapPosition, const float mapScale, const networking::Navmap& navmap)
{
    const auto& startCoord = navmap.GetNavmapCoord(sourceObjectPosition, mapPosition, mapScale);
    const auto& endCoord = navmap.GetNavmapCoord(targetObjectPosition, mapPosition, mapScale);
    
    auto beginTp = std::chrono::high_resolution_clock::now();
    
    sOpenSet = {};
    sNodes.clear();
    closedSet = {};
    
    std::list<glm::vec3> path;
    
    auto HeuristicLambda = [](int x1, int y1, int x2, int y2)
    {
        return std::abs(x1 - x2) + std::abs(y1 - y2);
    };
    
    auto AddOrUpdateNode = [&](int row, int col, float gCost, Node* parent)
    {
        if (sNodes[row][col] == nullptr || gCost < sNodes[row][col]->gCost)
        {
            if (sNodes[row][col] == nullptr) 
            {
                sNodes[row][col] = std::make_unique<Node>(row, col);
            }
            
            sNodes[row][col]->gCost = gCost;
            sNodes[row][col]->hCost = HeuristicLambda(row, col, endCoord.y, endCoord.x);
            sNodes[row][col]->parent = parent;
            sOpenSet.push(sNodes[row][col].get());
        }
    };
    
    AddOrUpdateNode(startCoord.y, startCoord.x, 0, nullptr);
    
    while (!sOpenSet.empty())
    {
        Node* current = sOpenSet.top();
        sOpenSet.pop();
        
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
                path.push_front(navmap.GetMapPositionFromNavmapCoord(glm::ivec2(node->col, node->row), mapPosition, mapScale, sourceObjectPosition.z) + glm::vec3(TILE_SIZE/4.0f, -TILE_SIZE/4.0f, 0.0f));
                node = node->parent;
            }
            break;
        }

        static const int directions[4][2] = { {0, 1}, {1, 0}, {0, -1}, {-1, 0} }; //, {1, 1}, {-1, 1}, {1, -1}, {-1, -1} };
        for (const auto& dir : directions) {
            int newRow = current->row + dir[0];
            int newCol = current->col + dir[1];

            if (newRow >= 0 && newRow < navmap.GetSize() && newCol >= 0 && newCol < navmap.GetSize() && navmap.GetNavmapTileAt(glm::ivec2(newCol, newRow)) == networking::NavmapTileType::EMPTY)
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
    
    return path;
}

///-----------------------------------------------------------------------------------------------

}
