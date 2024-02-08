//
// Copyright 2022 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#include "common/apple_platform_utils.h"
#include "common/debug.h"

#include <Metal/Metal.h>

namespace angle
{

bool IsMetalRendererAvailable()
{
#if defined(ANGLE_PLATFORM_MACOS) || defined(ANGLE_PLATFORM_MACCATALYST)
    static bool queriedMachineModel    = false;
    static bool machineModelSufficient = true;

    if (!queriedMachineModel)
    {
        queriedMachineModel = true;

        std::string fullMachineModel;
        if (GetMacosMachineModel(&fullMachineModel))
        {
            using MachineModelVersion = std::pair<int32_t, int32_t>;

            std::string name;
            MachineModelVersion version;
            ParseMacMachineModel(fullMachineModel, &name, &version.first, &version.second);

            std::optional<MachineModelVersion> minVersion;
            if (name == "MacBookAir")
            {
                minVersion = {8, 1};  // MacBook Air (Retina, 13-inch, 2018)
            }
            else if (name == "MacBookPro")
            {
                minVersion = {13, 1};  // MacBook Pro (13-inch, 2016)
            }
            else if (name == "MacBook")
            {
                minVersion = {9, 1};  // MacBook (Retina, 12-inch, Early 2016)
            }
            else if (name == "Macmini")
            {
                minVersion = {8, 1};  // Mac mini (2018)
            }
            else if (name == "iMac")
            {
                minVersion = {17, 1};  // iMac (Retina 5K, 27-inch, Late 2015)
            }
            else if (name == "iMacPro")
            {
                minVersion = {1, 1};  // iMac Pro
            }

            if (minVersion.has_value() && version < minVersion.value())
            {
                WARN() << "Disabling Metal because machine model \"" << fullMachineModel
                       << "\" is below the minium supported version.";
                machineModelSufficient = false;
            }
        }
    }

    if (!machineModelSufficient)
    {
        ASSERT(queriedMachineModel);
        return false;
    }
#endif

    static bool queriedSystemDevice = false;
    static bool gpuFamilySufficient = false;

    // We only support macos 10.13+ and 11 for now. Since they are requirements for Metal 2.0.
#if TARGET_OS_SIMULATOR
    if (ANGLE_APPLE_AVAILABLE_XCI(10.13, 13.1, 13))
#else
    if (ANGLE_APPLE_AVAILABLE_XCI(10.13, 13.1, 11))
#endif
    {
        if (!queriedSystemDevice)
        {
            ANGLE_APPLE_OBJC_SCOPE
            {
                queriedSystemDevice = true;
                auto device         = [MTLCreateSystemDefaultDevice() ANGLE_APPLE_AUTORELEASE];
                if (!device)
                {
                    return false;
                }

                // -[MTLDevice supportsFamily] introduced in macOS 10.15, Catalyst 13.1, iOS 13.
#if defined(ANGLE_PLATFORM_MACOS) || defined(ANGLE_PLATFORM_MACCATALYST)
                // Old Macs, such as MacBookPro11,4, cannot use ANGLE's Metal backend.
                // This check can be removed once they are no longer supported.
                if (ANGLE_APPLE_AVAILABLE_XCI(10.15, 13.1, 13))
                {
                    if ([device supportsFamily:MTLGPUFamilyMac2])
                        gpuFamilySufficient = true;
                }
                else
                {
                    // Hardcode constant to sidestep compiler errors. Call will
                    // return false on older macOS versions.
                    const NSUInteger macFamily2v1 = 10005;
                    ANGLE_APPLE_ALLOW_DEPRECATED_BEGIN
                    if ([device supportsFeatureSet:static_cast<MTLFeatureSet>(macFamily2v1)])
                        gpuFamilySufficient = true;
                    ANGLE_APPLE_ALLOW_DEPRECATED_END
                }
#elif ANGLE_PLATFORM_IOS_FAMILY && !ANGLE_PLATFORM_IOS_FAMILY_SIMULATOR
                // Hardcode constant to sidestep compiler errors. Call will
                // return false on older macOS versions.
                const NSUInteger iosFamily3v1 = 4;
                if ([device supportsFeatureSet:static_cast<MTLFeatureSet>(iosFamily3v1)])
                    gpuFamilySufficient = true;
#elif ANGLE_PLATFORM_IOS_FAMILY && ANGLE_PLATFORM_IOS_FAMILY_SIMULATOR
                // FIXME: Currently we do not have good simulator query, as it does not support
                // the whole feature set needed for iOS.
                gpuFamilySufficient = true;
#endif
            }
        }

        return gpuFamilySufficient;
    }
    return false;
}

bool GetMacosMachineModel(std::string *outMachineModel)
{
#if defined(ANGLE_PLATFORM_MACOS) || defined(ANGLE_PLATFORM_MACCATALYST)
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
        return false;
    }

    CFDataRef modelData = static_cast<CFDataRef>(
        IORegistryEntryCreateCFProperty(platformExpert, CFSTR("model"), kCFAllocatorDefault, 0));
    if (modelData == nullptr)
    {
        IOObjectRelease(platformExpert);
        return false;
    }

    *outMachineModel = reinterpret_cast<const char *>(CFDataGetBytePtr(modelData));

    IOObjectRelease(platformExpert);
    CFRelease(modelData);

    return true;
#else
    return false;
#endif
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

}  // namespace angle
