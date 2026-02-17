///------------------------------------------------------------------------------------------------
///  EventSystem.h
///  TinyMMOServer
///
///  Created by Alex Koukoulas on 01/11/2023
///------------------------------------------------------------------------------------------------

#ifndef EventSystem_h
#define EventSystem_h

///------------------------------------------------------------------------------------------------

#include "../util/TypeTraits.h"
#include "Events.h"

#include <functional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <set>
#include <vector>
#include <memory>

///------------------------------------------------------------------------------------------------

namespace events
{

///------------------------------------------------------------------------------------------------

class IListener
{
public:
    IListener();
    virtual ~IListener();
    
public:
    const std::size_t mInstanceId;
};

///------------------------------------------------------------------------------------------------

struct DeadListenerHasher
{
    std::size_t operator()(const std::pair<const IListener*, std::size_t>& key) const
    {
        return key.second;
    }
};

template<typename EventType>
bool operator < (const std::pair<IListener*, std::function<void(const EventType&)>>& lhs, const std::pair<IListener*, std::function<void(const EventType&)>>& rhs)
{
    return lhs.first->mInstanceId < rhs.first->mInstanceId;
}

///------------------------------------------------------------------------------------------------

class EventSystem final
{
public:
    static EventSystem& GetInstance();
    
    template<typename EventType, class... Args>
    void DispatchEvent(Args&&... args)
    {
        EventType event(std::forward<Args>(args)...);
        
        CleanCallbacks<EventType>();
        if (!mEventCallbacks<EventType>.empty())
        {
            for (auto callbackIter = mEventCallbacks<EventType>.begin(); callbackIter != mEventCallbacks<EventType>.end();)
            {
                callbackIter->second(event);
                callbackIter++;
            }
        }
    };
    
    template<typename EventType, typename FunctionType>
    [[nodiscard]] std::unique_ptr<IListener> RegisterForEvent(FunctionType callback)
    {
        CleanCallbacks<EventType>();
        auto listener = std::make_unique<IListener>();
        mEventCallbacks<EventType>.insert(std::make_pair(listener.get(), callback));
        return listener;
    }
    
    template<typename EventType, typename InstanceType, typename FunctionType>
    void RegisterForEvent(InstanceType* listener, FunctionType callback)
    {
        CleanCallbacks<EventType>();
        mEventCallbacks<EventType>.insert(std::make_pair(listener, [listener, callback](const EventType& e){ (listener->*callback)(e); }));
    }
    
    template<typename EventType>
    void UnregisterForEvent(IListener* listener)
    {
        mEventCallbacks<EventType>.erase(std::make_pair(listener, nullptr));
    }
    
    void UnregisterAllEventsForListener(const IListener* listener);
    
private:
    template<typename EventType>
    void CleanCallbacks()
    {
        auto eventTypeId = GetTypeHash<EventType>();
        mEventIdToDeadListenerIds[eventTypeId];
        
        for (auto callbackIter = mEventCallbacks<EventType>.begin(); callbackIter != mEventCallbacks<EventType>.end();)
        {
            bool foundInDeadListenerIds = false;
            for (auto deadListenerIter = mEventIdToDeadListenerIds[eventTypeId].begin(); deadListenerIter != mEventIdToDeadListenerIds[eventTypeId].end() && !foundInDeadListenerIds;)
            {
                if (deadListenerIter->first == callbackIter->first)
                {
                    callbackIter = mEventCallbacks<EventType>.erase(callbackIter);
                    mEventIdToDeadListenerIds[eventTypeId].erase(deadListenerIter);
                    foundInDeadListenerIds = true;
                    break;
                }
                deadListenerIter++;
            }
            
            if (!foundInDeadListenerIds)
            {
                callbackIter++;
            }
        }
    }
    
    EventSystem() = default;
    
private:
    template<typename EventType>
    static inline std::set<std::pair<IListener*, std::function<void(const EventType&)>>> mEventCallbacks;
    std::unordered_map<std::size_t, std::unordered_set<std::pair<const IListener*, std::size_t>, DeadListenerHasher>> mEventIdToDeadListenerIds;
};

///------------------------------------------------------------------------------------------------

}

///------------------------------------------------------------------------------------------------

#endif /* EventSystem_h */
