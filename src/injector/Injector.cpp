#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <tlhelp32.h>

#include <filesystem>
#include <iostream>
#include <string>

namespace
{
    constexpr const char* kProcessName = "Granny Legacy.exe";
    constexpr const char* kDllName = "hid.dll";

    void PrintErrorRed(const std::string& message)
    {
        HANDLE console = GetStdHandle(STD_OUTPUT_HANDLE);
        CONSOLE_SCREEN_BUFFER_INFO info{};
        GetConsoleScreenBufferInfo(console, &info);
        SetConsoleTextAttribute(console, FOREGROUND_RED | FOREGROUND_INTENSITY);
        std::cout << "ERROR: " << message << "\n";
        SetConsoleTextAttribute(console, info.wAttributes);
    }

    DWORD FindProcessIdByName(const std::string& processName)
    {
        PROCESSENTRY32 entry{};
        entry.dwSize = sizeof(entry);

        HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
        if (snapshot == INVALID_HANDLE_VALUE)
        {
            return 0;
        }

        DWORD pid = 0;
        if (Process32First(snapshot, &entry))
        {
            do
            {
                if (_stricmp(entry.szExeFile, processName.c_str()) == 0)
                {
                    pid = entry.th32ProcessID;
                    break;
                }
            } while (Process32Next(snapshot, &entry));
        }

        CloseHandle(snapshot);
        return pid;
    }

    bool IsModuleLoaded(DWORD pid, const std::string& moduleName)
    {
        MODULEENTRY32 moduleEntry{};
        moduleEntry.dwSize = sizeof(moduleEntry);

        HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, pid);
        if (snapshot == INVALID_HANDLE_VALUE)
        {
            return false;
        }

        bool found = false;
        if (Module32First(snapshot, &moduleEntry))
        {
            do
            {
                if (_stricmp(moduleEntry.szModule, moduleName.c_str()) == 0)
                {
                    found = true;
                    break;
                }
            } while (Module32Next(snapshot, &moduleEntry));
        }

