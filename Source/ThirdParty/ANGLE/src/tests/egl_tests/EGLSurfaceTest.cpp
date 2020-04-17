//
// Copyright 2015 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// EGLSurfaceTest:
//   Tests pertaining to egl::Surface.
//

#include <gtest/gtest.h>

#include <vector>

#include "common/platform.h"
#include "test_utils/ANGLETest.h"
#include "util/EGLWindow.h"
#include "util/OSWindow.h"
#include "util/Timer.h"

#if defined(ANGLE_ENABLE_D3D11)
#    define INITGUID
#    include <guiddef.h>

#    include <d3d11.h>
#    include <dcomp.h>
#endif

using namespace angle;

namespace
{

class EGLSurfaceTest : public ANGLETest
{
  protected:
    EGLSurfaceTest()
        : mDisplay(EGL_NO_DISPLAY),
          mWindowSurface(EGL_NO_SURFACE),
          mPbufferSurface(EGL_NO_SURFACE),
          mContext(EGL_NO_CONTEXT),
          mSecondContext(EGL_NO_CONTEXT),
          mOSWindow(nullptr)
    {}

    void testSetUp() override
    {
        mOSWindow = OSWindow::New();
        mOSWindow->initialize("EGLSurfaceTest", 64, 64);
    }

    // Release any resources created in the test body
    void testTearDown() override
    {
        if (mDisplay != EGL_NO_DISPLAY)
        {
            eglMakeCurrent(mDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);

            if (mWindowSurface != EGL_NO_SURFACE)
            {
                eglDestroySurface(mDisplay, mWindowSurface);
                mWindowSurface = EGL_NO_SURFACE;
            }

            if (mPbufferSurface != EGL_NO_SURFACE)
            {
                eglDestroySurface(mDisplay, mPbufferSurface);
                mPbufferSurface = EGL_NO_SURFACE;
            }

            if (mContext != EGL_NO_CONTEXT)
            {
                eglDestroyContext(mDisplay, mContext);
                mContext = EGL_NO_CONTEXT;
            }

            if (mSecondContext != EGL_NO_CONTEXT)
            {
                eglDestroyContext(mDisplay, mSecondContext);
                mSecondContext = EGL_NO_CONTEXT;
            }

            eglTerminate(mDisplay);
            mDisplay = EGL_NO_DISPLAY;
        }

        mOSWindow->destroy();
        OSWindow::Delete(&mOSWindow);

        ASSERT_TRUE(mWindowSurface == EGL_NO_SURFACE && mContext == EGL_NO_CONTEXT);
    }

    void initializeDisplay()
    {
        GLenum platformType = GetParam().getRenderer();
        GLenum deviceType   = GetParam().getDeviceType();

        std::vector<EGLint> displayAttributes;
        displayAttributes.push_back(EGL_PLATFORM_ANGLE_TYPE_ANGLE);
        displayAttributes.push_back(platformType);
        displayAttributes.push_back(EGL_PLATFORM_ANGLE_MAX_VERSION_MAJOR_ANGLE);
        displayAttributes.push_back(EGL_DONT_CARE);
        displayAttributes.push_back(EGL_PLATFORM_ANGLE_MAX_VERSION_MINOR_ANGLE);
        displayAttributes.push_back(EGL_DONT_CARE);
        displayAttributes.push_back(EGL_PLATFORM_ANGLE_DEVICE_TYPE_ANGLE);
        displayAttributes.push_back(deviceType);
        displayAttributes.push_back(EGL_NONE);

        mDisplay = eglGetPlatformDisplayEXT(EGL_PLATFORM_ANGLE_ANGLE,
                                            reinterpret_cast<void *>(mOSWindow->getNativeDisplay()),
                                            displayAttributes.data());
        ASSERT_TRUE(mDisplay != EGL_NO_DISPLAY);

        EGLint majorVersion, minorVersion;
        ASSERT_TRUE(eglInitialize(mDisplay, &majorVersion, &minorVersion) == EGL_TRUE);

        eglBindAPI(EGL_OPENGL_ES_API);
        ASSERT_TRUE(eglGetError() == EGL_SUCCESS);
    }

    void initializeContext()
    {
        EGLint contextAttibutes[] = {EGL_CONTEXT_CLIENT_VERSION, GetParam().majorVersion, EGL_NONE};

        mContext = eglCreateContext(mDisplay, mConfig, nullptr, contextAttibutes);
        ASSERT_TRUE(eglGetError() == EGL_SUCCESS);

        mSecondContext = eglCreateContext(mDisplay, mConfig, nullptr, contextAttibutes);
        ASSERT_TRUE(eglGetError() == EGL_SUCCESS);
    }

    void initializeSurface(EGLConfig config)
    {
        mConfig = config;

        std::vector<EGLint> surfaceAttributes;
        surfaceAttributes.push_back(EGL_NONE);
        surfaceAttributes.push_back(EGL_NONE);

        // Create first window surface
        mWindowSurface = eglCreateWindowSurface(mDisplay, mConfig, mOSWindow->getNativeWindow(),
                                                &surfaceAttributes[0]);
        ASSERT_TRUE(eglGetError() == EGL_SUCCESS);

        mPbufferSurface = eglCreatePbufferSurface(mDisplay, mConfig, &surfaceAttributes[0]);
        initializeContext();
    }

