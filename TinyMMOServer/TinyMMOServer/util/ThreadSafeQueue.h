///------------------------------------------------------------------------------------------------
///  ThreadSafeQueue.h
///  TinyMMOClient
///
///  Created by Alex Koukoulas on 08/12/2023
///------------------------------------------------------------------------------------------------

#ifndef ThreadSafeQueue_h
#define ThreadSafeQueue_h

///------------------------------------------------------------------------------------------------

#include <queue>
#include <mutex>
#include <condition_variable>

///------------------------------------------------------------------------------------------------

template <class T>
class ThreadSafeQueue
{
public:
    ThreadSafeQueue()
        : q()
        , m()
        , c()
    {
        
    }

    ~ThreadSafeQueue()
    {
      
    }

    // Add an element to the queue.
    void enqueue(T t)
    {
        std::lock_guard<std::mutex> lock(m);
        q.push(t);
        c.notify_one();
    }

    // Get the "front"-element.
    // If the queue is empty, wait till a element is avaiable.
    T dequeue(void)
    {
        std::unique_lock<std::mutex> lock(m);
        while(q.empty())
        {
            // release lock as long as the wait and reaquire it afterwards.
            c.wait(lock);
        }
    
        T val = q.front();
        q.pop();
        
        return val;
    }
    
    size_t size() const
    {
        return q.size();
    }

private:
    std::queue<T> q;
    mutable std::mutex m;
    std::condition_variable c;
};

///------------------------------------------------------------------------------------------------

#endif /* ThreadSafeQueue_h */
