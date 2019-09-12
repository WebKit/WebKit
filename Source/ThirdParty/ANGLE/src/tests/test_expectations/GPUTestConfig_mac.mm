//
// Copyright 2019 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// GPUTestConfig_mac.mm:
//   Helper functions for GPUTestConfig that have to be compiled in ObjectiveC++

#include "GPUTestConfig_mac.h"

#import <Cocoa/Cocoa.h>

// OSX 10.8 deprecates Gestalt but doesn't make the operatingSystemVersion property part of the
// public interface of NSProcessInfo until 10.10. Add a forward declaration.
#if !defined(MAC_OS_X_VERSION_10_10) || MAC_OS_X_VERSION_MIN_REQUIRED < MAC_OS_X_VERSION_10_10
@interface NSProcessInfo (YosemiteSDK)
@property(readonly) NSOperatingSystemVersion operatingSystemVersion;
@end
#endif

namespace angle
{

void GetOperatingSystemVersionNumbers(int32_t *majorVersion, int32_t *minorVersion)
{
#if MAC_OS_X_VERSION_MIN_REQUIRED < MAC_OS_X_VERSION_10_8
    Gestalt(gestaltSystemVersionMajor, reinterpret_cast<SInt32 *>(majorVersion));
    Gestalt(gestaltSystemVersionMinor, reinterpret_cast<SInt32 *>(minorVersion));
#else
    if (@available(macOS 10.10, *))
    {
        NSOperatingSystemVersion version = [[NSProcessInfo processInfo] operatingSystemVersion];
        *majorVersion                    = version.majorVersion;
        *minorVersion                    = version.minorVersion;
    }
    else
    {
        // This can only happen on 10.9
        *majorVersion = 10;
        *minorVersion = 9;
    }
#endif
}

}  // namespace angle
