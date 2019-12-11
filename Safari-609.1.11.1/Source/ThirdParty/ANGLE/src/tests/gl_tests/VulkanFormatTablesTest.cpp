//
// Copyright 2018 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// VulkanFormatTablesTest:
//   Tests to validate our Vulkan support tables match hardware support.
//

#include "test_utils/ANGLETest.h"
#include "test_utils/angle_test_instantiate.h"
// 'None' is defined as 'struct None {};' in
// third_party/googletest/src/googletest/include/gtest/internal/gtest-type-util.h.
// But 'None' is also defined as a numeric constant 0L in <X11/X.h>.
// So we need to include ANGLETest.h first to avoid this conflict.

#include "libANGLE/Context.h"
#include "libANGLE/angletypes.h"
#include "libANGLE/formatutils.h"
#include "libANGLE/renderer/vulkan/ContextVk.h"
#include "libANGLE/renderer/vulkan/RendererVk.h"
#include "util/EGLWindow.h"

using namespace angle;

namespace
{

class VulkanFormatTablesTest : public ANGLETest
{};

struct ParametersToTest
{
    VkImageType imageType;
    VkImageCreateFlags createFlags;
};

// This test enumerates all GL formats - for each, it queries the Vulkan support for
// using it as a texture, filterable, and a render target. It checks this against our
// speed-optimized baked tables, and validates they would give the same result.
TEST_P(VulkanFormatTablesTest, TestFormatSupport)
{
    ASSERT_TRUE(IsVulkan());

    // Hack the angle!
    const gl::Context *context = static_cast<gl::Context *>(getEGLWindow()->getContext());
    auto *contextVk            = rx::GetImplAs<rx::ContextVk>(context);
    rx::RendererVk *renderer   = contextVk->getRenderer();

    // We need to test normal 2D images as well as Cube images.
    const std::vector<ParametersToTest> parametersToTest = {
        {VK_IMAGE_TYPE_2D, 0}, {VK_IMAGE_TYPE_2D, VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT}};

    const gl::FormatSet &allFormats = gl::GetAllSizedInternalFormats();
    for (GLenum internalFormat : allFormats)
    {
        const rx::vk::Format &vkFormat = renderer->getFormat(internalFormat);

        // Similar loop as when we build caps in vk_caps_utils.cpp, but query using
        // vkGetPhysicalDeviceImageFormatProperties instead of vkGetPhysicalDeviceFormatProperties
        // and verify we have all the same caps.
        if (!vkFormat.valid())
        {
            // TODO(jmadill): Every angle format should be mapped to a vkFormat.
            // This hasn't been defined in our vk_format_map.json yet so the caps won't be filled.
            continue;
        }

        const gl::TextureCaps &textureCaps = renderer->getNativeTextureCaps().get(internalFormat);

        for (const ParametersToTest params : parametersToTest)
        {
            // Now lets verify that that agaisnt vulkan.
            VkFormatProperties formatProperties;
            vkGetPhysicalDeviceFormatProperties(renderer->getPhysicalDevice(),
                                                vkFormat.vkImageFormat, &formatProperties);

            VkImageFormatProperties imageProperties;

            // isTexturable?
            bool isTexturable =
                vkGetPhysicalDeviceImageFormatProperties(
                    renderer->getPhysicalDevice(), vkFormat.vkImageFormat, params.imageType,
                    VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_SAMPLED_BIT, params.createFlags,
                    &imageProperties) == VK_SUCCESS;
            EXPECT_EQ(isTexturable, textureCaps.texturable) << vkFormat.vkImageFormat;

            // TODO(jmadill): Support ES3 textures.

            // isFilterable?
            bool isFilterable = (formatProperties.optimalTilingFeatures &
                                 VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT) ==
                                VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT;
            EXPECT_EQ(isFilterable, textureCaps.filterable) << vkFormat.vkImageFormat;

            // isRenderable?
            const bool isRenderableColor =
                (vkGetPhysicalDeviceImageFormatProperties(
                    renderer->getPhysicalDevice(), vkFormat.vkImageFormat, params.imageType,
                    VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
                    params.createFlags, &imageProperties)) == VK_SUCCESS;
            const bool isRenderableDepthStencil =
                (vkGetPhysicalDeviceImageFormatProperties(
                    renderer->getPhysicalDevice(), vkFormat.vkImageFormat, params.imageType,
                    VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
                    params.createFlags, &imageProperties)) == VK_SUCCESS;

            bool isRenderable = isRenderableColor || isRenderableDepthStencil;
            EXPECT_EQ(isRenderable, textureCaps.textureAttachment) << vkFormat.vkImageFormat;
            EXPECT_EQ(isRenderable, textureCaps.renderbuffer) << vkFormat.vkImageFormat;
        }
    }
}

ANGLE_INSTANTIATE_TEST(VulkanFormatTablesTest, ES2_VULKAN());

}  // anonymous namespace
