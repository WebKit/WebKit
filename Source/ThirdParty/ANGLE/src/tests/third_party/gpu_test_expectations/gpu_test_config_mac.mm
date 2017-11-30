// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// gpu_test_config_mac.mm:
//   Helper functions for gpu_test_config that have to be compiled in ObjectiveC++

#include "gpu_test_config_mac.h"

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

void GetOperatingSystemVersionNumbers(int32_t *major_version,
                                      int32_t *minor_version,
                                      int32_t *bugfix_version)
{
#if MAC_OS_X_VERSION_MIN_REQUIRED < MAC_OS_X_VERSION_10_8
    Gestalt(gestaltSystemVersionMajor, reinterpret_cast<SInt32 *>(major_version));
    Gestalt(gestaltSystemVersionMinor, reinterpret_cast<SInt32 *>(minor_version));
    Gestalt(gestaltSystemVersionBugFix, reinterpret_cast<SInt32 *>(bugfix_version));
#else
    if (@available(macOS 10.10, *))
    {
        NSOperatingSystemVersion version = [[NSProcessInfo processInfo] operatingSystemVersion];
        *major_version                   = version.majorVersion;
        *minor_version                   = version.minorVersion;
        *bugfix_version                  = version.patchVersion;
    }
    else
    {
        // This can only happen on 10.9
        *major_version  = 10;
        *minor_version  = 9;
        *bugfix_version = 0;
    }
#endif
}

} // namespace angle
