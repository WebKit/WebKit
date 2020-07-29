//
// Copyright 2020 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// VulkanFramebufferTest:
//   Tests to validate our Vulkan framebuffer image allocation.
//

#include "test_utils/ANGLETest.h"
#include "test_utils/angle_test_instantiate.h"
// 'None' is defined as 'struct None {};' in
// third_party/googletest/src/googletest/include/gtest/internal/gtest-type-util.h.
// But 'None' is also defined as a numeric constant 0L in <X11/X.h>.
// So we need to include ANGLETest.h first to avoid this conflict.

#include "libANGLE/Context.h"
#include "libANGLE/angletypes.h"
#include "libANGLE/renderer/vulkan/ContextVk.h"
#include "libANGLE/renderer/vulkan/ProgramVk.h"
#include "libANGLE/renderer/vulkan/TextureVk.h"
#include "test_utils/gl_raii.h"
#include "util/EGLWindow.h"
#include "util/shader_utils.h"

using namespace angle;

namespace
{

class VulkanFramebufferTest : public ANGLETest
{
  protected:
    rx::ContextVk *hackANGLE() const
    {
        // Hack the angle!
        const gl::Context *context = static_cast<gl::Context *>(getEGLWindow()->getContext());
        return rx::GetImplAs<rx::ContextVk>(context);
    }

    rx::TextureVk *hackTexture(GLuint handle) const
    {
        // Hack the angle!
        const gl::Context *context = static_cast<gl::Context *>(getEGLWindow()->getContext());
        const gl::Texture *texture = context->getTexture({handle});
        return rx::vk::GetImpl(texture);
    }
};

// Test that framebuffer can be created from a mip-incomplete texture, and that its allocation only
// includes the framebuffer's attached mip.
TEST_P(VulkanFramebufferTest, TextureAttachmentMipIncomplete)
{
    GLFramebuffer framebuffer;
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);

    GLTexture texture;
    glBindTexture(GL_TEXTURE_2D, texture);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 100, 100, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexImage2D(GL_TEXTURE_2D, 1, GL_RGBA8, 2, 2, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexImage2D(GL_TEXTURE_2D, 2, GL_RGBA8, 5, 8, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);

    // Set framebuffer to mip 0.  Framebuffer should be complete, and make the texture allocate
    // an image of only 1 level.
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture, 0);
    EXPECT_GL_FRAMEBUFFER_COMPLETE(GL_FRAMEBUFFER);

    glClearColor(0, 0, 0, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::black);

    // http://anglebug.com/4686: The Vulkan backend is allocating three mips of sizes 100x100,
    // 50x50 and 25x25 instead of one mip of size 100x100.
    ANGLE_SKIP_TEST_IF(IsVulkan());

    rx::TextureVk *textureVk = hackTexture(texture);
    EXPECT_EQ(textureVk->getImage().getLevelCount(), 1u);
}

ANGLE_INSTANTIATE_TEST(VulkanFramebufferTest, ES3_VULKAN());

}  // anonymous namespace
