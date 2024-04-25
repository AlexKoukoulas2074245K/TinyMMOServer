///------------------------------------------------------------------------------------------------
///  Logging.cpp
///  TinyMMOServer
///
///  Created by Alex Koukoulas on 25/04/2024
///------------------------------------------------------------------------------------------------

#include <mutex>

#include "Logging.h"
#include "Date.h"

///------------------------------------------------------------------------------------------------

static std::mutex sLoggingMutex;

///------------------------------------------------------------------------------------------------

namespace logging
{

///------------------------------------------------------------------------------------------------

void Log(const LogType logType, const char* message, ...)
{
    std::lock_guard<std::mutex> loggingGuard(sLoggingMutex);
    switch(logType)
    {
        case LogType::INFO: printf("[INFO] "); break;
        case LogType::WARNING: printf("[WARNING] "); break;
        case LogType::ERROR: printf("[ERROR] "); break;
    }
    
    auto timePointNow = std::chrono::system_clock::now();
    using namespace date;
    std::stringstream s;
    s << timePointNow;
    printf("(%s) ", s.str().c_str());
    
    va_list args;
    va_start (args, message);
    vprintf (message, args);
    va_end (args);
    
    printf("\n");
    
    fflush(stdout);
}

///------------------------------------------------------------------------------------------------

}

///------------------------------------------------------------------------------------------------
