//
// Copyright 2017 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// PrintSystemInfoTest.cpp:
//     prints the information gathered about the system so that it appears in the test logs

#include <gtest/gtest.h>

#include <iostream>

#include "common/platform.h"
#include "gpu_info_util/SystemInfo.h"

using namespace angle;

namespace
{

#if defined(ANGLE_PLATFORM_WINDOWS) || defined(ANGLE_PLATFORM_LINUX) || \
    defined(ANGLE_PLATFORM_APPLE)
#define SYSTEM_INFO_IMPLEMENTED
#endif

#if defined(SYSTEM_INFO_IMPLEMENTED)
std::string VendorName(VendorID vendor)
{
    switch (vendor)
    {
        case kVendorID_AMD:
            return "AMD";
        case kVendorID_Intel:
            return "Intel";
        case kVendorID_Nvidia:
            return "Nvidia";
        case kVendorID_Qualcomm:
            return "Qualcomm";
        default:
            return "Unknown (" + std::to_string(vendor) + ")";
    }
}
#endif

// Prints the information gathered about the system
TEST(PrintSystemInfoTest, Print)
{
#if defined(SYSTEM_INFO_IMPLEMENTED)
    SystemInfo info;

    ASSERT_TRUE(GetSystemInfo(&info));
    ASSERT_GT(info.gpus.size(), 0u);

    std::cout << info.gpus.size() << " GPUs:\n";

    for (size_t i = 0; i < info.gpus.size(); i++)
    {
        const auto &gpu = info.gpus[i];

        std::cout << "  " << i << " - " << VendorName(gpu.vendorId) << " device " << gpu.deviceId
                  << "\n";
        if (!gpu.driverVendor.empty())
        {
            std::cout << "       Driver Vendor: " << gpu.driverVendor << "\n";
        }
        if (!gpu.driverVersion.empty())
        {
            std::cout << "       Driver Version: " << gpu.driverVersion << "\n";
        }
        if (!gpu.driverDate.empty())
        {
            std::cout << "       Driver Date: " << gpu.driverDate << "\n";
        }
    }

    std::cout << "\n";
    std::cout << "Active GPU: " << info.activeGPUIndex << "\n";
    std::cout << "Primary GPU: " << info.primaryGPUIndex << "\n";

    std::cout << "\n";
    std::cout << "Optimus: " << (info.isOptimus ? "true" : "false") << "\n";
    std::cout << "AMD Switchable: " << (info.isAMDSwitchable ? "true" : "false") << "\n";

    std::cout << "\n";
    if (!info.machineModelName.empty() || !info.machineModelVersion.empty())
    {
        std::cout << "Machine Model: " << info.machineModelName << " version "
                  << info.machineModelVersion << "\n";
    }

    if (!info.primaryDisplayDeviceId.empty())
    {
        std::cout << "Primary Display Device: " << info.primaryDisplayDeviceId << "\n";
    }
    std::cout << std::endl;
#else
    std::cerr << "GetSystemInfo not implemented, skipping" << std::endl;
#endif
}

}  // anonymous namespace
