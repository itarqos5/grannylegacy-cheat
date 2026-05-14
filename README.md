# grannylegacy-cheat

![Language](https://img.shields.io/badge/Language-C%2B%2B-blue.svg)
![Build](https://img.shields.io/badge/Build-MSVC%20x64-orange.svg)
![Target](https://img.shields.io/badge/Target-Unity%20IL2CPP-green.svg)
![License](https://img.shields.io/badge/License-MIT-lightgrey.svg)

Internal Granny Legacy mod framework using a hid.dll proxy entrypoint, MinHook detours, and IL2CPP/Unity IMGUI calls for runtime feature rendering.

## Technical Overview

- Proxy layer: hijacks hid.dll load and forwards exports to the system hid.dll.
- Runtime hook layer: installs detours for RVA targets in GameAssembly.dll.
- UI layer: draws an in-game blue menu panel using Unity GUI calls.
- Feature layer: includes a beta Freeze Granny control to validate mapping behavior.
- Injector layer: helper executable that finds/starts the game and injects hid.dll when needed.

## Current Hook Targets

- MainMenu.Update: 0x276240
- AI_Granny.FixedUpdate: 0x253450
- AI_Granny.FrozenEnemy: 0x256240

## Menu Behavior

- Title: NEXUS MODLOADER
- Position: top-left corner
- Style: blue background panel
- Minimize button: `-` in expanded mode
- Plus button: `+` in minimized mode
- INSERT key: toggles minimized/expanded state
- Container option: Freeze Granny toggle (beta)

## Project Structure

```text
.
├── CMakeLists.txt
├── build.bat
├── build.sh
├── src/
│   ├── cs/
│   │   ├── Main.cpp
│   │   ├── core/
│   │   ├── hooks/
│   │   ├── features/
│   │   ├── ui/
│   │   └── util/
│   ├── injector/
│   │   └── Injector.cpp
│   └── resources/
│       ├── mappings.json
│       └── flagged_methods.json
├── mapping/
│   ├── dump.cs
│   └── scripts/
└── dist/
	├── hid.dll
	├── inject.exe
	└── resources/
```

## Build

### Windows (recommended)

Use Visual Studio 2022 x64 Native Tools through:

```cmd
build.bat Release
```

This will:

1. Configure CMake for VS 2022 x64.
2. Build hid.dll and inject.exe.
3. Package outputs into dist/ including resources.

### build.sh

`build.sh` provides a best-effort clang-based path for compatible Windows-like shells and will otherwise exit with a clear unsupported message.

## Injector Flow

1. Prints `Searching for Granny Legacy process..`.
2. Searches for `Granny Legacy.exe` process.
3. If missing, attempts to start the game from current directory.
4. Ensures `hid.dll` is present in the game directory.
5. Skips injection if `hid.dll` is already loaded.
6. Injects with `LoadLibraryA` when needed.
7. Displays `Press any key to exit . . .` after success/failure.
8. Failure states print `ERROR` in red.

## Notes

- The Freeze Granny option is beta and intended as a mapping/hook validation control.
- Windows-only runtime behavior is expected for proxying and injection.

## Disclaimer

This project is intended for educational and reverse engineering research use only. Ensure usage complies with the game terms and applicable laws.