//
// Copyright 2017 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// SystemInfo_macos.mm: implementation of the macOS-specific parts of SystemInfo.h

#include "common/platform.h"

#if defined(ANGLE_PLATFORM_MACOS) || defined(ANGLE_PLATFORM_MACCATALYST)

#    include "gpu_info_util/SystemInfo_internal.h"

#    import <Cocoa/Cocoa.h>
#    import <IOKit/IOKitLib.h>

#    include "common/gl/cgl/FunctionsCGL.h"

#    if !defined(__MAC_OS_X_VERSION_MIN_REQUIRED) || __MAC_OS_X_VERSION_MIN_REQUIRED >= 120000
#        define HAVE_MAIN_PORT_DEFAULT 1
#    else
#        define HAVE_MAIN_PORT_DEFAULT 0
#    endif

namespace angle
{

namespace
{

constexpr CGLRendererProperty kCGLRPRegistryIDLow  = static_cast<CGLRendererProperty>(140);
constexpr CGLRendererProperty kCGLRPRegistryIDHigh = static_cast<CGLRendererProperty>(141);

std::string GetMachineModel()
{
#    if HAVE_MAIN_PORT_DEFAULT
    const mach_port_t mainPort = kIOMainPortDefault;
#    else
#        pragma clang diagnostic push
#        pragma clang diagnostic ignored "-Wdeprecated-declarations"
    const mach_port_t mainPort = kIOMasterPortDefault;
#        pragma clang diagnostic pop
#    endif
    io_service_t platformExpert =
        IOServiceGetMatchingService(mainPort, IOServiceMatching("IOPlatformExpertDevice"));

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

// Gathers the vendor and device IDs for GPUs listed in the IORegistry.
void GetIORegistryDevices(std::vector<GPUDeviceInfo> *devices)
{
    constexpr uint32_t kNumServices         = 2;
    const char *kServiceNames[kNumServices] = {"IOGraphicsAccelerator2", "AGXAccelerator"};
    const bool kServiceIsGraphicsAccelerator2[kNumServices] = {true, false};
    for (uint32_t i = 0; i < kNumServices; ++i)
    {
        // matchDictionary will be consumed by IOServiceGetMatchingServices, no need to release it.
        CFMutableDictionaryRef matchDictionary = IOServiceMatching(kServiceNames[i]);

        io_iterator_t entryIterator;
#    if HAVE_MAIN_PORT_DEFAULT
        const mach_port_t mainPort = kIOMainPortDefault;
#    else
#        pragma clang diagnostic push
#        pragma clang diagnostic ignored "-Wdeprecated-declarations"
        const mach_port_t mainPort = kIOMasterPortDefault;
#        pragma clang diagnostic pop
#    endif
        if (IOServiceGetMatchingServices(mainPort, matchDictionary, &entryIterator) !=
            kIOReturnSuccess)
        {
            continue;
        }

        io_registry_entry_t entry = IO_OBJECT_NULL;
        while ((entry = IOIteratorNext(entryIterator)) != IO_OBJECT_NULL)
        {
            constexpr uint32_t kClassCodeDisplayVGA = 0x30000;
            uint32_t classCode;
            GPUDeviceInfo info;
            // The registry ID of an IOGraphicsAccelerator2 or AGXAccelerator matches the ID used
            // for GPU selection by ANGLE_platform_angle_device_id
            if (IORegistryEntryGetRegistryEntryID(entry, &info.systemDeviceId) != kIOReturnSuccess)
            {
                IOObjectRelease(entry);
                continue;
            }

            io_registry_entry_t queryEntry = entry;
            if (kServiceIsGraphicsAccelerator2[i])
            {
                // If the matching entry is an IOGraphicsAccelerator2, get the parent entry that
                // will be the IOPCIDevice which holds vendor-id and device-id
                io_registry_entry_t deviceEntry = IO_OBJECT_NULL;
                if (IORegistryEntryGetParentEntry(entry, kIOServicePlane, &deviceEntry) !=
                        kIOReturnSuccess ||
                    deviceEntry == IO_OBJECT_NULL)
                {
                    IOObjectRelease(entry);
                    continue;
                }
                IOObjectRelease(entry);
                queryEntry = deviceEntry;
            }

            if (!GetEntryProperty(queryEntry, CFSTR("vendor-id"), &info.vendorId))
            {
                IOObjectRelease(queryEntry);
                continue;
            }
            // AGXAccelerator entries only provide a vendor ID.
            if (kServiceIsGraphicsAccelerator2[i])
            {
                if (!GetEntryProperty(queryEntry, CFSTR("class-code"), &classCode))
                {
                    IOObjectRelease(queryEntry);
                    continue;
                }
                if (classCode != kClassCodeDisplayVGA)
                {
                    IOObjectRelease(queryEntry);
                    continue;
                }
                if (!GetEntryProperty(queryEntry, CFSTR("device-id"), &info.deviceId))
                {
                    IOObjectRelease(queryEntry);
                    continue;
                }
                // Populate revisionId if we can
                GetEntryProperty(queryEntry, CFSTR("revision-id"), &info.revisionId);
            }

            devices->push_back(info);
            IOObjectRelease(queryEntry);
        }
        IOObjectRelease(entryIterator);

        // If any devices have been populated by IOGraphicsAccelerator2, do not continue to
        // AGXAccelerator.
        if (!devices->empty())
        {
            break;
        }
    }
}

void ForceGPUSwitchIndex(SystemInfo *info)
{
    VendorID activeVendor = 0;
    DeviceID activeDevice = 0;

    uint64_t gpuID = GetGpuIDFromDisplayID(kCGDirectMainDisplay);

    if (gpuID == 0)
        return;

    CFMutableDictionaryRef matchDictionary = IORegistryEntryIDMatching(gpuID);
#    if HAVE_MAIN_PORT_DEFAULT
    const mach_port_t mainPort = kIOMainPortDefault;
#    else
#        pragma clang diagnostic push
#        pragma clang diagnostic ignored "-Wdeprecated-declarations"
    const mach_port_t mainPort = kIOMasterPortDefault;
#        pragma clang diagnostic pop
#    endif
    io_service_t gpuEntry = IOServiceGetMatchingService(mainPort, matchDictionary);

    if (gpuEntry == IO_OBJECT_NULL)
    {
        IOObjectRelease(gpuEntry);
        return;
    }

    if (!(GetEntryProperty(gpuEntry, CFSTR("vendor-id"), &activeVendor) &&
          GetEntryProperty(gpuEntry, CFSTR("device-id"), &activeDevice)))
    {
        IOObjectRelease(gpuEntry);
        return;
    }

    IOObjectRelease(gpuEntry);

    for (size_t i = 0; i < info->gpus.size(); ++i)
    {
        if (info->gpus[i].vendorId == activeVendor && info->gpus[i].deviceId == activeDevice)
        {
            info->activeGPUIndex = static_cast<int>(i);
            break;
        }
    }
}

}  // anonymous namespace

// Code from WebKit to get the active GPU's ID given a Core Graphics display ID.
// https://trac.webkit.org/browser/webkit/trunk/Source/WebCore/platform/mac/PlatformScreenMac.mm
// Used with permission.
uint64_t GetGpuIDFromDisplayID(uint32_t displayID)
{
    return GetGpuIDFromOpenGLDisplayMask(CGDisplayIDToOpenGLDisplayMask(displayID));
}

// Code from WebKit to query the GPU ID given an OpenGL display mask.
// https://trac.webkit.org/browser/webkit/trunk/Source/WebCore/platform/mac/PlatformScreenMac.mm
// Used with permission.
uint64_t GetGpuIDFromOpenGLDisplayMask(uint32_t displayMask)
{
    if (@available(macOS 10.13, *))
    {
        GLint numRenderers              = 0;
        CGLRendererInfoObj rendererInfo = nullptr;
        CGLError error = CGLQueryRendererInfo(displayMask, &rendererInfo, &numRenderers);
        if (!numRenderers || !rendererInfo || error != kCGLNoError)
            return 0;

        // The 0th renderer should not be the software renderer.
        GLint isAccelerated;
        error = CGLDescribeRenderer(rendererInfo, 0, kCGLRPAccelerated, &isAccelerated);
        if (!isAccelerated || error != kCGLNoError)
        {
            CGLDestroyRendererInfo(rendererInfo);
            return 0;
        }

        GLint gpuIDLow  = 0;
        GLint gpuIDHigh = 0;

        error = CGLDescribeRenderer(rendererInfo, 0, kCGLRPRegistryIDLow, &gpuIDLow);
        if (error != kCGLNoError)
        {
            CGLDestroyRendererInfo(rendererInfo);
            return 0;
        }

        error = CGLDescribeRenderer(rendererInfo, 0, kCGLRPRegistryIDHigh, &gpuIDHigh);
        if (error != kCGLNoError)
        {
            CGLDestroyRendererInfo(rendererInfo);
            return 0;
        }

        CGLDestroyRendererInfo(rendererInfo);
        return (static_cast<uint64_t>(static_cast<uint32_t>(gpuIDHigh)) << 32) |
               static_cast<uint64_t>(static_cast<uint32_t>(gpuIDLow));
    }

    return 0;
}

// Get VendorID from metal device's registry ID
VendorID GetVendorIDFromMetalDeviceRegistryID(uint64_t registryID)
{
#    if defined(ANGLE_PLATFORM_MACOS)
    // On macOS, the following code is only supported since 10.13.
    if (@available(macOS 10.13, *))
#    endif
    {
        // Get a matching dictionary for the IOGraphicsAccelerator2
        CFMutableDictionaryRef matchingDict = IORegistryEntryIDMatching(registryID);
        if (matchingDict == nullptr)
        {
            return 0;
        }

        // IOServiceGetMatchingService will consume the reference on the matching dictionary,
        // so we don't need to release the dictionary.
#    if HAVE_MAIN_PORT_DEFAULT
        const mach_port_t mainPort = kIOMainPortDefault;
#    else
#        pragma clang diagnostic push
#        pragma clang diagnostic ignored "-Wdeprecated-declarations"
        const mach_port_t mainPort = kIOMasterPortDefault;
#        pragma clang diagnostic pop
#    endif
        io_registry_entry_t acceleratorEntry = IOServiceGetMatchingService(mainPort, matchingDict);
        if (acceleratorEntry == IO_OBJECT_NULL)
        {
            return 0;
        }

        // Get the parent entry that will be the IOPCIDevice
        io_registry_entry_t deviceEntry = IO_OBJECT_NULL;
        if (IORegistryEntryGetParentEntry(acceleratorEntry, kIOServicePlane, &deviceEntry) !=
                kIOReturnSuccess ||
            deviceEntry == IO_OBJECT_NULL)
        {
            IOObjectRelease(acceleratorEntry);
            return 0;
        }

        IOObjectRelease(acceleratorEntry);

        uint32_t vendorId;
        if (!GetEntryProperty(deviceEntry, CFSTR("vendor-id"), &vendorId))
        {
            vendorId = 0;
        }

        IOObjectRelease(deviceEntry);

        return vendorId;
    }
    return 0;
}

bool GetSystemInfo_mac(SystemInfo *info)
{
    {
        int32_t major = 0;
        int32_t minor = 0;
        ParseMacMachineModel(GetMachineModel(), &info->machineModelName, &major, &minor);
        info->machineModelVersion = std::to_string(major) + "." + std::to_string(minor);
    }

    GetIORegistryDevices(&info->gpus);
    if (info->gpus.empty())
    {
        return false;
    }

    // Call the generic GetDualGPUInfo function to initialize info fields
    // such as isOptimus, isAMDSwitchable, and the activeGPUIndex
    GetDualGPUInfo(info);

    // Then override the activeGPUIndex field of info to reflect the current
    // GPU instead of the non-intel GPU
    if (@available(macOS 10.13, *))
    {
        ForceGPUSwitchIndex(info);
    }

    // Figure out whether this is a dual-GPU system.
    //
    // TODO(kbr): this code was ported over from Chromium, and its correctness
    // could be improved - need to use Mac-specific APIs to determine whether
    // offline renderers are allowed, and whether these two GPUs are really the
    // integrated/discrete GPUs in a laptop.
    if (info->gpus.size() == 2 &&
        ((IsIntel(info->gpus[0].vendorId) && !IsIntel(info->gpus[1].vendorId)) ||
         (!IsIntel(info->gpus[0].vendorId) && IsIntel(info->gpus[1].vendorId))))
    {
        info->isMacSwitchable = true;
    }

#    if defined(ANGLE_PLATFORM_MACCATALYST) && defined(ANGLE_CPU_ARM64)
    info->needsEAGLOnMac = true;
#    endif

    return true;
}

}  // namespace angle

#endif  // defined(ANGLE_PLATFORM_MACOS) || defined(ANGLE_PLATFORM_MACCATALYST)
