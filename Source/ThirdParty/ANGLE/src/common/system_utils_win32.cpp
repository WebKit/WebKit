//
// Copyright 2019 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// system_utils_win32.cpp: Implementation of OS-specific functions for Windows.

#include "system_utils.h"

#include <windows.h>
#include <array>

namespace angle
{
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

class Win32Library : public Library
{
  public:
    Win32Library(const char *libraryName, SearchType searchType)
    {
        switch (searchType)
        {
            case SearchType::ApplicationDir:
                mModule = LoadLibraryA(libraryName);
                break;
            case SearchType::SystemDir:
                mModule = LoadLibraryExA(libraryName, nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
                break;
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
    char buffer[MAX_PATH];
    int ret = snprintf(buffer, MAX_PATH, "%s.%s", libraryName, GetSharedLibraryExtension());
    if (ret > 0 && ret < MAX_PATH)
    {
        return new Win32Library(buffer, searchType);
    }
    else
    {
        fprintf(stderr, "Error loading shared library: 0x%x", ret);
        return nullptr;
    }
}

Library *OpenSharedLibraryWithExtension(const char *libraryName)
{
    return new Win32Library(libraryName, SearchType::SystemDir);
}
}  // namespace angle
