//
// Copyright (c) 2013-2017 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// SystemInfo.h: gathers information available without starting a GPU driver.

#ifndef GPU_INFO_UTIL_SYSTEM_INFO_H_
#define GPU_INFO_UTIL_SYSTEM_INFO_H_

#include <cstdint>
#include <string>
#include <vector>

namespace angle
{

using VendorID = uint32_t;
using DeviceID = uint32_t;

constexpr VendorID kVendorID_AMD      = 0x1002;
constexpr VendorID kVendorID_Intel    = 0x8086;
constexpr VendorID kVendorID_Nvidia   = 0x10DE;
constexpr VendorID kVendorID_Qualcomm = 0x5143;

struct GPUDeviceInfo
{
    GPUDeviceInfo();
    ~GPUDeviceInfo();

    GPUDeviceInfo(const GPUDeviceInfo &other);

    VendorID vendorId = 0;
    DeviceID deviceId = 0;

    std::string driverVendor;
    std::string driverVersion;
    std::string driverDate;
};

struct SystemInfo
{
    SystemInfo();
    ~SystemInfo();

    SystemInfo(const SystemInfo &other);

    std::vector<GPUDeviceInfo> gpus;
    int primaryGPUIndex = -1;
    int activeGPUIndex  = -1;

    bool isOptimus       = false;
    bool isAMDSwitchable = false;

    // Only available on macOS
    std::string machineModelName;
    std::string machineModelVersion;

    // Only available on Windows, set even on failure.
    std::string primaryDisplayDeviceId;
};

bool GetSystemInfo(SystemInfo *info);

bool IsAMD(VendorID vendorId);
bool IsIntel(VendorID vendorId);
bool IsNvidia(VendorID vendorId);
bool IsQualcomm(VendorID vendorId);

}  // namespace angle

#endif  // GPU_INFO_UTIL_SYSTEM_INFO_H_