    void initializeSurfaceWithDefaultConfig()
    {
        const EGLint configAttributes[] = {EGL_RED_SIZE,
                                           EGL_DONT_CARE,
                                           EGL_GREEN_SIZE,
                                           EGL_DONT_CARE,
                                           EGL_BLUE_SIZE,
                                           EGL_DONT_CARE,
                                           EGL_ALPHA_SIZE,
                                           EGL_DONT_CARE,
                                           EGL_DEPTH_SIZE,
                                           EGL_DONT_CARE,
                                           EGL_STENCIL_SIZE,
                                           EGL_DONT_CARE,
                                           EGL_SAMPLE_BUFFERS,
                                           0,
                                           EGL_NONE};

        EGLint configCount;
        EGLConfig config;
        ASSERT_TRUE(eglChooseConfig(mDisplay, configAttributes, &config, 1, &configCount) ||
                    (configCount != 1) == EGL_TRUE);

        initializeSurface(config);
    }

    GLuint createProgram()
    {
        return CompileProgram(angle::essl1_shaders::vs::Simple(), angle::essl1_shaders::fs::Red());
    }

    void drawWithProgram(GLuint program)
    {
        glClearColor(0, 0, 0, 1);
        glClear(GL_COLOR_BUFFER_BIT);

        GLint positionLocation =
            glGetAttribLocation(program, angle::essl1_shaders::PositionAttrib());

        glUseProgram(program);

        const GLfloat vertices[] = {
            -1.0f, 1.0f, 0.5f, -1.0f, -1.0f, 0.5f, 1.0f, -1.0f, 0.5f,

            -1.0f, 1.0f, 0.5f, 1.0f,  -1.0f, 0.5f, 1.0f, 1.0f,  0.5f,
        };

        glVertexAttribPointer(positionLocation, 3, GL_FLOAT, GL_FALSE, 0, vertices);
        glEnableVertexAttribArray(positionLocation);

        glDrawArrays(GL_TRIANGLES, 0, 6);

        glDisableVertexAttribArray(positionLocation);
        glVertexAttribPointer(positionLocation, 4, GL_FLOAT, GL_FALSE, 0, nullptr);

        EXPECT_PIXEL_EQ(mOSWindow->getWidth() / 2, mOSWindow->getHeight() / 2, 255, 0, 0, 255);
    }

    void runMessageLoopTest(EGLSurface secondSurface, EGLContext secondContext)
    {
        eglMakeCurrent(mDisplay, mWindowSurface, mWindowSurface, mContext);
        ASSERT_TRUE(eglGetError() == EGL_SUCCESS);

        // Make a second context current
        eglMakeCurrent(mDisplay, secondSurface, secondSurface, secondContext);
        eglDestroySurface(mDisplay, mWindowSurface);

        // Create second window surface
        std::vector<EGLint> surfaceAttributes;
        surfaceAttributes.push_back(EGL_NONE);
        surfaceAttributes.push_back(EGL_NONE);

        mWindowSurface = eglCreateWindowSurface(mDisplay, mConfig, mOSWindow->getNativeWindow(),
                                                &surfaceAttributes[0]);
        ASSERT_TRUE(eglGetError() == EGL_SUCCESS);

        eglMakeCurrent(mDisplay, mWindowSurface, mWindowSurface, mContext);
        ASSERT_TRUE(eglGetError() == EGL_SUCCESS);

        mOSWindow->signalTestEvent();
        mOSWindow->messageLoop();
        ASSERT_TRUE(mOSWindow->didTestEventFire());

        // Simple operation to test the FBO is set appropriately
        glClear(GL_COLOR_BUFFER_BIT);
    }

    EGLDisplay mDisplay;
    EGLSurface mWindowSurface;
    EGLSurface mPbufferSurface;
    EGLContext mContext;
    EGLContext mSecondContext;
    EGLConfig mConfig;
    OSWindow *mOSWindow;
};

class EGLFloatSurfaceTest : public EGLSurfaceTest
{
  protected:
    EGLFloatSurfaceTest() : EGLSurfaceTest()
    {
        setWindowWidth(512);
        setWindowHeight(512);
    }

    void testSetUp() override
    {
        mOSWindow = OSWindow::New();
        mOSWindow->initialize("EGLFloatSurfaceTest", 64, 64);
    }

    void testTearDown() override
    {
        EGLSurfaceTest::testTearDown();
        glDeleteProgram(mProgram);
    }

    GLuint createProgram()
    {
        constexpr char kFS[] =
            "precision highp float;\n"
            "void main()\n"
            "{\n"
            "   gl_FragColor = vec4(1.0, 2.0, 3.0, 4.0);\n"
            "}\n";
        return CompileProgram(angle::essl1_shaders::vs::Simple(), kFS);
    }

    bool initializeSurfaceWithFloatConfig()
    {
        const EGLint configAttributes[] = {EGL_RED_SIZE,
                                           16,
                                           EGL_GREEN_SIZE,
                                           16,
                                           EGL_BLUE_SIZE,
                                           16,
                                           EGL_ALPHA_SIZE,
                                           16,
                                           EGL_COLOR_COMPONENT_TYPE_EXT,
                                           EGL_COLOR_COMPONENT_TYPE_FLOAT_EXT,
                                           EGL_NONE,
                                           EGL_NONE};

        initializeDisplay();
        EGLConfig config;
        if (EGLWindow::FindEGLConfig(mDisplay, configAttributes, &config) == EGL_FALSE)
        {
            std::cout << "EGLConfig for a float surface is not supported, skipping test"
                      << std::endl;
            return false;
        }

        initializeSurface(config);

        eglMakeCurrent(mDisplay, mWindowSurface, mWindowSurface, mContext);
        mProgram = createProgram();
        return true;
    }

