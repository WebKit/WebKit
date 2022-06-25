//
// Copyright 2020 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// vk_android_utils.cpp: Vulkan utilities for using the Android platform

#include "libANGLE/renderer/vulkan/android/vk_android_utils.h"

#include "common/android_util.h"
#include "libANGLE/renderer/vulkan/ContextVk.h"
#include "libANGLE/renderer/vulkan/vk_utils.h"

#if defined(ANGLE_PLATFORM_ANDROID)
#    include "libANGLE/Display.h"
#    include "libANGLE/renderer/vulkan/android/AHBFunctions.h"
#    include "libANGLE/renderer/vulkan/android/DisplayVkAndroid.h"
#endif

namespace rx
{
namespace vk
{
angle::Result GetClientBufferMemoryRequirements(ContextVk *contextVk,
                                                const AHardwareBuffer *hardwareBuffer,
                                                VkMemoryRequirements &memRequirements)
{
#if defined(ANGLE_PLATFORM_ANDROID)
    ASSERT(GetImplAs<DisplayVkAndroid>(contextVk->getRenderer()->getDisplay())
               ->getAHBFunctions()
               .valid());

    // Get Android Buffer Properties
    VkAndroidHardwareBufferFormatPropertiesANDROID bufferFormatProperties = {};
    bufferFormatProperties.sType =
        VK_STRUCTURE_TYPE_ANDROID_HARDWARE_BUFFER_FORMAT_PROPERTIES_ANDROID;

    VkAndroidHardwareBufferPropertiesANDROID bufferProperties = {};
    bufferProperties.sType = VK_STRUCTURE_TYPE_ANDROID_HARDWARE_BUFFER_PROPERTIES_ANDROID;
    bufferProperties.pNext = &bufferFormatProperties;

    VkDevice device = contextVk->getRenderer()->getDevice();
    ANGLE_VK_TRY(contextVk, vkGetAndroidHardwareBufferPropertiesANDROID(device, hardwareBuffer,
                                                                        &bufferProperties));

    memRequirements.size           = bufferProperties.allocationSize;
    memRequirements.alignment      = 0;
    memRequirements.memoryTypeBits = bufferProperties.memoryTypeBits;

    return angle::Result::Continue;
#else
    ANGLE_VK_UNREACHABLE(contextVk);
    return angle::Result::Stop;

#endif
}

angle::Result InitAndroidExternalMemory(ContextVk *contextVk,
                                        EGLClientBuffer clientBuffer,
                                        VkMemoryPropertyFlags memoryProperties,
                                        Buffer *buffer,
                                        VkMemoryPropertyFlags *memoryPropertyFlagsOut,
                                        DeviceMemory *deviceMemoryOut)
{
#if defined(ANGLE_PLATFORM_ANDROID)
    const AHBFunctions &functions =
        GetImplAs<DisplayVkAndroid>(contextVk->getRenderer()->getDisplay())->getAHBFunctions();
    ASSERT(functions.valid());

    struct AHardwareBuffer *hardwareBuffer =
        angle::android::ClientBufferToAHardwareBuffer(clientBuffer);

    VkMemoryRequirements externalMemoryRequirements = {};
    ANGLE_TRY(
        GetClientBufferMemoryRequirements(contextVk, hardwareBuffer, externalMemoryRequirements));

    // Import Vulkan DeviceMemory from Android Hardware Buffer.
    VkImportAndroidHardwareBufferInfoANDROID importHardwareBufferInfo = {};
    importHardwareBufferInfo.sType  = VK_STRUCTURE_TYPE_IMPORT_ANDROID_HARDWARE_BUFFER_INFO_ANDROID;
    importHardwareBufferInfo.buffer = hardwareBuffer;

    ANGLE_TRY(AllocateBufferMemoryWithRequirements(
        contextVk, memoryProperties, externalMemoryRequirements, &importHardwareBufferInfo, buffer,
        memoryPropertyFlagsOut, deviceMemoryOut));

    functions.acquire(hardwareBuffer);

    return angle::Result::Continue;
#else
    ANGLE_VK_UNREACHABLE(contextVk);
    return angle::Result::Stop;
#endif
}

void ReleaseAndroidExternalMemory(RendererVk *rendererVk, EGLClientBuffer clientBuffer)
{
#if defined(ANGLE_PLATFORM_ANDROID)
    const AHBFunctions &functions =
        GetImplAs<DisplayVkAndroid>(rendererVk->getDisplay())->getAHBFunctions();
    ASSERT(functions.valid());
    struct AHardwareBuffer *hardwareBuffer =
        angle::android::ClientBufferToAHardwareBuffer(clientBuffer);
    functions.release(hardwareBuffer);
#endif
}
}  // namespace vk
}  // namespace rx
