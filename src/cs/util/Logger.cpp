#include "Logger.h"

#include <windows.h>
#include <fstream>
#include <mutex>

namespace
{
    std::string g_logPath;
    std::mutex g_logMutex;
}

void logger::SetLogPathFromModule(const char* moduleName, const char* fileName)
{
    char modulePath[MAX_PATH] = {};
    if (!GetModuleFileNameA(GetModuleHandleA(moduleName), modulePath, MAX_PATH))
    {
        g_logPath = fileName;
        return;
    }

    std::string path(modulePath);
    const auto slash = path.find_last_of("\\/");
    if (slash != std::string::npos)
    {
        path = path.substr(0, slash + 1);
    }
    path += fileName;
    g_logPath = path;
}

void logger::Write(const std::string& line)
{
    std::lock_guard<std::mutex> lock(g_logMutex);
    std::ofstream file(g_logPath.empty() ? "proxy_log.txt" : g_logPath, std::ios::app);
    if (!file.is_open())
    {
        return;
    }

    file << line << "\n";
}
