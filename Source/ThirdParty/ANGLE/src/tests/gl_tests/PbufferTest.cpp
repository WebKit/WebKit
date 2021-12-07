//
// Copyright 2015 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#include "test_utils/ANGLETest.h"
#include "test_utils/gl_raii.h"
#include "util/EGLWindow.h"

using namespace angle;

class PbufferTest : public ANGLETest
{
  protected:
    PbufferTest()
    {
        setWindowWidth(512);
        setWindowHeight(512);
        setConfigRedBits(8);
        setConfigGreenBits(8);
        setConfigBlueBits(8);
        setConfigAlphaBits(8);
    }

    void testSetUp() override
    {
        constexpr char kVS[] =
            R"(precision highp float;
            attribute vec4 position;
            varying vec2 texcoord;

            void main()
            {
                gl_Position = position;
                texcoord = (position.xy * 0.5) + 0.5;
                texcoord.y = 1.0 - texcoord.y;
            })";

        constexpr char kFS[] =
            R"(precision highp float;
            uniform sampler2D tex;
            varying vec2 texcoord;

            void main()
            {
                gl_FragColor = texture2D(tex, texcoord);
            })";

        mTextureProgram = CompileProgram(kVS, kFS);
        if (mTextureProgram == 0)
        {
            FAIL() << "shader compilation failed.";
        }

        mTextureUniformLocation = glGetUniformLocation(mTextureProgram, "tex");

        EGLWindow *window = getEGLWindow();

        EGLint surfaceType = 0;
        eglGetConfigAttrib(window->getDisplay(), window->getConfig(), EGL_SURFACE_TYPE,
                           &surfaceType);
        mSupportsPbuffers = (surfaceType & EGL_PBUFFER_BIT) != 0;

        EGLint bindToTextureRGBA = 0;
        eglGetConfigAttrib(window->getDisplay(), window->getConfig(), EGL_BIND_TO_TEXTURE_RGBA,
                           &bindToTextureRGBA);
        mSupportsBindTexImage = (bindToTextureRGBA == EGL_TRUE);

        const EGLint pBufferAttributes[] = {
            EGL_WIDTH,          static_cast<EGLint>(mPbufferSize),
            EGL_HEIGHT,         static_cast<EGLint>(mPbufferSize),
            EGL_TEXTURE_FORMAT, mSupportsBindTexImage ? EGL_TEXTURE_RGBA : EGL_NO_TEXTURE,
            EGL_TEXTURE_TARGET, mSupportsBindTexImage ? EGL_TEXTURE_2D : EGL_NO_TEXTURE,
            EGL_NONE,           EGL_NONE,
        };

        mPbuffer =
            eglCreatePbufferSurface(window->getDisplay(), window->getConfig(), pBufferAttributes);
        if (mSupportsPbuffers)
        {
            ASSERT_NE(mPbuffer, EGL_NO_SURFACE);
            ASSERT_EGL_SUCCESS();
        }
        else
        {
            ASSERT_EQ(mPbuffer, EGL_NO_SURFACE);
            ASSERT_EGL_ERROR(EGL_BAD_MATCH);
        }

        ASSERT_GL_NO_ERROR();
    }

    void testTearDown() override
    {
        glDeleteProgram(mTextureProgram);

        if (mPbuffer)
        {
            EGLWindow *window = getEGLWindow();
            eglDestroySurface(window->getDisplay(), mPbuffer);
        }
    }

    void recreatePbufferInSrgbColorspace()
    {
        EGLWindow *window = getEGLWindow();

        if (mPbuffer)
        {
            eglDestroySurface(window->getDisplay(), mPbuffer);
        }

        const EGLint pBufferSrgbAttributes[] = {
            EGL_WIDTH,
            static_cast<EGLint>(mPbufferSize),
            EGL_HEIGHT,
            static_cast<EGLint>(mPbufferSize),
            EGL_TEXTURE_FORMAT,
            mSupportsBindTexImage ? EGL_TEXTURE_RGBA : EGL_NO_TEXTURE,
            EGL_TEXTURE_TARGET,
            mSupportsBindTexImage ? EGL_TEXTURE_2D : EGL_NO_TEXTURE,
            EGL_GL_COLORSPACE_KHR,
            EGL_GL_COLORSPACE_SRGB_KHR,
            EGL_NONE,
            EGL_NONE,
        };

        mPbuffer = eglCreatePbufferSurface(window->getDisplay(), window->getConfig(),
                                           pBufferSrgbAttributes);
    }

    GLuint mTextureProgram;
    GLint mTextureUniformLocation;

    const size_t mPbufferSize = 32;
    EGLSurface mPbuffer       = EGL_NO_SURFACE;
    bool mSupportsPbuffers;
    bool mSupportsBindTexImage;
};

