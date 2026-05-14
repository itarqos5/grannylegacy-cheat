#include "Menu.h"

#include "../core/UnityTypes.h"
#include "../features/FreezeGranny.h"
#include "../util/Logger.h"

#include <windows.h>
#include <string>

namespace
{
    using IcallResolverFn = void* (*)(const char*);
    using Il2CppStringNewFn = void* (*)(const char*);

    using GUIBoxFn = void (*)(UnityRect, void*);
    using GUILabelFn = void (*)(UnityRect, void*);
    using GUIButtonFn = bool (*)(UnityRect, void*);
    using GUISetBackgroundColorFn = void (*)(UnityColor);

    IcallResolverFn g_resolveIcall = nullptr;
    Il2CppStringNewFn g_il2cppStringNew = nullptr;
    GUIBoxFn g_guiBox = nullptr;
    GUILabelFn g_guiLabel = nullptr;
    GUIButtonFn g_guiButton = nullptr;
    GUISetBackgroundColorFn g_setBackgroundColor = nullptr;

    bool g_showMenu = true;
    bool g_minimized = false;
    bool g_loggedGuiResolve = false;
    bool g_lastLoggedMinimized = false;
    bool g_disableGuiAfterException = false;
    bool g_loggedMissingStringNew = false;
    void* NewUnityString(const char* text)
    {
        if (!g_il2cppStringNew)
        {
            if (!g_loggedMissingStringNew)
            {
                logger::Warn("il2cpp_string_new is unresolved");
                g_loggedMissingStringNew = true;
            }
            return nullptr;
        }
        return g_il2cppStringNew(text);
    }

    template <typename T>
    void ResolveIcall(T& out, const char* signature)
    {
        if (out || !g_resolveIcall)
        {
            return;
        }

        out = reinterpret_cast<T>(g_resolveIcall(signature));
        if (out)
        {
            logger::Write(std::string("Resolved icall: ") + signature);
        }
    }

    template <typename T>
    void ResolveExport(T& out, HMODULE module, const char* exportName)
    {
        if (out || !module)
        {
            return;
        }

        out = reinterpret_cast<T>(GetProcAddress(module, exportName));
        if (out)
        {
            logger::Write(std::string("Resolved export fallback: ") + exportName);
        }
    }

    void EnsureUnityGuiFunctions()
    {
        if (g_disableGuiAfterException)
        {
            return;
        }

        HMODULE gameAssembly = GetModuleHandleA("GameAssembly.dll");
        HMODULE unityPlayer = GetModuleHandleA("UnityPlayer.dll");
        if (!gameAssembly)
        {
            logger::Warn("GameAssembly.dll not loaded while resolving GUI functions");
            return;
        }

        if (!g_resolveIcall)
        {
            g_resolveIcall = reinterpret_cast<IcallResolverFn>(GetProcAddress(gameAssembly, "il2cpp_resolve_icall"));
        }
        if (!g_il2cppStringNew)
        {
            g_il2cppStringNew = reinterpret_cast<Il2CppStringNewFn>(GetProcAddress(gameAssembly, "il2cpp_string_new"));
        }

        ResolveIcall(g_guiBox, "UnityEngine.GUI::Box(UnityEngine.Rect,System.String)");
        ResolveIcall(g_guiLabel, "UnityEngine.GUI::Label(UnityEngine.Rect,System.String)");
        ResolveIcall(g_guiButton, "UnityEngine.GUI::Button(UnityEngine.Rect,System.String)");
        ResolveIcall(g_setBackgroundColor, "UnityEngine.GUI::set_backgroundColor(UnityEngine.Color)");

        // Export fallback keeps the request contract: icall first, then exports.
        ResolveExport(g_guiBox, unityPlayer, "Unity_GUI_Box");
        ResolveExport(g_guiLabel, unityPlayer, "Unity_GUI_Label");

        if (!g_loggedGuiResolve)
        {
            logger::Info(std::string("GUI resolve status box=") + (g_guiBox ? "yes" : "no") +
                " label=" + (g_guiLabel ? "yes" : "no") +
                " button=" + (g_guiButton ? "yes" : "no") +
                " bgColor=" + (g_setBackgroundColor ? "yes" : "no"));
            g_loggedGuiResolve = true;
        }
    }

