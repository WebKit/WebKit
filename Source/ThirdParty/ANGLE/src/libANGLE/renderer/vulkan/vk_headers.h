//
// Copyright 2016 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// vk_headers:
//    This file should be included to ensure the vulkan headers are included
//

#ifndef LIBANGLE_RENDERER_VULKAN_VK_HEADERS_H_
#define LIBANGLE_RENDERER_VULKAN_VK_HEADERS_H_

#if ANGLE_SHARED_LIBVULKAN
#    include "third_party/volk/volk.h"
#else
#    include <vulkan/vulkan.h>
#endif

#if !defined(ANGLE_SHARED_LIBVULKAN)

namespace rx
{
// VK_EXT_debug_utils
extern PFN_vkCreateDebugUtilsMessengerEXT vkCreateDebugUtilsMessengerEXT;
extern PFN_vkDestroyDebugUtilsMessengerEXT vkDestroyDebugUtilsMessengerEXT;
extern PFN_vkCmdBeginDebugUtilsLabelEXT vkCmdBeginDebugUtilsLabelEXT;
extern PFN_vkCmdEndDebugUtilsLabelEXT vkCmdEndDebugUtilsLabelEXT;
extern PFN_vkCmdInsertDebugUtilsLabelEXT vkCmdInsertDebugUtilsLabelEXT;

// VK_EXT_debug_report
extern PFN_vkCreateDebugReportCallbackEXT vkCreateDebugReportCallbackEXT;
extern PFN_vkDestroyDebugReportCallbackEXT vkDestroyDebugReportCallbackEXT;

// VK_KHR_get_physical_device_properties2
extern PFN_vkGetPhysicalDeviceProperties2KHR vkGetPhysicalDeviceProperties2KHR;
extern PFN_vkGetPhysicalDeviceFeatures2KHR vkGetPhysicalDeviceFeatures2KHR;

// VK_KHR_external_semaphore_fd
extern PFN_vkImportSemaphoreFdKHR vkImportSemaphoreFdKHR;

// VK_EXT_external_memory_host
extern PFN_vkGetMemoryHostPointerPropertiesEXT vkGetMemoryHostPointerPropertiesEXT;

// VK_EXT_transform_feedback
extern PFN_vkCmdBindTransformFeedbackBuffersEXT vkCmdBindTransformFeedbackBuffersEXT;
extern PFN_vkCmdBeginTransformFeedbackEXT vkCmdBeginTransformFeedbackEXT;
extern PFN_vkCmdEndTransformFeedbackEXT vkCmdEndTransformFeedbackEXT;
extern PFN_vkCmdBeginQueryIndexedEXT vkCmdBeginQueryIndexedEXT;
extern PFN_vkCmdEndQueryIndexedEXT vkCmdEndQueryIndexedEXT;
extern PFN_vkCmdDrawIndirectByteCountEXT vkCmdDrawIndirectByteCountEXT;

extern PFN_vkGetBufferMemoryRequirements2KHR vkGetBufferMemoryRequirements2KHR;
extern PFN_vkGetImageMemoryRequirements2KHR vkGetImageMemoryRequirements2KHR;

// VK_KHR_external_fence_capabilities
extern PFN_vkGetPhysicalDeviceExternalFencePropertiesKHR
    vkGetPhysicalDeviceExternalFencePropertiesKHR;

// VK_KHR_external_fence_fd
extern PFN_vkGetFenceFdKHR vkGetFenceFdKHR;
extern PFN_vkImportFenceFdKHR vkImportFenceFdKHR;

// VK_KHR_external_semaphore_capabilities
extern PFN_vkGetPhysicalDeviceExternalSemaphorePropertiesKHR
    vkGetPhysicalDeviceExternalSemaphorePropertiesKHR;

#    if defined(ANGLE_PLATFORM_FUCHSIA)
// VK_FUCHSIA_imagepipe_surface
extern PFN_vkCreateImagePipeSurfaceFUCHSIA vkCreateImagePipeSurfaceFUCHSIA;
#    endif

#    if defined(ANGLE_PLATFORM_ANDROID)
extern PFN_vkGetAndroidHardwareBufferPropertiesANDROID vkGetAndroidHardwareBufferPropertiesANDROID;
extern PFN_vkGetMemoryAndroidHardwareBufferANDROID vkGetMemoryAndroidHardwareBufferANDROID;
#    endif

#    if defined(ANGLE_PLATFORM_GGP)
extern PFN_vkCreateStreamDescriptorSurfaceGGP vkCreateStreamDescriptorSurfaceGGP;
#    endif  // defined(ANGLE_PLATFORM_GGP)

}  // namespace rx

#endif  // ANGLE_SHARED_LIBVULKAN

#endif  // LIBANGLE_RENDERER_VULKAN_VK_HEADERS_H_
