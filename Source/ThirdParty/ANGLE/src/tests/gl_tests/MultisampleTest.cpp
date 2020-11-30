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

using MultisampleTestParams = std::tuple<angle::PlatformParameters, bool>;

std::string PrintToStringParamName(const ::testing::TestParamInfo<MultisampleTestParams> &info)
{
    ::std::stringstream ss;
    ss << std::get<0>(info.param);
    if (std::get<1>(info.param))
    {
        ss << "__NoStoreAndResolve";
    }
    return ss.str();
}

class MultisampleTest : public ANGLETestWithParam<MultisampleTestParams>
{
  protected:
    void testSetUp() override
    {
        const angle::PlatformParameters platform = ::testing::get<0>(GetParam());
        std::vector<const char *> disabledFeatures;
        if (::testing::get<1>(GetParam()))
        {
            disabledFeatures.push_back("allow_msaa_store_and_resolve");
        }
        disabledFeatures.push_back(nullptr);

        // Get display.
        EGLAttrib dispattrs[] = {EGL_PLATFORM_ANGLE_TYPE_ANGLE, platform.getRenderer(),
                                 EGL_FEATURE_OVERRIDES_DISABLED_ANGLE,
                                 reinterpret_cast<EGLAttrib>(disabledFeatures.data()), EGL_NONE};
        mDisplay              = eglGetPlatformDisplay(EGL_PLATFORM_ANGLE_ANGLE,
                                         reinterpret_cast<void *>(EGL_DEFAULT_DISPLAY), dispattrs);
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
            platform.majorVersion,
            EGL_CONTEXT_MINOR_VERSION_KHR,
            platform.minorVersion,
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

class MultisampleTestES3 : public MultisampleTest
{};

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

// Test polygon rendering on a multisampled surface. And rendering is interrupted by a compute pass
// that converts the index buffer. Make sure the rendering's multisample result is preserved after
// interruption.
TEST_P(MultisampleTest, ContentPresevedAfterInterruption)
{
    ANGLE_SKIP_TEST_IF(!mMultisampledConfigExists);
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_OES_rgb8_rgba8"));
    // http://anglebug.com/3470
    ANGLE_SKIP_TEST_IF(IsAndroid() && IsNVIDIAShield() && IsOpenGLES());

    // http://anglebug.com/4609
    ANGLE_SKIP_TEST_IF(IsD3D11());

    ANGLE_GL_PROGRAM(program, essl1_shaders::vs::Simple(), essl1_shaders::fs::Red());
    glUseProgram(program);
    const GLint positionLocation = glGetAttribLocation(program, essl1_shaders::PositionAttrib());

    if (IsGLExtensionEnabled("GL_EXT_discard_framebuffer"))
    {
        GLenum attachments[] = {GL_COLOR_EXT, GL_DEPTH_EXT, GL_STENCIL_EXT};
        glDiscardFramebufferEXT(GL_FRAMEBUFFER, 3, attachments);
    }
    // Draw triangle
    GLBuffer vertexBuffer;
    const Vector3 vertices[3] = {{-1.0f, -1.0f, 0.0f}, {-1.0f, 1.0f, 0.0f}, {1.0f, -1.0f, 0.0f}};
    prepareVertexBuffer(vertexBuffer, vertices, 3, positionLocation);

    glClear(GL_COLOR_BUFFER_BIT);
    glDrawArrays(GL_TRIANGLES, 0, 3);

    ASSERT_GL_NO_ERROR();

    // Draw a line
    GLBuffer vertexBuffer2;
    GLBuffer indexBuffer2;
    const Vector3 vertices2[2] = {{-1.0f, -0.3f, 0.0f}, {1.0f, 0.3f, 0.0f}};
    const GLubyte indices2[]   = {0, 1};
    prepareVertexBuffer(vertexBuffer2, vertices2, 2, positionLocation);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indexBuffer2);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices2), indices2, GL_STATIC_DRAW);

    glDrawElements(GL_LINES, 2, GL_UNSIGNED_BYTE, 0);

    ASSERT_GL_NO_ERROR();

    // Top-left pixels should be all red.
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::red);
    EXPECT_PIXEL_COLOR_EQ(kWindowSize / 4, kWindowSize / 4, GLColor::red);

    // Triangle edge:
    // Diagonal pixels from bottom-left to top-right are between red and black.  Pixels above the
    // diagonal are red and pixels below it are black.
    {
        const GLColor kMidRed      = {128, 0, 0, 128};
        constexpr int kErrorMargin = 16;

        for (int i = 1; i + 1 < kWindowSize; ++i)
        {
            // Exclude the middle pixel where the triangle and line cross each other.
            if (abs(kWindowSize / 2 - i) <= 1)
            {
                continue;
            }
            int j = kWindowSize - 1 - i;
            EXPECT_PIXEL_COLOR_NEAR(i, j, kMidRed, kErrorMargin);
            EXPECT_PIXEL_COLOR_EQ(i, j - 1, GLColor::red);
            EXPECT_PIXEL_COLOR_EQ(i, j + 1, GLColor::transparentBlack);
        }
    }

    // Line edge:
    {
        const GLColor kDarkRed     = {128, 0, 0, 128};
        constexpr int kErrorMargin = 16;
        constexpr int kLargeMargin = 80;

        static_assert(kWindowSize == 8, "Verification code written for 8x8 window");
        // Exclude the triangle region.
        EXPECT_PIXEL_COLOR_NEAR(5, 4, GLColor::red, kErrorMargin);
        EXPECT_PIXEL_COLOR_NEAR(6, 4, GLColor::red, kLargeMargin);
        EXPECT_PIXEL_COLOR_NEAR(7, 5, kDarkRed, kLargeMargin);
    }
}