    void DrawExpandedMenu()
    {
        if (!g_guiBox || !g_guiLabel || !g_guiButton)
        {
            return;
        }

        if (g_setBackgroundColor)
        {
            g_setBackgroundColor(UnityColor{0.08f, 0.30f, 0.85f, 0.92f});
        }

        const UnityRect panel{15.0f, 15.0f, 320.0f, 170.0f};
        const UnityRect title{25.0f, 25.0f, 230.0f, 22.0f};
        const UnityRect minButton{285.0f, 22.0f, 40.0f, 24.0f};
        const UnityRect freezeLabel{25.0f, 70.0f, 180.0f, 22.0f};
        const UnityRect freezeToggle{210.0f, 67.0f, 115.0f, 28.0f};

        g_guiBox(panel, NewUnityString(""));
        g_guiLabel(title, NewUnityString("NEXUS MODLOADER"));

        if (g_guiButton(minButton, NewUnityString("-")))
        {
            g_minimized = true;
            logger::Info("UI minimize button pressed");
        }

        g_guiLabel(freezeLabel, NewUnityString("Freeze Granny"));
        if (g_guiButton(freezeToggle, NewUnityString(feature::freeze::IsEnabled() ? "ON" : "OFF")))
        {
            feature::freeze::Toggle();
            logger::Info(std::string("UI Freeze Granny toggled to ") + (feature::freeze::IsEnabled() ? "ON" : "OFF"));
        }
    }

    void DrawMinimizedMenu()
    {
        if (!g_guiBox || !g_guiLabel || !g_guiButton)
        {
            return;
        }

        if (g_setBackgroundColor)
        {
            g_setBackgroundColor(UnityColor{0.08f, 0.30f, 0.85f, 0.92f});
        }

        const UnityRect bar{15.0f, 15.0f, 320.0f, 42.0f};
        const UnityRect title{25.0f, 25.0f, 230.0f, 22.0f};
        const UnityRect plusButton{285.0f, 22.0f, 40.0f, 24.0f};

        g_guiBox(bar, NewUnityString(""));
        g_guiLabel(title, NewUnityString("NEXUS MODLOADER"));

        if (g_guiButton(plusButton, NewUnityString("+")))
        {
            g_minimized = false;
            logger::Info("UI restore button pressed");
        }
    }

    bool GuardedDrawOverlay()
    {
        __try
        {
            EnsureUnityGuiFunctions();
            if (g_minimized)
            {
                DrawMinimizedMenu();
            }
            else
            {
                DrawExpandedMenu();
            }
            return true;
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            return false;
        }
    }
}

namespace menu
{
    void TickToggleInput()
    {
        if ((GetAsyncKeyState(VK_INSERT) & 1) != 0)
        {
            g_minimized = !g_minimized;
            logger::Info(std::string("INSERT toggle changed minimized state to ") + (g_minimized ? "true" : "false"));
        }
    }

    void DrawOverlay()
    {
        if (g_disableGuiAfterException)
        {
            return;
        }

        if (!g_showMenu)
        {
            return;
        }

        if (!GuardedDrawOverlay())
        {
            logger::Error("Exception while drawing Unity GUI; disabling GUI drawing to prevent crash");
            g_disableGuiAfterException = true;
            return;
        }

        if (g_lastLoggedMinimized != g_minimized)
        {
            logger::Info(std::string("Menu minimized state changed: ") + (g_minimized ? "true" : "false"));
            g_lastLoggedMinimized = g_minimized;
        }
    }

    bool IsFreezeEnabled()
    {
        return feature::freeze::IsEnabled();
    }
}
