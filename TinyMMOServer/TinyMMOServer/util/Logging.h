///------------------------------------------------------------------------------------------------
///  Logging.h
///  TinyMMOServer
///
///  Created by Alex Koukoulas on 25/04/2024
///------------------------------------------------------------------------------------------------

#ifndef Logging_h
#define Logging_h

///-----------------------------------------------------------------------------------------------

#include "Date.h"
#include <stdarg.h>
#include <stdio.h>
#include <string>
#include <sstream>
#include <chrono>

///-----------------------------------------------------------------------------------------------

namespace logging
{

///-----------------------------------------------------------------------------------------------

#define LOG_IN_RELEASE

///-----------------------------------------------------------------------------------------------
/// Different types of logging available
enum class LogType
{
    INFO, WARNING, ERROR
};

///-----------------------------------------------------------------------------------------------
/// Logs a message to the std out, with a custom log type tag \see LogType
/// @param[in] logType the category of logging message
/// @param[in] message the message itself as a c-string
#if !defined(NDEBUG) || defined(LOG_IN_RELEASE)
void Log(const LogType logType, const char* message, ...);
#else
void Log(const LogType, const char*, ...) {}
#endif /* not NDEBUG */

///-----------------------------------------------------------------------------------------------

}

///-----------------------------------------------------------------------------------------------

#endif /* Logging_h */
