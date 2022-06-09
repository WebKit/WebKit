//
// Copyright 2013 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// SystemInfo.cpp: implementation of the system-agnostic parts of SystemInfo.h

#include "gpu_info_util/SystemInfo.h"

#include <cstring>
#include <iostream>
#include <sstream>

#include "common/debug.h"
#include "common/string_utils.h"

namespace angle
{
std::string VendorName(VendorID vendor)
{
    switch (vendor)
    {
        case kVendorID_AMD:
            return "AMD";
        case kVendorID_ARM:
            return "ARM";
        case kVendorID_Broadcom:
            return "Broadcom";
        case kVendorID_GOOGLE:
            return "Google";
        case kVendorID_ImgTec:
            return "ImgTec";
        case kVendorID_Intel:
            return "Intel";
        case kVendorID_Kazan:
            return "Kazan";
        case kVendorID_NVIDIA:
            return "NVIDIA";
        case kVendorID_Qualcomm:
            return "Qualcomm";
        case kVendorID_VeriSilicon:
            return "VeriSilicon";
        case kVendorID_Vivante:
            return "Vivante";
        case kVendorID_VMWare:
            return "VMWare";
        case kVendorID_Apple:
            return "Apple";
        case kVendorID_Microsoft:
            return "Microsoft";
        default:
            return "Unknown (" + std::to_string(vendor) + ")";
    }
}

GPUDeviceInfo::GPUDeviceInfo() = default;

GPUDeviceInfo::~GPUDeviceInfo() = default;

GPUDeviceInfo::GPUDeviceInfo(const GPUDeviceInfo &other) = default;

SystemInfo::SystemInfo() = default;

SystemInfo::~SystemInfo() = default;

SystemInfo::SystemInfo(const SystemInfo &other) = default;

bool SystemInfo::hasNVIDIAGPU() const
{
    for (const GPUDeviceInfo &gpu : gpus)
    {
        if (IsNVIDIA(gpu.vendorId))
        {
            return true;
        }
    }
    return false;
}

bool SystemInfo::hasIntelGPU() const
{
    for (const GPUDeviceInfo &gpu : gpus)
    {
        if (IsIntel(gpu.vendorId))
        {
            return true;
        }
    }
    return false;
}

bool SystemInfo::hasAMDGPU() const
{
    for (const GPUDeviceInfo &gpu : gpus)
    {
        if (IsAMD(gpu.vendorId))
        {
            return true;
        }
    }
    return false;
}

bool IsAMD(VendorID vendorId)
{
    return vendorId == kVendorID_AMD;
}

bool IsARM(VendorID vendorId)
{
    return vendorId == kVendorID_ARM;
}

bool IsBroadcom(VendorID vendorId)
{
    return vendorId == kVendorID_Broadcom;
}

bool IsImgTec(VendorID vendorId)
{
    return vendorId == kVendorID_ImgTec;
}

bool IsKazan(VendorID vendorId)
{
    return vendorId == kVendorID_Kazan;
}

bool IsIntel(VendorID vendorId)
{
    return vendorId == kVendorID_Intel;
}

bool IsNVIDIA(VendorID vendorId)
{
    return vendorId == kVendorID_NVIDIA;
}

bool IsQualcomm(VendorID vendorId)
{
    return vendorId == kVendorID_Qualcomm;
}

bool IsGoogle(VendorID vendorId)
{
    return vendorId == kVendorID_GOOGLE;
}

bool IsVeriSilicon(VendorID vendorId)
{
    return vendorId == kVendorID_VeriSilicon;
}

bool IsVMWare(VendorID vendorId)
{
    return vendorId == kVendorID_VMWare;
}

bool IsVivante(VendorID vendorId)
{
    return vendorId == kVendorID_Vivante;
}

bool IsApple(VendorID vendorId)
{
    return vendorId == kVendorID_Apple;
}

bool IsMicrosoft(VendorID vendorId)
{
    return vendorId == kVendorID_Microsoft;
}

bool ParseAMDBrahmaDriverVersion(const std::string &content, std::string *version)
{
    const size_t begin = content.find_first_of("0123456789");
    if (begin == std::string::npos)
    {
        return false;
    }

    const size_t end = content.find_first_not_of("0123456789.", begin);
    if (end == std::string::npos)
    {
        *version = content.substr(begin);
    }
    else
    {
        *version = content.substr(begin, end - begin);
    }
    return true;
}

bool ParseAMDCatalystDriverVersion(const std::string &content, std::string *version)
{
    std::istringstream stream(content);

    std::string line;
    while (std::getline(stream, line))
    {
        static const char kReleaseVersion[] = "ReleaseVersion=";
        if (line.compare(0, std::strlen(kReleaseVersion), kReleaseVersion) != 0)
        {
            continue;
        }

        if (ParseAMDBrahmaDriverVersion(line, version))
        {
            return true;
        }
    }
    return false;
}

bool ParseMacMachineModel(const std::string &identifier,
                          std::string *type,
                          int32_t *major,
                          int32_t *minor)
{
    size_t numberLoc = identifier.find_first_of("0123456789");
    if (numberLoc == std::string::npos)
    {
        return false;
    }

    size_t commaLoc = identifier.find(',', numberLoc);
    if (commaLoc == std::string::npos || commaLoc >= identifier.size())
    {
        return false;
    }

    const char *numberPtr = &identifier[numberLoc];
    const char *commaPtr  = &identifier[commaLoc + 1];
    char *endPtr          = nullptr;

    int32_t majorTmp = static_cast<int32_t>(std::strtol(numberPtr, &endPtr, 10));
    if (endPtr == numberPtr)
    {
        return false;
    }

    int32_t minorTmp = static_cast<int32_t>(std::strtol(commaPtr, &endPtr, 10));
    if (endPtr == commaPtr)
    {
        return false;
    }

    *major = majorTmp;
    *minor = minorTmp;
    *type  = identifier.substr(0, numberLoc);

    return true;
}

bool CMDeviceIDToDeviceAndVendorID(const std::string &id, uint32_t *vendorId, uint32_t *deviceId)
{
    unsigned int vendor = 0;
    unsigned int device = 0;

    bool success = id.length() >= 21 && HexStringToUInt(id.substr(8, 4), &vendor) &&
                   HexStringToUInt(id.substr(17, 4), &device);

    *vendorId = vendor;
    *deviceId = device;
    return success;
}

void GetDualGPUInfo(SystemInfo *info)
{
    ASSERT(!info->gpus.empty());

    // On dual-GPU systems we assume the non-Intel GPU is the graphics one.
    // TODO: this is incorrect and problematic.  activeGPUIndex must be removed if it cannot be
    // determined correctly.  A potential solution is to create an OpenGL context and parse
    // GL_VENDOR.  Currently, our test infrastructure is relying on this information and incorrectly
    // applies test expectations on dual-GPU systems when the Intel GPU is active.
    // http://anglebug.com/6174.
    int active    = 0;
    bool hasIntel = false;
    for (size_t i = 0; i < info->gpus.size(); ++i)
    {
        if (IsIntel(info->gpus[i].vendorId))
        {
            hasIntel = true;
        }
        if (IsIntel(info->gpus[active].vendorId))
        {
            active = static_cast<int>(i);
        }
    }

    // Assume that a combination of NVIDIA or AMD with Intel means Optimus or AMD Switchable
    info->activeGPUIndex  = active;
    info->isOptimus       = hasIntel && IsNVIDIA(info->gpus[active].vendorId);
    info->isAMDSwitchable = hasIntel && IsAMD(info->gpus[active].vendorId);
}

void PrintSystemInfo(const SystemInfo &info)
{
    std::cout << info.gpus.size() << " GPUs:\n";

    for (size_t i = 0; i < info.gpus.size(); i++)
    {
        const auto &gpu = info.gpus[i];

        std::cout << "  " << i << " - " << VendorName(gpu.vendorId) << " device id: 0x" << std::hex
                  << std::uppercase << gpu.deviceId << std::dec << ", revision id: 0x" << std::hex
                  << std::uppercase << gpu.revisionId << std::dec << ", system device id: 0x"
                  << std::hex << std::uppercase << gpu.systemDeviceId << std::dec << "\n";
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
        if (gpu.detailedDriverVersion.major != 0 || gpu.detailedDriverVersion.minor != 0 ||
            gpu.detailedDriverVersion.subMinor != 0 || gpu.detailedDriverVersion.patch != 0)
        {
            std::cout << "       Detailed Driver Version:\n"
                      << "           major: " << gpu.detailedDriverVersion.major
                      << "           minor: " << gpu.detailedDriverVersion.minor
                      << "           subMinor: " << gpu.detailedDriverVersion.subMinor
                      << "           patch: " << gpu.detailedDriverVersion.patch << "\n";
        }
    }

    std::cout << "\n";
    std::cout << "Active GPU: " << info.activeGPUIndex << "\n";

    std::cout << "\n";
    std::cout << "Optimus: " << (info.isOptimus ? "true" : "false") << "\n";
    std::cout << "AMD Switchable: " << (info.isAMDSwitchable ? "true" : "false") << "\n";
    std::cout << "Mac Switchable: " << (info.isMacSwitchable ? "true" : "false") << "\n";
    std::cout << "Needs EAGL on Mac: " << (info.needsEAGLOnMac ? "true" : "false") << "\n";

    std::cout << "\n";
    if (!info.machineManufacturer.empty())
    {
        std::cout << "Machine Manufacturer: " << info.machineManufacturer << "\n";
    }
    if (info.androidSdkLevel != 0)
    {
        std::cout << "Android SDK Level: " << info.androidSdkLevel << "\n";
    }
    if (!info.machineModelName.empty())
    {
        std::cout << "Machine Model: " << info.machineModelName << "\n";
    }
    if (!info.machineModelVersion.empty())
    {
        std::cout << "Machine Model Version: " << info.machineModelVersion << "\n";
    }
    std::cout << std::endl;
}

VersionInfo ParseNvidiaDriverVersion(uint32_t version)
{
    return {
        version >> 22,         // major
        version >> 14 & 0xff,  // minor
        version >> 6 & 0xff,   // subMinor
        version & 0x3f         // patch
    };
}

uint64_t GetSystemDeviceIdFromParts(uint32_t highPart, uint32_t lowPart)
{
    return (static_cast<uint64_t>(highPart) << 32) | lowPart;
}

uint32_t GetSystemDeviceIdHighPart(uint64_t systemDeviceId)
{
    return (systemDeviceId >> 32) & 0xffffffff;
}

uint32_t GetSystemDeviceIdLowPart(uint64_t systemDeviceId)
{
    return systemDeviceId & 0xffffffff;
}

}  // namespace angle
