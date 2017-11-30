//
// Copyright (c) 2013-2017 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// SystemInfo.cpp: implementation of the system-agnostic parts of SystemInfo.h

#include "gpu_info_util/SystemInfo.h"

#include <cstring>
#include <sstream>

#include "common/debug.h"
#include "common/string_utils.h"

namespace angle
{

GPUDeviceInfo::GPUDeviceInfo() = default;

GPUDeviceInfo::~GPUDeviceInfo() = default;

GPUDeviceInfo::GPUDeviceInfo(const GPUDeviceInfo &other) = default;

SystemInfo::SystemInfo() = default;

SystemInfo::~SystemInfo() = default;

SystemInfo::SystemInfo(const SystemInfo &other) = default;

bool IsAMD(VendorID vendorId)
{
    return vendorId == kVendorID_AMD;
}

bool IsIntel(VendorID vendorId)
{
    return vendorId == kVendorID_Intel;
}

bool IsNvidia(VendorID vendorId)
{
    return vendorId == kVendorID_Nvidia;
}

bool IsQualcomm(VendorID vendorId)
{
    return vendorId == kVendorID_Qualcomm;
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

    int32_t majorTmp = std::strtol(numberPtr, &endPtr, 10);
    if (endPtr == numberPtr)
    {
        return false;
    }

    int32_t minorTmp = std::strtol(commaPtr, &endPtr, 10);
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

void FindPrimaryGPU(SystemInfo *info)
{
    ASSERT(!info->gpus.empty());

    // On dual-GPU systems we assume the non-Intel GPU is the primary one.
    int primary   = 0;
    bool hasIntel = false;
    for (size_t i = 0; i < info->gpus.size(); ++i)
    {
        if (IsIntel(info->gpus[i].vendorId))
        {
            hasIntel = true;
        }
        if (IsIntel(info->gpus[primary].vendorId))
        {
            primary = static_cast<int>(i);
        }
    }

    // Assume that a combination of AMD or Nvidia with Intel means Optimus or AMD Switchable
    info->primaryGPUIndex = primary;
    info->isOptimus       = hasIntel && IsNvidia(info->gpus[primary].vendorId);
    info->isAMDSwitchable = hasIntel && IsAMD(info->gpus[primary].vendorId);
}

}  // namespace angle