// Test that resolve from multisample default framebuffer works.
TEST_P(MultisampleTestES3, ResolveToFBO)
{
    ANGLE_SKIP_TEST_IF(!mMultisampledConfigExists);

    GLTexture resolveTexture;
    glBindTexture(GL_TEXTURE_2D, resolveTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, kWindowSize, kWindowSize, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                 nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    GLFramebuffer resolveFBO;
    glBindFramebuffer(GL_FRAMEBUFFER, resolveFBO);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, resolveTexture, 0);

    // Clear the default framebuffer
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glClearColor(0.25, 0.5, 0.75, 0.25);
    glClear(GL_COLOR_BUFFER_BIT);

    // Resolve into FBO
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, resolveFBO);
    glClearColor(1, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    glBlitFramebuffer(0, 0, kWindowSize, kWindowSize, 0, 0, kWindowSize, kWindowSize,
                      GL_COLOR_BUFFER_BIT, GL_NEAREST);
    ASSERT_GL_NO_ERROR();

    const GLColor kResult = GLColor(63, 127, 191, 63);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, resolveFBO);
    EXPECT_PIXEL_COLOR_NEAR(0, 0, kResult, 1);
    EXPECT_PIXEL_COLOR_NEAR(kWindowSize - 1, 0, kResult, 1);
    EXPECT_PIXEL_COLOR_NEAR(0, kWindowSize - 1, kResult, 1);
    EXPECT_PIXEL_COLOR_NEAR(kWindowSize - 1, kWindowSize - 1, kResult, 1);
    EXPECT_PIXEL_COLOR_NEAR(kWindowSize / 2, kWindowSize / 2, kResult, 1);
}

ANGLE_INSTANTIATE_TEST_COMBINE_1(MultisampleTest,
                                 PrintToStringParamName,
                                 testing::Values(false),
                                 WithNoFixture(ES2_D3D11()),
                                 WithNoFixture(ES3_D3D11()),
                                 WithNoFixture(ES31_D3D11()),
                                 WithNoFixture(ES2_METAL()),
                                 WithNoFixture(ES2_OPENGL()),
                                 WithNoFixture(ES3_OPENGL()),
                                 WithNoFixture(ES31_OPENGL()),
                                 WithNoFixture(ES2_OPENGLES()),
                                 WithNoFixture(ES3_OPENGLES()),
                                 WithNoFixture(ES31_OPENGLES()),
                                 WithNoFixture(ES2_VULKAN()),
                                 WithNoFixture(ES3_VULKAN()),
                                 WithNoFixture(ES31_VULKAN()));

namespace store_and_resolve_feature_off
{
// Simulate missing msaa auto resolve feature in Metal.
ANGLE_INSTANTIATE_TEST_COMBINE_1(MultisampleTest,
                                 PrintToStringParamName,
                                 testing::Values(true),
                                 WithNoFixture(ES2_METAL()));
}  // namespace store_and_resolve_feature_off

ANGLE_INSTANTIATE_TEST_COMBINE_1(MultisampleTestES3,
                                 PrintToStringParamName,
                                 testing::Values(false),
                                 WithNoFixture(ES3_D3D11()),
                                 WithNoFixture(ES31_D3D11()),
                                 WithNoFixture(ES3_OPENGL()),
                                 WithNoFixture(ES31_OPENGL()),
                                 WithNoFixture(ES3_OPENGLES()),
                                 WithNoFixture(ES31_OPENGLES()),
                                 WithNoFixture(ES3_VULKAN()),
                                 WithNoFixture(ES31_VULKAN()));
}  // anonymous namespace
