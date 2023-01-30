//
// Copyright 2022 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#include "common/apple_platform_utils.h"

#include <Metal/Metal.h>

namespace angle
{

bool IsMetalRendererAvailable()
{
    static bool queriedSystemDevice = false;
    static bool gpuFamilySufficient = false;

    // We only support macos 10.13+ and 11 for now. Since they are requirements for Metal 2.0.
#if TARGET_OS_SIMULATOR
    if (ANGLE_APPLE_AVAILABLE_XCI(10.13, 13.0, 13))
#else
    if (ANGLE_APPLE_AVAILABLE_XCI(10.13, 13.0, 11))
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
#elif defined(ANGLE_PLATFORM_IOS) && !TARGET_OS_SIMULATOR
                // Hardcode constant to sidestep compiler errors. Call will
                // return false on older macOS versions.
                const NSUInteger iosFamily3v1 = 4;
                if ([device supportsFeatureSet:static_cast<MTLFeatureSet>(iosFamily3v1)])
                    gpuFamilySufficient = true;
#elif defined(ANGLE_PLATFORM_IOS) && TARGET_OS_SIMULATOR
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

}  // namespace angle
