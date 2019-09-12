//
// Copyright 2018 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// DepthStencilTest:
//   Tests covering depth- or stencil-only rendering to make sure the other non-existing aspect is
//   not affecting the results (since the format may be emulated with one that has both aspects).
//

#include "test_utils/ANGLETest.h"

#include "platform/FeaturesVk.h"
#include "test_utils/gl_raii.h"

using namespace angle;

namespace
{

class DepthStencilTest : public ANGLETest
{
  protected:
    DepthStencilTest()
    {
        setWindowWidth(128);
        setWindowHeight(128);
        setConfigRedBits(8);
        setConfigGreenBits(8);
        setConfigBlueBits(8);
        setConfigAlphaBits(8);
        setConfigDepthBits(24);
        setConfigStencilBits(8);
    }

    void SetUp() override
    {
        ANGLETest::SetUp();

        glBindTexture(GL_TEXTURE_2D, mColorTexture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, getWindowWidth(), getWindowHeight(), 0, GL_RGBA,
                     GL_UNSIGNED_BYTE, nullptr);

        // Setup Color/Stencil FBO with a stencil format that's emulated with packed depth/stencil.
        glBindFramebuffer(GL_FRAMEBUFFER, mColorStencilFBO);

        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, mColorTexture,
                               0);
        glBindRenderbuffer(GL_RENDERBUFFER, mStencilTexture);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_STENCIL_INDEX8, getWindowWidth(),
                              getWindowHeight());
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_RENDERBUFFER,
                                  mStencilTexture);

        ASSERT_GL_NO_ERROR();

        // Note: GL_DEPTH_COMPONENT24 is not allowed in GLES2.
        if (getClientMajorVersion() >= 3)
        {
            // Setup Color/Depth FBO with a depth format that's emulated with packed depth/stencil.
            glBindFramebuffer(GL_FRAMEBUFFER, mColorDepthFBO);

            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                                   mColorTexture, 0);
            glBindRenderbuffer(GL_RENDERBUFFER, mDepthTexture);
            glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, getWindowWidth(),
                                  getWindowHeight());
            glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER,
                                      mDepthTexture);
        }

        ASSERT_GL_NO_ERROR();
    }

    void TearDown() override { ANGLETest::TearDown(); }

    void bindColorStencilFBO()
    {
        glBindFramebuffer(GL_FRAMEBUFFER, mColorStencilFBO);
        mHasDepth = false;
    }

    void bindColorDepthFBO()
    {
        glBindFramebuffer(GL_FRAMEBUFFER, mColorDepthFBO);
        mHasStencil = false;
    }

    // Override a feature to force emulation of stencil-only and depth-only formats with a packed
    // depth/stencil format
    void overrideFeaturesVk(FeaturesVk *featuresVk) override
    {
        featuresVk->forceFallbackFormat = true;
    }

    void prepareSingleEmulatedWithPacked();
    void ensureColor(GLColor color);
    void ensureDepthUnaffected();
    void ensureStencilUnaffected();

  private:
    GLFramebuffer mColorStencilFBO;
    GLFramebuffer mColorDepthFBO;
    GLTexture mColorTexture;
    GLRenderbuffer mDepthTexture;
    GLRenderbuffer mStencilTexture;

    bool mHasDepth   = true;
    bool mHasStencil = true;
};

void DepthStencilTest::ensureColor(GLColor color)
{
    const int width  = getWindowWidth();
    const int height = getWindowHeight();

    std::vector<GLColor> pixelData(width * height);
    glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, pixelData.data());

    for (int i = 0; i < width * height; i += 16)
    {
        GLColor actualColor = pixelData[i];
        EXPECT_NEAR(color.R, actualColor.R, 1);
        EXPECT_NEAR(color.G, actualColor.G, 1);
        EXPECT_NEAR(color.B, actualColor.B, 1);
        EXPECT_NEAR(color.A, actualColor.A, 1);

        if (i % width == 0)
            i += 16 * width;
    }
}

void DepthStencilTest::ensureDepthUnaffected()
{
    ANGLE_GL_PROGRAM(depthTestProgram, essl1_shaders::vs::Passthrough(), essl1_shaders::fs::Blue());
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_EQUAL);
    drawQuad(depthTestProgram, essl1_shaders::PositionAttrib(), 0.123f);
    glDisable(GL_DEPTH_TEST);
    ASSERT_GL_NO_ERROR();

    // Since depth shouldn't exist, the drawQuad above should succeed in turning the whole image
    // blue.
    ensureColor(GLColor::blue);
}

void DepthStencilTest::ensureStencilUnaffected()
{
    ANGLE_GL_PROGRAM(stencilTestProgram, essl1_shaders::vs::Passthrough(),
                     essl1_shaders::fs::Green());
    glEnable(GL_STENCIL_TEST);
    glStencilFunc(GL_EQUAL, 0x1B, 0xFF);
    drawQuad(stencilTestProgram, essl1_shaders::PositionAttrib(), 0.0f);
    glDisable(GL_STENCIL_TEST);
    ASSERT_GL_NO_ERROR();

    // Since stencil shouldn't exist, the drawQuad above should succeed in turning the whole image
    // green.
    ensureColor(GLColor::green);
}

