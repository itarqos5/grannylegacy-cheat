#include "Hooks.h"

#include "../core/MinHookDecls.h"
#include "../core/Rva.h"
#include "../features/FreezeGranny.h"
#include "../ui/Menu.h"
#include "../util/Logger.h"

#include <windows.h>
#include <string>

namespace
{
    using MainMenuUpdateFn = void (*)(void*);
    using AIGrannyFixedUpdateFn = void (*)(void*);
    using AIGrannyFrozenEnemyFn = void (*)(void*);

    MainMenuUpdateFn g_originalMainMenuUpdate = nullptr;
    AIGrannyFixedUpdateFn g_originalAIGrannyFixedUpdate = nullptr;
    AIGrannyFrozenEnemyFn g_originalAIGrannyFrozenEnemy = nullptr;

    std::uintptr_t ResolveAbsolute(std::uintptr_t relativeRva)
    {
        HMODULE gameAssembly = GetModuleHandleA("GameAssembly.dll");
        if (!gameAssembly)
        {
            return 0;
        }
        return reinterpret_cast<std::uintptr_t>(gameAssembly) + relativeRva;
    }

    void __fastcall hkMainMenuUpdate(void* self)
    {
        menu::TickToggleInput();
        menu::DrawOverlay();

        if (g_originalMainMenuUpdate)
        {
            g_originalMainMenuUpdate(self);
        }
    }

    void __fastcall hkAIGrannyFixedUpdate(void* self)
    {
        if (feature::freeze::IsEnabled())
        {
            if (g_originalAIGrannyFrozenEnemy)
            {
                g_originalAIGrannyFrozenEnemy(self);
            }
            return;
        }

        if (g_originalAIGrannyFixedUpdate)
        {
            g_originalAIGrannyFixedUpdate(self);
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
            logger::Write(std::string("MH_CreateHook failed for ") + name);
            return false;
        }

        status = MH_EnableHook(reinterpret_cast<void*>(absoluteTarget));
        if (status != MH_OK)
        {
            logger::Write(std::string("MH_EnableHook failed for ") + name);
            return false;
        }

        logger::Write(std::string("Hook enabled: ") + name);
        return true;
    }
}

bool hooks::Install()
{
    while (!GetModuleHandleA("GameAssembly.dll") || !GetModuleHandleA("UnityPlayer.dll"))
    {
        Sleep(250);
    }

    const MH_STATUS initStatus = MH_Initialize();
    if (initStatus != MH_OK)
    {
        logger::Write(std::string("MH_Initialize failed with status: ") + std::to_string(static_cast<int>(initStatus)));
        return false;
    }

    const std::uintptr_t mainMenuUpdate = ResolveAbsolute(rva::MainMenuUpdate);
    const std::uintptr_t aiGrannyFixedUpdate = ResolveAbsolute(rva::AIGrannyFixedUpdate);
    const std::uintptr_t aiGrannyFrozenEnemy = ResolveAbsolute(rva::AIGrannyFrozenEnemy);

    g_originalAIGrannyFrozenEnemy = reinterpret_cast<AIGrannyFrozenEnemyFn>(aiGrannyFrozenEnemy);

    bool ok = true;
    ok = InstallHook(mainMenuUpdate, reinterpret_cast<void*>(&hkMainMenuUpdate), reinterpret_cast<void**>(&g_originalMainMenuUpdate), "MainMenu.Update") && ok;
    ok = InstallHook(aiGrannyFixedUpdate, reinterpret_cast<void*>(&hkAIGrannyFixedUpdate), reinterpret_cast<void**>(&g_originalAIGrannyFixedUpdate), "AI_Granny.FixedUpdate") && ok;

    return ok;
}

void hooks::Shutdown()
{
    MH_Uninitialize();
}
