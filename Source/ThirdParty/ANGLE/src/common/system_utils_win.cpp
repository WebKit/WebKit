//
// Copyright 2014 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// system_utils_win.cpp: Implementation of OS-specific functions for Windows

#include "system_utils.h"

#include <stdarg.h>
#include <windows.h>
#include <array>
#include <vector>

namespace angle
{
std::string GetExecutablePath()
{
    std::array<char, MAX_PATH> executableFileBuf;
    DWORD executablePathLen = GetModuleFileNameA(nullptr, executableFileBuf.data(),
                                                 static_cast<DWORD>(executableFileBuf.size()));
    return (executablePathLen > 0 ? std::string(executableFileBuf.data()) : "");
}

std::string GetExecutableDirectory()
{
    std::string executablePath = GetExecutablePath();
    size_t lastPathSepLoc      = executablePath.find_last_of("\\/");
    return (lastPathSepLoc != std::string::npos) ? executablePath.substr(0, lastPathSepLoc) : "";
}

const char *GetSharedLibraryExtension()
{
    return "dll";
}

Optional<std::string> GetCWD()
{
    std::array<char, MAX_PATH> pathBuf;
    DWORD result = GetCurrentDirectoryA(static_cast<DWORD>(pathBuf.size()), pathBuf.data());
    if (result == 0)
    {
        return Optional<std::string>::Invalid();
    }
    return std::string(pathBuf.data());
}

bool SetCWD(const char *dirName)
{
    return (SetCurrentDirectoryA(dirName) == TRUE);
}

bool UnsetEnvironmentVar(const char *variableName)
{
    return (SetEnvironmentVariableA(variableName, nullptr) == TRUE);
}

bool SetEnvironmentVar(const char *variableName, const char *value)
{
    return (SetEnvironmentVariableA(variableName, value) == TRUE);
}

std::string GetEnvironmentVar(const char *variableName)
{
    std::array<char, MAX_PATH> oldValue;
    DWORD result =
        GetEnvironmentVariableA(variableName, oldValue.data(), static_cast<DWORD>(oldValue.size()));
    if (result == 0)
    {
        return std::string();
    }
    else
    {
        return std::string(oldValue.data());
    }
}

const char *GetPathSeparatorForEnvironmentVar()
{
    return ";";
}

double GetCurrentTime()
{
    LARGE_INTEGER frequency = {};
    QueryPerformanceFrequency(&frequency);

    LARGE_INTEGER curTime;
    QueryPerformanceCounter(&curTime);

    return static_cast<double>(curTime.QuadPart) / frequency.QuadPart;
}

class Win32Library : public Library
{
  public:
    Win32Library(const char *libraryName, SearchType searchType)
    {
        char buffer[MAX_PATH];
        int ret = snprintf(buffer, MAX_PATH, "%s.%s", libraryName, GetSharedLibraryExtension());
        if (ret > 0 && ret < MAX_PATH)
        {
            switch (searchType)
            {
                case SearchType::ApplicationDir:
                    mModule = LoadLibraryA(buffer);
                    break;
                case SearchType::SystemDir:
                    mModule = LoadLibraryExA(buffer, nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
                    break;
            }
        }
    }

    ~Win32Library() override
    {
        if (mModule)
        {
            FreeLibrary(mModule);
        }
    }

    void *getSymbol(const char *symbolName) override
    {
        if (!mModule)
        {
            return nullptr;
        }

        return reinterpret_cast<void *>(GetProcAddress(mModule, symbolName));
    }

    void *getNative() const override { return reinterpret_cast<void *>(mModule); }

  private:
    HMODULE mModule = nullptr;
};

Library *OpenSharedLibrary(const char *libraryName, SearchType searchType)
{
    return new Win32Library(libraryName, searchType);
}

bool IsDirectory(const char *filename)
{
    WIN32_FILE_ATTRIBUTE_DATA fileInformation;

    BOOL result = GetFileAttributesExA(filename, GetFileExInfoStandard, &fileInformation);
    if (result)
    {
        DWORD attribs = fileInformation.dwFileAttributes;
        return (attribs != INVALID_FILE_ATTRIBUTES) && ((attribs & FILE_ATTRIBUTE_DIRECTORY) > 0);
    }

    return false;
}

bool IsDebuggerAttached()
{
    return !!::IsDebuggerPresent();
}

void BreakDebugger()
{
    __debugbreak();
}

}  // namespace angle
