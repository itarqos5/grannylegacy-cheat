#pragma once

#include <string>

namespace logger
{
    void SetLogPathFromModule(const char* moduleName, const char* fileName);
    void Write(const std::string& line);
}
