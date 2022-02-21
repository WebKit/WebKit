//
// Copyright 2019 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// system_utils_winuwp.cpp: Implementation of OS-specific functions for Windows UWP

#include "common/debug.h"
#include "system_utils.h"

#include <stdarg.h>
#include <windows.h>
#include <array>
#include <codecvt>
#include <locale>
#include <string>

namespace angle
{

bool SetEnvironmentVar(const char *variableName, const char *value)
{
    // Not supported for UWP
    return false;
}

std::string GetEnvironmentVar(const char *variableName)
{
    // Not supported for UWP
    return "";
}

class UwpLibrary : public Library
{
  public:
    UwpLibrary(const char *libraryName, SearchType searchType, std::string *errorOut)
    {
        std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;
        std::wstring wideBuffer = converter.from_bytes(libraryName);

        switch (searchType)
        {
            case SearchType::ModuleDir:
                if (errorOut)
                {
                    *errorOut = libraryName;
                }
                mModule = LoadPackagedLibrary(wideBuffer.c_str(), 0);
                break;
            case SearchType::SystemDir:
            case SearchType::AlreadyLoaded:
                // Not supported in UWP
                break;
        }
    }

    ~UwpLibrary() override
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
    return OpenSharedLibraryAndGetError(libraryName, searchType, nullptr);
}

Library *OpenSharedLibraryWithExtension(const char *libraryName, SearchType searchType)
{
    return OpenSharedLibraryWithExtensionAndGetError(libraryName, searchType, nullptr);
}

Library *OpenSharedLibraryAndGetError(const char *libraryName,
                                      SearchType searchType,
                                      std::string *errorOut)
{
    char buffer[MAX_PATH];
    int ret = snprintf(buffer, MAX_PATH, "%s.%s", libraryName, GetSharedLibraryExtension());

    if (ret > 0 && ret < MAX_PATH)
    {
        return OpenSharedLibraryWithExtensionAndGetError(buffer, searchType, errorOut);
    }
    else
    {
        fprintf(stderr, "Error loading shared library: 0x%x", ret);
        return nullptr;
    }
}

Library *OpenSharedLibraryWithExtensionAndGetError(const char *libraryName,
                                                   SearchType searchType,
                                                   std::string *errorOut)
{
    return new UwpLibrary(libraryName, searchType, errorOut);
}

namespace
{
class UwpPageFaultHandler : public PageFaultHandler
{
  public:
    UwpPageFaultHandler(PageFaultCallback callback) : PageFaultHandler(callback) {}
    ~UwpPageFaultHandler() override {}

    bool enable() override;
    bool disable() override;
};

bool UwpPageFaultHandler::disable()
{
    UNIMPLEMENTED();
    return true;
}

bool UwpPageFaultHandler::enable()
{
    UNIMPLEMENTED();
    return true;
}
}  // namespace

bool ProtectMemory(uintptr_t start, size_t size)
{
    UNIMPLEMENTED();
    return true;
}

bool UnprotectMemory(uintptr_t start, size_t size)
{
    UNIMPLEMENTED();
    return true;
}

size_t GetPageSize()
{
    UNIMPLEMENTED();
    return 4096;
}

PageFaultHandler *CreatePageFaultHandler(PageFaultCallback callback)
{
    return new UwpPageFaultHandler(callback);
}
}  // namespace angle
