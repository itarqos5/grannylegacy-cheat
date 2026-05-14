#pragma once

#include <string>

namespace logger
{
    void Initialize(const char* moduleName);
    void Shutdown();

    void Info(const std::string& line);
    void Warn(const std::string& line);
    void Error(const std::string& line);

    // Backward compatibility alias.
    void Write(const std::string& line);
}
