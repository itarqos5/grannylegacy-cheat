#include "MinHookDecls.h"

#ifndef HAVE_MINHOOK
#include <windows.h>

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <vector>

namespace
{
    struct HookEntry
    {
        LPVOID target = nullptr;
        LPVOID detour = nullptr;
        LPVOID trampoline = nullptr;
        bool enabled = false;
        std::uint8_t original[16]{};
        std::size_t patchLength = 16;
    };

    bool g_initialized = false;
    std::vector<HookEntry> g_hooks;

    HookEntry* FindHook(LPVOID target)
    {
        auto it = std::find_if(g_hooks.begin(), g_hooks.end(), [target](const HookEntry& entry)
        {
            return entry.target == target;
        });
        return it == g_hooks.end() ? nullptr : &(*it);
    }

    bool WriteAbsoluteJump(void* address, void* destination)
    {
        std::uint8_t patch[16] = {};
        patch[0] = 0x48; // mov rax, imm64
        patch[1] = 0xB8;

        const auto dst = reinterpret_cast<std::uint64_t>(destination);
        std::memcpy(&patch[2], &dst, sizeof(dst));

        patch[10] = 0xFF; // jmp rax
        patch[11] = 0xE0;

        for (std::size_t i = 12; i < 16; ++i)
        {
            patch[i] = 0x90; // nop
        }

        DWORD oldProtect = 0;
        if (!VirtualProtect(address, sizeof(patch), PAGE_EXECUTE_READWRITE, &oldProtect))
        {
            return false;
        }

        std::memcpy(address, patch, sizeof(patch));
        FlushInstructionCache(GetCurrentProcess(), address, sizeof(patch));
        DWORD ignored = 0;
        VirtualProtect(address, sizeof(patch), oldProtect, &ignored);
        return true;
    }

    LPVOID CreateTrampoline(LPVOID target, const std::uint8_t* originalBytes, std::size_t patchLength)
    {
        constexpr std::size_t kTrampolineBytes = 32;
        auto* trampoline = static_cast<std::uint8_t*>(VirtualAlloc(nullptr, kTrampolineBytes, MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE));
        if (!trampoline)
        {
            return nullptr;
        }

        std::memcpy(trampoline, originalBytes, patchLength);

        auto* jumpBackLocation = trampoline + patchLength;
        auto* continuation = reinterpret_cast<std::uint8_t*>(target) + patchLength;
        if (!WriteAbsoluteJump(jumpBackLocation, continuation))
        {
            VirtualFree(trampoline, 0, MEM_RELEASE);
            return nullptr;
        }

        return trampoline;
    }
}

extern "C"
{
    MH_STATUS WINAPI MH_Initialize(VOID)
    {
        g_initialized = true;
        return MH_OK;
    }

    MH_STATUS WINAPI MH_Uninitialize(VOID)
    {
        if (!g_initialized)
        {
            return MH_ERROR_NOT_INITIALIZED;
        }

        for (auto& hook : g_hooks)
        {
            if (hook.enabled)
            {
                DWORD oldProtect = 0;
                if (VirtualProtect(hook.target, hook.patchLength, PAGE_EXECUTE_READWRITE, &oldProtect))
                {
                    std::memcpy(hook.target, hook.original, hook.patchLength);
                    FlushInstructionCache(GetCurrentProcess(), hook.target, hook.patchLength);
                    DWORD ignored = 0;
                    VirtualProtect(hook.target, hook.patchLength, oldProtect, &ignored);
                }
            }

            if (hook.trampoline)
            {
                VirtualFree(hook.trampoline, 0, MEM_RELEASE);
            }
        }

        g_hooks.clear();
        g_initialized = false;
        return MH_OK;
    }

    MH_STATUS WINAPI MH_CreateHook(LPVOID pTarget, LPVOID pDetour, LPVOID* ppOriginal)
    {
        if (!g_initialized)
        {
            return MH_ERROR_NOT_INITIALIZED;
        }
        if (!pTarget || !pDetour)
        {
            return MH_ERROR_NOT_EXECUTABLE;
        }
        if (FindHook(pTarget))
        {
            return MH_ERROR_ALREADY_CREATED;
        }

        HookEntry hook;
        hook.target = pTarget;
        hook.detour = pDetour;

        std::memcpy(hook.original, pTarget, hook.patchLength);
        hook.trampoline = CreateTrampoline(pTarget, hook.original, hook.patchLength);
        if (!hook.trampoline)
        {
            return MH_ERROR_MEMORY_ALLOC;
        }

        if (ppOriginal)
        {
            *ppOriginal = hook.trampoline;
        }

        g_hooks.push_back(hook);
        return MH_OK;
    }

    MH_STATUS WINAPI MH_EnableHook(LPVOID pTarget)
    {
        if (!g_initialized)
        {
            return MH_ERROR_NOT_INITIALIZED;
        }

        HookEntry* hook = FindHook(pTarget);
        if (!hook)
        {
            return MH_ERROR_NOT_CREATED;
        }
        if (hook->enabled)
        {
            return MH_ERROR_ENABLED;
        }

        if (!WriteAbsoluteJump(hook->target, hook->detour))
        {
            return MH_ERROR_MEMORY_PROTECT;
        }

        hook->enabled = true;
        return MH_OK;
    }

    MH_STATUS WINAPI MH_DisableHook(LPVOID pTarget)
    {
        if (!g_initialized)
        {
            return MH_ERROR_NOT_INITIALIZED;
        }

        HookEntry* hook = FindHook(pTarget);
        if (!hook)
        {
            return MH_ERROR_NOT_CREATED;
        }
        if (!hook->enabled)
        {
            return MH_ERROR_DISABLED;
        }

        DWORD oldProtect = 0;
        if (!VirtualProtect(hook->target, hook->patchLength, PAGE_EXECUTE_READWRITE, &oldProtect))
        {
            return MH_ERROR_MEMORY_PROTECT;
        }

        std::memcpy(hook->target, hook->original, hook->patchLength);
        FlushInstructionCache(GetCurrentProcess(), hook->target, hook->patchLength);

        DWORD ignored = 0;
        VirtualProtect(hook->target, hook->patchLength, oldProtect, &ignored);
        hook->enabled = false;
        return MH_OK;
    }
}
#endif
