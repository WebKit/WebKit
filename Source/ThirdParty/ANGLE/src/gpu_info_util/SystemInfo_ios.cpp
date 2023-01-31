//
// Copyright 2020 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// SystemInfo_ios.cpp: implementation of the iOS-specific parts of SystemInfo.h

#include "common/platform.h"

#if defined(ANGLE_PLATFORM_IOS) || (defined(ANGLE_PLATFORM_MACCATALYST) && defined(ANGLE_CPU_ARM64))

#    include <sys/sysctl.h>
#    include <sys/types.h>
#    include "gpu_info_util/SystemInfo_internal.h"

namespace angle
{

bool GetSystemInfo_ios(SystemInfo *info)
{
    size_t len;
    sysctlbyname("hw.model", NULL, &len, NULL, 0);

    char *model = static_cast<char *>(malloc(len));
    sysctlbyname("hw.model", model, &len, NULL, 0);

    info->machineModelVersion = std::string(model);
    free(model);

    GPUDeviceInfo deviceInfo;
    deviceInfo.vendorId = kVendorID_Apple;
    info->gpus.push_back(deviceInfo);

    return true;
}

}  // namespace angle

#endif  // defined(ANGLE_PLATFORM_IOS) || (defined(ANGLE_PLATFORM_MACCATALYST) &&
        // defined(ANGLE_CPU_ARM64))