    GLuint mProgram;
};

// Test clearing and checking the color is correct
TEST_P(EGLFloatSurfaceTest, Clearing)
{
    ANGLE_SKIP_TEST_IF(!initializeSurfaceWithFloatConfig());

    ASSERT_NE(0u, mProgram) << "shader compilation failed.";
    ASSERT_GL_NO_ERROR();

    GLColor32F clearColor(0.0f, 1.0f, 2.0f, 3.0f);
    glClearColor(clearColor.R, clearColor.G, clearColor.B, clearColor.A);
    glClear(GL_COLOR_BUFFER_BIT);
    ASSERT_GL_NO_ERROR();

    EXPECT_PIXEL_COLOR32F_EQ(0, 0, clearColor);
}

// Test drawing and checking the color is correct
TEST_P(EGLFloatSurfaceTest, Drawing)
{
    ANGLE_SKIP_TEST_IF(!initializeSurfaceWithFloatConfig());

    ASSERT_NE(0u, mProgram) << "shader compilation failed.";
    ASSERT_GL_NO_ERROR();

    glUseProgram(mProgram);
    drawQuad(mProgram, essl1_shaders::PositionAttrib(), 0.5f);

    EXPECT_PIXEL_32F_EQ(0, 0, 1.0f, 2.0f, 3.0f, 4.0f);
}

class EGLSurfaceTest3 : public EGLSurfaceTest
{};

// Test a surface bug where we could have two Window surfaces active
// at one time, blocking message loops. See http://crbug.com/475085
TEST_P(EGLSurfaceTest, MessageLoopBug)
{
    // TODO(syoussefi): http://anglebug.com/3123
    ANGLE_SKIP_TEST_IF(IsAndroid());

    // http://anglebug.com/3138
    ANGLE_SKIP_TEST_IF(IsOzone());

    initializeDisplay();
    initializeSurfaceWithDefaultConfig();

    runMessageLoopTest(EGL_NO_SURFACE, EGL_NO_CONTEXT);
}

// Tests the message loop bug, but with setting a second context
// instead of null.
TEST_P(EGLSurfaceTest, MessageLoopBugContext)
{
    // TODO(syoussefi): http://anglebug.com/3123
    ANGLE_SKIP_TEST_IF(IsAndroid());

    // http://anglebug.com/3138
    ANGLE_SKIP_TEST_IF(IsOzone());

    initializeDisplay();
    initializeSurfaceWithDefaultConfig();

    runMessageLoopTest(mPbufferSurface, mSecondContext);
}

// Test a bug where calling makeCurrent twice would release the surface
TEST_P(EGLSurfaceTest, MakeCurrentTwice)
{
    // TODO(syoussefi): http://anglebug.com/3123
    ANGLE_SKIP_TEST_IF(IsAndroid());

    initializeDisplay();
    initializeSurfaceWithDefaultConfig();

    eglMakeCurrent(mDisplay, mWindowSurface, mWindowSurface, mContext);
    ASSERT_TRUE(eglGetError() == EGL_SUCCESS);

    eglMakeCurrent(mDisplay, mWindowSurface, mWindowSurface, mContext);
    ASSERT_TRUE(eglGetError() == EGL_SUCCESS);

    // Simple operation to test the FBO is set appropriately
    glClear(GL_COLOR_BUFFER_BIT);
}

// Test that the window surface is correctly resized after calling swapBuffers
TEST_P(EGLSurfaceTest, ResizeWindow)
{
    // TODO(syoussefi): http://anglebug.com/3123
    ANGLE_SKIP_TEST_IF(IsAndroid());

    // Necessary for a window resizing test if there is no per-frame window size query
    mOSWindow->setVisible(true);

    GLenum platform               = GetParam().getRenderer();
    bool platformSupportsZeroSize = platform == EGL_PLATFORM_ANGLE_TYPE_D3D11_ANGLE ||
                                    platform == EGL_PLATFORM_ANGLE_TYPE_D3D9_ANGLE;
    int minSize = platformSupportsZeroSize ? 0 : 1;

    initializeDisplay();
    initializeSurfaceWithDefaultConfig();
    initializeContext();

    eglMakeCurrent(mDisplay, mWindowSurface, mWindowSurface, mContext);
    eglSwapBuffers(mDisplay, mWindowSurface);
    ASSERT_EGL_SUCCESS();

    EGLint height;
    eglQuerySurface(mDisplay, mWindowSurface, EGL_HEIGHT, &height);
    ASSERT_EGL_SUCCESS();
    ASSERT_EQ(64, height);  // initial size

    // set window's height to 0 (if possible) or 1
    mOSWindow->resize(64, minSize);

    eglSwapBuffers(mDisplay, mWindowSurface);
    ASSERT_EGL_SUCCESS();

    // TODO(syoussefi): the GLX implementation still reads the window size as 64x64 through
    // XGetGeometry.  http://anglebug.com/3122
    ANGLE_SKIP_TEST_IF(IsLinux() && IsOpenGL());

    eglQuerySurface(mDisplay, mWindowSurface, EGL_HEIGHT, &height);
    ASSERT_EGL_SUCCESS();
    ASSERT_EQ(minSize, height);

    // restore window's height
    mOSWindow->resize(64, 64);

    eglSwapBuffers(mDisplay, mWindowSurface);
    ASSERT_EGL_SUCCESS();

    eglQuerySurface(mDisplay, mWindowSurface, EGL_HEIGHT, &height);
    ASSERT_EGL_SUCCESS();
    ASSERT_EQ(64, height);
}

// Test that the backbuffer is correctly resized after calling swapBuffers
TEST_P(EGLSurfaceTest, ResizeWindowWithDraw)
{
    // Necessary for a window resizing test if there is no per-frame window size query

    mOSWindow->setVisible(true);

    initializeDisplay();
    initializeSurfaceWithDefaultConfig();
    initializeContext();

    int size      = 64;
    EGLint height = 0;
    EGLint width  = 0;

    eglMakeCurrent(mDisplay, mWindowSurface, mWindowSurface, mContext);
    eglSwapBuffers(mDisplay, mWindowSurface);
    ASSERT_EGL_SUCCESS();

    // Clear to red
    glClearColor(1.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    eglQuerySurface(mDisplay, mWindowSurface, EGL_HEIGHT, &height);
    eglQuerySurface(mDisplay, mWindowSurface, EGL_WIDTH, &width);
    ASSERT_EGL_SUCCESS();

    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::red);
    EXPECT_PIXEL_COLOR_EQ(size - 1, 0, GLColor::red);
    EXPECT_PIXEL_COLOR_EQ(size - 1, size - 1, GLColor::red);
    EXPECT_PIXEL_COLOR_EQ(0, size - 1, GLColor::red);
    EXPECT_PIXEL_COLOR_EQ(-1, -1, GLColor::transparentBlack);
    EXPECT_PIXEL_COLOR_EQ(size, 0, GLColor::transparentBlack);
    EXPECT_PIXEL_COLOR_EQ(0, size, GLColor::transparentBlack);
    EXPECT_PIXEL_COLOR_EQ(size, size, GLColor::transparentBlack);

    // set window's size small
    size = 1;
    mOSWindow->resize(size, size);

    eglSwapBuffers(mDisplay, mWindowSurface);
    ASSERT_EGL_SUCCESS();

    // Clear to green
    glClearColor(0.0f, 1.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    eglQuerySurface(mDisplay, mWindowSurface, EGL_HEIGHT, &height);
    eglQuerySurface(mDisplay, mWindowSurface, EGL_WIDTH, &width);
    ASSERT_EGL_SUCCESS();

    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::green);
    EXPECT_PIXEL_COLOR_EQ(size - 1, 0, GLColor::green);
    EXPECT_PIXEL_COLOR_EQ(size - 1, size - 1, GLColor::green);
    EXPECT_PIXEL_COLOR_EQ(0, size - 1, GLColor::green);
    EXPECT_PIXEL_COLOR_EQ(-1, -1, GLColor::transparentBlack);
    EXPECT_PIXEL_COLOR_EQ(size, 0, GLColor::transparentBlack);
    EXPECT_PIXEL_COLOR_EQ(0, size, GLColor::transparentBlack);
    EXPECT_PIXEL_COLOR_EQ(size, size, GLColor::transparentBlack);

    // set window's height large
    size = 128;
    mOSWindow->resize(size, size);

    eglSwapBuffers(mDisplay, mWindowSurface);
    ASSERT_EGL_SUCCESS();

    // Clear to blue
    glClearColor(0.0f, 0.0f, 1.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    eglQuerySurface(mDisplay, mWindowSurface, EGL_HEIGHT, &height);
    eglQuerySurface(mDisplay, mWindowSurface, EGL_WIDTH, &width);
    ASSERT_EGL_SUCCESS();

    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::blue);
    EXPECT_PIXEL_COLOR_EQ(size - 1, 0, GLColor::blue);
    EXPECT_PIXEL_COLOR_EQ(size - 1, size - 1, GLColor::blue);
    EXPECT_PIXEL_COLOR_EQ(0, size - 1, GLColor::blue);
    EXPECT_PIXEL_COLOR_EQ(-1, -1, GLColor::transparentBlack);
    EXPECT_PIXEL_COLOR_EQ(size, 0, GLColor::transparentBlack);
    EXPECT_PIXEL_COLOR_EQ(0, size, GLColor::transparentBlack);
    EXPECT_PIXEL_COLOR_EQ(size, size, GLColor::transparentBlack);
}

// Test that the window can be reset repeatedly before surface creation.
TEST_P(EGLSurfaceTest, ResetNativeWindow)
{
    mOSWindow->setVisible(true);

    initializeDisplay();

    for (int i = 0; i < 10; ++i)
    {
        mOSWindow->resetNativeWindow();
    }

    initializeSurfaceWithDefaultConfig();
    initializeContext();

    eglMakeCurrent(mDisplay, mWindowSurface, mWindowSurface, mContext);

    eglSwapBuffers(mDisplay, mWindowSurface);
    ASSERT_EGL_SUCCESS();
}

// Test that swap interval works.
TEST_P(EGLSurfaceTest, SwapInterval)
{
    // On OSX, maxInterval >= 1 is advertised, but is not implemented.  http://anglebug.com/3140
    ANGLE_SKIP_TEST_IF(IsOSX());
    // Flaky hang on Nexus 5X and 6P. http://anglebug.com/3364
    ANGLE_SKIP_TEST_IF((IsNexus5X() || IsNexus6P()) && isGLESRenderer());
    // Flaky hang on Ubuntu 19.04 NVIDIA Vulkan. http://anglebug.com/3618
    // Maybe hang due to bug in NVIDIA Linux Vulkan driver. http://anglebug.com/3450
    ANGLE_SKIP_TEST_IF(IsLinux() && IsNVIDIA() && isVulkanRenderer());
    // Flaky on Linux NVIDIA OpenGL driver. http://anglebug.com/3807
    ANGLE_SKIP_TEST_IF(IsLinux() && IsNVIDIA() && isGLRenderer());
    // Test fails on Swangle http://anglebug.com/4169
    ANGLE_SKIP_TEST_IF(isVulkanSwiftshaderRenderer());

    ANGLE_SKIP_TEST_IF(IsARM64() && IsWindows() && IsD3D());

    initializeDisplay();
    initializeSurfaceWithDefaultConfig();
    initializeContext();

    eglMakeCurrent(mDisplay, mWindowSurface, mWindowSurface, mContext);
    eglSwapBuffers(mDisplay, mWindowSurface);
    ASSERT_EGL_SUCCESS();

    EGLint minInterval, maxInterval;

    ASSERT_TRUE(eglGetConfigAttrib(mDisplay, mConfig, EGL_MIN_SWAP_INTERVAL, &minInterval));
    ASSERT_TRUE(eglGetConfigAttrib(mDisplay, mConfig, EGL_MAX_SWAP_INTERVAL, &maxInterval));

    for (int iter = 0; iter < 2; ++iter)
    {
        if (maxInterval >= 1)
        {
            Timer timer;

            eglSwapInterval(mDisplay, 1);
            timer.start();
            for (int i = 0; i < 180; ++i)
            {
                eglSwapBuffers(mDisplay, mWindowSurface);
            }
            timer.stop();
            ASSERT_EGL_SUCCESS();

            // 120 frames at 60fps should take 3s.  At lower fps, it should take even longer.  At
            // 144fps, it would take 1.25s.  Let's use 1s as a lower bound.
            ASSERT_GT(timer.getElapsedTime(), 1);
        }

        if (minInterval <= 0)
        {
            Timer timer;

            eglSwapInterval(mDisplay, 0);
            timer.start();
            for (int i = 0; i < 100; ++i)
            {
                eglSwapBuffers(mDisplay, mWindowSurface);

                // Second eglSwapBuffers causes an EGL_BAD_SURFACE on Nvidia shield tv.
                // http://anglebug.com/3144.
                ANGLE_SKIP_TEST_IF(IsNVIDIAShield());
            }
            timer.stop();
            ASSERT_EGL_SUCCESS();

            // 100 no-op swaps should be fairly fast, though there is no guarantee how fast it can
            // be. 10ms per swap is probably a safe upper bound.
            //
            // TODO(syoussefi): if a surface doesn't truly allow no-vsync, this can fail.  Until
            // there's a way to query the exact minInterval from the surface, this test cannot be
            // enabled.
            // ASSERT_LT(timer.getElapsedTime(), 1);
        }
    }
}

// Test creating a surface that supports a EGLConfig with 16bit
// support GL_RGB565
TEST_P(EGLSurfaceTest, CreateWithEGLConfig5650Support)
{
    const EGLint configAttributes[] = {
        EGL_RED_SIZE,   5, EGL_GREEN_SIZE,   6, EGL_BLUE_SIZE,      5, EGL_ALPHA_SIZE, 0,
        EGL_DEPTH_SIZE, 0, EGL_STENCIL_SIZE, 0, EGL_SAMPLE_BUFFERS, 0, EGL_NONE};

    initializeDisplay();
    EGLConfig config;
    if (EGLWindow::FindEGLConfig(mDisplay, configAttributes, &config) == EGL_FALSE)
    {
        std::cout << "EGLConfig for a GL_RGB565 surface is not supported, skipping test"
                  << std::endl;
        return;
    }

    initializeSurface(config);

    eglMakeCurrent(mDisplay, mWindowSurface, mWindowSurface, mContext);
    ASSERT_EGL_SUCCESS();

    GLuint program = createProgram();
    ASSERT_NE(0u, program);
    drawWithProgram(program);
    EXPECT_GL_NO_ERROR();
    glDeleteProgram(program);
}

// Test creating a surface that supports a EGLConfig with 16bit
// support GL_RGBA4
TEST_P(EGLSurfaceTest, CreateWithEGLConfig4444Support)
{
    const EGLint configAttributes[] = {
        EGL_RED_SIZE,   4, EGL_GREEN_SIZE,   4, EGL_BLUE_SIZE,      4, EGL_ALPHA_SIZE, 4,
        EGL_DEPTH_SIZE, 0, EGL_STENCIL_SIZE, 0, EGL_SAMPLE_BUFFERS, 0, EGL_NONE};

    initializeDisplay();
    EGLConfig config;
    if (EGLWindow::FindEGLConfig(mDisplay, configAttributes, &config) == EGL_FALSE)
    {
        std::cout << "EGLConfig for a GL_RGBA4 surface is not supported, skipping test"
                  << std::endl;
        return;
    }

    initializeSurface(config);

    eglMakeCurrent(mDisplay, mWindowSurface, mWindowSurface, mContext);
    ASSERT_EGL_SUCCESS();

    GLuint program = createProgram();
    ASSERT_NE(0u, program);
    drawWithProgram(program);
    EXPECT_GL_NO_ERROR();
    glDeleteProgram(program);
}

// Test creating a surface that supports a EGLConfig with 16bit
// support GL_RGB5_A1
TEST_P(EGLSurfaceTest, CreateWithEGLConfig5551Support)
{
    const EGLint configAttributes[] = {
        EGL_RED_SIZE,   5, EGL_GREEN_SIZE,   5, EGL_BLUE_SIZE,      5, EGL_ALPHA_SIZE, 1,
        EGL_DEPTH_SIZE, 0, EGL_STENCIL_SIZE, 0, EGL_SAMPLE_BUFFERS, 0, EGL_NONE};

    initializeDisplay();
    EGLConfig config;
    if (EGLWindow::FindEGLConfig(mDisplay, configAttributes, &config) == EGL_FALSE)
    {
        std::cout << "EGLConfig for a GL_RGB5_A1 surface is not supported, skipping test"
                  << std::endl;
        return;
    }

    initializeSurface(config);

    eglMakeCurrent(mDisplay, mWindowSurface, mWindowSurface, mContext);
    ASSERT_EGL_SUCCESS();

    GLuint program = createProgram();
    ASSERT_NE(0u, program);
    drawWithProgram(program);
    EXPECT_GL_NO_ERROR();
    glDeleteProgram(program);
}

// Test creating a surface that supports a EGLConfig without alpha support
TEST_P(EGLSurfaceTest, CreateWithEGLConfig8880Support)
{
    const EGLint configAttributes[] = {
        EGL_RED_SIZE,   8, EGL_GREEN_SIZE,   8, EGL_BLUE_SIZE,      8, EGL_ALPHA_SIZE, 0,
        EGL_DEPTH_SIZE, 0, EGL_STENCIL_SIZE, 0, EGL_SAMPLE_BUFFERS, 0, EGL_NONE};

    initializeDisplay();
    EGLConfig config;
    if (EGLWindow::FindEGLConfig(mDisplay, configAttributes, &config) == EGL_FALSE)
    {
        std::cout << "EGLConfig for a GL_RGB8_OES surface is not supported, skipping test"
                  << std::endl;
        return;
    }

    initializeSurface(config);

    eglMakeCurrent(mDisplay, mWindowSurface, mWindowSurface, mContext);
    ASSERT_EGL_SUCCESS();

    GLuint program = createProgram();
    ASSERT_NE(0u, program);
    drawWithProgram(program);
    EXPECT_GL_NO_ERROR();
    glDeleteProgram(program);
}

TEST_P(EGLSurfaceTest, FixedSizeWindow)
{
    const EGLint configAttributes[] = {
        EGL_RED_SIZE,   8, EGL_GREEN_SIZE,   8, EGL_BLUE_SIZE,      8, EGL_ALPHA_SIZE, 0,
        EGL_DEPTH_SIZE, 0, EGL_STENCIL_SIZE, 0, EGL_SAMPLE_BUFFERS, 0, EGL_NONE};

    initializeDisplay();
    ANGLE_SKIP_TEST_IF(EGLWindow::FindEGLConfig(mDisplay, configAttributes, &mConfig) == EGL_FALSE);

    ANGLE_SKIP_TEST_IF(!IsEGLDisplayExtensionEnabled(mDisplay, "EGL_ANGLE_window_fixed_size"));

    constexpr EGLint kInitialSize = 64;
    constexpr EGLint kUpdateSize  = 32;

    EGLint surfaceAttributes[] = {
        EGL_FIXED_SIZE_ANGLE, EGL_TRUE, EGL_WIDTH, kInitialSize, EGL_HEIGHT, kInitialSize, EGL_NONE,
    };

    // Create first window surface
    mWindowSurface =
        eglCreateWindowSurface(mDisplay, mConfig, mOSWindow->getNativeWindow(), surfaceAttributes);
    ASSERT_EGL_SUCCESS();
    ASSERT_NE(EGL_NO_SURFACE, mWindowSurface);

    initializeContext();
    EXPECT_EGL_TRUE(eglMakeCurrent(mDisplay, mWindowSurface, mWindowSurface, mContext));
    ASSERT_EGL_SUCCESS();

    EGLint queryIsFixedSize = 0;
    EXPECT_EGL_TRUE(
        eglQuerySurface(mDisplay, mWindowSurface, EGL_FIXED_SIZE_ANGLE, &queryIsFixedSize));
    ASSERT_EGL_SUCCESS();
    EXPECT_EGL_TRUE(queryIsFixedSize);

    EGLint queryWidth = 0;
    EXPECT_EGL_TRUE(eglQuerySurface(mDisplay, mWindowSurface, EGL_WIDTH, &queryWidth));
    ASSERT_EGL_SUCCESS();
    EXPECT_EQ(kInitialSize, queryWidth);

    EGLint queryHeight = 0;
    EXPECT_EGL_TRUE(eglQuerySurface(mDisplay, mWindowSurface, EGL_HEIGHT, &queryHeight));
    ASSERT_EGL_SUCCESS();
    EXPECT_EQ(kInitialSize, queryHeight);

    // Update the size
    EXPECT_EGL_TRUE(eglSurfaceAttrib(mDisplay, mWindowSurface, EGL_WIDTH, kUpdateSize));
    ASSERT_EGL_SUCCESS();

    EXPECT_EGL_TRUE(eglWaitNative(EGL_CORE_NATIVE_ENGINE));
    ASSERT_EGL_SUCCESS();

    EGLint queryUpdatedWidth = 0;
    EXPECT_EGL_TRUE(eglQuerySurface(mDisplay, mWindowSurface, EGL_WIDTH, &queryUpdatedWidth));
    ASSERT_EGL_SUCCESS();
    EXPECT_EQ(kUpdateSize, queryUpdatedWidth);
}

TEST_P(EGLSurfaceTest3, MakeCurrentDifferentSurfaces)
{
    const EGLint configAttributes[] = {
        EGL_RED_SIZE,   8, EGL_GREEN_SIZE,   8, EGL_BLUE_SIZE,      8, EGL_ALPHA_SIZE, 8,
        EGL_DEPTH_SIZE, 0, EGL_STENCIL_SIZE, 0, EGL_SAMPLE_BUFFERS, 0, EGL_NONE};
    EGLSurface firstPbufferSurface;
    EGLSurface secondPbufferSurface;

    initializeDisplay();
    ANGLE_SKIP_TEST_IF(EGLWindow::FindEGLConfig(mDisplay, configAttributes, &mConfig) == EGL_FALSE);

    EGLint surfaceType = 0;
    eglGetConfigAttrib(mDisplay, mConfig, EGL_SURFACE_TYPE, &surfaceType);
    bool supportsPbuffers    = (surfaceType & EGL_PBUFFER_BIT) != 0;
    EGLint bindToTextureRGBA = 0;
    eglGetConfigAttrib(mDisplay, mConfig, EGL_BIND_TO_TEXTURE_RGBA, &bindToTextureRGBA);
    bool supportsBindTexImage = (bindToTextureRGBA == EGL_TRUE);

    const EGLint pBufferAttributes[] = {
        EGL_WIDTH,          64,
        EGL_HEIGHT,         64,
        EGL_TEXTURE_FORMAT, supportsPbuffers ? EGL_TEXTURE_RGBA : EGL_NO_TEXTURE,
        EGL_TEXTURE_TARGET, supportsBindTexImage ? EGL_TEXTURE_2D : EGL_NO_TEXTURE,
        EGL_NONE,           EGL_NONE,
    };

    // Create the surfaces
    firstPbufferSurface = eglCreatePbufferSurface(mDisplay, mConfig, pBufferAttributes);
    ASSERT_EGL_SUCCESS();
    ASSERT_NE(EGL_NO_SURFACE, firstPbufferSurface);
    secondPbufferSurface = eglCreatePbufferSurface(mDisplay, mConfig, pBufferAttributes);
    ASSERT_EGL_SUCCESS();
    ASSERT_NE(EGL_NO_SURFACE, secondPbufferSurface);

    initializeContext();

    // Use the same surface for both draw and read
    EXPECT_EGL_TRUE(eglMakeCurrent(mDisplay, firstPbufferSurface, firstPbufferSurface, mContext));
    glClearColor(GLColor::red.R, GLColor::red.G, GLColor::red.B, GLColor::red.A);
    glClear(GL_COLOR_BUFFER_BIT);
    ASSERT_GL_NO_ERROR();
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::red);

