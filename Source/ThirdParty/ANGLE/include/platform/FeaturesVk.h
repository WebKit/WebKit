//
// Copyright 2018 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// FeaturesVk.h: Optional features for the Vulkan renderer.
//

#ifndef ANGLE_PLATFORM_FEATURESVK_H_
#define ANGLE_PLATFORM_FEATURESVK_H_

#include "platform/Feature.h"

#include <array>

namespace angle
{

struct FeaturesVk : FeatureSetBase
{
    FeaturesVk();
    ~FeaturesVk();

    // Line segment rasterization must follow OpenGL rules. This means using an algorithm similar
    // to Bresenham's. Vulkan uses a different algorithm. This feature enables the use of pixel
    // shader patching to implement OpenGL basic line rasterization rules. This feature will
    // normally always be enabled. Exposing it as an option enables performance testing.
    Feature basicGLLineRasterization = {
        "basicGLLineRasterization", FeatureCategory::VulkanFeatures,
        "Enable the use of pixel shader patching to implement OpenGL basic line "
        "rasterization rules",
        &members};

    // If the VK_EXT_line_rasterization extension is available we'll use it to get
    // Bresenham line rasterization.
    Feature bresenhamLineRasterization = {
        "bresenhamLineRasterization", FeatureCategory::VulkanFeatures,
        "Enable Bresenham line rasterization via VK_EXT_line_rasterization extension", &members};

    // If the VK_EXT_provoking_vertex extension is available, we'll use it to set
    // the provoking vertex mode
    Feature provokingVertex = {"provokingVertex", FeatureCategory::VulkanFeatures,
                               "Enable provoking vertex mode via VK_EXT_provoking_vertex extension",
                               &members};

    // This flag is added for the sole purpose of end2end tests, to test the correctness
    // of various algorithms when a fallback format is used, such as using a packed format to
    // emulate a depth- or stencil-only format.
    Feature forceFallbackFormat = {"forceFallbackFormat", FeatureCategory::VulkanWorkarounds,
                                   "Force a fallback format for angle_end2end_tests", &members};

    // On some NVIDIA drivers the point size range reported from the API is inconsistent with the
    // actual behavior. Clamp the point size to the value from the API to fix this.
    // Tracked in http://anglebug.com/2970.
    Feature clampPointSize = {
        "clampPointSize", FeatureCategory::VulkanWorkarounds,
        "The point size range reported from the API is inconsistent with the actual behavior",
        &members, "http://anglebug.com/2970"};

    // On some NVIDIA drivers the depth value is not clamped to [0,1] for floating point depth
    // buffers. This is NVIDIA bug 3171019, see http://anglebug.com/3970 for details.
    Feature depthClamping = {
        "depth_clamping", FeatureCategory::VulkanWorkarounds,
        "The depth value is not clamped to [0,1] for floating point depth buffers.", &members,
        "http://anglebug.com/3970"};

    Feature supportsRenderpass2 = {"supportsRenderpass2", FeatureCategory::VulkanFeatures,
                                   "VkDevice supports the VK_KHR_create_renderpass2 extension",
                                   &members};

    // Whether the VkDevice supports the VK_KHR_incremental_present extension, on which the
    // EGL_KHR_swap_buffers_with_damage extension can be layered.
    Feature supportsIncrementalPresent = {
        "supportsIncrementalPresent", FeatureCategory::VulkanFeatures,
        "VkDevice supports the VK_KHR_incremental_present extension", &members};

    // Whether the VkDevice supports the VK_ANDROID_external_memory_android_hardware_buffer
    // extension, on which the EGL_ANDROID_image_native_buffer extension can be layered.
    Feature supportsAndroidHardwareBuffer = {
        "supportsAndroidHardwareBuffer", FeatureCategory::VulkanFeatures,
        "VkDevice supports the VK_ANDROID_external_memory_android_hardware_buffer extension",
        &members};

    // Whether the VkDevice supports the VK_GGP_frame_token extension, on which
    // the EGL_ANGLE_swap_with_frame_token extension can be layered.
    Feature supportsGGPFrameToken = {"supportsGGPFrameToken", FeatureCategory::VulkanFeatures,
                                     "VkDevice supports the VK_GGP_frame_token extension",
                                     &members};

