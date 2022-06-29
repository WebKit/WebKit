//
// Copyright 2016 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// driver_utils.h : provides more information about current driver.

#ifndef LIBANGLE_RENDERER_DRIVER_UTILS_H_
#define LIBANGLE_RENDERER_DRIVER_UTILS_H_

#include "common/platform.h"
#include "libANGLE/angletypes.h"

namespace rx
{

enum VendorID : uint32_t
{
    VENDOR_ID_UNKNOWN = 0x0,
    VENDOR_ID_AMD     = 0x1002,
    VENDOR_ID_APPLE   = 0x106B,
    VENDOR_ID_ARM     = 0x13B5,
    // Broadcom devices won't use PCI, but this is their Vulkan vendor id.
    VENDOR_ID_BROADCOM  = 0x14E4,
    VENDOR_ID_GOOGLE    = 0x1AE0,
    VENDOR_ID_INTEL     = 0x8086,
    VENDOR_ID_MESA      = 0x10005,
    VENDOR_ID_MICROSOFT = 0x1414,
    VENDOR_ID_NVIDIA    = 0x10DE,
    VENDOR_ID_POWERVR   = 0x1010,
    // This is Qualcomm PCI Vendor ID.
    // Android doesn't have a PCI bus, but all we need is a unique id.
    VENDOR_ID_QUALCOMM = 0x5143,
    VENDOR_ID_SAMSUNG  = 0x144D,
    VENDOR_ID_VIVANTE  = 0x9999,
    VENDOR_ID_VMWARE   = 0x15AD,
};

enum AndroidDeviceID : uint32_t
{
    ANDROID_DEVICE_ID_UNKNOWN     = 0x0,
    ANDROID_DEVICE_ID_NEXUS5X     = 0x4010800,
    ANDROID_DEVICE_ID_PIXEL2      = 0x5040001,
    ANDROID_DEVICE_ID_PIXEL1XL    = 0x5030004,
    ANDROID_DEVICE_ID_PIXEL4      = 0x6040001,
    ANDROID_DEVICE_ID_SWIFTSHADER = 0xC0DE,
};

inline bool IsAMD(uint32_t vendorId)
{
    return vendorId == VENDOR_ID_AMD;
}

inline bool IsApple(uint32_t vendorId)
{
    return vendorId == VENDOR_ID_APPLE;
}

inline bool IsARM(uint32_t vendorId)
{
    return vendorId == VENDOR_ID_ARM;
}

inline bool IsBroadcom(uint32_t vendorId)
{
    return vendorId == VENDOR_ID_BROADCOM;
}

inline bool IsIntel(uint32_t vendorId)
{
    return vendorId == VENDOR_ID_INTEL;
}

inline bool IsGoogle(uint32_t vendorId)
{
    return vendorId == VENDOR_ID_GOOGLE;
}

inline bool IsMicrosoft(uint32_t vendorId)
{
    return vendorId == VENDOR_ID_MICROSOFT;
}

inline bool IsNvidia(uint32_t vendorId)
{
    return vendorId == VENDOR_ID_NVIDIA;
}

inline bool IsPowerVR(uint32_t vendorId)
{
    return vendorId == VENDOR_ID_POWERVR;
}

inline bool IsQualcomm(uint32_t vendorId)
{
    return vendorId == VENDOR_ID_QUALCOMM;
}

inline bool IsSamsung(uint32_t vendorId)
{
    return vendorId == VENDOR_ID_SAMSUNG;
}

inline bool IsVivante(uint32_t vendorId)
{
    return vendorId == VENDOR_ID_VIVANTE;
}

inline bool IsVMWare(uint32_t vendorId)
{
    return vendorId == VENDOR_ID_VMWARE;
}

inline bool IsNexus5X(uint32_t vendorId, uint32_t deviceId)
{
    return IsQualcomm(vendorId) && deviceId == ANDROID_DEVICE_ID_NEXUS5X;
}

inline bool IsPixel1XL(uint32_t vendorId, uint32_t deviceId)
{
    return IsQualcomm(vendorId) && deviceId == ANDROID_DEVICE_ID_PIXEL1XL;
}

inline bool IsPixel2(uint32_t vendorId, uint32_t deviceId)
{
    return IsQualcomm(vendorId) && deviceId == ANDROID_DEVICE_ID_PIXEL2;
}

inline bool IsPixel4(uint32_t vendorId, uint32_t deviceId)
{
    return IsQualcomm(vendorId) && deviceId == ANDROID_DEVICE_ID_PIXEL4;
}

inline bool IsSwiftshader(uint32_t vendorId, uint32_t deviceId)
{
    return IsGoogle(vendorId) && deviceId == ANDROID_DEVICE_ID_SWIFTSHADER;
}

const char *GetVendorString(uint32_t vendorId);

// For Linux, Intel graphics driver version is the Mesa version. The version number has three
// fields: major revision, minor revision and release number.
// For Windows, The version number includes 3rd and 4th fields. Please refer the details at
// http://www.intel.com/content/www/us/en/support/graphics-drivers/000005654.html.
// Current implementation only supports Windows.
class IntelDriverVersion
{
  public:
    IntelDriverVersion(uint32_t buildNumber);
    bool operator==(const IntelDriverVersion &);
    bool operator!=(const IntelDriverVersion &);
    bool operator<(const IntelDriverVersion &);
    bool operator>=(const IntelDriverVersion &);