    // Use different surfaces for draw and read, read should stay the same
    EXPECT_EGL_TRUE(eglMakeCurrent(mDisplay, secondPbufferSurface, firstPbufferSurface, mContext));
    glClearColor(GLColor::blue.R, GLColor::blue.G, GLColor::blue.B, GLColor::blue.A);
    glClear(GL_COLOR_BUFFER_BIT);
    ASSERT_GL_NO_ERROR();
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::red);
    // Verify draw surface was cleared
    EXPECT_EGL_TRUE(eglMakeCurrent(mDisplay, secondPbufferSurface, secondPbufferSurface, mContext));
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::blue);

    EXPECT_EGL_TRUE(eglMakeCurrent(mDisplay, firstPbufferSurface, secondPbufferSurface, mContext));
    ASSERT_EGL_SUCCESS();

    // Blit the source surface to the destination surface
    glBlitFramebuffer(0, 0, 64, 64, 0, 0, 64, 64, GL_COLOR_BUFFER_BIT, GL_NEAREST);
    ASSERT_GL_NO_ERROR();
    EXPECT_EGL_TRUE(eglMakeCurrent(mDisplay, firstPbufferSurface, firstPbufferSurface, mContext));
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::blue);
}

#if defined(ANGLE_ENABLE_D3D11)
class EGLSurfaceTestD3D11 : public EGLSurfaceTest
{};

