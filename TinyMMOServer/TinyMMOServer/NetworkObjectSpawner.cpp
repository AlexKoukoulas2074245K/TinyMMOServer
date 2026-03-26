///------------------------------------------------------------------------------------------------
///  NetworkObjectSpawner.cpp
///  TinyMMOServer
///
///  Created by Alex Koukoulas on 26/03/2026
///------------------------------------------------------------------------------------------------

#include "NetworkObjectSpawner.h"

///------------------------------------------------------------------------------------------------

namespace network
{

///------------------------------------------------------------------------------------------------

NetworkObjectBuilder::NetworkObjectBuilder(const objectId_t objectId)
{
    mObjectData = {};
    mObjectData.objectId = objectId;
    mObjectData.parentObjectId = objectId;
}

///------------------------------------------------------------------------------------------------

NetworkObjectBuilder& NetworkObjectBuilder::SetParentObjectId(const objectId_t parentObjectId)
{
    mObjectData.parentObjectId = parentObjectId;
    return *this;
}

///------------------------------------------------------------------------------------------------

NetworkObjectBuilder& NetworkObjectBuilder::SetObjectType(const ObjectType objectType)
{
    mObjectData.objectType = objectType;
    return *this;
}

///------------------------------------------------------------------------------------------------

NetworkObjectBuilder& NetworkObjectBuilder::SetAttackType(const AttackType attackType)
{
    mObjectData.attackType = attackType;
    return *this;
}

///------------------------------------------------------------------------------------------------

NetworkObjectBuilder& NetworkObjectBuilder::SetProjectileType(const ProjectileType projectileType)
{
    mObjectData.projectileType = projectileType;
    return *this;
}

///------------------------------------------------------------------------------------------------

NetworkObjectBuilder& NetworkObjectBuilder::SetFacingDirection(const FacingDirection facingDirection)
{
    mObjectData.facingDirection = facingDirection;
    return *this;
}

///------------------------------------------------------------------------------------------------

NetworkObjectBuilder& NetworkObjectBuilder::SetObjectState(const ObjectState objectState)
{
    mObjectData.objectState = objectState;
    return *this;
}

///------------------------------------------------------------------------------------------------

NetworkObjectBuilder& NetworkObjectBuilder::SetColliderType(const ColliderType colliderType)
{
    mObjectData.colliderData.colliderType = colliderType;
    return *this;
}

///------------------------------------------------------------------------------------------------

NetworkObjectBuilder& NetworkObjectBuilder::SetColliderRelativeDimensions(const glm::vec2& colliderRelativeDimensions)
{
    mObjectData.colliderData.colliderRelativeDimensions = colliderRelativeDimensions;
    return *this;
}

///------------------------------------------------------------------------------------------------

NetworkObjectBuilder& NetworkObjectBuilder::SetObjectFaction(const ObjectFaction objectFaction)
{
    mObjectData.objectFaction = objectFaction;
    return *this;
}

///------------------------------------------------------------------------------------------------

NetworkObjectBuilder& NetworkObjectBuilder::SetPosition(const glm::vec3& position)
{
    mObjectData.position = position;
    return *this;
}

///------------------------------------------------------------------------------------------------

NetworkObjectBuilder& NetworkObjectBuilder::SetVelocity(const glm::vec3& velocity)
{
    mObjectData.velocity = velocity;
    return *this;
}

///------------------------------------------------------------------------------------------------

NetworkObjectBuilder& NetworkObjectBuilder::SetMaxHealthPoints(const health_t maxHealthPoints)
{
    mObjectData.maxHealthPoints = maxHealthPoints;
    return *this;
}

///------------------------------------------------------------------------------------------------

NetworkObjectBuilder& NetworkObjectBuilder::SetCurrentHealthPoints(const health_t currentHealthPoints)
{
    mObjectData.currentHealthPoints = currentHealthPoints;
    return *this;
}

///------------------------------------------------------------------------------------------------

NetworkObjectBuilder& NetworkObjectBuilder::SetDamagePoints(const health_t damagePoints)
{
    mObjectData.damagePoints = damagePoints;
    return *this;
}


///------------------------------------------------------------------------------------------------

NetworkObjectBuilder& NetworkObjectBuilder::SetSpeed(const float speed)
{
    mObjectData.speed = speed;
    return *this;
}

///------------------------------------------------------------------------------------------------

NetworkObjectBuilder& NetworkObjectBuilder::SetObjectScale(const float objectScale)
{
    mObjectData.objectScale = objectScale;
    return *this;
}

///------------------------------------------------------------------------------------------------

NetworkObjectBuilder& NetworkObjectBuilder::SetActionTimer(const float actionTimer)
{
    mObjectData.actionTimer = actionTimer;
    return *this;
}

///------------------------------------------------------------------------------------------------

NetworkObjectBuilder& NetworkObjectBuilder::SetDisplayName(const std::string& displayName)
{
    std::strcpy(mObjectData.displayName, displayName.c_str());
    return *this;
}

///------------------------------------------------------------------------------------------------

NetworkObjectBuilder& NetworkObjectBuilder::SetCurrentMap(const std::string& currentMap)
{
    std::strcpy(mObjectData.currentMap, currentMap.c_str());
    return *this;
}

///------------------------------------------------------------------------------------------------

ObjectData& NetworkObjectBuilder::GetObjectData()
{
    return mObjectData;
}

///------------------------------------------------------------------------------------------------

NetworkObjectSpawner::NetworkObjectSpawner()
    : mNextId(1)
{
}

///------------------------------------------------------------------------------------------------

NetworkObjectBuilder NetworkObjectSpawner::NewObject()
{
    return NetworkObjectBuilder(mNextId++);
}

///------------------------------------------------------------------------------------------------

objectId_t& NetworkObjectSpawner::GetNextId()
{
    return mNextId;
}

///------------------------------------------------------------------------------------------------

const objectId_t& NetworkObjectSpawner::GetNextId() const
{
    return mNextId;
}

///------------------------------------------------------------------------------------------------

}
