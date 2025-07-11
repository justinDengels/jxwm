#include "logger.hpp"
#include <iostream>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <ostream>
#include <string>

namespace JLogger
{
    void SetLogType(int type) { logType = type; }
    
    void SetLogLevel(int level) { minLogLevel = level; }
    
    void SetLogPath(const std::string& name)
    {
        logFile.open(name);
        if (!logFile.is_open())
        {
            std::cerr << "Error: Failed to open file " << name << std::endl;
            return;
        }
    }

    void Trace(const std::string& log) { if (minLogLevel >= LOGTRACE) { Log(GetTime() + " TRACE: " + log); } }
    
    void Info(const std::string& log) { if (minLogLevel >= LOGINFO) { Log(GetTime() + " TRACE: " + log); } }
    
    void Error(const std::string& log) { if (minLogLevel >= LOGERROR) { Log(GetTime() + " TRACE: " + log); } }
    
    void Warning(const std::string& log) { if (minLogLevel >= LOGWARNING) { Log(GetTime() + " TRACE: " + log); } }

    void Log(const std::string& log) 
    {
        if ((minLogLevel | LOGCONSOLE) == LOGCONSOLE) { std::cout << log << std::endl; }
        if ((minLogLevel | LOGFILE) == LOGFILE) { logFile << log << std::endl; logFile.flush(); }
    }

    std::string GetTime()
    {
        auto now = std::chrono::system_clock::now();
        std::time_t now_time_t = std::chrono::system_clock::to_time_t(now);
        std::tm local = *std::localtime(&now_time_t);
        std::ostringstream ss;
        ss << std::put_time(&local, "%m/%d/%Y %I:%M:%S %p");
        return ss.str();
    }
};
