//
// Copyright 2019 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// MultisampleTest: Tests of multisampled default framebuffer

#include "test_utils/ANGLETest.h"

#include "test_utils/gl_raii.h"
#include "util/OSWindow.h"
#include "util/shader_utils.h"

using namespace angle;

namespace
{
class MultisampleTest : public ANGLETest
{
  protected:
    void testSetUp() override
    {
        // Get display.
        EGLint dispattrs[] = {EGL_PLATFORM_ANGLE_TYPE_ANGLE, GetParam().getRenderer(), EGL_NONE};
        mDisplay           = eglGetPlatformDisplayEXT(
            EGL_PLATFORM_ANGLE_ANGLE, reinterpret_cast<void *>(EGL_DEFAULT_DISPLAY), dispattrs);
        ASSERT_TRUE(mDisplay != EGL_NO_DISPLAY);

        ASSERT_TRUE(eglInitialize(mDisplay, nullptr, nullptr) == EGL_TRUE);

        // Nexus 5X and 6P fail to eglMakeCurrent with a config they advertise they support.
        // http://anglebug.com/3464
        ANGLE_SKIP_TEST_IF(IsNexus5X() || IsNexus6P());

        // Find a config that uses RGBA8 and allows 4x multisampling.
        const EGLint configAttributes[] = {
            EGL_RED_SIZE,       8, EGL_GREEN_SIZE, 8,  EGL_BLUE_SIZE,    8,
            EGL_ALPHA_SIZE,     8, EGL_DEPTH_SIZE, 24, EGL_STENCIL_SIZE, 8,
            EGL_SAMPLE_BUFFERS, 1, EGL_SAMPLES,    4,  EGL_NONE};

        EGLint configCount;
        EGLConfig multisampledConfig;
        EGLint ret =
            eglChooseConfig(mDisplay, configAttributes, &multisampledConfig, 1, &configCount);
        mMultisampledConfigExists = ret && configCount > 0;

        if (!mMultisampledConfigExists)
        {
            return;
        }

        // Create a window, context and surface if multisampling is possible.
        mOSWindow = OSWindow::New();
        mOSWindow->initialize("MultisampleTest", kWindowSize, kWindowSize);
        setWindowVisible(mOSWindow, true);

        EGLint contextAttributes[] = {
            EGL_CONTEXT_MAJOR_VERSION_KHR,
            GetParam().majorVersion,
            EGL_CONTEXT_MINOR_VERSION_KHR,
            GetParam().minorVersion,
            EGL_NONE,
        };

        mContext =
            eglCreateContext(mDisplay, multisampledConfig, EGL_NO_CONTEXT, contextAttributes);
        ASSERT_TRUE(mContext != EGL_NO_CONTEXT);

        mSurface = eglCreateWindowSurface(mDisplay, multisampledConfig,
                                          mOSWindow->getNativeWindow(), nullptr);
        ASSERT_EGL_SUCCESS();

        eglMakeCurrent(mDisplay, mSurface, mSurface, mContext);
        ASSERT_EGL_SUCCESS();
    }

    void testTearDown() override
    {
        if (mSurface)
        {
            eglSwapBuffers(mDisplay, mSurface);
        }

        eglMakeCurrent(mDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);

        if (mSurface)
        {
            eglDestroySurface(mDisplay, mSurface);
            ASSERT_EGL_SUCCESS();
        }

        if (mContext != EGL_NO_CONTEXT)
        {
            eglDestroyContext(mDisplay, mContext);
            ASSERT_EGL_SUCCESS();
        }

        if (mOSWindow)
        {
            OSWindow::Delete(&mOSWindow);
        }

        eglTerminate(mDisplay);
    }

    void prepareVertexBuffer(GLBuffer &vertexBuffer,
                             const Vector3 *vertices,
                             size_t vertexCount,
                             GLint positionLocation)
    {
        glBindBuffer(GL_ARRAY_BUFFER, vertexBuffer);
        glBufferData(GL_ARRAY_BUFFER, sizeof(*vertices) * vertexCount, vertices, GL_STATIC_DRAW);
        glVertexAttribPointer(positionLocation, 3, GL_FLOAT, GL_FALSE, 0, nullptr);
        glEnableVertexAttribArray(positionLocation);
    }

  protected:
    static constexpr int kWindowSize = 8;

