#include "Bootstrap.h"

#include "../hooks/Hooks.h"
#include "../util/Logger.h"

DWORD WINAPI bootstrap::RuntimeThread(LPVOID)
{
    logger::SetLogPathFromModule("hid.dll", "proxy_log.txt");
    logger::Write("Runtime thread started");

    if (!hooks::Install())
    {
        logger::Write("Hook installation failed");
        return 1;
    }

    logger::Write("Hook installation complete");
    return 0;
}
