#include "Hooks.h"

#include "../core/MinHookDecls.h"
#include "../core/Rva.h"
#include "../features/FreezeGranny.h"
#include "../ui/Menu.h"
#include "../util/Logger.h"

#include <windows.h>
#include <sstream>
#include <string>

namespace
{
    using MainMenuUpdateFn = void (*)(void*);
    using AIGrannyFixedUpdateFn = void (*)(void*);
    using AIGrannyFrozenEnemyFn = void (*)(void*);

    MainMenuUpdateFn g_originalMainMenuUpdate = nullptr;
    AIGrannyFixedUpdateFn g_originalAIGrannyFixedUpdate = nullptr;
    AIGrannyFrozenEnemyFn g_originalAIGrannyFrozenEnemy = nullptr;
    bool g_loggedMainMenuEntry = false;
    bool g_loggedAIEntry = false;
    bool g_lastFreezeState = false;

    std::uintptr_t ResolveAbsolute(std::uintptr_t relativeRva)
    {
        HMODULE gameAssembly = GetModuleHandleA("GameAssembly.dll");
        if (!gameAssembly)
        {
            return 0;
        }
        return reinterpret_cast<std::uintptr_t>(gameAssembly) + relativeRva;
    }

    bool GuardedMainMenuCall(void* self)
    {
        __try
        {
            if (g_originalMainMenuUpdate)
            {
                g_originalMainMenuUpdate(self);
            }
            return true;
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            return false;
        }
    }

    bool GuardedMenuDraw()
    {
        __try
        {
            menu::TickToggleInput();
            menu::DrawOverlay();
            return true;
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            return false;
        }
    }

    bool GuardedAIGrannyFixedUpdateCall(void* self)
    {
        __try
        {
            if (feature::freeze::IsEnabled())
            {
                if (g_originalAIGrannyFrozenEnemy)
                {
                    g_originalAIGrannyFrozenEnemy(self);
                }
                return true;
            }

            if (g_originalAIGrannyFixedUpdate)
            {
                g_originalAIGrannyFixedUpdate(self);
            }

            return true;
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            return false;
        }
    }

    void __fastcall hkMainMenuUpdate(void* self)
    {
        if (!g_loggedMainMenuEntry)
        {
            logger::Info("hkMainMenuUpdate entered for the first time");
            g_loggedMainMenuEntry = true;
        }

        if (!GuardedMenuDraw())
        {
            logger::Error("Exception while drawing menu in hkMainMenuUpdate");
        }

        if (!GuardedMainMenuCall(self))
        {
            logger::Error("Exception in original MainMenu.Update");
        }
    }

    void __fastcall hkAIGrannyFixedUpdate(void* self)
    {
        if (!g_loggedAIEntry)
        {
            logger::Info("hkAIGrannyFixedUpdate entered for the first time");
            g_loggedAIEntry = true;
        }

        const bool freezeEnabled = feature::freeze::IsEnabled();
        if (freezeEnabled != g_lastFreezeState)
        {
            logger::Info(std::string("Freeze Granny state changed: ") + (freezeEnabled ? "ENABLED" : "DISABLED"));
            g_lastFreezeState = freezeEnabled;
        }

        if (freezeEnabled && !g_originalAIGrannyFrozenEnemy)
        {
            logger::Warn("Freeze enabled but AI_Granny.FrozenEnemy pointer is null");
        }

        if (!freezeEnabled && !g_originalAIGrannyFixedUpdate)
        {
            logger::Warn("Original AI_Granny.FixedUpdate pointer is null");
        }

        if (!GuardedAIGrannyFixedUpdateCall(self))
        {
            logger::Error("Exception inside hkAIGrannyFixedUpdate");
        }
    }

    bool InstallHook(std::uintptr_t absoluteTarget, void* detour, void** original, const char* name)
    {
        if (!absoluteTarget)
        {
            logger::Write(std::string("Cannot install hook for ") + name + ": target address is null");
            return false;
        }

        MH_STATUS status = MH_CreateHook(reinterpret_cast<void*>(absoluteTarget), detour, reinterpret_cast<void**>(original));
        if (status != MH_OK)
        {
            logger::Error(std::string("MH_CreateHook failed for ") + name + " status=" + std::to_string(static_cast<int>(status)));
            return false;
        }

        status = MH_EnableHook(reinterpret_cast<void*>(absoluteTarget));
        if (status != MH_OK)
        {
            logger::Error(std::string("MH_EnableHook failed for ") + name + " status=" + std::to_string(static_cast<int>(status)));
            return false;
        }

        logger::Info(std::string("Hook enabled: ") + name);
        return true;
    }
}

bool hooks::Install()
{
    logger::Info("Waiting for GameAssembly.dll and UnityPlayer.dll");
    while (!GetModuleHandleA("GameAssembly.dll") || !GetModuleHandleA("UnityPlayer.dll"))
    {
        Sleep(250);
    }
    logger::Info("Required modules detected");

#ifndef HAVE_MINHOOK
    logger::Error("Real MinHook backend is missing (libMinHook-x64.lib not found). Hooks are disabled to prevent crashes.");
    return false;
#endif

    const MH_STATUS initStatus = MH_Initialize();
    if (initStatus != MH_OK)
    {
        logger::Error(std::string("MH_Initialize failed with status: ") + std::to_string(static_cast<int>(initStatus)));
        return false;
    }
    logger::Info("MH_Initialize succeeded");

    const std::uintptr_t mainMenuUpdate = ResolveAbsolute(rva::MainMenuUpdate);
    const std::uintptr_t aiGrannyFixedUpdate = ResolveAbsolute(rva::AIGrannyFixedUpdate);
    const std::uintptr_t aiGrannyFrozenEnemy = ResolveAbsolute(rva::AIGrannyFrozenEnemy);

    {
        std::ostringstream oss;
        oss << "Resolved MainMenu.Update address: 0x" << std::hex << mainMenuUpdate;
        logger::Info(oss.str());
    }
    {
        std::ostringstream oss;
        oss << "Resolved AI_Granny.FixedUpdate address: 0x" << std::hex << aiGrannyFixedUpdate;
        logger::Info(oss.str());
    }
    {
        std::ostringstream oss;
        oss << "Resolved AI_Granny.FrozenEnemy address: 0x" << std::hex << aiGrannyFrozenEnemy;
        logger::Info(oss.str());
    }

    g_originalAIGrannyFrozenEnemy = reinterpret_cast<AIGrannyFrozenEnemyFn>(aiGrannyFrozenEnemy);

    bool ok = true;
    ok = InstallHook(mainMenuUpdate, reinterpret_cast<void*>(&hkMainMenuUpdate), reinterpret_cast<void**>(&g_originalMainMenuUpdate), "MainMenu.Update") && ok;
    ok = InstallHook(aiGrannyFixedUpdate, reinterpret_cast<void*>(&hkAIGrannyFixedUpdate), reinterpret_cast<void**>(&g_originalAIGrannyFixedUpdate), "AI_Granny.FixedUpdate") && ok;

    return ok;
}

void hooks::Shutdown()
{
    logger::Warn("Shutting down hooks");
    MH_Uninitialize();
}
