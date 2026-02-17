///------------------------------------------------------------------------------------------------
///  EventSystem.cpp
///  TinyMMOServer
///
///  Created by Alex Koukoulas on 01/11/2023
///------------------------------------------------------------------------------------------------

#include "EventSystem.h"

///------------------------------------------------------------------------------------------------

namespace events
{

///------------------------------------------------------------------------------------------------

EventSystem& EventSystem::GetInstance()
{
    static EventSystem instance;
    return instance;
}

///------------------------------------------------------------------------------------------------

void EventSystem::UnregisterAllEventsForListener(const IListener* listener)
{
    for (auto& eventEntry: mEventIdToDeadListenerIds)
    {
        eventEntry.second.insert(std::make_pair(listener, listener->mInstanceId));
    }
}

///------------------------------------------------------------------------------------------------

static std::size_t sInstanceIdCounter = 0;
IListener::IListener()
    : mInstanceId(sInstanceIdCounter++)
{
}

///------------------------------------------------------------------------------------------------

IListener::~IListener()
{
    EventSystem::GetInstance().UnregisterAllEventsForListener(this);
}

///------------------------------------------------------------------------------------------------

}
