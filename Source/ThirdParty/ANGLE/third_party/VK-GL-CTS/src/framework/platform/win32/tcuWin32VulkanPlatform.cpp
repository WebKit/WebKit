/*-------------------------------------------------------------------------
 * drawElements Quality Program Tester Core
 * ----------------------------------------
 *
 * Copyright 2016 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 *//*!
 * \file
 * \brief Win32 Vulkan platform
 *//*--------------------------------------------------------------------*/

#include "tcuWin32VulkanPlatform.hpp"
#include "tcuWin32Window.hpp"

#include "tcuFormatUtil.hpp"
#include "tcuFunctionLibrary.hpp"
#include "tcuVector.hpp"

#include "vkWsiPlatform.hpp"

#include "deUniquePtr.hpp"
#include "deMemory.h"

#if !defined(DEQP_VULKAN_LIBRARY_PATH)
#ifdef CTS_USES_VULKANSC
#define DEQP_VULKAN_LIBRARY_PATH "vulkansc-1.dll"
#else
#define DEQP_VULKAN_LIBRARY_PATH "vulkan-1.dll"
#endif
#endif

namespace tcu
{
namespace win32
{

using de::MovePtr;
using de::UniquePtr;

DE_STATIC_ASSERT(sizeof(vk::pt::Win32InstanceHandle) == sizeof(HINSTANCE));
DE_STATIC_ASSERT(sizeof(vk::pt::Win32WindowHandle) == sizeof(HWND));

class VulkanWindow : public vk::wsi::Win32WindowInterface
{
public:
    VulkanWindow(MovePtr<win32::Window> window)
        : vk::wsi::Win32WindowInterface(vk::pt::Win32WindowHandle(window->getHandle()))
        , m_window(window)
    {
    }

    void setVisible(bool visible)
    {
        m_window->setVisible(visible);
    }

    bool setForeground(void)
    {
        return m_window->setForeground();
    }

    void resize(const UVec2 &newSize)
    {
        m_window->setSize((int)newSize.x(), (int)newSize.y());
    }

    void setMinimized(bool minimized)
    {
        m_window->setMinimized(minimized);
    }

private:
    UniquePtr<win32::Window> m_window;
};

class VulkanDisplay : public vk::wsi::Win32DisplayInterface
{
public:
    VulkanDisplay(HINSTANCE instance) : vk::wsi::Win32DisplayInterface(vk::pt::Win32InstanceHandle(instance))
    {
    }

    vk::wsi::Window *createWindow(const Maybe<UVec2> &initialSize) const
    {
        const HINSTANCE instance = (HINSTANCE)m_native.internal;
        const uint32_t width     = !initialSize ? 400 : initialSize->x();
        const uint32_t height    = !initialSize ? 300 : initialSize->y();

        return new VulkanWindow(MovePtr<win32::Window>(new win32::Window(instance, (int)width, (int)height)));
    }
};

class VulkanLibrary : public vk::Library
{
public:
    VulkanLibrary(const char *libraryPath)
        : m_library(libraryPath != nullptr ? libraryPath : DEQP_VULKAN_LIBRARY_PATH)
        , m_driver(m_library)
    {
    }

