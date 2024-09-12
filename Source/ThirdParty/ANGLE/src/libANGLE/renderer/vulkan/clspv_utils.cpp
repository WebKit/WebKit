//
// Copyright 2024 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Utilities to map clspv interface variables to OpenCL and Vulkan mappings.
//

#include "libANGLE/renderer/vulkan/clspv_utils.h"
#include "libANGLE/renderer/vulkan/CLDeviceVk.h"

#include <string>

namespace rx
{
std::string ClspvGetCompilerOptions(const CLDeviceVk *device)
{
    ASSERT(device && device->getRenderer());
    const vk::Renderer *rendererVk = device->getRenderer();
    std::string options{""};

    cl_uint addressBits;
    if (IsError(device->getInfoUInt(cl::DeviceInfo::AddressBits, &addressBits)))
    {
        // This should'nt fail here
        ASSERT(false);
    }
    options += addressBits == 64 ? " -arch=spir64" : " -arch=spir";

    // Other internal Clspv compiler flags that are needed/required
    options += " --long-vector";
    options += " --global-offset";

    // 8 bit storage buffer support
    if (!rendererVk->getFeatures().supports8BitStorageBuffer.enabled)
    {
        options += " --no-8bit-storage=ssbo";
    }
    if (!rendererVk->getFeatures().supports8BitUniformAndStorageBuffer.enabled)
    {
        options += " --no-8bit-storage=ubo";
    }
    if (!rendererVk->getFeatures().supports8BitPushConstant.enabled)
    {
        options += " --no-8bit-storage=pushconstant";
    }

    // 16 bit storage options
    if (!rendererVk->getFeatures().supports16BitStorageBuffer.enabled)
    {
        options += " --no-16bit-storage=ssbo";
    }
    if (!rendererVk->getFeatures().supports16BitUniformAndStorageBuffer.enabled)
    {
        options += " --no-16bit-storage=ubo";
    }
    if (!rendererVk->getFeatures().supports16BitPushConstant.enabled)
    {
        options += " --no-16bit-storage=pushconstant";
    }

    return options;
}

}  // namespace rx
