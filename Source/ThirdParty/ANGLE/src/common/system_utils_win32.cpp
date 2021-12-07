//
// Copyright 2019 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// system_utils_win32.cpp: Implementation of OS-specific functions for Windows.

#include "common/FastVector.h"
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
    FastVector<char, MAX_PATH> value;

    DWORD result;

    // First get the length of the variable, including the null terminator
    result = GetEnvironmentVariableA(variableName, nullptr, 0);

    // Zero means the variable was not found, so return now.
    if (result == 0)
    {
        return std::string();
    }

    // Now size the vector to fit the data, and read the environment variable.
    value.resize(result, 0);
    result = GetEnvironmentVariableA(variableName, value.data(), result);

    return std::string(value.data());
}

class Win32Library : public Library
{
  public:
    Win32Library(const char *libraryName, SearchType searchType)
    {
        switch (searchType)
        {
            case SearchType::ModuleDir:
            {
                std::string moduleRelativePath = ConcatenatePath(GetModuleDirectory(), libraryName);
                mModule                        = LoadLibraryA(moduleRelativePath.c_str());
                break;
            }

            case SearchType::SystemDir:
                mModule = LoadLibraryExA(libraryName, nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
                break;
            case SearchType::AlreadyLoaded:
                mModule = GetModuleHandleA(libraryName);
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
            fprintf(stderr, "Module was not loaded\n");
            return nullptr;
        }

        return reinterpret_cast<void *>(GetProcAddress(mModule, symbolName));
    }

    void *getNative() const override { return reinterpret_cast<void *>(mModule); }

    std::string getPath() const override
    {
        if (!mModule)
        {
            return "";
        }

        std::array<char, MAX_PATH> buffer;
        if (GetModuleFileNameA(mModule, buffer.data(), buffer.size()) == 0)
        {
            return "";
        }

        return std::string(buffer.data());
    }

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

Library *OpenSharedLibraryWithExtension(const char *libraryName, SearchType searchType)
{
    return new Win32Library(libraryName, searchType);
}
}  // namespace angle