    OSWindow *mOSWindow            = nullptr;
    EGLDisplay mDisplay            = EGL_NO_DISPLAY;
    EGLContext mContext            = EGL_NO_CONTEXT;
    EGLSurface mSurface            = EGL_NO_SURFACE;
    bool mMultisampledConfigExists = false;
};

// Test point rendering on a multisampled surface.  GLES2 section 3.3.1.
TEST_P(MultisampleTest, Point)
{
    ANGLE_SKIP_TEST_IF(!mMultisampledConfigExists);
    // http://anglebug.com/3470
    ANGLE_SKIP_TEST_IF(IsAndroid() && IsNVIDIAShield() && IsOpenGLES());

    constexpr char kPointsVS[] = R"(precision highp float;
attribute vec4 a_position;

void main()
{
    gl_PointSize = 3.0;
    gl_Position = a_position;
})";

    ANGLE_GL_PROGRAM(program, kPointsVS, essl1_shaders::fs::Red());
    glUseProgram(program);
    const GLint positionLocation = glGetAttribLocation(program, "a_position");

    GLBuffer vertexBuffer;
    const Vector3 vertices[1] = {{0.0f, 0.0f, 0.0f}};
    prepareVertexBuffer(vertexBuffer, vertices, 1, positionLocation);

    glClear(GL_COLOR_BUFFER_BIT);
    glDrawArrays(GL_POINTS, 0, 1);

    ASSERT_GL_NO_ERROR();

    // The center pixels should be all red.
    EXPECT_PIXEL_COLOR_EQ(kWindowSize / 2, kWindowSize / 2, GLColor::red);
    EXPECT_PIXEL_COLOR_EQ(kWindowSize / 2 - 1, kWindowSize / 2, GLColor::red);
    EXPECT_PIXEL_COLOR_EQ(kWindowSize / 2, kWindowSize / 2 - 1, GLColor::red);
    EXPECT_PIXEL_COLOR_EQ(kWindowSize / 2 - 1, kWindowSize / 2 - 1, GLColor::red);

    // Border pixels should be between red and black, and not exactly either; corners are darker and
    // sides are brighter.
    const GLColor kSideColor   = {128, 0, 0, 128};
    const GLColor kCornerColor = {64, 0, 0, 64};
    constexpr int kErrorMargin = 16;
    EXPECT_PIXEL_COLOR_NEAR(kWindowSize / 2 - 2, kWindowSize / 2 - 2, kCornerColor, kErrorMargin);
    EXPECT_PIXEL_COLOR_NEAR(kWindowSize / 2 - 2, kWindowSize / 2 + 1, kCornerColor, kErrorMargin);
    EXPECT_PIXEL_COLOR_NEAR(kWindowSize / 2 + 1, kWindowSize / 2 - 2, kCornerColor, kErrorMargin);
    EXPECT_PIXEL_COLOR_NEAR(kWindowSize / 2 + 1, kWindowSize / 2 + 1, kCornerColor, kErrorMargin);

    EXPECT_PIXEL_COLOR_NEAR(kWindowSize / 2 - 2, kWindowSize / 2 - 1, kSideColor, kErrorMargin);
    EXPECT_PIXEL_COLOR_NEAR(kWindowSize / 2 - 2, kWindowSize / 2, kSideColor, kErrorMargin);
    EXPECT_PIXEL_COLOR_NEAR(kWindowSize / 2 - 1, kWindowSize / 2 - 2, kSideColor, kErrorMargin);
    EXPECT_PIXEL_COLOR_NEAR(kWindowSize / 2 - 1, kWindowSize / 2 + 1, kSideColor, kErrorMargin);
    EXPECT_PIXEL_COLOR_NEAR(kWindowSize / 2, kWindowSize / 2 - 2, kSideColor, kErrorMargin);
    EXPECT_PIXEL_COLOR_NEAR(kWindowSize / 2, kWindowSize / 2 + 1, kSideColor, kErrorMargin);
    EXPECT_PIXEL_COLOR_NEAR(kWindowSize / 2 + 1, kWindowSize / 2 - 1, kSideColor, kErrorMargin);
    EXPECT_PIXEL_COLOR_NEAR(kWindowSize / 2 + 1, kWindowSize / 2, kSideColor, kErrorMargin);
}

