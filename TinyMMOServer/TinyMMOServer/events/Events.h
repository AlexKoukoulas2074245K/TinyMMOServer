///------------------------------------------------------------------------------------------------
///  Events.h
///  TinyMMOServer
///
///  Created by Alex Koukoulas on 02/11/2023
///------------------------------------------------------------------------------------------------

#ifndef Events_h
#define Events_h

///------------------------------------------------------------------------------------------------

#include "../util/StringUtils.h"
#include "net_common/NetworkCommon.h"

///------------------------------------------------------------------------------------------------

namespace events
{

///------------------------------------------------------------------------------------------------

class DummyEvent
{
    
};

///------------------------------------------------------------------------------------------------

class MapChangeEvent
{
public:
    MapChangeEvent(const strutils::StringId& newMapName)
    : mNewMapName(newMapName)
    {
    }
    
    const strutils::StringId mNewMapName;
};

///------------------------------------------------------------------------------------------------

class NetworkObjectCollisionEvent
{
public:
    // rhs == 0 signifies collision with solid/geometry
    NetworkObjectCollisionEvent(network::objectId_t lhs, network::objectId_t rhs = 0)
    : mLhs(lhs)
    , mRhs(rhs)
    {
    }
    
    const network::objectId_t mLhs;
    const network::objectId_t mRhs;
};

///------------------------------------------------------------------------------------------------

}

///------------------------------------------------------------------------------------------------

#endif /* Events_h */