    // Whether the VkDevice supports the VK_KHR_external_memory_fd extension, on which the
    // GL_EXT_memory_object_fd extension can be layered.
    Feature supportsExternalMemoryFd = {"supportsExternalMemoryFd", FeatureCategory::VulkanFeatures,
                                        "VkDevice supports the VK_KHR_external_memory_fd extension",
                                        &members};

    // Whether the VkDevice supports the VK_FUCHSIA_external_memory
    // extension, on which the GL_ANGLE_memory_object_fuchsia extension can be layered.
    Feature supportsExternalMemoryFuchsia = {
        "supportsExternalMemoryFuchsia", FeatureCategory::VulkanFeatures,
        "VkDevice supports the VK_FUCHSIA_external_memory extension", &members};

    Feature supportsFilteringPrecision = {
        "supportsFilteringPrecision", FeatureCategory::VulkanFeatures,
        "VkDevice supports the VK_GOOGLE_sampler_filtering_precision extension", &members};

    // Whether the VkDevice supports the VK_KHR_external_fence_capabilities extension.
    Feature supportsExternalFenceCapabilities = {
        "supportsExternalFenceCapabilities", FeatureCategory::VulkanFeatures,
        "VkDevice supports the VK_KHR_external_fence_capabilities extension", &members};

    // Whether the VkDevice supports the VK_KHR_external_semaphore_capabilities extension.
    Feature supportsExternalSemaphoreCapabilities = {
        "supportsExternalSemaphoreCapabilities", FeatureCategory::VulkanFeatures,
        "VkDevice supports the VK_KHR_external_semaphore_capabilities extension", &members};

    // Whether the VkDevice supports the VK_KHR_external_semaphore_fd extension, on which the
    // GL_EXT_semaphore_fd extension can be layered.
    Feature supportsExternalSemaphoreFd = {
        "supportsExternalSemaphoreFd", FeatureCategory::VulkanFeatures,
        "VkDevice supports the VK_KHR_external_semaphore_fd extension", &members};

    // Whether the VkDevice supports the VK_FUCHSIA_external_semaphore
    // extension, on which the GL_ANGLE_semaphore_fuchsia extension can be layered.
    angle::Feature supportsExternalSemaphoreFuchsia = {
        "supportsExternalSemaphoreFuchsia", FeatureCategory::VulkanFeatures,
        "VkDevice supports the VK_FUCHSIA_external_semaphore extension", &members};

    // Whether the VkDevice supports the VK_KHR_external_fence_fd extension, on which the
    // EGL_ANDROID_native_fence extension can be layered.
    Feature supportsExternalFenceFd = {"supportsExternalFenceFd", FeatureCategory::VulkanFeatures,
                                       "VkDevice supports the VK_KHR_external_fence_fd extension",
                                       &members, "http://anglebug.com/2517"};

    // Whether the VkDevice can support EGL_ANDROID_native_fence_sync extension.
    Feature supportsAndroidNativeFenceSync = {
        "supportsAndroidNativeFenceSync", FeatureCategory::VulkanFeatures,
        "VkDevice supports the EGL_ANDROID_native_fence_sync extension", &members,
        "http://anglebug.com/2517"};

    // Whether the VkDevice can support the imageCubeArray feature properly.
    Feature supportsImageCubeArray = {"supportsImageCubeArray", FeatureCategory::VulkanFeatures,
                                      "VkDevice supports the imageCubeArray feature properly",
                                      &members, "http://anglebug.com/3584"};

    // Whether the VkDevice supports the pipelineStatisticsQuery feature.
    Feature supportsPipelineStatisticsQuery = {
        "supportsPipelineStatisticsQuery", FeatureCategory::VulkanFeatures,
        "VkDevice supports the pipelineStatisticsQuery feature", &members,
        "http://anglebug.com/5430"};

    // Whether the VkDevice supports the VK_EXT_shader_stencil_export extension, which is used to
    // perform multisampled resolve of stencil buffer.  A multi-step workaround is used instead if
    // this extension is not available.
    Feature supportsShaderStencilExport = {
        "supportsShaderStencilExport", FeatureCategory::VulkanFeatures,
        "VkDevice supports the VK_EXT_shader_stencil_export extension", &members};

    // Whether the VkDevice supports the VK_KHR_sampler_ycbcr_conversion extension, which is needed
    // to support Ycbcr conversion with external images.
    Feature supportsYUVSamplerConversion = {
        "supportsYUVSamplerConversion", FeatureCategory::VulkanFeatures,
        "VkDevice supports the VK_KHR_sampler_ycbcr_conversion extension", &members};

