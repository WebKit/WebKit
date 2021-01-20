//
// Copyright 2020 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// Test that invokes a usecase where there is a feedback loop but the framebuffer
// depth attachment is only read from

#include "test_utils/ANGLETest.h"
#include "test_utils/gl_raii.h"

using namespace angle;

class ReadOnlyFeedbackLoopTest : public ANGLETest
{
  protected:
    ReadOnlyFeedbackLoopTest()
    {
        setWindowWidth(256);
        setWindowHeight(256);
        setConfigRedBits(8);
        setConfigGreenBits(8);
        setConfigBlueBits(8);
        setConfigAlphaBits(8);
    }

    void testSetUp() override
    {
        glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
        glClearDepthf(1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glDepthRangef(-1.0f, 1.0f);

        glEnable(GL_BLEND);
        glDisable(GL_DEPTH_TEST);

        ASSERT_GL_NO_ERROR();
    }
};

// Fill out a depth texture to specific values and use it both as a sampler and a depth texture
// with depth write disabled. This is to test a "read-only feedback loop" that needs to be
// supported to match industry standard.
TEST_P(ReadOnlyFeedbackLoopTest, DepthFeedbackLoop)
{
    // TODO - Add support for readonly feedback loops (http://anglebug.com/4778)
    ANGLE_SKIP_TEST_IF(true);

    const GLuint width  = getWindowWidth();
    const GLuint height = getWindowHeight();

    GLTexture colorTex;
    GLTexture depthTex;
    GLTexture finalTex;

    GLFramebuffer gbufferFbo;
    GLFramebuffer finalFbo;

    ANGLE_GL_PROGRAM(colorFillProgram, essl1_shaders::vs::Simple(), essl1_shaders::fs::Red());
    ANGLE_GL_PROGRAM(textureFillProgram, essl1_shaders::vs::Texture2D(),
                     essl1_shaders::fs::Texture2D());

    glBindTexture(GL_TEXTURE_2D, colorTex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    EXPECT_GL_NO_ERROR();

    glBindTexture(GL_TEXTURE_2D, depthTex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, width, height, 0, GL_DEPTH_COMPONENT,
                 GL_UNSIGNED_INT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    EXPECT_GL_NO_ERROR();

    glBindFramebuffer(GL_FRAMEBUFFER, gbufferFbo);
    EXPECT_GL_NO_ERROR();

    // Attach a color and depth texture to the FBO
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, colorTex, 0);
    EXPECT_GL_NO_ERROR();
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depthTex, 0);
    EXPECT_GL_NO_ERROR();

    ASSERT_GLENUM_EQ(GL_FRAMEBUFFER_COMPLETE, glCheckFramebufferStatus(GL_FRAMEBUFFER));

    // Set the color texture to blue and depth texture to 1.0f
    glClearColor(0.0f, 0.0f, 1.0f, 1.0f);
    glClearDepthf(1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    ASSERT_GL_NO_ERROR();

    // Enable Depth test with passing always to write depth.
    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);
    glDepthFunc(GL_ALWAYS);

    // Fill the middle of the depth texture with 0.0f. while the border remains 1.0f as
    // previously cleared.
    const GLfloat depthValue = 0.0f;
    drawQuad(colorFillProgram, essl1_shaders::PositionAttrib(), depthValue, 0.6f);

    EXPECT_GL_NO_ERROR();

    glBindTexture(GL_TEXTURE_2D, finalTex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    EXPECT_GL_NO_ERROR();

    glBindFramebuffer(GL_FRAMEBUFFER, finalFbo);
    EXPECT_GL_NO_ERROR();

    // Enable Depth test without depth write.
    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    glDepthFunc(GL_GREATER);

    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, finalTex, 0);
    EXPECT_GL_NO_ERROR();
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depthTex, 0);
    EXPECT_GL_NO_ERROR();
    ASSERT_GLENUM_EQ(GL_FRAMEBUFFER_COMPLETE, glCheckFramebufferStatus(GL_FRAMEBUFFER));

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glBindTexture(GL_TEXTURE_2D, depthTex);

    // Fill finalTex with values read from depthTex. This should work even though depthTex
    // is also bound as the depth attachment, because depth write is disabled.
    // The write to finalTex only succeeds for the middle region due to depth test.
    drawQuad(textureFillProgram, essl1_shaders::PositionAttrib(), 0.7f, 1.0f);

    // Copy finalTex to default framebuffer for verification. Depth values written in the first
    // draw call are expected in the middle, while the clear value in the clear before the
    // second draw call are expected at the border.
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    glBindTexture(GL_TEXTURE_2D, finalTex);
    drawQuad(textureFillProgram, essl1_shaders::PositionAttrib(), 0.0f, 1.0f);
    EXPECT_GL_NO_ERROR();

    GLint depthColorValue = (depthValue)*128 + 128;
    EXPECT_EQ(depthColorValue, angle::ReadColor(width / 2, height / 2).R);
    EXPECT_PIXEL_EQ(0, 0, 0, 0, 255, 255);
}

// Instantiate the test for ES2 and ES3.
ANGLE_INSTANTIATE_TEST_ES2_AND_ES3(ReadOnlyFeedbackLoopTest);
