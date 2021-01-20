//
// Copyright 2013 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// SystemInfo_internal.h: Functions used by the SystemInfo_* files and unittests

#ifndef GPU_INFO_UTIL_SYSTEM_INFO_INTERNAL_H_
#define GPU_INFO_UTIL_SYSTEM_INFO_INTERNAL_H_

#include "gpu_info_util/SystemInfo.h"

namespace angle
{

// Defined in SystemInfo_libpci when GPU_INFO_USE_LIBPCI is defined.
bool GetPCIDevicesWithLibPCI(std::vector<GPUDeviceInfo> *devices);
// Defined in SystemInfo_x11 when GPU_INFO_USE_X11 is defined.
bool GetNvidiaDriverVersionWithXNVCtrl(std::string *version);

// Target specific helper functions that can be compiled on all targets
// Live in SystemInfo.cpp
bool ParseAMDBrahmaDriverVersion(const std::string &content, std::string *version);
bool ParseAMDCatalystDriverVersion(const std::string &content, std::string *version);
bool ParseMacMachineModel(const std::string &identifier,
                          std::string *type,
                          int32_t *major,
                          int32_t *minor);
bool CMDeviceIDToDeviceAndVendorID(const std::string &id, uint32_t *vendorId, uint32_t *deviceId);

#if defined(ANGLE_PLATFORM_MACOS) || defined(ANGLE_PLATFORM_MACCATALYST)
bool GetSystemInfo_mac(SystemInfo *info);
#endif

#if defined(ANGLE_PLATFORM_IOS) || (defined(ANGLE_PLATFORM_MACCATALYST) && defined(ANGLE_CPU_ARM64))
bool GetSystemInfo_ios(SystemInfo *info);
#endif

}  // namespace angle

#endif  // GPU_INFO_UTIL_SYSTEM_INFO_INTERNAL_H_