    // Where VK_EXT_transform_feedback is not support, an emulation path is used.
    // http://anglebug.com/3205
    Feature emulateTransformFeedback = {
        "emulateTransformFeedback", FeatureCategory::VulkanFeatures,
        "Emulate transform feedback as the VK_EXT_transform_feedback is not present.", &members,
        "http://anglebug.com/3205"};

    // Where VK_EXT_transform_feedback is supported, it's preferred over an emulation path.
    // http://anglebug.com/3206
    Feature supportsTransformFeedbackExtension = {
        "supportsTransformFeedbackExtension", FeatureCategory::VulkanFeatures,
        "Transform feedback uses the VK_EXT_transform_feedback extension.", &members,
        "http://anglebug.com/3206"};

    Feature supportsGeometryStreamsCapability = {
        "supportsGeometryStreamsCapability", FeatureCategory::VulkanFeatures,
        "Implementation supports the GeometryStreams SPIR-V capability.", &members,
        "http://anglebug.com/3206"};

    // Whether the VkDevice supports the VK_EXT_index_type_uint8 extension
    // http://anglebug.com/4405
    Feature supportsIndexTypeUint8 = {"supportsIndexTypeUint8", FeatureCategory::VulkanFeatures,
                                      "VkDevice supports the VK_EXT_index_type_uint8 extension",
                                      &members, "http://anglebug.com/4405"};

    // Whether the VkDevice supports the VK_EXT_custom_border_color extension
    // http://anglebug.com/3577
    Feature supportsCustomBorderColorEXT = {
        "supports_custom_border_color", FeatureCategory::VulkanFeatures,
        "VkDevice supports the VK_EXT_custom_border_color extension", &members,
        "http://anglebug.com/3577"};

    // Whether the VkDevice supports multiDrawIndirect (drawIndirect with drawCount > 1)
    // http://anglebug.com/6439
    Feature supportsMultiDrawIndirect = {
        "supportsMultiDrawIndirect", FeatureCategory::VulkanFeatures,
        "VkDevice supports the multiDrawIndirect extension", &members, "http://anglebug.com/6439"};

    // Whether the VkDevice supports the VK_KHR_depth_stencil_resolve extension with the
    // independentResolveNone feature.
    // http://anglebug.com/4836
    Feature supportsDepthStencilResolve = {"supportsDepthStencilResolve",
                                           FeatureCategory::VulkanFeatures,
                                           "VkDevice supports the VK_KHR_depth_stencil_resolve "
                                           "extension with the independentResolveNone feature",
                                           &members, "http://anglebug.com/4836"};

    // Whether the VkDevice supports the VK_EXT_multisampled_render_to_single_sampled extension.
    // http://anglebug.com/4836
    Feature supportsMultisampledRenderToSingleSampled = {
        "supportsMultisampledRenderToSingleSampled", FeatureCategory::VulkanFeatures,
        "VkDevice supports the VK_EXT_multisampled_render_to_single_sampled extension", &members,
        "http://anglebug.com/4836"};

    // Whether the VkDevice supports the VK_KHR_multiview extension.  http://anglebug.com/6048
    Feature supportsMultiview = {"supportsMultiview", FeatureCategory::VulkanFeatures,
                                 "VkDevice supports the VK_KHR_multiview extension", &members,
                                 "http://anglebug.com/6048"};

    // VK_PRESENT_MODE_FIFO_KHR causes random timeouts on Linux Intel. http://anglebug.com/3153
    Feature disableFifoPresentMode = {"disableFifoPresentMode", FeatureCategory::VulkanWorkarounds,
                                      "VK_PRESENT_MODE_FIFO_KHR causes random timeouts", &members,
                                      "http://anglebug.com/3153"};

    // On Qualcomm, gaps in bound descriptor set indices causes the post-gap sets to misbehave.
    // For example, binding only descriptor set 3 results in zero being read from a uniform buffer
    // object within that set.  This flag results in empty descriptor sets being bound for any
    // unused descriptor set to work around this issue.  http://anglebug.com/2727
    Feature bindEmptyForUnusedDescriptorSets = {
        "bindEmptyForUnusedDescriptorSets", FeatureCategory::VulkanWorkarounds,
        "Gaps in bound descriptor set indices causes the post-gap sets to misbehave", &members,
        "http://anglebug.com/2727"};

