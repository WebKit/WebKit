//
// Copyright 2021 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// RobustFragmentShaderOutputTest: Tests for the custom ANGLE extension.

#include "test_utils/ANGLETest.h"
#include "test_utils/gl_raii.h"

#include "util/random_utils.h"

#include <stdint.h>

using namespace angle;

namespace
{
bool ExtEnabled()
{
    return IsGLExtensionEnabled("GL_ANGLE_robust_fragment_shader_output");
}
}  // namespace

class RobustFragmentShaderOutputTest : public ANGLETest<>
{
  public:
    RobustFragmentShaderOutputTest() {}
};

// Basic behaviour from the extension.
TEST_P(RobustFragmentShaderOutputTest, Basic)
{
    ANGLE_SKIP_TEST_IF(!ExtEnabled());

    const char kFS[] = R"(#version 300 es
precision mediump float;
out vec4 outvar;
void main() {
    outvar = vec4(0.0, 1.0, 0.0, 1.0);
})";
    ANGLE_GL_PROGRAM(testProgram, essl3_shaders::vs::Simple(), kFS);

    GLFramebuffer framebuffer;
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);

    constexpr GLsizei kSize = 2;
    std::vector<GLColor> bluePixels(kSize * kSize, GLColor::blue);

    GLTexture texA;
    glBindTexture(GL_TEXTURE_2D, texA);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, kSize, kSize, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                 bluePixels.data());
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texA, 0);

    GLTexture texB;
    glBindTexture(GL_TEXTURE_2D, texB);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, kSize, kSize, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                 bluePixels.data());
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, texB, 0);

    ASSERT_GL_FRAMEBUFFER_COMPLETE(GL_FRAMEBUFFER);

    // Verify initial attachment colors (blue).
    glReadBuffer(GL_COLOR_ATTACHMENT0);
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::blue);

    glReadBuffer(GL_COLOR_ATTACHMENT1);
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::blue);

    constexpr std::array<GLenum, 2> kDrawBuffers = {GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1};
    glDrawBuffers(2, kDrawBuffers.data());
    glViewport(0, 0, kSize, kSize);
    glUseProgram(testProgram);
    ASSERT_GL_NO_ERROR();

    // Draw, verify first attachment is updated (green) and second is unchanged (blue).
    drawQuad(testProgram, essl3_shaders::PositionAttrib(), 0.5f, 1.0f, true);
    ASSERT_GL_NO_ERROR();

    glReadBuffer(GL_COLOR_ATTACHMENT0);
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::green);

    glReadBuffer(GL_COLOR_ATTACHMENT1);
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::blue);
}

ANGLE_INSTANTIATE_TEST_ES3(RobustFragmentShaderOutputTest);