        CloseHandle(snapshot);
        return found;
    }

    std::filesystem::path GetProcessExePath(DWORD pid)
    {
        std::filesystem::path result;
        HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
        if (!process)
        {
            return result;
        }

        char buffer[MAX_PATH] = {};
        DWORD size = MAX_PATH;
        if (QueryFullProcessImageNameA(process, 0, buffer, &size))
        {
            result = buffer;
        }

        CloseHandle(process);
        return result;
    }

    bool LaunchGameFromCurrentDirectory()
    {
        const auto gamePath = std::filesystem::current_path() / kProcessName;
        if (!std::filesystem::exists(gamePath))
        {
            return false;
        }

        STARTUPINFOA si{};
        si.cb = sizeof(si);
        PROCESS_INFORMATION pi{};

        std::string cmdLine = "\"" + gamePath.string() + "\"";
        if (!CreateProcessA(
                nullptr,
                cmdLine.data(),
                nullptr,
                nullptr,
                FALSE,
                0,
                nullptr,
                gamePath.parent_path().string().c_str(),
                &si,
                &pi))
        {
            return false;
        }

        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);
        return true;
    }

    bool EnsureDllInGameDirectory(const std::filesystem::path& gameDir)
    {
        const auto sourceDll = std::filesystem::current_path() / kDllName;
        const auto targetDll = gameDir / kDllName;

        if (!std::filesystem::exists(sourceDll))
        {
            PrintErrorRed("hid.dll is missing next to injector.exe");
            return false;
        }

        if (!std::filesystem::exists(targetDll))
        {
            std::error_code ec;
            std::filesystem::copy_file(sourceDll, targetDll, std::filesystem::copy_options::overwrite_existing, ec);
            if (ec)
            {
                PrintErrorRed("Failed to copy hid.dll to game directory");
                return false;
            }
        }

        return true;
    }

    bool InjectDll(DWORD pid, const std::filesystem::path& dllPath)
    {
        HANDLE process = OpenProcess(PROCESS_CREATE_THREAD | PROCESS_QUERY_INFORMATION | PROCESS_VM_OPERATION | PROCESS_VM_WRITE | PROCESS_VM_READ, FALSE, pid);
        if (!process)
        {
            PrintErrorRed("Unable to open target process for injection");
            return false;
        }

        const std::string dllUtf8 = dllPath.string();
        void* remoteBuffer = VirtualAllocEx(process, nullptr, dllUtf8.size() + 1, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
        if (!remoteBuffer)
        {
            CloseHandle(process);
            PrintErrorRed("VirtualAllocEx failed");
            return false;
        }

        if (!WriteProcessMemory(process, remoteBuffer, dllUtf8.c_str(), dllUtf8.size() + 1, nullptr))
        {
            VirtualFreeEx(process, remoteBuffer, 0, MEM_RELEASE);
            CloseHandle(process);
            PrintErrorRed("WriteProcessMemory failed");
            return false;
        }

        FARPROC loadLibrary = GetProcAddress(GetModuleHandleA("kernel32.dll"), "LoadLibraryA");
        if (!loadLibrary)
        {
            VirtualFreeEx(process, remoteBuffer, 0, MEM_RELEASE);
            CloseHandle(process);
            PrintErrorRed("Failed to resolve LoadLibraryA");
            return false;
        }

        HANDLE remoteThread = CreateRemoteThread(process, nullptr, 0, reinterpret_cast<LPTHREAD_START_ROUTINE>(loadLibrary), remoteBuffer, 0, nullptr);
        if (!remoteThread)
        {
            VirtualFreeEx(process, remoteBuffer, 0, MEM_RELEASE);
            CloseHandle(process);
            PrintErrorRed("CreateRemoteThread failed");
            return false;
        }

        WaitForSingleObject(remoteThread, 5000);

        CloseHandle(remoteThread);
        VirtualFreeEx(process, remoteBuffer, 0, MEM_RELEASE);
        CloseHandle(process);
        return true;
    }
}

int main()
{
    std::cout << "Searching for Granny Legacy process..\n";

    DWORD pid = FindProcessIdByName(kProcessName);
    if (!pid)
    {
        if (!LaunchGameFromCurrentDirectory())
        {
            PrintErrorRed("Granny Legacy process was not found and launch failed");
            std::cout << "Press any key to exit . . .\n";
            std::cin.get();
            return 1;
        }

        for (int i = 0; i < 120; ++i)
        {
            Sleep(500);
            pid = FindProcessIdByName(kProcessName);
            if (pid)
            {
                break;
            }
        }
    }

    if (!pid)
    {
        PrintErrorRed("Granny Legacy process not found");
        std::cout << "Press any key to exit . . .\n";
        std::cin.get();
        return 1;
    }

    const auto exePath = GetProcessExePath(pid);
    if (exePath.empty())
    {
        PrintErrorRed("Unable to resolve game executable path");
        std::cout << "Press any key to exit . . .\n";
        std::cin.get();
        return 1;
    }

    const auto gameDir = exePath.parent_path();
    const auto dllPath = gameDir / kDllName;

    if (!EnsureDllInGameDirectory(gameDir))
    {
        std::cout << "Press any key to exit . . .\n";
        std::cin.get();
        return 1;
    }

    if (IsModuleLoaded(pid, kDllName))
    {
        std::cout << "hid.dll is already loaded.\n";
        std::cout << "Press any key to exit . . .\n";
        std::cin.get();
        return 0;
    }

    if (!InjectDll(pid, dllPath))
    {
        std::cout << "Press any key to exit . . .\n";
        std::cin.get();
        return 1;
    }

    std::cout << "Injection complete.\n";
    std::cout << "Press any key to exit . . .\n";
    std::cin.get();
    return 0;
}