    // OES_depth_texture is a commonly expected feature on Android. However it
    // requires that D16_UNORM support texture filtering
    // (e.g. VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT) and some devices
    // do not. Work-around this by setting saying D16_UNORM supports filtering
    // anyway.
    Feature forceD16TexFilter = {
        "forceD16TexFilter", FeatureCategory::VulkanWorkarounds,
        "VK_FORMAT_D16_UNORM does not support VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT, "
        "which prevents OES_depth_texture from being supported.",
        &members, "http://anglebug.com/3452"};

    // On some android devices, vkCmdBlitImage with flipped coordinates blits incorrectly.  This
    // workaround makes sure this path is avoided.  http://anglebug.com/3498
    Feature disableFlippingBlitWithCommand = {
        "disableFlippingBlitWithCommand", FeatureCategory::VulkanWorkarounds,
        "vkCmdBlitImage with flipped coordinates blits incorrectly.", &members,
        "http://anglebug.com/3498"};

    // On platform with Intel or AMD GPU, a window resizing would not trigger the vulkan driver to
    // return VK_ERROR_OUT_OF_DATE on swapchain present.  Work-around by query current window extent
    // every frame to detect a window resizing.
    // http://anglebug.com/3623, http://anglebug.com/3624, http://anglebug.com/3625
    Feature perFrameWindowSizeQuery = {
        "perFrameWindowSizeQuery", FeatureCategory::VulkanWorkarounds,
        "Vulkan swapchain is not returning VK_ERROR_OUT_OF_DATE when window resizing", &members,
        "http://anglebug.com/3623, http://anglebug.com/3624, http://anglebug.com/3625"};

    // Seamful cube map emulation misbehaves on the AMD windows driver, so it's disallowed.
    Feature disallowSeamfulCubeMapEmulation = {
        "disallowSeamfulCubeMapEmulation", FeatureCategory::VulkanWorkarounds,
        "Seamful cube map emulation misbehaves on some drivers, so it's disallowed", &members,
        "http://anglebug.com/3243"};

    // Vulkan considers vertex attribute accesses to count up to the last multiple of the stride.
    // This additional access supports AMD's robust buffer access implementation.
    // AMDVLK in particular will return incorrect values when the vertex access extends into the
    // range that would be the stride padding and the buffer is too small.
    // This workaround limits GL_MAX_VERTEX_ATTRIB_STRIDE to a reasonable value and pads out
    // every buffer allocation size to be large enough to support a maximum vertex stride.
    // http://anglebug.com/4428
    Feature padBuffersToMaxVertexAttribStride = {
        "padBuffersToMaxVertexAttribStride", FeatureCategory::VulkanWorkarounds,
        "Vulkan considers vertex attribute accesses to count up to the last multiple of the "
        "stride. This additional access supports AMD's robust buffer access implementation. "
        "AMDVLK in particular will return incorrect values when the vertex access extends into "
        "the range that would be the stride padding and the buffer is too small. "
        "This workaround limits GL_MAX_VERTEX_ATTRIB_STRIDE to a maximum value and "
        "pads up every buffer allocation size to be a multiple of the maximum stride.",
        &members, "http://anglebug.com/4428"};

    // Whether the VkDevice supports the VK_EXT_external_memory_dma_buf and
    // VK_EXT_image_drm_format_modifier extensions.  These extensions are always used together to
    // implement EGL_EXT_image_dma_buf_import and EGL_EXT_image_dma_buf_import_modifiers.
    Feature supportsExternalMemoryDmaBufAndModifiers = {
        "supportsExternalMemoryDmaBufAndModifiers", FeatureCategory::VulkanFeatures,
        "VkDevice supports the VK_EXT_external_memory_dma_buf and VK_EXT_image_drm_format_modifier "
        "extensions",
        &members, "http://anglebug.com/6248"};

    // Whether the VkDevice supports the VK_EXT_external_memory_host extension, on which the
    // ANGLE_iosurface_client_buffer extension can be layered.
    Feature supportsExternalMemoryHost = {
        "supportsExternalMemoryHost", FeatureCategory::VulkanFeatures,
        "VkDevice supports the VK_EXT_external_memory_host extension", &members};

