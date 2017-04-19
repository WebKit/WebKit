//
// Copyright (c) 2017 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// SystemInfo_mac.cpp: implementation of the Mac-specific parts of SystemInfo.h

#include "gpu_info_util/SystemInfo_internal.h"

#import <Cocoa/Cocoa.h>
#import <IOKit/IOKitLib.h>

namespace angle
{

namespace
{

std::string GetMachineModel()
{
    io_service_t platformExpert = IOServiceGetMatchingService(
        kIOMasterPortDefault, IOServiceMatching("IOPlatformExpertDevice"));

    if (platformExpert == IO_OBJECT_NULL)
    {
        return "";
    }

    CFDataRef modelData = static_cast<CFDataRef>(
        IORegistryEntryCreateCFProperty(platformExpert, CFSTR("model"), kCFAllocatorDefault, 0));
    if (modelData == nullptr)
    {
        IOObjectRelease(platformExpert);
        return "";
    }

    std::string result = reinterpret_cast<const char *>(CFDataGetBytePtr(modelData));

    IOObjectRelease(platformExpert);
    CFRelease(modelData);

    return result;
}

// Extracts one integer property from a registry entry.
bool GetEntryProperty(io_registry_entry_t entry, CFStringRef name, uint32_t *value)
{
    *value = 0;

    CFDataRef data = static_cast<CFDataRef>(
        IORegistryEntrySearchCFProperty(entry, kIOServicePlane, name, kCFAllocatorDefault,
                                        kIORegistryIterateRecursively | kIORegistryIterateParents));

    if (data == nullptr)
    {
        return false;
    }

    const uint32_t *valuePtr = reinterpret_cast<const uint32_t *>(CFDataGetBytePtr(data));

    if (valuePtr == nullptr)
    {
        CFRelease(data);
        return false;
    }

    *value = *valuePtr;
    CFRelease(data);
    return true;
}

// CGDisplayIOServicePort is deprecated as of macOS 10.9, but has no replacement, see
// https://crbug.com/650837
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"

// Find the info of the current GPU.
bool GetActiveGPU(VendorID *vendorId, DeviceID *deviceId)
{
    io_registry_entry_t port = CGDisplayIOServicePort(kCGDirectMainDisplay);

    return GetEntryProperty(port, CFSTR("vendor-id"), vendorId) &&
           GetEntryProperty(port, CFSTR("device-id"), deviceId);
}

#pragma clang diagnostic pop

// Gathers the vendor and device IDs for the PCI GPUs
bool GetPCIDevices(std::vector<GPUDeviceInfo> *devices)
{
    // matchDictionary will be consumed by IOServiceGetMatchingServices, no need to release it.
    CFMutableDictionaryRef matchDictionary = IOServiceMatching("IOPCIDevice");

    io_iterator_t entryIterator;
    if (IOServiceGetMatchingServices(kIOMasterPortDefault, matchDictionary, &entryIterator) !=
        kIOReturnSuccess)
    {
        return false;
    }

    io_registry_entry_t entry = IO_OBJECT_NULL;

    while ((entry = IOIteratorNext(entryIterator)) != IO_OBJECT_NULL)
    {
        constexpr uint32_t kClassCodeDisplayVGA = 0x30000;
        uint32_t classCode;
        GPUDeviceInfo info;

        if (GetEntryProperty(entry, CFSTR("class-code"), &classCode) &&
            classCode == kClassCodeDisplayVGA &&
            GetEntryProperty(entry, CFSTR("vendor-id"), &info.vendorId) &&
            GetEntryProperty(entry, CFSTR("device-id"), &info.deviceId))
        {
            devices->push_back(info);
        }

        IOObjectRelease(entry);
    }
    IOObjectRelease(entryIterator);

    return true;
}

}  // anonymous namespace

bool GetSystemInfo(SystemInfo *info)
{
    {
        int32_t major = 0;
        int32_t minor = 0;
        ParseMacMachineModel(GetMachineModel(), &info->machineModelName, &major, &minor);
        info->machineModelVersion = std::to_string(major) + "." + std::to_string(minor);
    }

    if (!GetPCIDevices(&(info->gpus)))
    {
        return false;
    }

    if (info->gpus.empty())
    {
        return false;
    }

    // Find the active GPU
    {
        VendorID activeVendor;
        DeviceID activeDevice;
        if (!GetActiveGPU(&activeVendor, &activeDevice))
        {
            return false;
        }

        for (size_t i = 0; i < info->gpus.size(); ++i)
        {
            if (info->gpus[i].vendorId == activeVendor && info->gpus[i].deviceId == activeDevice)
            {
                info->activeGPUIndex = i;
                break;
            }
        }
    }

    FindPrimaryGPU(info);

    return true;
}

}  // namespace angle
