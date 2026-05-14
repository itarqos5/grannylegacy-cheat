#include "Logger.h"

#include <windows.h>

#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <sstream>
#include <cstring>

namespace
{
    std::string g_logPath;
    std::mutex g_logMutex;
    bool g_consoleReady = false;

    std::string ResolveDllDirectory(const char* moduleName)
    {
        char modulePath[MAX_PATH] = {};
        if (!GetModuleFileNameA(GetModuleHandleA(moduleName), modulePath, MAX_PATH))
        {
            return std::string(".");
        }

        std::string path(modulePath);
        const auto slash = path.find_last_of("\\/");
        if (slash != std::string::npos)
        {
            path = path.substr(0, slash + 1);
        }
        return path;
    }

    std::string NowStamp()
    {
        SYSTEMTIME st{};
        GetLocalTime(&st);

        std::ostringstream oss;
        oss << std::setfill('0')
            << std::setw(2) << st.wHour << ":"
            << std::setw(2) << st.wMinute << ":"
            << std::setw(2) << st.wSecond << "."
            << std::setw(3) << st.wMilliseconds;
        return oss.str();
    }

    void EnsureConsole()
    {
        if (g_consoleReady)
        {
            return;
        }

        if (!GetConsoleWindow())
        {
            AllocConsole();
        }

        FILE* stream = nullptr;
        freopen_s(&stream, "CONOUT$", "w", stdout);
        freopen_s(&stream, "CONOUT$", "w", stderr);
        freopen_s(&stream, "CONIN$", "r", stdin);

        SetConsoleTitleA("Nexus Modloader Logger");
        g_consoleReady = true;
    }

    WORD ColorForLevel(const char* level)
    {
        if (strcmp(level, "WARN") == 0)
        {
            return FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_INTENSITY;
        }
        if (strcmp(level, "ERROR") == 0)
        {
            return FOREGROUND_RED | FOREGROUND_INTENSITY;
        }
        return FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE;
    }

    void LogLine(const char* level, const std::string& line)
    {
        std::lock_guard<std::mutex> lock(g_logMutex);

        const std::string stamped = "[" + NowStamp() + "] [" + level + "] " + line;

        std::ofstream file(g_logPath.empty() ? "proxy_log.log" : g_logPath, std::ios::app);
        if (file.is_open())
        {
            file << stamped << "\n";
        }

        EnsureConsole();
        HANDLE console = GetStdHandle(STD_OUTPUT_HANDLE);
        CONSOLE_SCREEN_BUFFER_INFO info{};
        GetConsoleScreenBufferInfo(console, &info);
        SetConsoleTextAttribute(console, ColorForLevel(level));
        std::cout << stamped << "\n";
        SetConsoleTextAttribute(console, info.wAttributes);
    }
}

void logger::Initialize(const char* moduleName)
{
    const std::string dllDir = ResolveDllDirectory(moduleName);
    std::filesystem::path logDir = std::filesystem::path(dllDir) / "logs";
    std::error_code ec;
    std::filesystem::create_directories(logDir, ec);

    g_logPath = (logDir / "proxy_log.log").string();
    EnsureConsole();
    Info("Logger initialized");
}

void logger::Shutdown()
{
    Info("Logger shutdown requested. Waiting 5 seconds before closing console.");
    Sleep(5000);
    FreeConsole();
    g_consoleReady = false;
}

void logger::Info(const std::string& line)
{
    LogLine("INFO", line);
}

void logger::Warn(const std::string& line)
{
    LogLine("WARN", line);
}

void logger::Error(const std::string& line)
{
    LogLine("ERROR", line);
}

void logger::Write(const std::string& line)
{
    Info(line);
}
