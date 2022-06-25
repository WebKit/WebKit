
// Copyright 2019 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#include "GPUTestConfig.h"

#include <stdint.h>
#include <iostream>

#include "common/angleutils.h"
#include "common/debug.h"
#include "common/platform.h"
#include "common/string_utils.h"
#include "gpu_info_util/SystemInfo.h"

#if defined(ANGLE_PLATFORM_APPLE)
#    include "GPUTestConfig_mac.h"
#elif defined(ANGLE_PLATFORM_WINDOWS)
#    include <versionhelpers.h>
#endif

namespace angle
{

namespace
{

#if defined(ANGLE_PLATFORM_MACOS)
// Generic function call to get the OS version information from any platform
// defined below. This function will also cache the OS version info in static
// variables.
inline bool OperatingSystemVersionNumbers(int32_t *majorVersion, int32_t *minorVersion)
{
    static int32_t sSavedMajorVersion = -1;
    static int32_t sSavedMinorVersion = -1;
    bool ret                          = false;
    if (sSavedMajorVersion == -1 || sSavedMinorVersion == -1)
    {
        GetOperatingSystemVersionNumbers(&sSavedMajorVersion, &sSavedMinorVersion);
        *majorVersion = sSavedMajorVersion;
        *minorVersion = sSavedMinorVersion;
        ret           = true;
    }
    else
    {
        ret = true;
    }
    *majorVersion = sSavedMajorVersion;
    *minorVersion = sSavedMinorVersion;
    return ret;
}
#endif

// Check if the OS is any version of Windows
inline bool IsWin()
{
#if defined(ANGLE_PLATFORM_WINDOWS)
    return true;
#else
    return false;
#endif
}

// Check if the OS is Windows XP
inline bool IsWinXP()
{
#if defined(ANGLE_PLATFORM_WINDOWS)
    return IsWindowsXPOrGreater() && !IsWindowsVistaOrGreater();
#else
    return false;
#endif
}

// Check if the OS is Windows Vista
inline bool IsWinVista()
{
#if defined(ANGLE_PLATFORM_WINDOWS)
    return IsWindowsVistaOrGreater() && !IsWindows7OrGreater();
#else
    return false;
#endif
}

// Check if the OS is Windows 7
inline bool IsWin7()
{
#if defined(ANGLE_PLATFORM_WINDOWS)
    return IsWindows7OrGreater() && !IsWindows8OrGreater();
#else
    return false;
#endif
}

// Check if the OS is Windows 8
inline bool IsWin8()
{
#if defined(ANGLE_PLATFORM_WINDOWS)
    return IsWindows8OrGreater() && !IsWindows10OrGreater();
#else
    return false;
#endif
}

// Check if the OS is Windows 10
inline bool IsWin10()
{
#if defined(ANGLE_PLATFORM_WINDOWS)
    return IsWindows10OrGreater();
#else
    return false;
#endif
}

// Check if the OS is any version of iOS
inline bool IsIOS()
{
#if defined(ANGLE_PLATFORM_IOS)
    return true;
#else
    return false;
#endif
}

// Check if the OS is any version of OSX
inline bool IsMac()
{
#if defined(ANGLE_PLATFORM_MACOS)
    return true;
#else
    return false;
#endif
}

// Check if the OS is a specific major and minor version of OSX
inline bool IsMacVersion(const int32_t majorVersion, const int32_t minorVersion)
{
#if defined(ANGLE_PLATFORM_MACOS)
    int32_t currentMajorVersion = 0;
    int32_t currentMinorVersion = 0;
    if (OperatingSystemVersionNumbers(&currentMajorVersion, &currentMinorVersion))
    {
        if (currentMajorVersion == majorVersion && currentMinorVersion == minorVersion)
        {
            return true;
        }
    }
#endif
    return false;
}

// Check if the OS is OSX Leopard
inline bool IsMacLeopard()
{
    if (IsMacVersion(10, 5))
    {
        return true;
    }
    return false;
}

// Check if the OS is OSX Snow Leopard
inline bool IsMacSnowLeopard()
{
    if (IsMacVersion(10, 6))
    {
        return true;
    }
    return false;
}

// Check if the OS is OSX Lion
inline bool IsMacLion()
{
    if (IsMacVersion(10, 7))
    {
        return true;
    }
    return false;
}

// Check if the OS is OSX Mountain Lion
inline bool IsMacMountainLion()
{
    if (IsMacVersion(10, 8))
    {
        return true;
    }
    return false;
}

// Check if the OS is OSX Mavericks
inline bool IsMacMavericks()
{
    if (IsMacVersion(10, 9))
    {
        return true;
    }
    return false;
}

// Check if the OS is OSX Yosemite
inline bool IsMacYosemite()
{
    if (IsMacVersion(10, 10))
    {
        return true;
    }
    return false;
}

// Check if the OS is OSX El Capitan
inline bool IsMacElCapitan()
{
    if (IsMacVersion(10, 11))
    {
        return true;
    }
    return false;
}

// Check if the OS is OSX Sierra
inline bool IsMacSierra()
{
    if (IsMacVersion(10, 12))
    {
        return true;
    }
    return false;
}

// Check if the OS is OSX High Sierra
inline bool IsMacHighSierra()
{
    if (IsMacVersion(10, 13))
    {
        return true;
    }
    return false;
}

// Check if the OS is OSX Mojave
inline bool IsMacMojave()
{
    if (IsMacVersion(10, 14))
    {
        return true;
    }
    return false;
}

// Check if the OS is any version of Linux
inline bool IsLinux()
{
#if defined(ANGLE_PLATFORM_LINUX)
    return true;
#else
    return false;
#endif
}

// Check if the OS is any version of Android
inline bool IsAndroid()
{
#if defined(ANGLE_PLATFORM_ANDROID)
    return true;
#else
    return false;
#endif
}

// Generic function call to populate the SystemInfo struct. This function will
// also cache the SystemInfo struct for future calls. Returns false if the
// struct was not fully populated. Guaranteed to set sysInfo to a valid pointer
inline bool GetGPUTestSystemInfo(SystemInfo **sysInfo)
{
    static SystemInfo *sSystemInfo = nullptr;
    static bool sPopulated         = false;
    if (sSystemInfo == nullptr)
    {
        sSystemInfo = new SystemInfo;
        if (!GetSystemInfo(sSystemInfo))
        {
            std::cout << "Error populating SystemInfo." << std::endl;
        }
        else
        {
            // On dual-GPU Macs we want the active GPU to always appear to be the
            // high-performance GPU for tests.
            // We can call the generic GPU info collector which selects the
            // non-Intel GPU as the active one on dual-GPU machines.
            // See https://anglebug.com/3701.
            if (IsMac())
            {
                GetDualGPUInfo(sSystemInfo);
            }
            sPopulated = true;
        }
    }
    *sysInfo = sSystemInfo;
    ASSERT(*sysInfo != nullptr);
    return sPopulated;
}

// Get the active GPUDeviceInfo from the SystemInfo struct.
// Returns false if devInfo is not guaranteed to be set to the active device.
inline bool GetActiveGPU(GPUDeviceInfo **devInfo)
{
    SystemInfo *systemInfo = nullptr;
    GetGPUTestSystemInfo(&systemInfo);
    if (systemInfo->gpus.size() <= 0)
    {
        return false;
    }
    uint32_t index = systemInfo->activeGPUIndex;
    ASSERT(index < systemInfo->gpus.size());
    *devInfo = &(systemInfo->gpus[index]);
    return true;
}

// Get the vendor ID of the active GPU from the SystemInfo struct.
// Returns 0 if there is an error.
inline VendorID GetActiveGPUVendorID()
{
    GPUDeviceInfo *activeGPU = nullptr;
    if (GetActiveGPU(&activeGPU))
    {
        return activeGPU->vendorId;
    }
    else
    {
        return static_cast<VendorID>(0);
    }
}

// Get the device ID of the active GPU from the SystemInfo struct.
// Returns 0 if there is an error.
inline DeviceID GetActiveGPUDeviceID()
{
    GPUDeviceInfo *activeGPU = nullptr;
    if (GetActiveGPU(&activeGPU))
    {
        return activeGPU->deviceId;
    }
    else
    {
        return static_cast<DeviceID>(0);
    }
}

// Check whether the active GPU is NVIDIA.
inline bool IsNVIDIA()
{
    return angle::IsNVIDIA(GetActiveGPUVendorID());
}

// Check whether the active GPU is AMD.
inline bool IsAMD()
{
    return angle::IsAMD(GetActiveGPUVendorID());
}

// Check whether the active GPU is Intel.
inline bool IsIntel()
{
    return angle::IsIntel(GetActiveGPUVendorID());
}

// Check whether the active GPU is VMWare.
inline bool IsVMWare()
{
    return angle::IsVMWare(GetActiveGPUVendorID());
}

// Check whether the active GPU is Apple.
inline bool IsApple()
{
    return angle::IsApple(GetActiveGPUVendorID());
}

// Check whether this is a debug build.
inline bool IsDebug()
{
#if !defined(NDEBUG)
    return true;
#else
    return false;
#endif
}

// Check whether this is a release build.
inline bool IsRelease()
{
    return !IsDebug();
}

// Check whether the system is a specific Android device based on the name.
inline bool IsAndroidDevice(const std::string &deviceName)
{
    if (!IsAndroid())
    {
        return false;
    }
    SystemInfo *systemInfo = nullptr;
    GetGPUTestSystemInfo(&systemInfo);
    if (systemInfo->machineModelName == deviceName)
    {
        return true;
    }
    return false;
}

// Check whether the system is a Nexus 5X device.
inline bool IsNexus5X()
{
    return IsAndroidDevice("Nexus 5X");
}

// Check whether the system is a Pixel 2 device.
inline bool IsPixel2()
{
    return IsAndroidDevice("Pixel 2");
}

// Check whether the system is a Pixel 2XL device.
inline bool IsPixel2XL()
{
    return IsAndroidDevice("Pixel 2 XL");
}

inline bool IsPixel4()
{
    return IsAndroidDevice("Pixel 4");
}

inline bool IsPixel4XL()
{
    return IsAndroidDevice("Pixel 4 XL");
}

inline bool IsPixel6()
{
    return IsAndroidDevice("Pixel 6");
}

// Check whether the active GPU is a specific device based on the string device ID.
inline bool IsDeviceIdGPU(const std::string &gpuDeviceId)
{
    uint32_t deviceId = 0;
    if (!HexStringToUInt(gpuDeviceId, &deviceId) || deviceId == 0)
    {
        // PushErrorMessage(kErrorMessage[kErrorEntryWithGpuDeviceIdConflicts], line_number);
        return false;
    }
    return (deviceId == GetActiveGPUDeviceID());
}

// Check whether the active GPU is a NVIDIA Quadro P400
inline bool IsNVIDIAQuadroP400()
{
    return (IsNVIDIA() && IsDeviceIdGPU("0x1CB3"));
}

// Check whether the active GPU is a NVIDIA GTX 1660
inline bool IsNVIDIAGTX1660()
{
    return (IsNVIDIA() && IsDeviceIdGPU("0x2184"));
}

// Check whether the backend API has been set to D3D9 in the constructor
inline bool IsD3D9(const GPUTestConfig::API &api)
{
    return (api == GPUTestConfig::kAPID3D9);
}

// Check whether the backend API has been set to D3D11 in the constructor
inline bool IsD3D11(const GPUTestConfig::API &api)
{
    return (api == GPUTestConfig::kAPID3D11);
}

// Check whether the backend API has been set to OpenGL in the constructor
inline bool IsGLDesktop(const GPUTestConfig::API &api)
{
    return (api == GPUTestConfig::kAPIGLDesktop);
}

// Check whether the backend API has been set to OpenGLES in the constructor
inline bool IsGLES(const GPUTestConfig::API &api)
{
    return (api == GPUTestConfig::kAPIGLES);
}

// Check whether the backend API has been set to Vulkan in the constructor
inline bool IsVulkan(const GPUTestConfig::API &api)
{
    return (api == GPUTestConfig::kAPIVulkan) || (api == GPUTestConfig::kAPISwiftShader);
}

inline bool IsSwiftShader(const GPUTestConfig::API &api)
{
    return (api == GPUTestConfig::kAPISwiftShader);
}

// Check whether the backend API has been set to Metal in the constructor
inline bool IsMetal(const GPUTestConfig::API &api)
{
    return (api == GPUTestConfig::kAPIMetal);
}

}  // anonymous namespace

// Load all conditions in the constructor since this data will not change during a test set.
GPUTestConfig::GPUTestConfig() : GPUTestConfig(false) {}

GPUTestConfig::GPUTestConfig(bool isSwiftShader)
{
    mConditions[kConditionNone]            = false;
    mConditions[kConditionWinXP]           = IsWinXP();
    mConditions[kConditionWinVista]        = IsWinVista();
    mConditions[kConditionWin7]            = IsWin7();
    mConditions[kConditionWin8]            = IsWin8();
    mConditions[kConditionWin10]           = IsWin10();
    mConditions[kConditionWin]             = IsWin();
    mConditions[kConditionMacLeopard]      = IsMacLeopard();
    mConditions[kConditionMacSnowLeopard]  = IsMacSnowLeopard();
    mConditions[kConditionMacLion]         = IsMacLion();
    mConditions[kConditionMacMountainLion] = IsMacMountainLion();
    mConditions[kConditionMacMavericks]    = IsMacMavericks();
    mConditions[kConditionMacYosemite]     = IsMacYosemite();
    mConditions[kConditionMacElCapitan]    = IsMacElCapitan();
    mConditions[kConditionMacSierra]       = IsMacSierra();
    mConditions[kConditionMacHighSierra]   = IsMacHighSierra();
    mConditions[kConditionMacMojave]       = IsMacMojave();
    mConditions[kConditionMac]             = IsMac();
    mConditions[kConditionIOS]             = IsIOS();
    mConditions[kConditionLinux]           = IsLinux();
    mConditions[kConditionAndroid]         = IsAndroid();
    // HW vendors are irrelevant if we are running on SW
    mConditions[kConditionNVIDIA]      = !isSwiftShader && IsNVIDIA();
    mConditions[kConditionAMD]         = !isSwiftShader && IsAMD();
    mConditions[kConditionIntel]       = !isSwiftShader && IsIntel();
    mConditions[kConditionVMWare]      = !isSwiftShader && IsVMWare();
    mConditions[kConditionApple]       = !isSwiftShader && IsApple();
    mConditions[kConditionSwiftShader] = isSwiftShader;

    mConditions[kConditionRelease] = IsRelease();
    mConditions[kConditionDebug]   = IsDebug();
    // If no API provided, pass these conditions by default
    mConditions[kConditionD3D9]      = true;
    mConditions[kConditionD3D11]     = true;
    mConditions[kConditionGLDesktop] = true;
    mConditions[kConditionGLES]      = true;
    mConditions[kConditionVulkan]    = true;
    mConditions[kConditionMetal]     = true;

    // Devices are irrelevant if we are running on SW
    mConditions[kConditionNexus5X]          = !isSwiftShader && IsNexus5X();
    mConditions[kConditionPixel2OrXL]       = !isSwiftShader && (IsPixel2() || IsPixel2XL());
    mConditions[kConditionPixel4OrXL]       = !isSwiftShader && (IsPixel4() || IsPixel4XL());
    mConditions[kConditionPixel6]           = !isSwiftShader && (IsPixel6());
    mConditions[kConditionNVIDIAQuadroP400] = !isSwiftShader && IsNVIDIAQuadroP400();
    mConditions[kConditionNVIDIAGTX1660]    = !isSwiftShader && IsNVIDIAGTX1660();

    mConditions[kConditionPreRotation]    = false;
    mConditions[kConditionPreRotation90]  = false;
    mConditions[kConditionPreRotation180] = false;
    mConditions[kConditionPreRotation270] = false;

    mConditions[kConditionNoSan] = !IsASan() && !IsTSan() && !IsUBSan();
    mConditions[kConditionASan]  = IsASan();
    mConditions[kConditionTSan]  = IsTSan();
    mConditions[kConditionUBSan] = IsUBSan();
}

// If the constructor is passed an API, load those conditions as well
GPUTestConfig::GPUTestConfig(const API &api, uint32_t preRotation)
    : GPUTestConfig(IsSwiftShader(api))
{
    mConditions[kConditionD3D9]      = IsD3D9(api);
    mConditions[kConditionD3D11]     = IsD3D11(api);
    mConditions[kConditionGLDesktop] = IsGLDesktop(api);
    mConditions[kConditionGLES]      = IsGLES(api);
    mConditions[kConditionVulkan]    = IsVulkan(api);
    mConditions[kConditionMetal]     = IsMetal(api);

    switch (preRotation)
    {
        case 90:
            mConditions[kConditionPreRotation]   = true;
            mConditions[kConditionPreRotation90] = true;
            break;
        case 180:
            mConditions[kConditionPreRotation]    = true;
            mConditions[kConditionPreRotation180] = true;
            break;
        case 270:
            mConditions[kConditionPreRotation]    = true;
            mConditions[kConditionPreRotation270] = true;
            break;
        default:
            break;
    }
}

// Return a const reference to the list of all pre-calculated conditions.
const GPUTestConfig::ConditionArray &GPUTestConfig::getConditions() const
{
    return mConditions;
}

}  // namespace angle
