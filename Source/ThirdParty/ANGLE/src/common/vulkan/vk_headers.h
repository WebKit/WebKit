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

// For the unreleased VK_GOOGLEX_multisampled_render_to_single_sampled
#if !defined(VK_GOOGLEX_multisampled_render_to_single_sampled)
#    define VK_GOOGLEX_multisampled_render_to_single_sampled 1
#    define VK_GOOGLEX_MULTISAMPLED_RENDER_TO_SINGLE_SAMPLED_SPEC_VERSION 1
#    define VK_GOOGLEX_MULTISAMPLED_RENDER_TO_SINGLE_SAMPLED_EXTENSION_NAME \
        "VK_GOOGLEX_multisampled_render_to_single_sampled"

#    define VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MULTISAMPLED_RENDER_TO_SINGLE_SAMPLED_FEATURES_GOOGLEX \
        ((VkStructureType)(1000376000))
#    define VK_STRUCTURE_TYPE_MULTISAMPLED_RENDER_TO_SINGLE_SAMPLED_INFO_GOOGLEX \
        ((VkStructureType)(1000376001))

typedef struct VkPhysicalDeviceMultisampledRenderToSingleSampledFeaturesGOOGLEX
{
    VkStructureType sType;
    const void *pNext;
    VkBool32 multisampledRenderToSingleSampled;
} VkPhysicalDeviceMultisampledRenderToSingleSampledFeaturesGOOGLEX;

typedef struct VkMultisampledRenderToSingleSampledInfoGOOGLEX
{
    VkStructureType sType;
    const void *pNext;
    VkBool32 multisampledRenderToSingleSampledEnable;
    VkSampleCountFlagBits rasterizationSamples;
    VkResolveModeFlagBits depthResolveMode;
    VkResolveModeFlagBits stencilResolveMode;
} VkMultisampledRenderToSingleSampledInfoGOOGLEX;
#endif /* VK_GOOGLEX_multisampled_render_to_single_sampled */

#if !defined(ANGLE_SHARED_LIBVULKAN)

