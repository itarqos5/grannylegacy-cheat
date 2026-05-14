#include "Bootstrap.h"

#include "../hooks/Hooks.h"
#include "../util/Logger.h"

#include <filesystem>

namespace
{
    std::filesystem::path GetModuleDirectory(const char* moduleName)
    {
        char modulePath[MAX_PATH] = {};
        if (!GetModuleFileNameA(GetModuleHandleA(moduleName), modulePath, MAX_PATH))
        {
            return std::filesystem::current_path();
        }

        std::filesystem::path full(modulePath);
        return full.parent_path();
    }

    void CopyResourceFolder()
    {
        const auto dllDir = GetModuleDirectory("hid.dll");
        const auto sourceResources = dllDir / "resources";
        const auto targetResources = std::filesystem::current_path() / "resources";

        if (!std::filesystem::exists(sourceResources))
        {
            logger::Warn(std::string("Resource source folder not found: ") + sourceResources.string());
            return;
        }

        std::error_code ec;
        std::filesystem::create_directories(targetResources, ec);
        if (ec)
        {
            logger::Error("Failed to create target resources folder");
            return;
        }

        for (const auto& entry : std::filesystem::recursive_directory_iterator(sourceResources))
        {
            const auto relative = std::filesystem::relative(entry.path(), sourceResources, ec);
            if (ec)
            {
                logger::Warn("Failed to compute relative path for resource entry");
                continue;
            }

            const auto targetPath = targetResources / relative;
            if (entry.is_directory())
            {
                std::filesystem::create_directories(targetPath, ec);
                continue;
            }

            std::filesystem::create_directories(targetPath.parent_path(), ec);
            std::filesystem::copy_file(entry.path(), targetPath, std::filesystem::copy_options::overwrite_existing, ec);
            if (ec)
            {
                logger::Warn(std::string("Failed to copy resource: ") + entry.path().string());
                ec.clear();
                continue;
            }

            logger::Info(std::string("Copied resource: ") + targetPath.string());
        }
    }
}

DWORD WINAPI bootstrap::RuntimeThread(LPVOID)
{
    logger::Initialize("hid.dll");
    logger::Info("Runtime thread started");
    logger::Info("Beginning resource synchronization");
    CopyResourceFolder();
    logger::Info("Resource synchronization complete");

    logger::Info("Starting hook installation");

    if (!hooks::Install())
    {
        logger::Error("Hook installation failed");
        return 1;
    }

    logger::Info("Hook installation complete");
    return 0;
}
