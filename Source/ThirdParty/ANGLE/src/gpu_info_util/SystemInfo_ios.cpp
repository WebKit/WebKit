//
// Copyright 2020 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// SystemInfo_ios.cpp: implementation of the iOS-specific parts of SystemInfo.h

#include "common/platform.h"

#if defined(ANGLE_PLATFORM_IOS) && !defined(ANGLE_PLATFORM_MACCATALYST)

#    include "gpu_info_util/SystemInfo_internal.h"

namespace angle
{

bool GetSystemInfo(SystemInfo *info)
{
    {
        // TODO(anglebug.com/4275): Get the actual system version.
        info->machineModelVersion = "0.0";
    }

    return true;
}

}  // namespace angle

#endif  // defined(ANGLE_PLATFORM_IOS) && !defined(ANGLE_PLATFORM_MACCATALYST)