    // Whether to fill new buffers and textures with nonzero data to sanitize robust resource
    // initialization and flush out assumptions about zero init.
    Feature allocateNonZeroMemory = {
        "allocateNonZeroMemory", FeatureCategory::VulkanFeatures,
        "Fill new allocations with non-zero values to flush out errors.", &members,
        "http://anglebug.com/4384"};

    // Whether to log each callback from the VK_EXT_device_memory_report extension.  This feature is
    // used for trying to debug GPU memory leaks.
    Feature logMemoryReportCallbacks = {"logMemoryReportCallbacks", FeatureCategory::VulkanFeatures,
                                        "Log each callback from VK_EXT_device_memory_report",
                                        &members};

    // Whether to log statistics from the VK_EXT_device_memory_report extension each eglSwapBuffer.
    Feature logMemoryReportStats = {"logMemoryReportStats", FeatureCategory::VulkanFeatures,
                                    "Log stats from VK_EXT_device_memory_report each swap",
                                    &members};

    // Allocate a "shadow" buffer for GL buffer objects. For GPU-read only buffers
    // glMap* latency can be reduced by maintaining a copy of the buffer which is
    // writeable only by the CPU. We then return this shadow buffer on glMap* calls.
    Feature shadowBuffers = {
        "shadowBuffers", FeatureCategory::VulkanFeatures,
        "Allocate a shadow buffer for GL buffer objects to reduce glMap* latency.", &members,
        "http://anglebug.com/4339"};

    // When we update buffer data we usually face a choice to either clone a buffer and copy the
    // data or stage a buffer update and use the GPU to do the copy. For some GPUs, a performance
    // penalty to use the GPU to do copies. Setting this flag to true will always try to create a
    // new buffer and use the CPU to copy data when possible.
    Feature preferCPUForBufferSubData = {
        "preferCPUForBufferSubData", FeatureCategory::VulkanFeatures,
        "Prefer use CPU to do bufferSubData instead of staged update.", &members,
        "http://issuetracker.google.com/200067929"};

    // Persistently map buffer memory until destroy, saves on map/unmap IOCTL overhead
    // for buffers that are updated frequently.
    Feature persistentlyMappedBuffers = {
        "persistentlyMappedBuffers", FeatureCategory::VulkanFeatures,
        "Persistently map buffer memory to reduce map/unmap IOCTL overhead.", &members,
        "http://anglebug.com/2162"};

    // Android needs to pre-rotate surfaces that are not oriented per the native device's
    // orientation (e.g. a landscape application on a Pixel phone).  This feature works for
    // full-screen applications. http://anglebug.com/3502
    Feature enablePreRotateSurfaces = {"enablePreRotateSurfaces", FeatureCategory::VulkanFeatures,
                                       "Enable Android pre-rotation for landscape applications",
                                       &members, "http://anglebug.com/3502"};

    // Enable precision qualifiers for shaders generated by Vulkan backend http://anglebug.com/3078
    Feature enablePrecisionQualifiers = {
        "enablePrecisionQualifiers", FeatureCategory::VulkanFeatures,
        "Enable precision qualifiers in shaders", &members, "http://anglebug.com/3078"};

    // Desktop (at least NVIDIA) drivers prefer combining barriers into one vkCmdPipelineBarrier
    // call over issuing multiple barrier calls with fine grained dependency information to have
    // better performance. http://anglebug.com/4633
    Feature preferAggregateBarrierCalls = {
        "preferAggregateBarrierCalls", FeatureCategory::VulkanWorkarounds,
        "Single barrier call is preferred over multiple calls with "
        "fine grained pipeline stage dependency information",
        &members, "http://anglebug.com/4633"};

    // Tell the Vulkan back-end to use the async command queue to dispatch work to the GPU. Command
    // buffer work will happened in a worker thread. Otherwise use Renderer::CommandQueue directly.
    Feature asyncCommandQueue = {"asyncCommandQueue", FeatureCategory::VulkanFeatures,
                                 "Use CommandQueue worker thread to dispatch work to GPU.",
                                 &members, "http://anglebug.com/4324"};

    // Whether the VkDevice supports the VK_KHR_shader_float16_int8 extension and has the
    // shaderFloat16 feature.
    Feature supportsShaderFloat16 = {"supportsShaderFloat16", FeatureCategory::VulkanFeatures,
                                     "VkDevice supports the VK_KHR_shader_float16_int8 extension "
                                     "and has the shaderFloat16 feature",
                                     &members, "http://anglebug.com/4551"};

