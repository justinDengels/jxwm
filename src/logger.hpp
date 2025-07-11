#pragma once 

#include <string>
#include <fstream>

namespace JLogger
{
    enum { LOGCONSOLE = 1, LOGFILE, LOGBOTH };
    enum { LOGTRACE, LOGINFO, LOGERROR, LOGWARNING };
    static int logType;
    static int minLogLevel;
    static std::fstream logFile;

    void SetLogType(int type);
    void SetLogLevel(int level);
    void SetLogPath(const std::string& name);

    void Trace(const std::string& log);
    void Info(const std::string& log);
    void Error(const std::string& log);
    void Warning(const std::string& log);

    static void Log(const std::string& log);
    static std::string GetTime();
};
