#ifndef PITAYA_LOGGER_H
#define PITAYA_LOGGER_H

#include <iostream>
#include <sstream>

#ifdef LOG
#error "The LOG macro is already defined"
#else
#define LOG(level) pitaya::StreamLogger(pitaya::LogLevel_##level, __FUNCTION__, __LINE__)
#endif

namespace pitaya {

enum LogLevel
{
    LogLevel_Debug,
    LogLevel_Info,
    LogLevel_Warn,
    LogLevel_Error,
    LogLevel_Fatal,
};

static const char* LogLevelNames[] = {
    "[DEBU] ", "[INFO] ", "[WARN] ", "[ERRO] ", "[FATA] ",
};

class StreamLogger
{
public:
    StreamLogger(LogLevel level, const char* function, int line)
    {
        _oss << LogLevelNames[level] << function << "(" << line << "): ";
    }

    ~StreamLogger() { std::cout << _oss.str() << "\n"; }

    template<typename T>
    StreamLogger& operator<<(const T& t)
    {
        _oss << t;
        return *this;
    }

private:
    std::ostringstream _oss;
};

} // namespace pitaya

#endif // PITAYA_LOGGER_H