    const vk::PlatformInterface &getPlatformInterface(void) const
    {
        return m_driver;
    }
    const tcu::FunctionLibrary &getFunctionLibrary(void) const
    {
        return m_library;
    }

private:
    const tcu::DynamicFunctionLibrary m_library;
    const vk::PlatformDriver m_driver;
};

VulkanPlatform::VulkanPlatform(HINSTANCE instance) : m_instance(instance)
{
}

VulkanPlatform::~VulkanPlatform(void)
{
}

vk::Library *VulkanPlatform::createLibrary(LibraryType libraryType, const char *libraryPath) const
{
    switch (libraryType)
    {
    case LIBRARY_TYPE_VULKAN:
        return new VulkanLibrary(libraryPath);
    default:
        TCU_THROW(InternalError, "Unknown library type requested");
    }
}

ULONG getStringRegKey(const std::string &regKey, const std::string &strValueName, std::string &strValue)
{
    HKEY hKey;
    ULONG nError;
    CHAR szBuffer[512];
    DWORD dwBufferSize = sizeof(szBuffer);

    nError = RegOpenKeyExA(HKEY_LOCAL_MACHINE, regKey.c_str(), 0, KEY_READ, &hKey);

    if (ERROR_SUCCESS == nError)
        nError = RegQueryValueExA(hKey, strValueName.c_str(), 0, nullptr, (LPBYTE)szBuffer, &dwBufferSize);

    if (ERROR_SUCCESS == nError)
        strValue = szBuffer;

    return nError;
}

void getWindowsBits(std::ostream &dst)
{
#if defined(_WIN64)
    dst << "64"; // 64-bit programs run only on Win64
    return;
#elif defined(_WIN32)
    BOOL is64 = false;
    // 32-bit programs run on both 32-bit and 64-bit Windows.
    // Function is defined from XP SP2 onwards, so we don't need to
    // check if it exists.
    if (IsWow64Process(GetCurrentProcess(), &is64))
    {
        if (is64)
            dst << "64";
        else
            dst << "32";
        return;
    }
#endif
#if !defined(_WIN64)
    // IsWow64Process returns failure or neither of
    // _WIN64 or _WIN32 is defined
    dst << "Unknown";
#endif
}

void getOSNameFromRegistry(std::ostream &dst)
{
    const char *keypath     = "SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion";
    std::string productname = "Unknown";
    std::string releaseid   = "Unknown";
    std::string optional;

    getStringRegKey(keypath, "ProductName", productname);
    getStringRegKey(keypath, "ReleaseId", releaseid);

    getWindowsBits(dst);

    dst << " bit Windows Product: " << productname << ", Release: " << releaseid;

    if (ERROR_SUCCESS == getStringRegKey(keypath, "EditionID", optional))
    {
        dst << ", Edition: " << optional;
        if (ERROR_SUCCESS == getStringRegKey(keypath, "EditionSubstring", optional) && optional.length() > 0)
            dst << " " << optional;
    }
}

void getOSVersionFromDLL(std::ostream &dst)
{
    DWORD buffer_size = GetFileVersionInfoSize(("kernel32.dll"), nullptr);
    char *buffer      = 0;

    if (buffer_size != 0)
    {
        buffer = new char[buffer_size];
        if (buffer != 0)
        {
            if (GetFileVersionInfo("kernel32.dll", 0, buffer_size, buffer))
            {
                VS_FIXEDFILEINFO *version = nullptr;
                UINT version_len          = 0;

                if (VerQueryValue(buffer, "\\", (LPVOID *)&version, &version_len))
                {
                    dst << ", DLL Version: " << HIWORD(version->dwProductVersionMS) << "."
                        << LOWORD(version->dwProductVersionMS) << ", DLL Build: " << HIWORD(version->dwProductVersionLS)
                        << "." << LOWORD(version->dwProductVersionLS);
                }
            }
            delete[] buffer;
        }
    }
}

// Old windows version query APIs lie about the version number. There's no replacement
// API, and instead applications are supposed to queriy about capabilities instead of
// relying on operating system version numbers.
//
// Since we want to actually know the version number for debugging purposes, we need
// to use roundabout ways to fetch the information.
//
// The registry contains some useful strings, which we print out if the keys
// are available. The current official way to get version number is to look at a
// system DLL file and read its version number, so we do that too, in case the
// registry becomes unreliable in the future.
//
// If the DLL method fails, we simply don't print out anything about it.
// The minimal output from this function is "Windows Product: Unknown, Release: Unknown"
static void getOSInfo(std::ostream &dst)
{
    getOSNameFromRegistry(dst);
    getOSVersionFromDLL(dst);
}

const char *getProcessorArchitectureName(WORD arch)
{
    switch (arch)
    {
    case PROCESSOR_ARCHITECTURE_AMD64:
        return "AMD64";
    case PROCESSOR_ARCHITECTURE_ARM:
        return "ARM";
    case PROCESSOR_ARCHITECTURE_IA64:
        return "IA64";
    case PROCESSOR_ARCHITECTURE_INTEL:
        return "INTEL";
    case PROCESSOR_ARCHITECTURE_UNKNOWN:
        return "UNKNOWN";
    default:
        return nullptr;
    }
}

static void getProcessorInfo(std::ostream &dst)
{
    SYSTEM_INFO sysInfo;

    deMemset(&sysInfo, 0, sizeof(sysInfo));
    GetSystemInfo(&sysInfo);

    dst << "arch ";
    {
        const char *const archName = getProcessorArchitectureName(sysInfo.wProcessorArchitecture);

        if (archName)
            dst << archName;
        else
            dst << tcu::toHex(sysInfo.wProcessorArchitecture);
    }

    dst << ", level " << tcu::toHex(sysInfo.wProcessorLevel) << ", revision " << tcu::toHex(sysInfo.wProcessorRevision);
}

void VulkanPlatform::describePlatform(std::ostream &dst) const
{
    dst << "OS: ";
    getOSInfo(dst);
    dst << "\n";

    dst << "CPU: ";
    getProcessorInfo(dst);
    dst << "\n";
}

vk::wsi::Display *VulkanPlatform::createWsiDisplay(vk::wsi::Type wsiType) const
{
    if (wsiType != vk::wsi::TYPE_WIN32)
        TCU_THROW(NotSupportedError, "WSI type not supported");

    return new VulkanDisplay(m_instance);
}

bool VulkanPlatform::hasDisplay(vk::wsi::Type wsiType) const
{
    if (wsiType != vk::wsi::TYPE_WIN32)
        return false;

    return true;
}

} // namespace win32
} // namespace tcu