// Test line rendering on a multisampled surface.  GLES2 section 3.4.4.
TEST_P(MultisampleTest, Line)
{
    ANGLE_SKIP_TEST_IF(!mMultisampledConfigExists);
    ANGLE_SKIP_TEST_IF(IsARM64() && IsWindows() && IsD3D());

    ANGLE_GL_PROGRAM(program, essl1_shaders::vs::Simple(), essl1_shaders::fs::Red());
    glUseProgram(program);
    const GLint positionLocation = glGetAttribLocation(program, essl1_shaders::PositionAttrib());

    GLBuffer vertexBuffer;
    const Vector3 vertices[2] = {{-1.0f, -0.3f, 0.0f}, {1.0f, 0.3f, 0.0f}};
    prepareVertexBuffer(vertexBuffer, vertices, 2, positionLocation);

    glClear(GL_COLOR_BUFFER_BIT);
    glDrawArrays(GL_LINES, 0, 2);

    ASSERT_GL_NO_ERROR();

    // The line goes from left to right at about -17 degrees slope.  It renders as such (captured
    // with renderdoc):
    //
    // D                    D = Dark Red (0.25) or (0.5)
    //  BRA                 R = Red (1.0)
    //     ARB              M = Middle Red (0.75)
    //        D             B = Bright Red (1.0 or 0.75)
    //                      A = Any red (0.5, 0.75 or 1.0)
    //
    // Verify that rendering is done as above.

    const GLColor kDarkRed     = {128, 0, 0, 128};
    const GLColor kMidRed      = {192, 0, 0, 192};
    constexpr int kErrorMargin = 16;
    constexpr int kLargeMargin = 80;

    static_assert(kWindowSize == 8, "Verification code written for 8x8 window");
    EXPECT_PIXEL_COLOR_NEAR(0, 2, kDarkRed, kLargeMargin);
    EXPECT_PIXEL_COLOR_NEAR(1, 3, GLColor::red, kLargeMargin);
    EXPECT_PIXEL_COLOR_NEAR(2, 3, GLColor::red, kErrorMargin);
    EXPECT_PIXEL_COLOR_NEAR(3, 3, kMidRed, kLargeMargin);
    EXPECT_PIXEL_COLOR_NEAR(4, 4, kMidRed, kLargeMargin);
    EXPECT_PIXEL_COLOR_NEAR(5, 4, GLColor::red, kErrorMargin);
    EXPECT_PIXEL_COLOR_NEAR(6, 4, GLColor::red, kLargeMargin);
    EXPECT_PIXEL_COLOR_NEAR(7, 5, kDarkRed, kLargeMargin);
}

// Test polygon rendering on a multisampled surface.  GLES2 section 3.5.3.
TEST_P(MultisampleTest, Triangle)
{
    ANGLE_SKIP_TEST_IF(!mMultisampledConfigExists);
    // http://anglebug.com/3470
    ANGLE_SKIP_TEST_IF(IsAndroid() && IsNVIDIAShield() && IsOpenGLES());

    ANGLE_GL_PROGRAM(program, essl1_shaders::vs::Simple(), essl1_shaders::fs::Red());
    glUseProgram(program);
    const GLint positionLocation = glGetAttribLocation(program, essl1_shaders::PositionAttrib());

    GLBuffer vertexBuffer;
    const Vector3 vertices[3] = {{-1.0f, -1.0f, 0.0f}, {-1.0f, 1.0f, 0.0f}, {1.0f, -1.0f, 0.0f}};
    prepareVertexBuffer(vertexBuffer, vertices, 3, positionLocation);

    glClear(GL_COLOR_BUFFER_BIT);
    glDrawArrays(GL_TRIANGLES, 0, 3);

    ASSERT_GL_NO_ERROR();

    // Top-left pixels should be all red.
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::red);
    EXPECT_PIXEL_COLOR_EQ(kWindowSize / 4, kWindowSize / 4, GLColor::red);

    // Diagonal pixels from bottom-left to top-right are between red and black.  Pixels above the
    // diagonal are red and pixels below it are black.
    const GLColor kMidRed      = {128, 0, 0, 128};
    constexpr int kErrorMargin = 16;

    for (int i = 1; i + 1 < kWindowSize; ++i)
    {
        int j = kWindowSize - 1 - i;
        EXPECT_PIXEL_COLOR_NEAR(i, j, kMidRed, kErrorMargin);
        EXPECT_PIXEL_COLOR_EQ(i, j - 1, GLColor::red);
        EXPECT_PIXEL_COLOR_EQ(i, j + 1, GLColor::transparentBlack);
    }
}

ANGLE_INSTANTIATE_TEST(MultisampleTest,
                       WithNoFixture(ES2_D3D11()),
                       WithNoFixture(ES3_D3D11()),
                       WithNoFixture(ES31_D3D11()),
                       WithNoFixture(ES2_OPENGL()),
                       WithNoFixture(ES3_OPENGL()),
                       WithNoFixture(ES31_OPENGL()),
                       WithNoFixture(ES2_OPENGLES()),
                       WithNoFixture(ES3_OPENGLES()),
                       WithNoFixture(ES31_OPENGLES()),
                       WithNoFixture(ES2_VULKAN()),
                       WithNoFixture(ES3_VULKAN()));
}  // anonymous namespace
