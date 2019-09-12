//
// Copyright 2018 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// FeaturesVk.h: Optional features for the Vulkan renderer.
//

#ifndef ANGLE_PLATFORM_FEATURESVK_H_
#define ANGLE_PLATFORM_FEATURESVK_H_

namespace angle
{

struct FeaturesVk
{
    FeaturesVk();

    // Line segment rasterization must follow OpenGL rules. This means using an algorithm similar
    // to Bresenham's. Vulkan uses a different algorithm. This feature enables the use of pixel
    // shader patching to implement OpenGL basic line rasterization rules. This feature will
    // normally always be enabled. Exposing it as an option enables performance testing.
    bool basicGLLineRasterization = false;

    // Flips the viewport to render upside-down. This has the effect to render the same way as
    // OpenGL. If this feature gets enabled, we enable the KHR_MAINTENANCE_1 extension to allow
    // negative viewports. We inverse rendering to the backbuffer by reversing the height of the
    // viewport and increasing Y by the height. So if the viewport was (0,0,width,height), it
    // becomes (0, height, width, -height). Unfortunately, when we start doing this, we also need
    // to adjust a lot of places since the rendering now happens upside-down. Affected places so
    // far:
    // -readPixels
    // -copyTexImage
    // -framebuffer blit
    // -generating mipmaps
    // -Point sprites tests
    // -texStorage
    bool flipViewportY = false;

    // Add an extra copy region when using vkCmdCopyBuffer as the Windows Intel driver seems
    // to have a bug where the last region is ignored.
    bool extraCopyBufferRegion = false;

    // This flag is added for the sole purpose of end2end tests, to test the correctness
    // of various algorithms when a fallback format is used, such as using a packed format to
    // emulate a depth- or stencil-only format.
    bool forceFallbackFormat = false;

    // On some NVIDIA drivers the point size range reported from the API is inconsistent with the
    // actual behavior. Clamp the point size to the value from the API to fix this.
    // Tracked in http://anglebug.com/2970.
    bool clampPointSize = false;

    // On some android devices, the memory barrier between the compute shader that converts vertex
    // attributes and the vertex shader that reads from it is ineffective.  Only known workaround is
    // to perform a flush after the conversion.  http://anglebug.com/3016
    bool flushAfterVertexConversion = false;

    // Whether the VkDevice supports the VK_KHR_incremental_present extension, on which the
    // EGL_KHR_swap_buffers_with_damage extension can be layered.
    bool supportsIncrementalPresent = false;

    // Whether texture copies on cube map targets should be done on GPU.  This is a workaround for
    // Intel drivers on windows that have an issue with creating single-layer views on cube map
    // textures.
    bool forceCpuPathForCubeMapCopy = false;

    // Whether the VkDevice supports the VK_ANDROID_external_memory_android_hardware_buffer
    // extension, on which the EGL_ANDROID_image_native_buffer extension can be layered.
    bool supportsAndroidHardwareBuffer = false;

    // Whether the VkDevice supports the VK_KHR_external_memory_fd
    // extension, on which the GL_EXT_memory_object_fd extension can be layered.
    bool supportsExternalMemoryFd = false;

    // Whether the VkDevice supports the VK_KHR_external_semaphore_fd
    // extension, on which the GL_EXT_semaphore_fd extension can be layered.
    bool supportsExternalSemaphoreFd = false;

    // VK_PRESENT_MODE_FIFO_KHR causes random timeouts on Linux Intel. http://anglebug.com/3153
    bool disableFifoPresentMode = false;

    // On Qualcomm, a bug is preventing us from using loadOp=Clear with inline commands in the
    // render pass.  http://anglebug.com/2361
    bool restartRenderPassAfterLoadOpClear = false;
};

inline FeaturesVk::FeaturesVk() = default;
}  // namespace angle

#endif  // ANGLE_PLATFORM_FEATURESVK_H_
