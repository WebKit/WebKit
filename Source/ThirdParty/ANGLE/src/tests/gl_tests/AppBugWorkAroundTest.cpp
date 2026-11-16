//
// Copyright 2026 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#include <variant>

#include "test_utils/ANGLETest.h"

#include "common/gl_enum_utils.h"
#include "test_utils/angle_test_configs.h"
#include "test_utils/gl_raii.h"
#include "util/random_utils.h"
#include "util/shader_utils.h"
#include "util/test_utils.h"

using namespace angle;

namespace
{

class initializeColorAttachmentWithWhiteTest : public ANGLETest<>
{
  protected:
    initializeColorAttachmentWithWhiteTest()
    {
        setConfigRedBits(8);
        setConfigGreenBits(8);
        setConfigBlueBits(8);
        setConfigAlphaBits(8);
    }
};

// Test app bug workaround feature flag. InitializeColorAttachmentWithWhite should always clear the
// fbo color attachment to white even if not covered by quad.
TEST_P(initializeColorAttachmentWithWhiteTest, Run)
{
    constexpr uint32_t kWidth  = 128;
    constexpr uint32_t kHeight = 128;

    GLTexture texture;
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, kWidth, kHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);

    GLFramebuffer fbo;
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture, 0);
    glViewport(0, 0, kWidth, kHeight);

    // Draw top half with red
    ANGLE_GL_PROGRAM(redProgram, essl1_shaders::vs::Simple(), essl1_shaders::fs::Red());
    glScissor(0, 0, kWidth, kHeight / 2);
    glEnable(GL_SCISSOR_TEST);
    drawQuad(redProgram, essl1_shaders::PositionAttrib(), 0.5f);

    // The top half should be red
    EXPECT_PIXEL_RECT_EQ(0, 0, kWidth, kHeight / 2, GLColor::red);
    // The bottom half is un-rendered, should be white
    EXPECT_PIXEL_RECT_EQ(0, kHeight / 2, kWidth, kHeight / 2, GLColor::white);
}

GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(initializeColorAttachmentWithWhiteTest);
ANGLE_INSTANTIATE_TEST(initializeColorAttachmentWithWhiteTest,
                       ES3_VULKAN().enable(Feature::InitializeColorAttachmentWithWhite));

}  // anonymous namespace