namespace rx
{
// VK_EXT_debug_utils
extern PFN_vkCreateDebugUtilsMessengerEXT vkCreateDebugUtilsMessengerEXT;
extern PFN_vkDestroyDebugUtilsMessengerEXT vkDestroyDebugUtilsMessengerEXT;
extern PFN_vkCmdBeginDebugUtilsLabelEXT vkCmdBeginDebugUtilsLabelEXT;
extern PFN_vkCmdEndDebugUtilsLabelEXT vkCmdEndDebugUtilsLabelEXT;
extern PFN_vkCmdInsertDebugUtilsLabelEXT vkCmdInsertDebugUtilsLabelEXT;
extern PFN_vkSetDebugUtilsObjectNameEXT vkSetDebugUtilsObjectNameEXT;

// VK_EXT_debug_report
extern PFN_vkCreateDebugReportCallbackEXT vkCreateDebugReportCallbackEXT;
extern PFN_vkDestroyDebugReportCallbackEXT vkDestroyDebugReportCallbackEXT;

// VK_KHR_get_physical_device_properties2
extern PFN_vkGetPhysicalDeviceProperties2KHR vkGetPhysicalDeviceProperties2KHR;
extern PFN_vkGetPhysicalDeviceFeatures2KHR vkGetPhysicalDeviceFeatures2KHR;
extern PFN_vkGetPhysicalDeviceMemoryProperties2KHR vkGetPhysicalDeviceMemoryProperties2KHR;

// VK_KHR_external_semaphore_fd
extern PFN_vkImportSemaphoreFdKHR vkImportSemaphoreFdKHR;

// VK_EXT_external_memory_host
extern PFN_vkGetMemoryHostPointerPropertiesEXT vkGetMemoryHostPointerPropertiesEXT;

// VK_EXT_host_query_reset
extern PFN_vkResetQueryPoolEXT vkResetQueryPoolEXT;

// VK_EXT_transform_feedback
extern PFN_vkCmdBindTransformFeedbackBuffersEXT vkCmdBindTransformFeedbackBuffersEXT;
extern PFN_vkCmdBeginTransformFeedbackEXT vkCmdBeginTransformFeedbackEXT;
extern PFN_vkCmdEndTransformFeedbackEXT vkCmdEndTransformFeedbackEXT;
extern PFN_vkCmdBeginQueryIndexedEXT vkCmdBeginQueryIndexedEXT;
extern PFN_vkCmdEndQueryIndexedEXT vkCmdEndQueryIndexedEXT;
extern PFN_vkCmdDrawIndirectByteCountEXT vkCmdDrawIndirectByteCountEXT;

// VK_KHR_get_memory_requirements2
extern PFN_vkGetBufferMemoryRequirements2KHR vkGetBufferMemoryRequirements2KHR;
extern PFN_vkGetImageMemoryRequirements2KHR vkGetImageMemoryRequirements2KHR;

// VK_KHR_bind_memory2
extern PFN_vkBindBufferMemory2KHR vkBindBufferMemory2KHR;
extern PFN_vkBindImageMemory2KHR vkBindImageMemory2KHR;

// VK_KHR_external_fence_capabilities
extern PFN_vkGetPhysicalDeviceExternalFencePropertiesKHR
    vkGetPhysicalDeviceExternalFencePropertiesKHR;

// VK_KHR_external_fence_fd
extern PFN_vkGetFenceFdKHR vkGetFenceFdKHR;
extern PFN_vkImportFenceFdKHR vkImportFenceFdKHR;

// VK_KHR_external_semaphore_capabilities
extern PFN_vkGetPhysicalDeviceExternalSemaphorePropertiesKHR
    vkGetPhysicalDeviceExternalSemaphorePropertiesKHR;

// VK_KHR_sampler_ycbcr_conversion
extern PFN_vkCreateSamplerYcbcrConversionKHR vkCreateSamplerYcbcrConversionKHR;
extern PFN_vkDestroySamplerYcbcrConversionKHR vkDestroySamplerYcbcrConversionKHR;

// VK_KHR_create_renderpass2
extern PFN_vkCreateRenderPass2KHR vkCreateRenderPass2KHR;

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

// VK_KHR_shared_presentable_image
extern PFN_vkGetSwapchainStatusKHR vkGetSwapchainStatusKHR;

// VK_EXT_extended_dynamic_state
extern PFN_vkCmdBindVertexBuffers2EXT vkCmdBindVertexBuffers2EXT;
extern PFN_vkCmdSetCullModeEXT vkCmdSetCullModeEXT;
extern PFN_vkCmdSetDepthBoundsTestEnableEXT vkCmdSetDepthBoundsTestEnableEXT;
extern PFN_vkCmdSetDepthCompareOpEXT vkCmdSetDepthCompareOpEXT;
extern PFN_vkCmdSetDepthTestEnableEXT vkCmdSetDepthTestEnableEXT;
extern PFN_vkCmdSetDepthWriteEnableEXT vkCmdSetDepthWriteEnableEXT;
extern PFN_vkCmdSetFrontFaceEXT vkCmdSetFrontFaceEXT;
extern PFN_vkCmdSetPrimitiveTopologyEXT vkCmdSetPrimitiveTopologyEXT;
extern PFN_vkCmdSetScissorWithCountEXT vkCmdSetScissorWithCountEXT;
extern PFN_vkCmdSetStencilOpEXT vkCmdSetStencilOpEXT;
extern PFN_vkCmdSetStencilTestEnableEXT vkCmdSetStencilTestEnableEXT;
extern PFN_vkCmdSetViewportWithCountEXT vkCmdSetViewportWithCountEXT;

// VK_EXT_extended_dynamic_state2
extern PFN_vkCmdSetDepthBiasEnableEXT vkCmdSetDepthBiasEnableEXT;
extern PFN_vkCmdSetLogicOpEXT vkCmdSetLogicOpEXT;
extern PFN_vkCmdSetPatchControlPointsEXT vkCmdSetPatchControlPointsEXT;
extern PFN_vkCmdSetPrimitiveRestartEnableEXT vkCmdSetPrimitiveRestartEnableEXT;
extern PFN_vkCmdSetRasterizerDiscardEnableEXT vkCmdSetRasterizerDiscardEnableEXT;

// VK_KHR_fragment_shading_rate
extern PFN_vkGetPhysicalDeviceFragmentShadingRatesKHR vkGetPhysicalDeviceFragmentShadingRatesKHR;
extern PFN_vkCmdSetFragmentShadingRateKHR vkCmdSetFragmentShadingRateKHR;

// VK_GOOGLE_display_timing
extern PFN_vkGetPastPresentationTimingGOOGLE vkGetPastPresentationTimingGOOGLE;

}  // namespace rx

#endif  // ANGLE_SHARED_LIBVULKAN

#endif  // LIBANGLE_RENDERER_VULKAN_VK_HEADERS_H_
