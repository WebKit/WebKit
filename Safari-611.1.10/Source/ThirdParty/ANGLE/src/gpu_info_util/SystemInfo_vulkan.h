//
// Copyright 2020 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// SystemInfo_vulkan.h: Reusable Vulkan implementation for SystemInfo.h

#ifndef GPU_INFO_UTIL_SYSTEM_INFO_VULKAN_H_
#define GPU_INFO_UTIL_SYSTEM_INFO_VULKAN_H_

namespace angle
{

struct SystemInfo;

// Reusable vulkan implementation of GetSystemInfo(). See SystemInfo.h.
bool GetSystemInfoVulkan(SystemInfo *info);

}  // namespace angle

#endif  // GPU_INFO_UTIL_SYSTEM_INFO_VULKAN_H_