    // Some devices don't meet the limits required to perform mipmap generation using the built-in
    // compute shader.  On some other devices, VK_IMAGE_USAGE_STORAGE_BIT is detrimental to
    // performance, making this solution impractical.
    Feature allowGenerateMipmapWithCompute = {
        "allowGenerateMipmapWithCompute", FeatureCategory::VulkanFeatures,
        "Use the compute path to generate mipmaps on devices that meet the minimum requirements, "
        "and the performance is better.",
        &members, "http://anglebug.com/4551"};

    // Whether the VkDevice supports the VK_QCOM_render_pass_store_ops extension
    // http://anglebug.com/5505
    Feature supportsRenderPassStoreOpNoneQCOM = {
        "supportsRenderPassStoreOpNoneQCOM", FeatureCategory::VulkanFeatures,
        "VkDevice supports VK_QCOM_render_pass_store_ops extension.", &members,
        "http://anglebug.com/5055"};

    // Whether the VkDevice supports the VK_EXT_load_store_op_none extension
    // http://anglebug.com/5371
    Feature supportsRenderPassLoadStoreOpNone = {
        "supportsRenderPassLoadStoreOpNone", FeatureCategory::VulkanFeatures,
        "VkDevice supports VK_EXT_load_store_op_none extension.", &members,
        "http://anglebug.com/5371"};

    // Force maxUniformBufferSize to 16K on Qualcomm's Adreno 540. Pixel2's Adreno540 reports
    // maxUniformBufferSize 64k but various tests failed with that size. For that specific
    // device, we set to 16k for now which is known to pass all tests.
    // https://issuetracker.google.com/161903006
    Feature forceMaxUniformBufferSize16KB = {
        "forceMaxUniformBufferSize16KB", FeatureCategory::VulkanWorkarounds,
        "Force max uniform buffer size to 16K on some device due to bug", &members,
        "https://issuetracker.google.com/161903006"};

    // Enable mutable bit by default for ICD's that support VK_KHR_image_format_list.
    // http://anglebug.com/5281
    Feature supportsImageFormatList = {
        "supportsImageFormatList", FeatureCategory::VulkanFeatures,
        "Enable VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT by default for ICDs "
        "that support VK_KHR_image_format_list",
        &members, "http://anglebug.com/5281"};

    // Swiftshader on mac fails to initialize WebGL context when EXT_multisampled_render_to_texture
    // is used by Chromium.
    // http://anglebug.com/4937
    Feature enableMultisampledRenderToTexture = {
        "enableMultisampledRenderToTexture", FeatureCategory::VulkanWorkarounds,
        "Expose EXT_multisampled_render_to_texture", &members, "http://anglebug.com/4937"};

    // Qualcomm fails some tests when reducing the preferred block size to 4M.
    // http://anglebug.com/4995
    Feature preferredLargeHeapBlockSize4MB = {
        "preferredLargeHeapBlockSize4MB", FeatureCategory::VulkanWorkarounds,
        "Use 4 MB preferred large heap block size with AMD allocator", &members,
        "http://anglebug.com/4995"};

    // Manhattan is calling glFlush in the middle of renderpass which breaks renderpass and hurts
    // performance on tile based GPU. When this is enabled, we will defer the glFlush call made in
    // the middle of renderpass to the end of renderpass.
    // https://issuetracker.google.com/issues/166475273
    Feature deferFlushUntilEndRenderPass = {
        "deferFlushUntilEndRenderPass", FeatureCategory::VulkanWorkarounds,
        "Allow glFlush to be deferred until renderpass ends", &members,
        "https://issuetracker.google.com/issues/166475273"};

    // Android mistakenly destroys oldSwapchain passed to vkCreateSwapchainKHR, causing crashes on
    // certain drivers.  http://anglebug.com/5061
    Feature waitIdleBeforeSwapchainRecreation = {
        "waitIdleBeforeSwapchainRecreation", FeatureCategory::VulkanWorkarounds,
        "Before passing an oldSwapchain to VkSwapchainCreateInfoKHR, wait for queue to be idle. "
        "Works around a bug on platforms which destroy oldSwapchain in vkCreateSwapchainKHR.",
        &members, "http://anglebug.com/5061"};

