//
// Copyright 2020 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// DisplayVkHeadless.cpp:
//    Implements the class methods for DisplayVkHeadless.
//

#include "DisplayVkHeadless.h"
#include "WindowSurfaceVkHeadless.h"

#include "libANGLE/Display.h"
#include "libANGLE/renderer/vulkan/vk_caps_utils.h"
#include "libANGLE/renderer/vulkan/vk_renderer.h"

namespace rx
{

DisplayVkHeadless::DisplayVkHeadless(const egl::DisplayState &state) : DisplayVkLinux(state) {}

void DisplayVkHeadless::terminate()
{
    DisplayVk::terminate();
}

bool DisplayVkHeadless::isValidNativeWindow(EGLNativeWindowType window) const
{
    return true;
}

SurfaceImpl *DisplayVkHeadless::createWindowSurfaceVk(const egl::SurfaceState &state,
                                                      EGLNativeWindowType window)
{
    return new WindowSurfaceVkHeadless(state, window);
}

egl::ConfigSet DisplayVkHeadless::generateConfigs()
{
    std::vector<GLenum> kColorFormats        = {GL_RGBA8, GL_BGRA8_EXT, GL_RGB565, GL_RGB8};
    std::vector<GLenum> kDesiredColorFormats = {GL_RGB10_A2};

    for (GLenum glFormat : kDesiredColorFormats)
    {
        VkFormat vkFormat = mRenderer->getFormat(glFormat).getActualRenderableImageVkFormat();
        ASSERT(vkFormat != VK_FORMAT_UNDEFINED);

        angle::FormatID actualFormatID = vk::GetFormatIDFromVkFormat(vkFormat);
        if (mRenderer->hasImageFormatFeatureBits(
                actualFormatID, VK_FORMAT_FEATURE_BLIT_SRC_BIT | VK_FORMAT_FEATURE_BLIT_DST_BIT |
                                    VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT |
                                    VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BLEND_BIT |
                                    VK_FORMAT_FEATURE_TRANSFER_SRC_BIT |
                                    VK_FORMAT_FEATURE_TRANSFER_DST_BIT))
        {
            kColorFormats.push_back(glFormat);
        }
    }

    return egl_vk::GenerateConfigs(kColorFormats.data(), kColorFormats.size(),
                                   egl_vk::kConfigDepthStencilFormats,
                                   ArraySize(egl_vk::kConfigDepthStencilFormats), this);
}

void DisplayVkHeadless::checkConfigSupport(egl::Config *config) {}

const char *DisplayVkHeadless::getWSIExtension() const
{
    return VK_EXT_HEADLESS_SURFACE_EXTENSION_NAME;
}

bool IsVulkanHeadlessDisplayAvailable()
{
    return true;
}

DisplayImpl *CreateVulkanHeadlessDisplay(const egl::DisplayState &state)
{
    return new DisplayVkHeadless(state);
}

}  // namespace rx
