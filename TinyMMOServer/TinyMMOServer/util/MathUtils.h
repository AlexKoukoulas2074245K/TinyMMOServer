///------------------------------------------------------------------------------------------------
///  Logging.h
///  TinyMMOServer
///
///  Created by Alex Koukoulas on 25/04/2024
///------------------------------------------------------------------------------------------------

#ifndef MathUtils_h
#define MathUtils_h

///-----------------------------------------------------------------------------------------------

#include <array>
#include <cmath>                       
#include <ctime>                       
#include <functional>                  
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>                
#include <glm/mat4x4.hpp>              
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/ext/quaternion_float.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/matrix_access.hpp>   
#include <random>
#include <vector>

///-----------------------------------------------------------------------------------------------

using uint = unsigned int;

///-----------------------------------------------------------------------------------------------

namespace math
{

///-----------------------------------------------------------------------------------------------

static constexpr unsigned int FRUSTUM_SIDES = 6;

static constexpr float PI              = 3.141592f;
static constexpr float EQ_THRESHOLD    = 1e-5f;
static constexpr float DEGREES_PER_RAD = 57.2958f;
static constexpr float RADS_PER_DEGREE = 0.0174532925f;

static const glm::vec3 X_AXIS = {1.0f, 0.0f, 0.0f};
static const glm::vec3 Y_AXIS = {0.0f, 1.0f, 0.0f};
static const glm::vec3 Z_AXIS = {0.0f, 0.0f, 1.0f};

///-----------------------------------------------------------------------------------------------

using Frustum = std::array<glm::vec4, FRUSTUM_SIDES>;
using ProbabilityDistribution = std::vector<float>;

///-----------------------------------------------------------------------------------------------

struct Rectangle
{
    glm::vec2 bottomLeft;
    glm::vec2 topRight;
};

///-----------------------------------------------------------------------------------------------
/// Returns the minimum of the 2 given values.
/// @tparam T type class of the arguments given (can be inferred from the parameters passed in).
/// @param[in] a the first value.
/// @param[in] b the second value.
/// @returns the strictly smallest value.
template<class T>
inline T Min(T a, T b)
{
    return a < b ? a : b;
}

///-----------------------------------------------------------------------------------------------
/// Returns the maximum of the 2 given values.
/// @tparam T type class of the arguments given (can be inferred from the parameters passed in).
/// @param[in] a the first value.
/// @param[in] b the second value.
/// @returns the strictly largest value.
template<class T>
inline T Max(T a, T b)
{
    return a > b ? a : b;
}

///-----------------------------------------------------------------------------------------------
/// Returns the absolute value.
/// @tparam T type class of the argument given (can be inferred from the parameter passed in).
/// @param[in] val the value.
/// @returns the absolute value of the parameter passed in.
template<class T>
inline T Abs(const T val)
{
    return val < 0 ? -val : val;
}

///-----------------------------------------------------------------------------------------------
/// Computes the sine of the given float value.
/// @param[in] val the value.
/// @returns the sin of the value.
inline float Sinf(const float val)
{
    return sinf(val);
}

///-----------------------------------------------------------------------------------------------
/// Computes the cosine of the given float value.
/// @param[in] val the value.
/// @returns the cosine of the value.
inline float Cosf(const float val)
{
    return cosf(val);
}

///-----------------------------------------------------------------------------------------------
/// Computes the arc tangent value of the given x and y float values.
/// @param[in] x the first value.
/// @param[in] y the second value.
/// @returns the arctan of the two values  given.
inline float Arctan2(const float x, const float y)
{
    return std::atan2(x, y);
}

///-----------------------------------------------------------------------------------------------
/// Returns the interpolated value between two values and the parametric argument
/// @tparam T type class of the arguments given (can be inferred from the parameters passed in).
/// @param[in] x the start value.
/// @param[in] y the end value.
/// @param[in] t the parametric float value.
/// @returns the interpolated value between the two values and the parametric t.
template <class T>
inline T Lerp(const T x, const T y, const float t)
{
    return (T)(x * (1.0f - t) + y * t);
}


///-----------------------------------------------------------------------------------------------
/// Returns the interpolated value between two values and the parametric argument
/// @tparam T type class of the arguments given (can be inferred from the parameters passed in).
/// @param[in] x the start value.
/// @param[in] y the end value.
/// @param[in] t the parametric float value.
/// @returns the interpolated value between the two values and the parametric t.
template <class T>
inline T Slerp(const T x, const T y, const float theta, const float t)
{
    return (T)((Sinf((1.0f - t) * theta)/(Sinf(theta)))*x + (Sinf(t * theta)/(Sinf(theta)))*y);
}

///-----------------------------------------------------------------------------------------------
/// Returns the square root of the value passed in.
/// @param[in] val the value.
/// @returns the square root of the value.
inline float Sqrt(const float val)
{
    return sqrtf(val);
}

///-----------------------------------------------------------------------------------------------
/// Tests whether 2 floats are sufficiently close.
/// @param[in] a the first float value
/// @param[in] b the second float value
/// @returns whether or not the 2 float values are close enough to be considered equal based
/// on a constant threshold
inline bool FloatsSufficientlyClose(const float a, const float b)
{
    return Abs(a - b) <= EQ_THRESHOLD;
}

///-----------------------------------------------------------------------------------------------
/// Returns the square distance of two vectors.
/// @param[in] vec1 first vector.
/// @param[in] vec2 second vector.
/// @returns the square distance of the two vectors.
inline float Distance2(const glm::vec3& vec1, const glm::vec3& vec2)
{
    return (vec2.x - vec1.x) * (vec2.x - vec1.x) + (vec2.y - vec1.y) * (vec2.y - vec1.y) + (vec2.z - vec1.z) * (vec2.z - vec1.z);
}

///-----------------------------------------------------------------------------------------------
/// Returns the square distance of two vectors without taking into account the z component.
/// @param[in] vec1 first vector.
/// @param[in] vec2 second vector.
/// @returns the square distance of the two vectors.
inline float Distance2IgnoreZ(const glm::vec3& vec1, const glm::vec3& vec2)
{
    return (vec2.x - vec1.x) * (vec2.x - vec1.x) + (vec2.y - vec1.y) * (vec2.y - vec1.y);
}

///-----------------------------------------------------------------------------------------------
/// Clamps the given 3d  vector to a maximum cap.
/// @param[in] vec the vector to clamp.
/// @param[in] maxVec the vector holding the maximum values.
/// @returns the input vector vec clamped based on the values of the vector maxVec.
inline glm::vec3 ClampToMax(const glm::vec3& vec, const glm::vec3& maxVec)
{
    return glm::vec3(Min(vec.x, maxVec.x), Min(vec.y, maxVec.y), Min(vec.z, maxVec.z));
}

///-----------------------------------------------------------------------------------------------
/// Clamps the given 3d  vector to a minimum cap.
/// @param[in] vec the vector to clamp.
/// @param[in] minVec the vector holding the minimum values.
/// @returns the input vector vec clamped based on the values of the vector minVec.
inline glm::vec3 ClampToMin(const glm::vec3& vec, const glm::vec3& minVec)
{
    return glm::vec3(Max(vec.x, minVec.x), Max(vec.y, minVec.y), Max(vec.z, minVec.z));
}


///-----------------------------------------------------------------------------------------------
/// Converts a glm::vec3 to a glm::vec2.
/// @param[in] vec the vec3 to transform.
/// @returns a vec2 holding only the first two values of the input vector vec.
inline glm::vec2 Vec3ToVec2(const glm::vec3& vec)
{
    return glm::vec2(vec.x, vec.y);
}

///-----------------------------------------------------------------------------------------------
/// Converts a glm::vec2 to a glm::vec3.
/// @param[in] vec the vec2 to transform.
/// @returns a vec3 holding both values of the input vector vec and a zeroed third element.
inline glm::vec3 Vec2ToVec3(const glm::vec2& vec)
{
    return glm::vec3(vec.x, vec.y, 0.0f);
}

///-----------------------------------------------------------------------------------------------
/// Tweening Modes Supported
///
/// EASE_IN (slow at first then accelerate)
/// EASE_OUT (fast at first then decelerate)
/// EASE_IN_OUT (EASE_IN before mid point, EASE_OUT after mid point)
enum class TweeningMode
{
    EASE_IN, EASE_OUT, EASE_IN_OUT
};

///-----------------------------------------------------------------------------------------------
/// Linear Tweening function to be used by clients of the TweenValue function. \see TweenValue()
/// @param[in] t the input value to the tween function.
/// @returns the transformed t value linearly.
inline float LinearFunction(const float t)
{
    return t;
}

///-----------------------------------------------------------------------------------------------
/// Quadratic Tweening function to be used by clients of the TweenValue function. \see TweenValue()
/// @param[in] t the input value to the tween function.
/// @returns the transformed t value quadratically.
inline float QuadFunction(const float t)
{
    return powf(t, 2.0f);
}

///-----------------------------------------------------------------------------------------------
/// Cubic Tweening function to be used by clients of the TweenValue function. \see TweenValue()
/// @param[in] t the input value to the tween function.
/// @returns the transformed t value cubically.
inline float CubicFunction(const float t)
{
    return powf(t, 3.0f);
}

///-----------------------------------------------------------------------------------------------
/// Quartic Tweening function to be used by clients of the TweenValue function. \see TweenValue()
/// @param[in] t the input value to the tween function.
/// @returns the transformed t value quartically.
inline float QuartFunction(const float t)
{
    return powf(t, 4.0f);
}

///-----------------------------------------------------------------------------------------------
/// Quint Tweening function to be used by clients of the TweenValue function. \see TweenValue()
/// @param[in] t the input value to the tween function.
/// @returns the transformed t value quintically.
inline float QuintFunction(const float t)
{
    return powf(t, 5.0f);
}

///-----------------------------------------------------------------------------------------------
/// Back Tweening function to be used by clients of the TweenValue function. \see TweenValue()
/// @param[in] t the input value to the tween function.
/// @returns the transformed t value based on the back function.
inline float BackFunction(const float t)
{
    return powf(t, 2.0f) * (2.70158f * t - 1.70158f);
}

///-----------------------------------------------------------------------------------------------
/// Bounce Tweening function to be used by clients of the TweenValue function. \see TweenValue()
/// @param[in] t the input value to the tween function.
/// @returns the transformed t value based on the bounce function.
inline float BounceFunction(const float t)
{
    if (t < 4.0f/11.0f)
    {
        return (121.0f * t * t)/16.0f;
    }
    else if (t < 8.0f/11.0f)
    {
        return (363.0f/40.0f * t * t) - (99.0f/10.0f * t) + 17.0f/5.0f;
    }
    else if (t < 9.0f/10.0f)
    {
        return (4356.0f/361.0f * t * t) - (35442.0f/1805.0f * t) + 16061.0f/1805.0f;
    }
    else
    {
        return (54.0f/5.0f * t * t) - (513.0f/25.0f * t) + 268.0f/25.0f;
    }
}

///-----------------------------------------------------------------------------------------------
/// Elastic Tweening function to be used by clients of the TweenValue function. \see TweenValue()
/// @param[in] t the input value to the tween function.
/// @returns the transformed t value based on the bounce function.
inline float ElasticFunction(const float t)
{
    const float c4 = (2.0f * PI) / 3.0f;

    if (t <= 0.0f) return 0.0f;
    else if (t >= 1.0f) return 1.0f;
    else return powf(2.0f, -10.0f * t) * sinf((t * 10.0f -0.75f) * c4) + 1.0f;
}

///-----------------------------------------------------------------------------------------------
/// Tweens the given value based on the tweening function and the tweening mode.
/// @param[in] val the value to be tweened in [0..1] range.
/// @param[in] tweeningFunc the tweening function to be used (defaults to linear).
/// @param[in] tweeningMode the tweening mode (defaults to ease out).
/// @returns the tweened value.
inline float TweenValue(const float val, const std::function<float(const float)> tweeningFunc = LinearFunction , const TweeningMode tweeningMode = TweeningMode::EASE_IN)
{
    switch (tweeningMode)
    {
        case TweeningMode::EASE_IN: return tweeningFunc(val); break;
        case TweeningMode::EASE_OUT: return 1.0f - tweeningFunc(1.0f - val); break;
        case TweeningMode::EASE_IN_OUT: return (val < 0.5f) ? TweenValue(val * 2.0f, tweeningFunc, TweeningMode::EASE_IN)/2.0f : 0.5f + (TweenValue((val - 0.5f) * 2.0f, tweeningFunc, TweeningMode::EASE_OUT)/2.0f);
    }
    
    return 0.0f;
}

///-----------------------------------------------------------------------------------------------
/// Gets the custom  seed for a controlled sequence of generated random numbers.
/// @returns the control seed that the random generation sequence will continue with/
int GetControlSeed();

///-----------------------------------------------------------------------------------------------
/// Sets a custom seed for a controlled sequence of generated random numbers.
/// @param[in] seed the seed to start the sequence with
void SetControlSeed(const int seed);

///-----------------------------------------------------------------------------------------------
/// Computes a random int based on the min and max inclusive values provided and the control seed specified at SetCRandomSeed.
/// @param[in] min the minimum value (inclusive) that the function can return (defaults to 0).
/// @param[in] max the maximum value (inclusive) that the function can return (defaults to 32767).
/// @returns a random integer that respects the given bounds.
int ControlledRandomInt(const int min = 0, const int max = RAND_MAX);

///-----------------------------------------------------------------------------------------------
/// Computes a random float based on the min and max inclusive values provided and the control seed specified at SetCRandomSeed.
/// @param[in] min the minimum value (inclusive) that the function can return (defaults to 0.0f).
/// @param[in] max the maximum value (inclusive) that the function can return (defaults to 1.0f).
/// @returns a random float that respects the given bounds.
float ControlledRandomFloat(const float min = 0.0f, const float max = 1.0f);

///-----------------------------------------------------------------------------------------------
/// Selects an entry from a probability distribution vector based on the control seed specified at SetCRandomSeed.
/// The sum of probabilities of the entries should naturally be 1, although this is not enforced.
/// @param[in] probDist a vector containing probability floats in the range of [0,1].
/// @returns the index of the selected entry
int ControlledIndexSelectionFromDistribution(const ProbabilityDistribution& probDist);

///-----------------------------------------------------------------------------------------------
/// Returns a mersenne_twister_engine
/// @returns the rng engine
std::mt19937& GetRandomEngine();

///-----------------------------------------------------------------------------------------------
/// Computes a random int based on the min and max inclusive values provided.
/// @param[in] min the minimum value (inclusive) that the function can return (defaults to 0).
/// @param[in] max the maximum value (inclusive) that the function can return (defaults to 32767).
/// @returns a random integer that respects the given bounds.
inline int RandomInt(const int min = 0, const int max = RAND_MAX)
{
    std::uniform_int_distribution<> distr(min, max);
    return distr(GetRandomEngine());
}

///-----------------------------------------------------------------------------------------------
/// Computes a random float based on the min and max inclusive values provided.
/// @param[in] min the minimum value (inclusive) that the function can return (defaults to 0.0f).
/// @param[in] max the maximum value (inclusive) that the function can return (defaults to 1.0f).
/// @returns a random float that respects the given bounds.
inline float RandomFloat(const float min = 0.0f, const float max = 1.0f)
{
    return min + static_cast <float> (RandomInt()) / (static_cast <float> (RAND_MAX / (max - min)));
}

///-----------------------------------------------------------------------------------------------
/// Computes a random sign, 1 or -1.
/// @returns a random sign (either -1 or 1).
inline int RandomSign()
{
    return RandomInt(0, 1) == 0 ? -1 : 1;
}

///-----------------------------------------------------------------------------------------------
/// Computes and returns whether the given position point is inside the given sphere.
///
/// @param[in] posPoint position point to test.
/// @param[in] sphereCenter the center of the sphere.
/// @param[in] sphereRadius the radius of the sphere.
/// @returns whether the point is inside the sphere.
bool PointInSphereTest(const glm::vec3& posPoint, const glm::vec3& sphereCenter, const float sphereRadius);

///-----------------------------------------------------------------------------------------------
/// Computes and returns whether the two given spheres intersect.
///
/// @param[in] sphere1Center the center of the first sphere.
/// @param[in] sphere1Radius the radius of the first sphere.
/// @param[in] sphere2Center the center of the second sphere.
/// @param[in] sphere2Radius the radius of the second sphere.
/// @returns whether an intersection is taking place.
bool SphereToSphereIntersection(const glm::vec3& sphere1Center, const float sphere1Radius, const glm::vec3& sphere2Center, const float sphere2Radius);

///-----------------------------------------------------------------------------------------------
/// Computes and returns whether the two given spheres intersect and also the penetration depth.
///
/// @param[in] sphere1Center the center of the first sphere.
/// @param[in] sphere1Radius the radius of the first sphere.
/// @param[in] sphere2Center the center of the second sphere.
/// @param[in] sphere2Radius the radius of the second sphere.
/// @param[out] penetration the penetration depth
/// @returns whether an intersection is taking place.
bool SphereToSphereIntersection(const glm::vec3& sphere1Center, const float sphere1Radius, const glm::vec3& sphere2Center, const float sphere2Radius, float& penetration);

///-----------------------------------------------------------------------------------------------
/// Computes and returns whether the given ray intersects with the given sphere.
/// @see https://www.scratchapixel.com/lessons/3d-basic-rendering/minimal-ray-tracer-rendering-simple-shapes/ray-sphere-intersection
/// @param[in] rayOrigin the origin of the ray (usually camera position for object picking).
/// @param[in] rayDirection the direction of the ray.
/// @param[in] sphereCenter the center of the sphere.
/// @param[in] sphereRadius the radius of the sphere.
/// @param[out] t the ray's t at the intersection point, if one actually happened
/// @returns whether an intersection is taking place.
bool RayToSphereIntersection(const glm::vec3& rayOrigin, const glm::vec3& rayDirection, const glm::vec3& sphereCenter, const float sphereRadius, float& t);

///-----------------------------------------------------------------------------------------------
/// Computes and returns whether the given ray intersects the given plane.
/// @param[in] rayOrigin the origin of the ray (usually camera position for object picking).
/// @param[in] rayDirection the direction of the ray.
/// @param[in] planeCenter the center of the plane.
/// @param[in] planeNormal the normal vector of the plane.
/// @param[out] intersectionPoint the intersection point found.
/// @returns whether an intersection is taking place.
bool RayToPlaneIntersection(const glm::vec3& rayOrigin, const glm::vec3& rayDirection, const glm::vec3& planeCenter, const glm::vec3& planeNormal, glm::vec3& intersectionPoint);

///-----------------------------------------------------------------------------------------------
/// Computes and returns whether the given mesh is at least partly inside a frustum.
/// @param[in] meshPosition the mesh's position.
/// @param[in] meshScale the scale of the mesh.
/// @param[in] meshDimensions the dimensions of the mesh in a glm::vec3 form.
/// @param[in] frustum the frustum to test containment in.
/// @returns whether the mesh is at least partly inside the frustum.
bool IsMeshAtLeastPartlyInsideFrustum(const glm::vec3& meshPosition, const glm::vec3& meshScale, const glm::vec3& meshDimensions, const Frustum& frustum, int& breachedSideIndex);

///-----------------------------------------------------------------------------------------------
/// Computes and returns whether the given mesh is fully inside a frustum.
/// @param[in] meshPosition the mesh's position.
/// @param[in] meshScale the scale of the mesh.
/// @param[in] meshDimensions the dimensions of the mesh in a glm::vec3 form.
/// @param[in] frustum the frustum to test containment in.
/// @returns whether the mesh is fully inside the frustum.
bool IsMeshFullyInsideFrustum(const glm::vec3& meshPosition, const glm::vec3& meshScale, const glm::vec3& meshDimensions, const Frustum& frustum, int& breachedSideIndex);

///-----------------------------------------------------------------------------------------------
/// Computes and returns whether the given point is inside a rectangle.
/// @param[in] rectangleBottomLeft rectangle's top left point.
/// @param[in] rectangleTopRight rectangle's bottom right.
/// @param[in] point point to test.
/// @returns whether the point is inside the rectangle.
bool IsPointInsideRectangle(const glm::vec2& rectangleBottomLeft, const glm::vec2& rectangleTopRight, const glm::vec2& point);

///------------------------------------------------------------------------------------------------

class BezierCurve
{
public:
    BezierCurve(const std::vector<glm::vec3> controlPoints)
        : mControlPoints(controlPoints)
    {
    }
    
    glm::vec3 ComputePointForT(const float t);
    
private:
    std::vector<glm::vec3> mControlPoints;
};

///------------------------------------------------------------------------------------------------

}

///-----------------------------------------------------------------------------------------------

#endif /* MathUtils_h */
