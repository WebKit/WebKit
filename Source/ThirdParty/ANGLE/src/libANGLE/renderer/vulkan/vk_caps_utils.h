//
// Copyright 2018 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// vk_utils:
//    Helper functions for the Vulkan Caps.
//

#ifndef LIBANGLE_RENDERER_VULKAN_VK_CAPS_UTILS_H_
#define LIBANGLE_RENDERER_VULKAN_VK_CAPS_UTILS_H_

#include <vulkan/vulkan.h>

#include "libANGLE/Config.h"

namespace gl
{
struct Limitations;
struct Extensions;
class TextureCapsMap;
struct Caps;
struct TextureCaps;
struct InternalFormat;
}  // namespace gl

namespace rx
{
struct FeaturesVk;

class DisplayVk;

namespace egl_vk
{
constexpr GLenum kConfigDepthStencilFormats[] = {GL_NONE, GL_DEPTH24_STENCIL8, GL_DEPTH_COMPONENT24,
                                                 GL_DEPTH_COMPONENT16};

// Permutes over all combinations of color format, depth stencil format and sample count and
// generates a basic config which is passed to DisplayVk::checkConfigSupport.
egl::ConfigSet GenerateConfigs(const GLenum *colorFormats,
                               size_t colorFormatsCount,
                               const GLenum *depthStencilFormats,
                               size_t depthStencilFormatCount,
                               const EGLint *sampleCounts,
                               size_t sampleCountsCount,
                               DisplayVk *display);

template <size_t ColorFormatCount, size_t DepthStencilFormatCount, size_t SampleCountsCount>
egl::ConfigSet GenerateConfigs(const GLenum (&colorFormats)[ColorFormatCount],
                               const GLenum (&depthStencilFormats)[DepthStencilFormatCount],
                               const EGLint (&sampleCounts)[SampleCountsCount],
                               DisplayVk *display)
{
    return GenerateConfigs(colorFormats, ColorFormatCount, depthStencilFormats,
                           DepthStencilFormatCount, sampleCounts, SampleCountsCount, display);
}
}  // namespace egl_vk

}  // namespace rx

#endif