// Test that rendering to an IDCompositionSurface using a pbuffer works.
TEST_P(EGLSurfaceTestD3D11, CreateDirectCompositionSurface)
{
    ANGLE_SKIP_TEST_IF(!IsEGLClientExtensionEnabled("EGL_ANGLE_platform_angle_d3d"));
    initializeDisplay();

    EGLAttrib device       = 0;
    EGLAttrib newEglDevice = 0;
    ASSERT_EGL_TRUE(eglQueryDisplayAttribEXT(mDisplay, EGL_DEVICE_EXT, &newEglDevice));
    ASSERT_EGL_TRUE(eglQueryDeviceAttribEXT(reinterpret_cast<EGLDeviceEXT>(newEglDevice),
                                            EGL_D3D11_DEVICE_ANGLE, &device));
    angle::ComPtr<ID3D11Device> d3d11Device(reinterpret_cast<ID3D11Device *>(device));
    ASSERT_TRUE(!!d3d11Device);

    HMODULE dcompLibrary = LoadLibraryA("dcomp.dll");
    if (!dcompLibrary)
    {
        std::cout << "DirectComposition not supported" << std::endl;
        return;
    }
    typedef HRESULT(WINAPI * PFN_DCOMPOSITION_CREATE_DEVICE2)(IUnknown * dxgiDevice, REFIID iid,
                                                              void **dcompositionDevice);
    PFN_DCOMPOSITION_CREATE_DEVICE2 createDComp = reinterpret_cast<PFN_DCOMPOSITION_CREATE_DEVICE2>(
        GetProcAddress(dcompLibrary, "DCompositionCreateDevice2"));
    if (!createDComp)
    {
        std::cout << "DirectComposition2 not supported" << std::endl;
        FreeLibrary(dcompLibrary);
        return;
    }

    angle::ComPtr<IDCompositionDevice> dcompDevice;
    HRESULT hr = createDComp(d3d11Device.Get(), IID_PPV_ARGS(dcompDevice.GetAddressOf()));
    ASSERT_TRUE(SUCCEEDED(hr));

    angle::ComPtr<IDCompositionSurface> dcompSurface;
    hr = dcompDevice->CreateSurface(100, 100, DXGI_FORMAT_B8G8R8A8_UNORM,
                                    DXGI_ALPHA_MODE_PREMULTIPLIED, dcompSurface.GetAddressOf());
    ASSERT_TRUE(SUCCEEDED(hr));

    angle::ComPtr<ID3D11Texture2D> texture;
    POINT updateOffset;
    hr = dcompSurface->BeginDraw(nullptr, IID_PPV_ARGS(texture.GetAddressOf()), &updateOffset);
    ASSERT_TRUE(SUCCEEDED(hr));

    const EGLint configAttributes[] = {
        EGL_RED_SIZE,   8, EGL_GREEN_SIZE,   8, EGL_BLUE_SIZE,      8, EGL_ALPHA_SIZE, 8,
        EGL_DEPTH_SIZE, 0, EGL_STENCIL_SIZE, 0, EGL_SAMPLE_BUFFERS, 0, EGL_NONE};

    EGLConfig config;
    ASSERT_EGL_TRUE(EGLWindow::FindEGLConfig(mDisplay, configAttributes, &config));

    const EGLint surfaceAttributes[] = {EGL_WIDTH, 64, EGL_HEIGHT, 64, EGL_NONE};

    EGLClientBuffer buffer = reinterpret_cast<EGLClientBuffer>(texture.Get());
    mPbufferSurface = eglCreatePbufferFromClientBuffer(mDisplay, EGL_D3D_TEXTURE_ANGLE, buffer,
                                                       config, surfaceAttributes);
    ASSERT_EGL_SUCCESS();

    mConfig = config;
    initializeContext();

    eglMakeCurrent(mDisplay, mPbufferSurface, mPbufferSurface, mContext);
    ASSERT_EGL_SUCCESS();

    GLuint program = createProgram();
    ASSERT_NE(0u, program);
    drawWithProgram(program);
    EXPECT_GL_NO_ERROR();
    glDeleteProgram(program);
}

