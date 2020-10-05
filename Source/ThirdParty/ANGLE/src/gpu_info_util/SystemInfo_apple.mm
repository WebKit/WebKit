//
// Copyright 2020 Apple, Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// SystemInfo_apple.cpp: implementation of the Apple-specific parts of SystemInfo.h

#import "common/platform.h"

#if defined(ANGLE_PLATFORM_APPLE)

#    import "gpu_info_util/SystemInfo_internal.h"
#    import <dispatch/dispatch.h>
#    import <Foundation/Foundation.h>

namespace angle
{

bool GetSystemInfo(SystemInfo *info)
{
#if defined(ANGLE_PLATFORM_MACCATALYST) && defined(ANGLE_CPU_ARM64)
    static bool isiOSAppOnMac = false;
    static dispatch_once_t once;
    dispatch_once(&once, ^{
        isiOSAppOnMac = [[NSProcessInfo processInfo] isiOSAppOnMac];
    });

    if (isiOSAppOnMac)
    {
        GetSystemInfo_ios(info);
        if (info)
        {
            info->isiOSAppOnMac = true;
        }
        return info;
    }

    return GetSystemInfo_mac(info);
#elif defined(ANGLE_PLATFORM_MACOS) || defined(ANGLE_PLATFORM_MACCATALYST)
    return GetSystemInfo_mac(info);
#else
    return GetSystemInfo_ios(info);
#endif
}

}  // namespace angle

#endif  // defined(ANGLE_PLATFORM_APPLE)