// Test clearing a Pbuffer and checking the color is correct
TEST_P(PbufferTest, Clearing)
{
    ANGLE_SKIP_TEST_IF(!mSupportsPbuffers);

    EGLWindow *window = getEGLWindow();

    // Clear the window surface to blue and verify
    window->makeCurrent();
    ASSERT_EGL_SUCCESS();

    glClearColor(0.0f, 0.0f, 1.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    ASSERT_GL_NO_ERROR();
    EXPECT_PIXEL_COLOR_EQ(getWindowWidth() / 2, getWindowHeight() / 2, GLColor::blue);

    // Apply the Pbuffer and clear it to purple and verify
    eglMakeCurrent(window->getDisplay(), mPbuffer, mPbuffer, window->getContext());
    ASSERT_EGL_SUCCESS();

    glViewport(0, 0, static_cast<GLsizei>(mPbufferSize), static_cast<GLsizei>(mPbufferSize));
    glClearColor(1.0f, 0.0f, 1.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    ASSERT_GL_NO_ERROR();
    EXPECT_PIXEL_EQ(static_cast<GLint>(mPbufferSize) / 2, static_cast<GLint>(mPbufferSize) / 2, 255,
                    0, 255, 255);

    // Rebind the window surface and verify that it is still blue
    window->makeCurrent();
    ASSERT_EGL_SUCCESS();
    EXPECT_PIXEL_EQ(getWindowWidth() / 2, getWindowHeight() / 2, 0, 0, 255, 255);
}

// Bind the Pbuffer to a texture and verify it renders correctly
TEST_P(PbufferTest, BindTexImage)
{
    // Test skipped because Pbuffers are not supported or Pbuffer does not support binding to RGBA
    // textures.
    ANGLE_SKIP_TEST_IF(!mSupportsPbuffers || !mSupportsBindTexImage);

    EGLWindow *window = getEGLWindow();

    // Apply the Pbuffer and clear it to purple
    eglMakeCurrent(window->getDisplay(), mPbuffer, mPbuffer, window->getContext());
    ASSERT_EGL_SUCCESS();

    glViewport(0, 0, static_cast<GLsizei>(mPbufferSize), static_cast<GLsizei>(mPbufferSize));
    glClearColor(1.0f, 0.0f, 1.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    ASSERT_GL_NO_ERROR();

    EXPECT_PIXEL_COLOR_EQ(static_cast<GLint>(mPbufferSize) / 2,
                          static_cast<GLint>(mPbufferSize) / 2, GLColor::magenta);

    // Apply the window surface
    window->makeCurrent();

    // Create a texture and bind the Pbuffer to it
    GLuint texture = 0;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    EXPECT_GL_NO_ERROR();

    eglBindTexImage(window->getDisplay(), mPbuffer, EGL_BACK_BUFFER);
    glViewport(0, 0, getWindowWidth(), getWindowHeight());
    ASSERT_EGL_SUCCESS();

    // Draw a quad and verify that it is purple
    glUseProgram(mTextureProgram);
    glUniform1i(mTextureUniformLocation, 0);

    drawQuad(mTextureProgram, "position", 0.5f);
    EXPECT_GL_NO_ERROR();

    // Unbind the texture
    eglReleaseTexImage(window->getDisplay(), mPbuffer, EGL_BACK_BUFFER);
    ASSERT_EGL_SUCCESS();

    // Verify that purple was drawn
    EXPECT_PIXEL_EQ(getWindowWidth() / 2, getWindowHeight() / 2, 255, 0, 255, 255);

    glDeleteTextures(1, &texture);
}

// Test clearing a Pbuffer in sRGB colorspace and checking the color is correct.
// Then bind the Pbuffer to a texture and verify it renders correctly
TEST_P(PbufferTest, ClearAndBindTexImageSrgb)
{
    EGLWindow *window = getEGLWindow();

    // Test skipped because Pbuffers are not supported or Pbuffer does not support binding to RGBA
    // textures.
    ANGLE_SKIP_TEST_IF(!mSupportsPbuffers || !mSupportsBindTexImage);
    ANGLE_SKIP_TEST_IF(
        !IsEGLDisplayExtensionEnabled(window->getDisplay(), "EGL_KHR_gl_colorspace"));
    // Possible GLES driver bug on Pixel2 devices: http://anglebug.com/5321
    ANGLE_SKIP_TEST_IF(IsPixel2() && IsOpenGLES());

    GLubyte kLinearColor[] = {132, 55, 219, 255};
    GLubyte kSrgbColor[]   = {190, 128, 238, 255};

    // Switch to sRGB
    recreatePbufferInSrgbColorspace();
    EGLint colorspace = 0;
    eglQuerySurface(window->getDisplay(), mPbuffer, EGL_GL_COLORSPACE, &colorspace);
    EXPECT_EQ(colorspace, EGL_GL_COLORSPACE_SRGB_KHR);

    // Clear the Pbuffer surface with `kLinearColor`
    eglMakeCurrent(window->getDisplay(), mPbuffer, mPbuffer, window->getContext());
    ASSERT_EGL_SUCCESS();

    glViewport(0, 0, static_cast<GLsizei>(mPbufferSize), static_cast<GLsizei>(mPbufferSize));
    glClearColor(kLinearColor[0] / 255.0f, kLinearColor[1] / 255.0f, kLinearColor[2] / 255.0f,
                 kLinearColor[3] / 255.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    ASSERT_GL_NO_ERROR();

    // Expect glReadPixels to be `kSrgbColor` with a tolerance of 1
    EXPECT_PIXEL_NEAR(static_cast<GLint>(mPbufferSize) / 2, static_cast<GLint>(mPbufferSize) / 2,
                      kSrgbColor[0], kSrgbColor[1], kSrgbColor[2], kSrgbColor[3], 1);

    window->makeCurrent();

    // Create a texture and bind the Pbuffer to it
    GLuint texture = 0;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    EXPECT_GL_NO_ERROR();

    eglBindTexImage(window->getDisplay(), mPbuffer, EGL_BACK_BUFFER);
    glViewport(0, 0, getWindowWidth(), getWindowHeight());
    ASSERT_EGL_SUCCESS();

    // Sample from a texture with `kSrgbColor` data and render into a surface in linear colorspace.
    glUseProgram(mTextureProgram);
    glUniform1i(mTextureUniformLocation, 0);

    drawQuad(mTextureProgram, "position", 0.5f);
    EXPECT_GL_NO_ERROR();

    // Unbind the texture
    eglReleaseTexImage(window->getDisplay(), mPbuffer, EGL_BACK_BUFFER);
    ASSERT_EGL_SUCCESS();

    // Expect glReadPixels to be `kLinearColor` with a tolerance of 1
    EXPECT_PIXEL_NEAR(getWindowWidth() / 2, getWindowHeight() / 2, kLinearColor[0], kLinearColor[1],
                      kLinearColor[2], kLinearColor[3], 1);

    glDeleteTextures(1, &texture);
}

// Test clearing a Pbuffer in sRGB colorspace and checking the color is correct.
// Then bind the Pbuffer to a texture and verify it renders correctly.
// Then change texture state to skip decode and verify it renders correctly.
TEST_P(PbufferTest, ClearAndBindTexImageSrgbSkipDecode)
{
    EGLWindow *window = getEGLWindow();

    // Test skipped because Pbuffers are not supported or Pbuffer does not support binding to RGBA
    // textures.
    ANGLE_SKIP_TEST_IF(!mSupportsPbuffers || !mSupportsBindTexImage);
    ANGLE_SKIP_TEST_IF(
        !IsEGLDisplayExtensionEnabled(window->getDisplay(), "EGL_KHR_gl_colorspace"));
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_EXT_texture_sRGB_decode"));
    // Possible GLES driver bug on Pixel devices: http://anglebug.com/5321
    ANGLE_SKIP_TEST_IF((IsPixel2() || IsPixel4()) && IsOpenGLES());

    GLubyte kLinearColor[] = {132, 55, 219, 255};
    GLubyte kSrgbColor[]   = {190, 128, 238, 255};

    // Switch to sRGB
    recreatePbufferInSrgbColorspace();
    EGLint colorspace = 0;
    eglQuerySurface(window->getDisplay(), mPbuffer, EGL_GL_COLORSPACE, &colorspace);
    EXPECT_EQ(colorspace, EGL_GL_COLORSPACE_SRGB_KHR);

    // Clear the Pbuffer surface with `kLinearColor`
    eglMakeCurrent(window->getDisplay(), mPbuffer, mPbuffer, window->getContext());
    ASSERT_EGL_SUCCESS();

    glViewport(0, 0, static_cast<GLsizei>(mPbufferSize), static_cast<GLsizei>(mPbufferSize));
    glClearColor(kLinearColor[0] / 255.0f, kLinearColor[1] / 255.0f, kLinearColor[2] / 255.0f,
                 kLinearColor[3] / 255.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    ASSERT_GL_NO_ERROR();

    // Expect glReadPixels to be `kSrgbColor` with a tolerance of 1
    EXPECT_PIXEL_NEAR(static_cast<GLint>(mPbufferSize) / 2, static_cast<GLint>(mPbufferSize) / 2,
                      kSrgbColor[0], kSrgbColor[1], kSrgbColor[2], kSrgbColor[3], 1);

    window->makeCurrent();

    // Create a texture and bind the Pbuffer to it
    GLuint texture = 0;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    EXPECT_GL_NO_ERROR();

    eglBindTexImage(window->getDisplay(), mPbuffer, EGL_BACK_BUFFER);
    glViewport(0, 0, getWindowWidth(), getWindowHeight());
    ASSERT_EGL_SUCCESS();

    // Sample from a texture with `kSrgbColor` data and render into a surface in linear colorspace.
    glUseProgram(mTextureProgram);
    glUniform1i(mTextureUniformLocation, 0);

    drawQuad(mTextureProgram, "position", 0.5f);
    EXPECT_GL_NO_ERROR();

    // Expect glReadPixels to be `kLinearColor` with a tolerance of 1
    EXPECT_PIXEL_NEAR(getWindowWidth() / 2, getWindowHeight() / 2, kLinearColor[0], kLinearColor[1],
                      kLinearColor[2], kLinearColor[3], 1);

    // Set skip decode for the texture
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SRGB_DECODE_EXT, GL_SKIP_DECODE_EXT);
    drawQuad(mTextureProgram, "position", 0.5f);

    // Texture is in skip decode mode, expect glReadPixels to be `kSrgbColor` with tolerance of 1
    EXPECT_PIXEL_NEAR(getWindowWidth() / 2, getWindowHeight() / 2, kSrgbColor[0], kSrgbColor[1],
                      kSrgbColor[2], kSrgbColor[3], 1);

    // Unbind the texture
    eglReleaseTexImage(window->getDisplay(), mPbuffer, EGL_BACK_BUFFER);
    ASSERT_EGL_SUCCESS();

    glDeleteTextures(1, &texture);
}

// Verify that when eglBind/ReleaseTexImage are called, the texture images are freed and their
// size information is correctly updated.
TEST_P(PbufferTest, TextureSizeReset)
{
    ANGLE_SKIP_TEST_IF(!mSupportsPbuffers);
    ANGLE_SKIP_TEST_IF(!mSupportsBindTexImage);
    ANGLE_SKIP_TEST_IF(IsARM64() && IsWindows() && IsD3D());

    GLTexture texture;
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    EXPECT_GL_NO_ERROR();

    glUseProgram(mTextureProgram);
    glUniform1i(mTextureUniformLocation, 0);

    // Fill the texture with white pixels
    std::vector<GLColor> whitePixels(mPbufferSize * mPbufferSize, GLColor::white);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, static_cast<GLsizei>(mPbufferSize),
                 static_cast<GLsizei>(mPbufferSize), 0, GL_RGBA, GL_UNSIGNED_BYTE,
                 whitePixels.data());
    EXPECT_GL_NO_ERROR();

    // Draw the white texture and verify that the pixels are correct
    drawQuad(mTextureProgram, "position", 0.5f);
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::white);

    // Bind the EGL surface and draw with it, results are undefined since nothing has
    // been written to it
    EGLWindow *window = getEGLWindow();
    eglBindTexImage(window->getDisplay(), mPbuffer, EGL_BACK_BUFFER);
    drawQuad(mTextureProgram, "position", 0.5f);
    EXPECT_GL_NO_ERROR();

    // Clear the back buffer to a unique color (green)
    glClearColor(0.0f, 1.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::green);

    // Unbind the EGL surface and try to draw with the texture again, the texture's size should
    // now be zero and incomplete so the back buffer should be black
    eglReleaseTexImage(window->getDisplay(), mPbuffer, EGL_BACK_BUFFER);
    drawQuad(mTextureProgram, "position", 0.5f);
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::black);
}

// Bind a Pbuffer, redefine the texture, and verify it renders correctly
TEST_P(PbufferTest, BindTexImageAndRedefineTexture)
{
    // Test skipped because Pbuffers are not supported or Pbuffer does not support binding to RGBA
    // textures.
    ANGLE_SKIP_TEST_IF(!mSupportsPbuffers || !mSupportsBindTexImage);

    EGLWindow *window = getEGLWindow();

    // Apply the Pbuffer and clear it to purple
    eglMakeCurrent(window->getDisplay(), mPbuffer, mPbuffer, window->getContext());
    ASSERT_EGL_SUCCESS();

    glViewport(0, 0, static_cast<GLsizei>(mPbufferSize), static_cast<GLsizei>(mPbufferSize));
    glClearColor(1.0f, 0.0f, 1.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    ASSERT_GL_NO_ERROR();

    EXPECT_PIXEL_EQ(static_cast<GLint>(mPbufferSize) / 2, static_cast<GLint>(mPbufferSize) / 2, 255,
                    0, 255, 255);

    // Apply the window surface
    window->makeCurrent();

    // Create a texture and bind the Pbuffer to it
    GLuint texture = 0;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    EXPECT_GL_NO_ERROR();

    eglBindTexImage(window->getDisplay(), mPbuffer, EGL_BACK_BUFFER);
    glViewport(0, 0, getWindowWidth(), getWindowHeight());
    ASSERT_EGL_SUCCESS();

    // Redefine the texture
    unsigned int pixelValue = 0xFFFF00FF;
    std::vector<unsigned int> pixelData(getWindowWidth() * getWindowHeight(), pixelValue);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, getWindowWidth(), getWindowHeight(), 0, GL_RGBA,
                 GL_UNSIGNED_BYTE, &pixelData[0]);

    // Draw a quad and verify that it is magenta
    glUseProgram(mTextureProgram);
    glUniform1i(mTextureUniformLocation, 0);

    drawQuad(mTextureProgram, "position", 0.5f);
    EXPECT_GL_NO_ERROR();

    // Verify that magenta was drawn
    EXPECT_PIXEL_EQ(getWindowWidth() / 2, getWindowHeight() / 2, 255, 0, 255, 255);

    glDeleteTextures(1, &texture);
}

ANGLE_INSTANTIATE_TEST_ES2(PbufferTest);
