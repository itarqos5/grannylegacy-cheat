#include "MinHookDecls.h"

#ifndef HAVE_MINHOOK
extern "C"
{
    MH_STATUS WINAPI MH_Initialize(VOID)
    {
        return MH_ERROR_NOT_INITIALIZED;
    }

    MH_STATUS WINAPI MH_Uninitialize(VOID)
    {
        return MH_ERROR_NOT_INITIALIZED;
    }

    MH_STATUS WINAPI MH_CreateHook(LPVOID, LPVOID, LPVOID*)
    {
        return MH_ERROR_NOT_INITIALIZED;
    }

    MH_STATUS WINAPI MH_EnableHook(LPVOID)
    {
        return MH_ERROR_NOT_INITIALIZED;
    }

    MH_STATUS WINAPI MH_DisableHook(LPVOID)
    {
        return MH_ERROR_NOT_INITIALIZED;
    }
}
#endif
