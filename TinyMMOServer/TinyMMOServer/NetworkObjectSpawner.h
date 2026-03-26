///------------------------------------------------------------------------------------------------
///  NetworkObjectSpawner.h
///  TinyMMOServer
///
///  Created by Alex Koukoulas on 26/03/2026
///------------------------------------------------------------------------------------------------

#ifndef NetworkObjectSpawner_h
#define NetworkObjectSpawner_h

///------------------------------------------------------------------------------------------------

#include "net_common/NetworkCommon.h"

///------------------------------------------------------------------------------------------------

namespace network
{

///------------------------------------------------------------------------------------------------

class NetworkObjectSpawner;
class NetworkObjectBuilder
{
    friend class NetworkObjectSpawner;
public:
    NetworkObjectBuilder& SetParentObjectId(const objectId_t parentObjectId); // Will be objectId by default
    NetworkObjectBuilder& SetObjectType(const ObjectType objectType);
    NetworkObjectBuilder& SetAttackType(const AttackType attackType);
    NetworkObjectBuilder& SetProjectileType(const ProjectileType projectileType);
    NetworkObjectBuilder& SetFacingDirection(const FacingDirection facingDirection);
    NetworkObjectBuilder& SetObjectState(const ObjectState objectState);
    NetworkObjectBuilder& SetColliderType(const ColliderType colliderType);
    NetworkObjectBuilder& SetColliderRelativeDimensions(const glm::vec2& colliderRelativeDimensions);
    NetworkObjectBuilder& SetObjectFaction(const ObjectFaction objectFaction);
    NetworkObjectBuilder& SetPosition(const glm::vec3& position);
    NetworkObjectBuilder& SetVelocity(const glm::vec3& velocity);
    NetworkObjectBuilder& SetMaxHealthPoints(const health_t maxHealthPoints);
    NetworkObjectBuilder& SetCurrentHealthPoints(const health_t currentHealthPoints);
    NetworkObjectBuilder& SetDamagePoints(const health_t damagePoints);
    NetworkObjectBuilder& SetSpeed(const float speed);
    NetworkObjectBuilder& SetObjectScale(const float objectScale);
    NetworkObjectBuilder& SetActionTimer(const float actionTimer);
    NetworkObjectBuilder& SetDisplayName(const std::string& displayName);
    NetworkObjectBuilder& SetCurrentMap(const std::string& currentMap);
    
    ObjectData& GetObjectData();
    
private:
    NetworkObjectBuilder(const objectId_t objectId);
private:
    ObjectData mObjectData;
};

///------------------------------------------------------------------------------------------------

class NetworkObjectSpawner final
{
public:
    NetworkObjectSpawner();
    
    NetworkObjectBuilder NewObject();
    
    objectId_t& GetNextId();
    const objectId_t& GetNextId() const;
    
private:
    objectId_t mNextId;
};

///------------------------------------------------------------------------------------------------

}

///------------------------------------------------------------------------------------------------

#endif /* NetworkObjectSpawner_h */