TEST_P(EGLSurfaceTestD3D11, CreateSurfaceWithMSAA)
{
    ANGLE_SKIP_TEST_IF(!IsEGLClientExtensionEnabled("EGL_ANGLE_platform_angle_d3d"));

    // clang-format off
    const EGLint configAttributes[] =
    {
        EGL_RED_SIZE, 8,
        EGL_GREEN_SIZE, 8,
        EGL_BLUE_SIZE, 8,
        EGL_ALPHA_SIZE, 0,
        EGL_DEPTH_SIZE, 0,
        EGL_STENCIL_SIZE, 0,
        EGL_SAMPLE_BUFFERS, 1,
        EGL_SAMPLES, 4,
        EGL_NONE
    };
    // clang-format on

    initializeDisplay();
    EGLConfig config;
    if (EGLWindow::FindEGLConfig(mDisplay, configAttributes, &config) == EGL_FALSE)
    {
        std::cout << "EGLConfig for 4xMSAA is not supported, skipping test" << std::endl;
        return;
    }

    initializeSurface(config);

    eglMakeCurrent(mDisplay, mWindowSurface, mWindowSurface, mContext);
    ASSERT_EGL_SUCCESS();

    GLuint program = createProgram();
    ASSERT_NE(0u, program);

    glClearColor(0, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT);

    GLint positionLocation = glGetAttribLocation(program, angle::essl1_shaders::PositionAttrib());
    ASSERT_NE(-1, positionLocation);

    glUseProgram(program);

    const GLfloat halfPixelOffset = 0.5f * 2.0f / mOSWindow->getWidth();
    // clang-format off
    const GLfloat vertices[] =
    {
        -1.0f + halfPixelOffset,  1.0f, 0.5f,
        -1.0f + halfPixelOffset, -1.0f, 0.5f,
         1.0f,                   -1.0f, 0.5f
    };
    // clang-format on

    glVertexAttribPointer(positionLocation, 3, GL_FLOAT, GL_FALSE, 0, vertices);
    glEnableVertexAttribArray(positionLocation);

    glDrawArrays(GL_TRIANGLES, 0, 3);

    glDisableVertexAttribArray(positionLocation);
    glVertexAttribPointer(positionLocation, 4, GL_FLOAT, GL_FALSE, 0, nullptr);

    EXPECT_PIXEL_NEAR(0, 0, 127, 0, 0, 255, 10);
    EXPECT_GL_NO_ERROR();

    glDeleteProgram(program);
}

