#pragma once

#include <cstdint>

namespace rva
{
    inline constexpr std::uintptr_t MainMenuUpdate = 0x276240;
    inline constexpr std::uintptr_t AIGrannyFixedUpdate = 0x253450;
    inline constexpr std::uintptr_t AIGrannyFrozenEnemy = 0x256240;

    // UnityEngine.GUI / UnityEngine.Input fallbacks from dump.cs
    inline constexpr std::uintptr_t GuiLabelRectString = 0xEF0700;
    inline constexpr std::uintptr_t GuiBoxRectString = 0xEEE5C0;
    inline constexpr std::uintptr_t GuiSetBackgroundColorInjected = 0xEF1050;
}
