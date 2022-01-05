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
    Win32Library(const char *libraryName, SearchType searchType, std::string *errorOut)
    {
        switch (searchType)
        {
            case SearchType::ModuleDir:
            {
                std::string moduleRelativePath = ConcatenatePath(GetModuleDirectory(), libraryName);
                if (errorOut)
                {
                    *errorOut = moduleRelativePath;
                }
                mModule = LoadLibraryA(moduleRelativePath.c_str());
                break;
            }

            case SearchType::SystemDir:
                if (errorOut)
                {
                    *errorOut = libraryName;
                }
                mModule = LoadLibraryExA(libraryName, nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
                break;
            case SearchType::AlreadyLoaded:
                if (errorOut)
                {
                    *errorOut = libraryName;
                }
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
        return new Win32Library(buffer, searchType, errorOut);
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
    return new Win32Library(libraryName, searchType, errorOut);
}

namespace
{
class Win32PageFaultHandler : public PageFaultHandler
{
  public:
    Win32PageFaultHandler(PageFaultCallback callback) : PageFaultHandler(callback) {}
    ~Win32PageFaultHandler() override {}

    bool enable() override;
    bool disable() override;

    LONG handle(PEXCEPTION_POINTERS pExceptionInfo);

  private:
    void *mVectoredExceptionHandler = nullptr;
};

Win32PageFaultHandler *gWin32PageFaultHandler = nullptr;
static LONG CALLBACK VectoredExceptionHandler(PEXCEPTION_POINTERS info)
{
    return gWin32PageFaultHandler->handle(info);
}

bool SetMemoryProtection(uintptr_t start, size_t size, DWORD protections)
{
    DWORD oldProtect;
    BOOL res = VirtualProtect(reinterpret_cast<LPVOID>(start), size, protections, &oldProtect);
    if (!res)
    {
        DWORD lastError = GetLastError();
        fprintf(stderr, "VirtualProtect failed: 0x%lx\n", lastError);
        return false;
    }

    return true;
}

LONG Win32PageFaultHandler::handle(PEXCEPTION_POINTERS info)
{
    bool found = false;

    if (info->ExceptionRecord->ExceptionCode == EXCEPTION_ACCESS_VIOLATION &&
        info->ExceptionRecord->NumberParameters >= 2 &&
        info->ExceptionRecord->ExceptionInformation[0] == 1)
    {
        found = mCallback(static_cast<uintptr_t>(info->ExceptionRecord->ExceptionInformation[1])) ==
                PageFaultHandlerRangeType::InRange;
    }

    if (found)
    {
        return EXCEPTION_CONTINUE_EXECUTION;
    }
    else
    {
        return EXCEPTION_CONTINUE_SEARCH;
    }
}

bool Win32PageFaultHandler::disable()
{
    if (mVectoredExceptionHandler)
    {
        ULONG res                 = RemoveVectoredExceptionHandler(mVectoredExceptionHandler);
        mVectoredExceptionHandler = nullptr;
        if (res == 0)
        {
            DWORD lastError = GetLastError();
            fprintf(stderr, "RemoveVectoredExceptionHandler failed: 0x%lx\n", lastError);
            return false;
        }
    }
    return true;
}

bool Win32PageFaultHandler::enable()
{
    if (mVectoredExceptionHandler)
    {
        return true;
    }

    PVECTORED_EXCEPTION_HANDLER handler =
        reinterpret_cast<PVECTORED_EXCEPTION_HANDLER>(&VectoredExceptionHandler);

    mVectoredExceptionHandler = AddVectoredExceptionHandler(1, handler);

    if (!mVectoredExceptionHandler)
    {
        DWORD lastError = GetLastError();
        fprintf(stderr, "AddVectoredExceptionHandler failed: 0x%lx\n", lastError);
        return false;
    }
    return true;
}
}  // namespace

// Set write protection
bool ProtectMemory(uintptr_t start, size_t size)
{
    return SetMemoryProtection(start, size, PAGE_READONLY);
}

// Allow reading and writing
bool UnprotectMemory(uintptr_t start, size_t size)
{
    return SetMemoryProtection(start, size, PAGE_READWRITE);
}

size_t GetPageSize()
{
    SYSTEM_INFO info;
    GetSystemInfo(&info);
    return static_cast<size_t>(info.dwPageSize);
}

PageFaultHandler *CreatePageFaultHandler(PageFaultCallback callback)
{
    gWin32PageFaultHandler = new Win32PageFaultHandler(callback);
    return gWin32PageFaultHandler;
}
}  // namespace angle
