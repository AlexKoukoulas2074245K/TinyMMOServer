///------------------------------------------------------------------------------------------------
///  Logging.h
///  TinyMMOServer
///
///  Created by Alex Koukoulas on 25/04/2024
///------------------------------------------------------------------------------------------------

#include "MathUtils.h"

///-----------------------------------------------------------------------------------------------

namespace math
{

///-----------------------------------------------------------------------------------------------

static int controlledRandomSeed = 0;
static int internalRand();

///-----------------------------------------------------------------------------------------------

int GetControlSeed()
{
    return controlledRandomSeed;
}


///-----------------------------------------------------------------------------------------------
void SetControlSeed(const int seed)
{
    controlledRandomSeed = seed;
}

///-----------------------------------------------------------------------------------------------

int ControlledRandomInt(const int min /* = 0 */, const int max /* = RAND_MAX */)
{
    return static_cast<int>(internalRand() % (static_cast<long>(max) + 1 - min) + min);
}

///-----------------------------------------------------------------------------------------------

float ControlledRandomFloat(const float min /* = 0.0f */, const float max /* = 1.0f */)
{
    return min + static_cast<float>(ControlledRandomInt())/(static_cast <float> (RAND_MAX / (max - min)));
}

///-----------------------------------------------------------------------------------------------

int ControlledIndexSelectionFromDistribution(const ProbabilityDistribution& probDist)
{
    assert(!probDist.empty());
    
    auto randomFloat = ControlledRandomFloat();
    auto probSum = 0.0f;
    for (int i = 0; i < static_cast<int>(probDist.size()); ++i)
    {
        probSum += probDist[i];
        if (randomFloat < probSum)
        {
            return i;
        }
    }
    
    return -1;
}

///-----------------------------------------------------------------------------------------------

std::mt19937& GetRandomEngine()
{
    static std::random_device rd;
    static std::mt19937 eng(rd());
    return eng;
}

///-----------------------------------------------------------------------------------------------

bool PointInSphereTest(const glm::vec3& posPoint, const glm::vec3& sphereCenter, const float sphereRadius)
{
    return glm::distance(sphereCenter, posPoint) < sphereRadius;
}

///------------------------------------------------------------------------------------------------

bool SphereToSphereIntersection(const glm::vec3& sphere1Center, const float sphere1Radius, const glm::vec3& sphere2Center, const float sphere2Radius)
{
    return glm::distance(sphere1Center, sphere2Center) < sphere1Radius + sphere2Radius;
}

///-----------------------------------------------------------------------------------------------

bool SphereToSphereIntersection(const glm::vec3& sphere1Center, const float sphere1Radius, const glm::vec3& sphere2Center, const float sphere2Radius, float& penetration)
{
    auto sphereDistance = glm::distance(sphere1Center, sphere2Center);
    penetration = (sphere1Radius + sphere2Radius) - sphereDistance;
    return  sphereDistance < sphere1Radius + sphere2Radius;
}

///------------------------------------------------------------------------------------------------

bool RayToSphereIntersection(const glm::vec3& rayOrigin, const glm::vec3& rayDirection, const glm::vec3& sphereCenter, const float sphereRadius, float& t)
{
    const auto& radius2 = sphereRadius * sphereRadius;
    const auto& L = sphereCenter - rayOrigin;
    const auto& tca = glm::dot(L, rayDirection);
    const auto& d2 = glm::dot(L, L) - tca * tca;
    
    if (d2 > radius2) return false;
    const auto& thc = Sqrt(radius2 - d2);
    auto t0 = tca - thc;
    auto t1 = tca + thc;
    
    if (t0 > t1) std::swap(t0, t1);
    
    if (t0 < 0) {
        t0 = t1; // if t0 is negative, let's use t1 instead
        if (t0 < 0) return false; // both t0 and t1 are negative
    }
    
    t = t0;
    return true;
}

///------------------------------------------------------------------------------------------------

bool RayToPlaneIntersection(const glm::vec3& rayOrigin, const glm::vec3& rayDirection, const glm::vec3& planeCenter, const glm::vec3& planeNormal, glm::vec3& intersectionPoint)
{
    float denom = glm::dot(planeNormal, rayDirection);
    if (abs(denom) > 0.0001f) // your favorite epsilon
    {
        float t = glm::dot(planeNormal, (planeCenter - rayOrigin)) / denom;
        if (t >= 0)
        {
            intersectionPoint = rayOrigin + t * rayDirection;
            intersectionPoint.z = planeCenter.z;
            return true;
        }
    }
    return false;
}

///------------------------------------------------------------------------------------------------

bool IsMeshAtLeastPartlyInsideFrustum(const glm::vec3& meshPosition, const glm::vec3& meshScale, const glm::vec3& meshDimensions, const Frustum& frustum, int& breachedSideIndex)
{
    const auto scaledMeshDimensions = meshDimensions * meshScale;
    const auto frustumCheckSphereRadius = math::Max(scaledMeshDimensions.x, math::Max(scaledMeshDimensions.y, scaledMeshDimensions.z)) * 0.5f;
    
    for (auto i = 0U; i < 6U; ++i)
    {
        float dist =
        frustum[i].x * meshPosition.x +
        frustum[i].y * meshPosition.y +
        frustum[i].z * meshPosition.z +
        frustum[i].w - frustumCheckSphereRadius;
        
        if (dist > 0)
        {
            breachedSideIndex = i;
            return false;
        }
                
    }
    
    breachedSideIndex = -1;
    return true;
}

///------------------------------------------------------------------------------------------------

bool IsMeshFullyInsideFrustum(const glm::vec3& meshPosition, const glm::vec3& meshScale, const glm::vec3& meshDimensions, const Frustum& frustum, int& breachedSideIndex)
{
    const auto scaledMeshDimensions = meshDimensions * meshScale;
    const auto frustumCheckSphereRadius = math::Max(scaledMeshDimensions.x, math::Max(scaledMeshDimensions.y, scaledMeshDimensions.z)) * 0.5f;
    
    for (auto i = 0U; i < 6U; ++i)
    {
        float dist =
        frustum[i].x * meshPosition.x +
        frustum[i].y * meshPosition.y +
        frustum[i].z * meshPosition.z +
        frustum[i].w + frustumCheckSphereRadius;
        
        if (dist > 0)
        {
            breachedSideIndex = i;
            return false;
        }
                
    }
    
    breachedSideIndex = -1;
    return true;
}

///------------------------------------------------------------------------------------------------

bool IsPointInsideRectangle(const glm::vec2& rectangleBottomLeft, const glm::vec2& rectangleTopRight, const glm::vec2& point)
{
    return point.x > rectangleBottomLeft.x && point.x < rectangleTopRight.x && point.y > rectangleBottomLeft.y && point.y < rectangleTopRight.y;
}

///------------------------------------------------------------------------------------------------

glm::vec3 ComputeTouchCoordsInWorldSpace(const glm::vec2& touchPosition, const glm::mat4& viewMatrix, const glm::mat4& projMatrix)
{
    const auto& normalizedTouchX =  (touchPosition.x/0.5f - 1.0f);
    const auto& normalizedTouchY = -(touchPosition.y/0.5f - 1.0f);
    
    const auto& invVP = glm::inverse(projMatrix * viewMatrix);
    const auto& screenPos = glm::vec4(normalizedTouchX, normalizedTouchY, 1.0f, 1.0f);
    const auto& worldPos = invVP * screenPos;
    return glm::vec3(worldPos.x, worldPos.y, 0.0f);
}

///------------------------------------------------------------------------------------------------

glm::vec3 BezierCurve::ComputePointForT(const float t)
{
    auto workingPoints = mControlPoints;
    int controlPointsN = static_cast<int>(mControlPoints.size());
    for (int j = 1; j < controlPointsN; ++j)
    {
        for (int k = 0; k < controlPointsN - j; ++k)
        {
            workingPoints[k] = workingPoints[k] * (1 - t) + workingPoints[k + 1] * t;
        }
    }
    
    return workingPoints[0];
}

///------------------------------------------------------------------------------------------------

// https://stackoverflow.com/questions/1026327/what-common-algorithms-are-used-for-cs-rand
int internalRand()
{
    unsigned int next = controlledRandomSeed;
    int result;

    next *= 1103515245;
    next += 12345;
    result = (unsigned int) (next / 65536) % 2048;

    next *= 1103515245;
    next += 12345;
    result <<= 10;
    result ^= (unsigned int) (next / 65536) % 1024;

    next *= 1103515245;
    next += 12345;
    result <<= 10;
    result ^= (unsigned int) (next / 65536) % 1024;

    controlledRandomSeed = next;

    return result;
}

///------------------------------------------------------------------------------------------------

}

///------------------------------------------------------------------------------------------------