#endif  // ANGLE_ENABLE_D3D11

}  // anonymous namespace

ANGLE_INSTANTIATE_TEST(EGLSurfaceTest,
                       WithNoFixture(ES2_D3D9()),
                       WithNoFixture(ES2_D3D11()),
                       WithNoFixture(ES3_D3D11()),
                       WithNoFixture(ES2_OPENGL()),
                       WithNoFixture(ES3_OPENGL()),
                       WithNoFixture(ES2_OPENGLES()),
                       WithNoFixture(ES3_OPENGLES()),
                       WithNoFixture(ES2_VULKAN()),
                       WithNoFixture(ES3_VULKAN()),
                       WithNoFixture(ES2_VULKAN_SWIFTSHADER()),
                       WithNoFixture(ES3_VULKAN_SWIFTSHADER()));
ANGLE_INSTANTIATE_TEST(EGLFloatSurfaceTest,
                       WithNoFixture(ES2_OPENGL()),
                       WithNoFixture(ES3_OPENGL()),
                       WithNoFixture(ES2_VULKAN()),
                       WithNoFixture(ES3_VULKAN()));
ANGLE_INSTANTIATE_TEST(EGLSurfaceTest3, WithNoFixture(ES3_VULKAN()));

#if defined(ANGLE_ENABLE_D3D11)
ANGLE_INSTANTIATE_TEST(EGLSurfaceTestD3D11, WithNoFixture(ES2_D3D11()), WithNoFixture(ES3_D3D11()));
#endif