void DepthStencilTest::prepareSingleEmulatedWithPacked()
{
    const int w     = getWindowWidth();
    const int h     = getWindowHeight();
    const int whalf = w >> 1;
    const int hhalf = h >> 1;

    // Clear to a random color, 0.75 depth and 0x36 stencil
    Vector4 color1(0.1f, 0.2f, 0.3f, 0.4f);
    GLColor color1RGB(color1);

    glClearColor(color1[0], color1[1], color1[2], color1[3]);
    glClearDepthf(0.75f);
    glClearStencil(0x36);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
    ASSERT_GL_NO_ERROR();

    // Verify color was cleared correctly.
    EXPECT_PIXEL_COLOR_NEAR(0, 0, color1RGB, 1);

    // Use masked color to clear two channels of the image to a second color, 0.25 depth and 0x59
    // stencil.
    Vector4 color2(0.2f, 0.4f, 0.6f, 0.8f);
    glClearColor(color2[0], color2[1], color2[2], color2[3]);
    glClearDepthf(0.25f);
    glClearStencil(0x59);
    glColorMask(GL_TRUE, GL_FALSE, GL_TRUE, GL_FALSE);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    ASSERT_GL_NO_ERROR();

    GLColor color2RGB(Vector4(color2[0], color1[1], color2[2], color1[3]));

    EXPECT_PIXEL_COLOR_NEAR(whalf, hhalf, color2RGB, 1);

    EXPECT_PIXEL_COLOR_NEAR(0, 0, color2RGB, 1);
    EXPECT_PIXEL_COLOR_NEAR(w - 1, 0, color2RGB, 1);
    EXPECT_PIXEL_COLOR_NEAR(0, h - 1, color2RGB, 1);
    EXPECT_PIXEL_COLOR_NEAR(w - 1, h - 1, color2RGB, 1);

    // Use scissor to clear the center to a third color, 0.5 depth and 0xA9 stencil.
    glEnable(GL_SCISSOR_TEST);
    glScissor(whalf / 2, hhalf / 2, whalf, hhalf);

    Vector4 color3(0.3f, 0.5f, 0.7f, 0.9f);
    GLColor color3RGB(color3);
    glClearColor(color3[0], color3[1], color3[2], color3[3]);
    glClearDepthf(0.5f);
    glClearStencil(0xA9);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
    glDisable(GL_SCISSOR_TEST);
    ASSERT_GL_NO_ERROR();

    EXPECT_PIXEL_COLOR_NEAR(whalf, hhalf, color3RGB, 1);

    EXPECT_PIXEL_COLOR_NEAR(0, 0, color2RGB, 1);
    EXPECT_PIXEL_COLOR_NEAR(w - 1, 0, color2RGB, 1);
    EXPECT_PIXEL_COLOR_NEAR(0, h - 1, color2RGB, 1);
    EXPECT_PIXEL_COLOR_NEAR(w - 1, h - 1, color2RGB, 1);

    // Use scissor to draw to the right half of the image with a fourth color, 0.6 depth and 0x84
    // stencil.
    glEnable(GL_SCISSOR_TEST);
    glScissor(whalf, 0, whalf, h);

    ANGLE_GL_PROGRAM(redProgram, essl1_shaders::vs::Simple(), essl1_shaders::fs::Red());
    glEnable(GL_STENCIL_TEST);
    glStencilFunc(GL_ALWAYS, 0x84, 0xFF);
    glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);
    glStencilMask(0xFF);
    drawQuad(redProgram, essl1_shaders::PositionAttrib(), 0.2f);

    glDisable(GL_STENCIL_TEST);
    glDisable(GL_SCISSOR_TEST);
}

// Tests that clearing or rendering into a depth-only format doesn't affect stencil.
TEST_P(DepthStencilTest, DepthOnlyEmulatedWithPacked)
{
    ANGLE_SKIP_TEST_IF(getClientMajorVersion() < 3);

    bindColorDepthFBO();
    prepareSingleEmulatedWithPacked();
    ensureStencilUnaffected();
}

// Tests that clearing or rendering into a stencil-only format doesn't affect depth.
TEST_P(DepthStencilTest, StencilOnlyEmulatedWithPacked)
{
    bindColorStencilFBO();
    prepareSingleEmulatedWithPacked();
    ensureDepthUnaffected();
}

ANGLE_INSTANTIATE_TEST(DepthStencilTest,
                       ES2_D3D11(),
                       ES3_D3D11(),
                       ES2_OPENGL(),
                       ES3_OPENGL(),
                       ES2_OPENGLES(),
                       ES3_OPENGLES(),
                       ES2_VULKAN());

}  // anonymous namespace