  private:
    uint32_t mBuildNumber;
};

bool IsSandyBridge(uint32_t DeviceId);
bool IsIvyBridge(uint32_t DeviceId);
bool IsHaswell(uint32_t DeviceId);
bool IsBroadwell(uint32_t DeviceId);
bool IsCherryView(uint32_t DeviceId);
bool IsSkylake(uint32_t DeviceId);
bool IsBroxton(uint32_t DeviceId);
bool IsKabyLake(uint32_t DeviceId);
bool IsGeminiLake(uint32_t DeviceId);
bool IsCoffeeLake(uint32_t DeviceId);
bool Is9thGenIntel(uint32_t DeviceId);
bool Is11thGenIntel(uint32_t DeviceId);
bool Is12thGenIntel(uint32_t DeviceId);

// Platform helpers
inline bool IsWindows()
{
#if defined(ANGLE_PLATFORM_WINDOWS)
    return true;
#else
    return false;
#endif
}

inline bool IsLinux()
{
#if defined(ANGLE_PLATFORM_LINUX)
    return true;
#else
    return false;
#endif
}

inline bool IsChromeOS()
{
#if defined(ANGLE_PLATFORM_CHROMEOS)
    return true;
#else
    return false;
#endif
}

inline bool IsApple()
{
#if defined(ANGLE_PLATFORM_APPLE)
    return true;
#else
    return false;
#endif
}

inline bool IsMac()
{
#if defined(ANGLE_PLATFORM_APPLE) && defined(ANGLE_PLATFORM_MACOS)
    return true;
#else
    return false;
#endif
}

inline bool IsFuchsia()
{
#if defined(ANGLE_PLATFORM_FUCHSIA)
    return true;
#else
    return false;
#endif
}

inline bool IsIOS()
{
#if defined(ANGLE_PLATFORM_IOS)
    return true;
#else
    return false;
#endif
}

bool IsWayland();
bool IsWin10OrGreater();

struct OSVersion
{
    OSVersion();
    OSVersion(int major, int minor, int patch);

    int majorVersion = 0;
    int minorVersion = 0;
    int patchVersion = 0;
};
bool operator==(const OSVersion &a, const OSVersion &b);
bool operator!=(const OSVersion &a, const OSVersion &b);
bool operator<(const OSVersion &a, const OSVersion &b);
bool operator>=(const OSVersion &a, const OSVersion &b);

OSVersion GetMacOSVersion();

OSVersion GetiOSVersion();

OSVersion GetLinuxOSVersion();

inline bool IsAndroid()
{
#if defined(ANGLE_PLATFORM_ANDROID)
    return true;
#else
    return false;
#endif
}

int GetAndroidSDKVersion();

}  // namespace rx
#endif  // LIBANGLE_RENDERER_DRIVER_UTILS_H_