    // Allow forcing an LOD offset on all sampling operations for performance comparisons. ANGLE is
    // non-conformant if this feature is enabled.
    std::array<angle::Feature, 4> forceTextureLODOffset = {
        angle::Feature{"force_texture_lod_offset_1", angle::FeatureCategory::VulkanWorkarounds,
                       "Increase the minimum texture level-of-detail by 1 when sampling.",
                       &members},
        angle::Feature{"force_texture_lod_offset_2", angle::FeatureCategory::VulkanWorkarounds,
                       "Increase the minimum texture level-of-detail by 2 when sampling.",
                       &members},
        angle::Feature{"force_texture_lod_offset_3", angle::FeatureCategory::VulkanWorkarounds,
                       "Increase the minimum texture level-of-detail by 3 when sampling.",
                       &members},
        angle::Feature{"force_texture_lod_offset_4", angle::FeatureCategory::VulkanWorkarounds,
                       "Increase the minimum texture level-of-detail by 4 when sampling.",
                       &members},
    };

    // Translate non-nearest filtering modes to nearest for all samplers for performance
    // comparisons. ANGLE is non-conformant if this feature is enabled.
    Feature forceNearestFiltering = {"force_nearest_filtering", FeatureCategory::VulkanWorkarounds,
                                     "Force nearest filtering when sampling.", &members};

    // Translate  non-nearest mip filtering modes to nearest mip for all samplers for performance
    // comparisons. ANGLE is non-conformant if this feature is enabled.
    Feature forceNearestMipFiltering = {"forceNearestMipFiltering",
                                        FeatureCategory::VulkanWorkarounds,
                                        "Force nearest mip filtering when sampling.", &members};

    // Compress float32 vertices in static buffers to float16 at draw time. ANGLE is non-conformant
    // if this feature is enabled.
    angle::Feature compressVertexData = {"compress_vertex_data",
                                         angle::FeatureCategory::VulkanWorkarounds,
                                         "Compress vertex data to smaller data types when "
                                         "possible. Using this feature makes ANGLE non-conformant.",
                                         &members};

    // Qualcomm missynchronizes vkCmdClearAttachments in the middle of render pass.
    // https://issuetracker.google.com/166809097
    Feature preferDrawClearOverVkCmdClearAttachments = {
        "preferDrawClearOverVkCmdClearAttachments", FeatureCategory::VulkanWorkarounds,
        "On some hardware, clear using a draw call instead of vkCmdClearAttachments in the middle "
        "of render pass due to bugs",
        &members, "https://issuetracker.google.com/166809097"};

    // Whether prerotation is being emulated for testing.  90 degree rotation.
    Feature emulatedPrerotation90 = {"emulatedPrerotation90", FeatureCategory::VulkanFeatures,
                                     "Emulate 90-degree prerotation.", &members,
                                     "http://anglebug.com/4901"};

    // Whether prerotation is being emulated for testing.  180 degree rotation.
    Feature emulatedPrerotation180 = {"emulatedPrerotation180", FeatureCategory::VulkanFeatures,
                                      "Emulate 180-degree prerotation.", &members,
                                      "http://anglebug.com/4901"};

    // Whether prerotation is being emulated for testing.  270 degree rotation.
    Feature emulatedPrerotation270 = {"emulatedPrerotation270", FeatureCategory::VulkanFeatures,
                                      "Emulate 270-degree prerotation.", &members,
                                      "http://anglebug.com/4901"};

    // Whether SPIR-V should be generated through glslang.  Transitory feature while confidence is
    // built on the SPIR-V generation code.
    Feature generateSPIRVThroughGlslang = {
        "generateSPIRVThroughGlslang", FeatureCategory::VulkanFeatures,
        "Translate SPIR-V through glslang.", &members, "http://anglebug.com/4889"};

    // Whether we should use driver uniforms over specialization constants for some shader
    // modifications like yflip and rotation.
    Feature forceDriverUniformOverSpecConst = {
        "forceDriverUniformOverSpecConst", FeatureCategory::VulkanWorkarounds,
        "Forces using driver uniforms instead of specialization constants.", &members,
        "http://issuetracker.google.com/173636783"};

    // Whether non-conformant configurations and extensions should be exposed.  When an extension is
    // in development, or a GLES version is not supported on a device, we may still want to expose
    // them for partial testing.  This feature is enabled by our test harness.
    Feature exposeNonConformantExtensionsAndVersions = {
        "exposeNonConformantExtensionsAndVersions", FeatureCategory::VulkanWorkarounds,
        "Expose GLES versions and extensions that are not conformant.", &members,
        "http://anglebug.com/5375"};

