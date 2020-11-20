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

    // Add an extra copy region when using vkCmdCopyBuffer as the Windows Intel driver seems
    // to have a bug where the last region is ignored.
    Feature extraCopyBufferRegion = {
        "extraCopyBufferRegion", FeatureCategory::VulkanWorkarounds,
        "Some drivers seem to have a bug where the last copy region in vkCmdCopyBuffer is ignored",
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

    // On some android devices, the memory barrier between the compute shader that converts vertex
    // attributes and the vertex shader that reads from it is ineffective.  Only known workaround is
    // to perform a flush after the conversion.  http://anglebug.com/3016
    Feature flushAfterVertexConversion = {
        "flushAfterVertexConversion", FeatureCategory::VulkanWorkarounds,
        "The memory barrier between the compute shader that converts vertex attributes and the "
        "vertex shader that reads from it is ineffective",
        &members, "http://anglebug.com/3016"};

    Feature supportsRenderpass2 = {"supportsRenderpass2", FeatureCategory::VulkanFeatures,
                                   "VkDevice supports the VK_KHR_create_renderpass2 extension",
                                   &members};

    // Whether the VkDevice supports the VK_KHR_incremental_present extension, on which the
    // EGL_KHR_swap_buffers_with_damage extension can be layered.
    Feature supportsIncrementalPresent = {
        "supportsIncrementalPresent", FeatureCategory::VulkanFeatures,
        "VkDevice supports the VK_KHR_incremental_present extension", &members};

    // Whether texture copies on cube map targets should be done on GPU.  This is a workaround for
    // Intel drivers on windows that have an issue with creating single-layer views on cube map
    // textures.
    Feature forceCPUPathForCubeMapCopy = {
        "forceCPUPathForCubeMapCopy", FeatureCategory::VulkanWorkarounds,
        "Some drivers have an issue with creating single-layer views on cube map textures",
        &members};

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

    // Whether the VkDevice can support imageCubeArray feature properly.
    Feature supportsImageCubeArray = {"supportsImageCubeArray", FeatureCategory::VulkanFeatures,
                                      "VkDevice supports the imageCubeArray feature properly",
                                      &members, "http://anglebug.com/3584"};

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

    // Whether the VkDevice supports the VK_EXT_index_type_uint8 extension
    // http://anglebug.com/4405
    Feature supportsIndexTypeUint8 = {"supportsIndexTypeUint8", FeatureCategory::VulkanFeatures,
                                      "VkDevice supports the VK_EXT_index_type_uint8 extension",
                                      &members, "http://anglebug.com/4405"};

    // Whether the VkDevice supports the VK_KHR_depth_stencil_resolve extension with the
    // independentResolveNone feature.
    // http://anglebug.com/4836
    Feature supportsDepthStencilResolve = {"supportsDepthStencilResolve",
                                           FeatureCategory::VulkanFeatures,
                                           "VkDevice supports the VK_KHR_depth_stencil_resolve "
                                           "extension with the independentResolveNone feature",
                                           &members, "http://anglebug.com/4836"};

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

    // Qualcomm and SwiftShader shader compiler doesn't support sampler arrays as parameters, so
    // revert to old RewriteStructSamplers behavior, which produces fewer.
    Feature forceOldRewriteStructSamplers = {
        "forceOldRewriteStructSamplers", FeatureCategory::VulkanWorkarounds,
        "Some shader compilers don't support sampler arrays as parameters, so revert to old "
        "RewriteStructSamplers behavior, which produces fewer.",
        &members, "http://anglebug.com/2703"};

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

    // Whether the VkDevice supports the VK_EXT_swapchain_colorspace extension
    // http://anglebug.com/2514
    Feature supportsSwapchainColorspace = {
        "supportsSwapchainColorspace", FeatureCategory::VulkanFeatures,
        "VkDevice supports the VK_EXT_swapchain_colorspace extension", &members,
        "http://anglebug.com/2514"};

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

    // Allocate a "shadow" buffer for GL buffer objects. For GPU-read only buffers
    // glMap* latency can be reduced by maintaining a copy of the buffer which is
    // writeable only by the CPU. We then return this shadow buffer on glMap* calls.
    Feature shadowBuffers = {
        "shadowBuffers", FeatureCategory::VulkanFeatures,
        "Allocate a shadow buffer for GL buffer objects to reduce glMap* latency.", &members,
        "http://anglebug.com/4339"};

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

    // Cache FramebufferVk objects. Currently hitting a bug on Apple: http://anglebug.com/4442
    Feature enableFramebufferVkCache = {"enableFramebufferVkCache", FeatureCategory::VulkanFeatures,
                                        "Enable FramebufferVk objects to be cached", &members,
                                        "http://anglebug.com/4442"};

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

    // Tell Vulkan back-end to use CommandProcessor class to dispatch work to the GPU. The work will
    // happen asynchronously in a different thread if asynchronousCommandProcessing is true.
    // Otherwise use Renderer::CommandQueue to dispatch work.
    Feature commandProcessor = {"commandProcessor", FeatureCategory::VulkanFeatures,
                                "Use CommandProcessor class to dispatch work to GPU.", &members,
                                "http://anglebug.com/4324"};

    // Enable parallel thread execution when commandProcessor is enabled.
    // Currently off by default.
    Feature asynchronousCommandProcessing = {"asynchronousCommandProcessing",
                                             FeatureCategory::VulkanFeatures,
                                             "Enable/Disable parallel processing of worker thread",
                                             &members, "http://anglebug.com/4324"};

    // Whether the VkDevice supports the VK_KHR_shader_float16_int8 extension and has the
    // shaderFloat16 feature.
    Feature supportsShaderFloat16 = {"supportsShaderFloat16", FeatureCategory::VulkanFeatures,
                                     "VkDevice supports the VK_KHR_shader_float16_int8 extension "
                                     "and has the shaderFloat16 feature",
                                     &members, "http://anglebug.com/4551"};

    // Whether the VkDevice supports the VK_EXT_shader_atomic_flat extension and has the
    // shaderImageFloat32Atomics feature
    Feature supportsShaderImageFloat32Atomics = {
        "supportsShaderImageFloat32Atomics", FeatureCategory::VulkanFeatures,
        "VkDevice supports the VK_EXT_shader_atomic_float extension and has the "
        "shaderImageFloat32Atomics feature.",
        &members, "http://anglebug.com/3578"};

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

    // Translate  non-nearest mip filtering modes to nearest mip for all samplers for performance
    // comparisons. ANGLE is non-conformant if this feature is enabled.
    Feature forceNearestMipFiltering = {"forceNearestMipFiltering",
                                        FeatureCategory::VulkanWorkarounds,
                                        "Force nearest mip filtering when sampling.", &members};

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
};

inline FeaturesVk::FeaturesVk()  = default;
inline FeaturesVk::~FeaturesVk() = default;

}  // namespace angle

#endif  // ANGLE_PLATFORM_FEATURESVK_H_