    // imageAtomicExchange is expected to work for r32f formats, but support for atomic operations
    // for VK_FORMAT_R32_SFLOAT is rare.  This support is emulated by using an r32ui format for such
    // images instead.
    Feature emulateR32fImageAtomicExchange = {
        "emulateR32fImageAtomicExchange", FeatureCategory::VulkanWorkarounds,
        "Emulate r32f images with r32ui to support imageAtomicExchange.", &members,
        "http://anglebug.com/5535"};

    Feature supportsNegativeViewport = {
        "supportsNegativeViewport", FeatureCategory::VulkanFeatures,
        "The driver supports inverting the viewport with a negative height.", &members};

    // Whether we should force any highp precision in the fragment shader to mediump.
    // ANGLE is non-conformant if this feature is enabled.
    Feature forceFragmentShaderPrecisionHighpToMediump = {
        "forceFragmentShaderPrecisionHighpToMediump", FeatureCategory::VulkanWorkarounds,
        "Forces highp precision in fragment shader to mediump.", &members,
        "https://issuetracker.google.com/184850002"};

    // Whether we should submit at each FBO boundary.
    Feature preferSubmitAtFBOBoundary = {
        "preferSubmitAtFBOBoundary", FeatureCategory::VulkanWorkarounds,
        "Submit commands to driver at each FBO boundary for performance improvements.", &members,
        "https://issuetracker.google.com/187425444"};

    // Workaround for gap in Vulkan spec related to querying descriptor count for immutable samplers
    // tied to an external format.
    Feature useMultipleDescriptorsForExternalFormats = {
        "useMultipleDescriptorsForExternalFormats", FeatureCategory::VulkanWorkarounds,
        "Return a default descriptor count for external formats.", &members,
        "http://anglebug.com/6141"};

    // Whether the VkDevice can support Protected Memory.
    Feature supportsProtectedMemory = {"supports_protected_memory", FeatureCategory::VulkanFeatures,
                                       "VkDevice supports protected memory", &members,
                                       "http://anglebug.com/3965"};

    // Whether the VkDevice supports the VK_EXT_host_query_reset extension
    // http://anglebug.com/6692
    Feature supportsHostQueryReset = {"supportsHostQueryReset", FeatureCategory::VulkanFeatures,
                                      "VkDevice supports VK_EXT_host_query_reset extension",
                                      &members, "http://anglebug.com/6692"};

    // Whether the VkInstance supports the VK_KHR_get_surface_capabilities2 extension.
    Feature supportsSurfaceCapabilities2Extension = {
        "supportsSurfaceCapabilities2Extension", FeatureCategory::VulkanFeatures,
        "VkInstance supports the VK_KHR_get_surface_capabilities2 extension", &members};

    // Whether the VkInstance supports the VK_KHR_surface_protected_capabilities extension.
    Feature supportsSurfaceProtectedCapabilitiesExtension = {
        "supportsSurfaceProtectedCapabilitiesExtension", FeatureCategory::VulkanFeatures,
        "VkInstance supports the VK_KHR_surface_protected_capabilities extension", &members};

    // Whether the VkSurface supports protected swapchains from
    // supportsSurfaceProtectedCapabilitiesExtension.
    Feature supportsSurfaceProtectedSwapchains = {
        "supportsSurfaceProtectedSwapchains", FeatureCategory::VulkanFeatures,
        "VkSurface supportsProtected for protected swapchains", &members};

    // Whether surface format GL_RGB8 should be overridden to GL_RGBA8.
    Feature overrideSurfaceFormatRGB8toRGBA8 = {
        "overrideSurfaceFormatRGB8toRGBA8", FeatureCategory::VulkanWorkarounds,
        "Override surface format GL_RGB8 to GL_RGBA8", &members, "http://anglebug.com/6651"};

    // Whether the VkSurface supports VK_KHR_shared_presentable_images.
    Feature supportsSharedPresentableImageExtension = {
        "supportsSharedPresentableImageExtension", FeatureCategory::VulkanFeatures,
        "VkSurface supports the VK_KHR_shared_presentable_images extension", &members};
};

inline FeaturesVk::FeaturesVk()  = default;
inline FeaturesVk::~FeaturesVk() = default;

}  // namespace angle

#endif  // ANGLE_PLATFORM_FEATURESVK_H_
