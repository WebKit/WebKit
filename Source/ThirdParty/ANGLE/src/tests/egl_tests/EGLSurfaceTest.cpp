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

#include "common/Color.h"
#include "common/platform.h"
#include "test_utils/ANGLETest.h"
#include "test_utils/gl_raii.h"
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

class EGLSurfaceTest : public ANGLETest<>
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

    void tearDownContextAndSurface()
    {
        if (mDisplay == EGL_NO_DISPLAY)
        {
            return;
        }

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
    }

    // Release any resources created in the test body
    void testTearDown() override
    {
        tearDownContextAndSurface();

        if (mDisplay != EGL_NO_DISPLAY)
        {
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
        ASSERT_EGL_SUCCESS();
    }

    void initializeSingleContext(EGLContext *context)
    {
        EGLint contextAttibutes[] = {EGL_CONTEXT_CLIENT_VERSION, GetParam().majorVersion, EGL_NONE};

        *context = eglCreateContext(mDisplay, mConfig, nullptr, contextAttibutes);
        ASSERT_EGL_SUCCESS();
    }

    void initializeContext()
    {
        // Only initialize the contexts once.
        if (mContext == EGL_NO_CONTEXT)
        {
            initializeSingleContext(&mContext);
        }
        if (mSecondContext == EGL_NO_CONTEXT)
        {
            initializeSingleContext(&mSecondContext);
        }
    }

    void initializeWindowSurfaceWithAttribs(EGLConfig config,
                                            const std::vector<EGLint> &additionalAttributes,
                                            EGLenum expectedResult)
    {
        ASSERT_EQ(mWindowSurface, EGL_NO_SURFACE);

        EGLint surfaceType = EGL_NONE;
        eglGetConfigAttrib(mDisplay, mConfig, EGL_SURFACE_TYPE, &surfaceType);

        if (surfaceType & EGL_WINDOW_BIT)
        {
            std::vector<EGLint> windowAttributes = additionalAttributes;
            windowAttributes.push_back(EGL_NONE);

            // Create first window surface
            mWindowSurface = eglCreateWindowSurface(mDisplay, mConfig, mOSWindow->getNativeWindow(),
                                                    windowAttributes.data());
        }

        ASSERT_EGLENUM_EQ(eglGetError(), expectedResult);
    }

    void initializeSurfaceWithAttribs(EGLConfig config,
                                      const std::vector<EGLint> &additionalAttributes)
    {
        mConfig = config;

        EGLint surfaceType = EGL_NONE;
        eglGetConfigAttrib(mDisplay, mConfig, EGL_SURFACE_TYPE, &surfaceType);

        if (surfaceType & EGL_WINDOW_BIT)
        {
            initializeWindowSurfaceWithAttribs(config, additionalAttributes, EGL_SUCCESS);
        }

        if (surfaceType & EGL_PBUFFER_BIT)
        {
            std::vector<EGLint> pbufferAttributes = additionalAttributes;

            // Give pbuffer non-zero dimensions.
            pbufferAttributes.push_back(EGL_WIDTH);
            pbufferAttributes.push_back(64);
            pbufferAttributes.push_back(EGL_HEIGHT);
            pbufferAttributes.push_back(64);
            pbufferAttributes.push_back(EGL_NONE);

            mPbufferSurface = eglCreatePbufferSurface(mDisplay, mConfig, pbufferAttributes.data());
            ASSERT_EGL_SUCCESS();
        }

        initializeContext();
    }

    void initializeSurface(EGLConfig config)
    {
        std::vector<EGLint> additionalAttributes;
        initializeSurfaceWithAttribs(config, additionalAttributes);
    }

    EGLConfig chooseDefaultConfig(bool requireWindowSurface) const
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
                                           EGL_SURFACE_TYPE,
                                           requireWindowSurface ? EGL_WINDOW_BIT : EGL_DONT_CARE,
                                           EGL_NONE};

        EGLint configCount;
        EGLConfig config;
        if (eglChooseConfig(mDisplay, configAttributes, &config, 1, &configCount) != EGL_TRUE)
            return nullptr;
        if (configCount != 1)
            return nullptr;
        return config;
    }

    void initializeSurfaceWithDefaultConfig(bool requireWindowSurface)
    {
        EGLConfig defaultConfig = chooseDefaultConfig(requireWindowSurface);
        ASSERT_NE(defaultConfig, nullptr);
        initializeSurface(defaultConfig);
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
        ASSERT_EGL_SUCCESS();

        // Make a second context current
        eglMakeCurrent(mDisplay, secondSurface, secondSurface, secondContext);
        eglDestroySurface(mDisplay, mWindowSurface);

        // Create second window surface
        std::vector<EGLint> surfaceAttributes;
        surfaceAttributes.push_back(EGL_NONE);
        surfaceAttributes.push_back(EGL_NONE);

        mWindowSurface = eglCreateWindowSurface(mDisplay, mConfig, mOSWindow->getNativeWindow(),
                                                &surfaceAttributes[0]);
        ASSERT_EGL_SUCCESS();

        eglMakeCurrent(mDisplay, mWindowSurface, mWindowSurface, mContext);
        ASSERT_EGL_SUCCESS();

        mOSWindow->signalTestEvent();
        mOSWindow->messageLoop();
        ASSERT_TRUE(mOSWindow->didTestEventFire());

        // Simple operation to test the FBO is set appropriately
        glClear(GL_COLOR_BUFFER_BIT);
    }

    void drawQuadThenTearDown();

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
        const EGLint configAttributes[] = {EGL_SURFACE_TYPE,
                                           EGL_WINDOW_BIT,
                                           EGL_RED_SIZE,
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

class EGLSingleBufferTest : public ANGLETest<>
{
  protected:
    EGLSingleBufferTest() {}

    void testSetUp() override
    {
        EGLint dispattrs[] = {EGL_PLATFORM_ANGLE_TYPE_ANGLE, GetParam().getRenderer(), EGL_NONE};
        mDisplay           = eglGetPlatformDisplayEXT(
                      EGL_PLATFORM_ANGLE_ANGLE, reinterpret_cast<void *>(EGL_DEFAULT_DISPLAY), dispattrs);
        ASSERT_TRUE(mDisplay != EGL_NO_DISPLAY);
        ASSERT_EGL_TRUE(eglInitialize(mDisplay, nullptr, nullptr));
        mMajorVersion = GetParam().majorVersion;
    }

    void testTearDown() override
    {
        eglMakeCurrent(mDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        eglTerminate(mDisplay);
    }

    bool chooseConfig(EGLConfig *config, bool mutableRenderBuffer) const
    {
        bool result          = false;
        EGLint count         = 0;
        EGLint clientVersion = mMajorVersion == 3 ? EGL_OPENGL_ES3_BIT : EGL_OPENGL_ES2_BIT;
        EGLint attribs[]     = {
                EGL_RED_SIZE,
                8,
                EGL_GREEN_SIZE,
                8,
                EGL_BLUE_SIZE,
                8,
                EGL_ALPHA_SIZE,
                0,
                EGL_RENDERABLE_TYPE,
                clientVersion,
                EGL_SURFACE_TYPE,
                EGL_WINDOW_BIT | (mutableRenderBuffer ? EGL_MUTABLE_RENDER_BUFFER_BIT_KHR : 0),
                EGL_NONE};

        result = eglChooseConfig(mDisplay, attribs, config, 1, &count);
        return result && (count > 0);
    }

    bool createContext(EGLConfig config, EGLContext *context)
    {
        bool result      = false;
        EGLint attribs[] = {EGL_CONTEXT_MAJOR_VERSION, mMajorVersion, EGL_NONE};

        *context = eglCreateContext(mDisplay, config, nullptr, attribs);
        result   = (*context != EGL_NO_CONTEXT);
        EXPECT_TRUE(result);
        return result;
    }

    bool createWindowSurface(EGLConfig config,
                             EGLNativeWindowType win,
                             EGLSurface *surface,
                             EGLint renderBuffer) const
    {
        bool result      = false;
        EGLint attribs[] = {EGL_RENDER_BUFFER, renderBuffer, EGL_NONE};

        *surface = eglCreateWindowSurface(mDisplay, config, win, attribs);
        result   = (*surface != EGL_NO_SURFACE);
        EXPECT_TRUE(result);
        return result;
    }

    EGLDisplay mDisplay  = EGL_NO_DISPLAY;
    EGLint mMajorVersion = 0;
    const EGLint kWidth  = 32;
    const EGLint kHeight = 32;
};

class EGLAndroidAutoRefreshTest : public EGLSingleBufferTest
{};

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
    // http://anglebug.com/3123
    ANGLE_SKIP_TEST_IF(IsAndroid());

    // http://anglebug.com/3138
    ANGLE_SKIP_TEST_IF(IsOzone());

    // http://anglebug.com/5485
    ANGLE_SKIP_TEST_IF(IsIOS());

    initializeDisplay();
    initializeSurfaceWithDefaultConfig(true);

    runMessageLoopTest(EGL_NO_SURFACE, EGL_NO_CONTEXT);
}

// Tests the message loop bug, but with setting a second context
// instead of null.
TEST_P(EGLSurfaceTest, MessageLoopBugContext)
{
    // http://anglebug.com/3123
    ANGLE_SKIP_TEST_IF(IsAndroid());

    // http://anglebug.com/3138
    ANGLE_SKIP_TEST_IF(IsOzone());

    // http://anglebug.com/5485
    ANGLE_SKIP_TEST_IF(IsIOS());

    initializeDisplay();
    initializeSurfaceWithDefaultConfig(true);

    ANGLE_SKIP_TEST_IF(!mPbufferSurface);
    runMessageLoopTest(mPbufferSurface, mSecondContext);
}

// Test a bug where calling makeCurrent twice would release the surface
TEST_P(EGLSurfaceTest, MakeCurrentTwice)
{
    initializeDisplay();
    initializeSurfaceWithDefaultConfig(false);

    eglMakeCurrent(mDisplay, mWindowSurface, mWindowSurface, mContext);
    ASSERT_EGL_SUCCESS();

    eglMakeCurrent(mDisplay, mWindowSurface, mWindowSurface, mContext);
    ASSERT_EGL_SUCCESS();

    // Simple operation to test the FBO is set appropriately
    glClear(GL_COLOR_BUFFER_BIT);
}

// This is a regression test to verify that surfaces are not prematurely destroyed.
TEST_P(EGLSurfaceTest, SurfaceUseAfterFreeBug)
{
    initializeDisplay();

    // Initialize an RGBA8 window and pbuffer surface
    constexpr EGLint kSurfaceAttributes[] = {EGL_RED_SIZE,     8,
                                             EGL_GREEN_SIZE,   8,
                                             EGL_BLUE_SIZE,    8,
                                             EGL_ALPHA_SIZE,   8,
                                             EGL_SURFACE_TYPE, EGL_WINDOW_BIT | EGL_PBUFFER_BIT,
                                             EGL_NONE};

    EGLint configCount      = 0;
    EGLConfig surfaceConfig = nullptr;
    ASSERT_EGL_TRUE(eglChooseConfig(mDisplay, kSurfaceAttributes, &surfaceConfig, 1, &configCount));
    ASSERT_NE(configCount, 0);
    ASSERT_NE(surfaceConfig, nullptr);

    initializeSurface(surfaceConfig);
    ASSERT_EGL_SUCCESS();
    ASSERT_NE(mWindowSurface, EGL_NO_SURFACE);
    ASSERT_NE(mPbufferSurface, EGL_NO_SURFACE);

    eglMakeCurrent(mDisplay, mWindowSurface, mWindowSurface, mSecondContext);
    ASSERT_EGL_SUCCESS();
    glClear(GL_COLOR_BUFFER_BIT);

    eglMakeCurrent(mDisplay, mPbufferSurface, mPbufferSurface, mContext);
    ASSERT_EGL_SUCCESS();
    glClear(GL_COLOR_BUFFER_BIT);

    eglDestroySurface(mDisplay, mPbufferSurface);
    ASSERT_EGL_SUCCESS();
    mPbufferSurface = EGL_NO_SURFACE;

    eglDestroyContext(mDisplay, mSecondContext);
    ASSERT_EGL_SUCCESS();
    mSecondContext = EGL_NO_CONTEXT;
}

// Test that the window surface is correctly resized after calling swapBuffers
TEST_P(EGLSurfaceTest, ResizeWindow)
{
    // http://anglebug.com/4453
    ANGLE_SKIP_TEST_IF(isVulkanRenderer() && IsLinux() && IsIntel());
    // Flaky on Linux SwANGLE http://anglebug.com/4453
    ANGLE_SKIP_TEST_IF(IsLinux() && isSwiftshader());
    // http://anglebug.com/5485
    ANGLE_SKIP_TEST_IF(IsIOS());
    ANGLE_SKIP_TEST_IF(IsLinux() && IsARM());

    // Necessary for a window resizing test if there is no per-frame window size query
    setWindowVisible(mOSWindow, true);

    GLenum platform               = GetParam().getRenderer();
    bool platformSupportsZeroSize = platform == EGL_PLATFORM_ANGLE_TYPE_D3D11_ANGLE ||
                                    platform == EGL_PLATFORM_ANGLE_TYPE_D3D9_ANGLE;
    int minSize = platformSupportsZeroSize ? 0 : 1;

    initializeDisplay();
    initializeSurfaceWithDefaultConfig(true);
    ASSERT_NE(mWindowSurface, EGL_NO_SURFACE);

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
    // http://anglebug.com/4453
    ANGLE_SKIP_TEST_IF(IsLinux());
    // http://anglebug.com/5485
    ANGLE_SKIP_TEST_IF(IsIOS());

    // Necessary for a window resizing test if there is no per-frame window size query
    setWindowVisible(mOSWindow, true);

    initializeDisplay();
    initializeSurfaceWithDefaultConfig(true);
    initializeContext();
    ASSERT_NE(mWindowSurface, EGL_NO_SURFACE);

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
    setWindowVisible(mOSWindow, true);

    initializeDisplay();

    for (int i = 0; i < 10; ++i)
    {
        mOSWindow->resetNativeWindow();
    }

    initializeSurfaceWithDefaultConfig(true);
    initializeContext();
    ASSERT_NE(mWindowSurface, EGL_NO_SURFACE);

    eglMakeCurrent(mDisplay, mWindowSurface, mWindowSurface, mContext);

    eglSwapBuffers(mDisplay, mWindowSurface);
    ASSERT_EGL_SUCCESS();
}

// Test creating a surface that supports a EGLConfig with 16bit
// support GL_RGB565
TEST_P(EGLSurfaceTest, CreateWithEGLConfig5650Support)
{
    const EGLint configAttributes[] = {EGL_SURFACE_TYPE,
                                       EGL_WINDOW_BIT,
                                       EGL_RED_SIZE,
                                       5,
                                       EGL_GREEN_SIZE,
                                       6,
                                       EGL_BLUE_SIZE,
                                       5,
                                       EGL_ALPHA_SIZE,
                                       0,
                                       EGL_DEPTH_SIZE,
                                       0,
                                       EGL_STENCIL_SIZE,
                                       0,
                                       EGL_SAMPLE_BUFFERS,
                                       0,
                                       EGL_NONE};

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
    const EGLint configAttributes[] = {EGL_SURFACE_TYPE,
                                       EGL_WINDOW_BIT,
                                       EGL_RED_SIZE,
                                       4,
                                       EGL_GREEN_SIZE,
                                       4,
                                       EGL_BLUE_SIZE,
                                       4,
                                       EGL_ALPHA_SIZE,
                                       4,
                                       EGL_DEPTH_SIZE,
                                       0,
                                       EGL_STENCIL_SIZE,
                                       0,
                                       EGL_SAMPLE_BUFFERS,
                                       0,
                                       EGL_NONE};

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
    const EGLint configAttributes[] = {EGL_SURFACE_TYPE,
                                       EGL_WINDOW_BIT,
                                       EGL_RED_SIZE,
                                       5,
                                       EGL_GREEN_SIZE,
                                       5,
                                       EGL_BLUE_SIZE,
                                       5,
                                       EGL_ALPHA_SIZE,
                                       1,
                                       EGL_DEPTH_SIZE,
                                       0,
                                       EGL_STENCIL_SIZE,
                                       0,
                                       EGL_SAMPLE_BUFFERS,
                                       0,
                                       EGL_NONE};

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
    const EGLint configAttributes[] = {EGL_SURFACE_TYPE,
                                       EGL_WINDOW_BIT,
                                       EGL_RED_SIZE,
                                       8,
                                       EGL_GREEN_SIZE,
                                       8,
                                       EGL_BLUE_SIZE,
                                       8,
                                       EGL_ALPHA_SIZE,
                                       0,
                                       EGL_DEPTH_SIZE,
                                       0,
                                       EGL_STENCIL_SIZE,
                                       0,
                                       EGL_SAMPLE_BUFFERS,
                                       0,
                                       EGL_NONE};

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
    const EGLint configAttributes[] = {EGL_SURFACE_TYPE,
                                       EGL_WINDOW_BIT,
                                       EGL_RED_SIZE,
                                       8,
                                       EGL_GREEN_SIZE,
                                       8,
                                       EGL_BLUE_SIZE,
                                       8,
                                       EGL_ALPHA_SIZE,
                                       0,
                                       EGL_DEPTH_SIZE,
                                       0,
                                       EGL_STENCIL_SIZE,
                                       0,
                                       EGL_SAMPLE_BUFFERS,
                                       0,
                                       EGL_NONE};

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

    // TODO(http://www.anglebug.com/6284): Failing with OpenGL ES backend on Android.
    // Must be after the eglMakeCurrent() so the renderer string is initialized.
    ANGLE_SKIP_TEST_IF(IsOpenGLES() && IsAndroid());

    glClearColor(kFloatRed.R, kFloatRed.G, kFloatRed.B, kFloatRed.A);
    glClear(GL_COLOR_BUFFER_BIT);
    ASSERT_GL_NO_ERROR();
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::red);

    // Use different surfaces for draw and read, read should stay the same
    EXPECT_EGL_TRUE(eglMakeCurrent(mDisplay, secondPbufferSurface, firstPbufferSurface, mContext));
    glClearColor(kFloatBlue.R, kFloatBlue.G, kFloatBlue.B, kFloatBlue.A);
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
{
  protected:
    // offset - draw into the texture at offset (|offset|, |offset|)
    // pix25 - the expected pixel value at (25, 25)
    // pix75 - the expected pixel value at (75, 75)
    void testTextureOffset(int offset, UINT pix25, UINT pix75)
    {
        initializeDisplay();

        const EGLint configAttributes[] = {
            EGL_RED_SIZE,   8, EGL_GREEN_SIZE,   8, EGL_BLUE_SIZE,      8, EGL_ALPHA_SIZE, 8,
            EGL_DEPTH_SIZE, 0, EGL_STENCIL_SIZE, 0, EGL_SAMPLE_BUFFERS, 0, EGL_NONE};

        EGLConfig config;
        ASSERT_EGL_TRUE(EGLWindow::FindEGLConfig(mDisplay, configAttributes, &config));

        mConfig = config;
        initializeContext();

        EGLAttrib device       = 0;
        EGLAttrib newEglDevice = 0;
        ASSERT_EGL_TRUE(eglQueryDisplayAttribEXT(mDisplay, EGL_DEVICE_EXT, &newEglDevice));
        ASSERT_EGL_TRUE(eglQueryDeviceAttribEXT(reinterpret_cast<EGLDeviceEXT>(newEglDevice),
                                                EGL_D3D11_DEVICE_ANGLE, &device));
        angle::ComPtr<ID3D11Device> d3d11Device(reinterpret_cast<ID3D11Device *>(device));
        ASSERT_TRUE(!!d3d11Device);

        constexpr UINT kTextureWidth  = 100;
        constexpr UINT kTextureHeight = 100;
        constexpr Color<uint8_t> kOpaqueBlack(0, 0, 0, 255);
        std::vector<Color<uint8_t>> textureData(kTextureWidth * kTextureHeight, kOpaqueBlack);

        D3D11_SUBRESOURCE_DATA initialData = {};
        initialData.pSysMem                = textureData.data();
        initialData.SysMemPitch            = kTextureWidth * sizeof(kOpaqueBlack);

        D3D11_TEXTURE2D_DESC desc = {};
        desc.Format               = DXGI_FORMAT_B8G8R8A8_UNORM;
        desc.Width                = kTextureWidth;
        desc.Height               = kTextureHeight;
        desc.ArraySize            = 1;
        desc.MipLevels            = 1;
        desc.SampleDesc.Count     = 1;
        desc.Usage                = D3D11_USAGE_DEFAULT;
        desc.BindFlags            = D3D11_BIND_RENDER_TARGET;
        angle::ComPtr<ID3D11Texture2D> texture;
        HRESULT hr = d3d11Device->CreateTexture2D(&desc, &initialData, &texture);
        ASSERT_TRUE(SUCCEEDED(hr));

        angle::ComPtr<ID3D11DeviceContext> d3d11Context;
        d3d11Device->GetImmediateContext(&d3d11Context);

        // Specify a texture offset of (50, 50) when rendering to the pbuffer surface.
        const EGLint surfaceAttributes[] = {EGL_WIDTH,
                                            kTextureWidth,
                                            EGL_HEIGHT,
                                            kTextureHeight,
                                            EGL_TEXTURE_OFFSET_X_ANGLE,
                                            offset,
                                            EGL_TEXTURE_OFFSET_Y_ANGLE,
                                            offset,
                                            EGL_NONE};
        EGLClientBuffer buffer           = reinterpret_cast<EGLClientBuffer>(texture.Get());
        mPbufferSurface = eglCreatePbufferFromClientBuffer(mDisplay, EGL_D3D_TEXTURE_ANGLE, buffer,
                                                           config, surfaceAttributes);
        ASSERT_EGL_SUCCESS();

        eglMakeCurrent(mDisplay, mPbufferSurface, mPbufferSurface, mContext);
        ASSERT_EGL_SUCCESS();

        // glClear should only clear subrect at offset (50, 50) without explicit scissor.
        glClearColor(0, 0, 1, 1);  // Blue
        glClear(GL_COLOR_BUFFER_BIT);
        EXPECT_PIXEL_EQ(25, 25, 0, 0, pix25, 255);
        EXPECT_PIXEL_EQ(75, 75, 0, 0, pix75, 255);
        EXPECT_GL_NO_ERROR();

        // Drawing with a shader should also update the same subrect only without explicit viewport.
        GLuint program = createProgram();  // Red
        ASSERT_NE(0u, program);
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

        EXPECT_PIXEL_EQ(25, 25, pix25, 0, 0, 255);
        EXPECT_PIXEL_EQ(75, 75, pix75, 0, 0, 255);
        EXPECT_GL_NO_ERROR();

        glDeleteProgram(program);
        EXPECT_GL_NO_ERROR();

        // Blit framebuffer should also blit to the same subrect despite the dstX/Y arguments.
        GLRenderbuffer renderBuffer;
        glBindRenderbuffer(GL_RENDERBUFFER, renderBuffer);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA8, 50, 50);
        EXPECT_GL_NO_ERROR();

        GLFramebuffer framebuffer;
        glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER,
                                  renderBuffer);
        EXPECT_GL_NO_ERROR();

        glClearColor(0, 1, 0, 1);  // Green
        glClear(GL_COLOR_BUFFER_BIT);
        EXPECT_PIXEL_EQ(25, 25, 0, 255, 0, 255);
        EXPECT_GL_NO_ERROR();

        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0u);
        glBindFramebuffer(GL_READ_FRAMEBUFFER, framebuffer);
        glBlitFramebuffer(0, 0, 50, 50, 0, 0, kTextureWidth, kTextureWidth, GL_COLOR_BUFFER_BIT,
                          GL_NEAREST);
        EXPECT_GL_NO_ERROR();

        glBindFramebuffer(GL_READ_FRAMEBUFFER, 0u);
        EXPECT_PIXEL_EQ(25, 25, 0, pix25, 0, 255);
        EXPECT_PIXEL_EQ(75, 75, 0, pix75, 0, 255);
        EXPECT_GL_NO_ERROR();
    }
};

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

    const EGLint surfaceAttributes[] = {EGL_WIDTH,
                                        100,
                                        EGL_HEIGHT,
                                        100,
                                        EGL_TEXTURE_OFFSET_X_ANGLE,
                                        updateOffset.x,
                                        EGL_TEXTURE_OFFSET_Y_ANGLE,
                                        updateOffset.y,
                                        EGL_NONE};

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

// Tests drawing into a surface created with negative offsets.
TEST_P(EGLSurfaceTestD3D11, CreateSurfaceWithTextureNegativeOffset)
{
    ANGLE_SKIP_TEST_IF(!IsEGLClientExtensionEnabled("EGL_ANGLE_platform_angle_d3d"));
    testTextureOffset(-50, 255, 0);
}

// Tests drawing into a surface created with offsets.
TEST_P(EGLSurfaceTestD3D11, CreateSurfaceWithTextureOffset)
{
    ANGLE_SKIP_TEST_IF(!IsEGLClientExtensionEnabled("EGL_ANGLE_platform_angle_d3d"));
    testTextureOffset(50, 0, 255);
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

// Verify bliting between two surfaces works correctly.
TEST_P(EGLSurfaceTest3, BlitBetweenSurfaces)
{
    initializeDisplay();
    ASSERT_NE(mDisplay, EGL_NO_DISPLAY);

    initializeSurfaceWithDefaultConfig(true);
    ASSERT_NE(mWindowSurface, EGL_NO_SURFACE);
    ASSERT_NE(mContext, EGL_NO_CONTEXT);

    EGLSurface surface1;
    EGLSurface surface2;

    const EGLint surfaceAttributes[] = {
        EGL_WIDTH, 64, EGL_HEIGHT, 64, EGL_NONE,
    };

    surface1 = eglCreatePbufferSurface(mDisplay, mConfig, surfaceAttributes);
    ASSERT_EGL_SUCCESS();
    surface2 = eglCreatePbufferSurface(mDisplay, mConfig, surfaceAttributes);
    ASSERT_EGL_SUCCESS();

    // Clear surface1.
    EXPECT_EGL_TRUE(eglMakeCurrent(mDisplay, surface1, surface1, mContext));

    // TODO(http://www.anglebug.com/6284): Failing with OpenGL ES backend on Android and Windows.
    // Must be after the eglMakeCurrent() so the renderer string is initialized.
    ANGLE_SKIP_TEST_IF(IsOpenGLES() && (IsAndroid() || IsWindows()));

    glClearColor(kFloatRed.R, kFloatRed.G, kFloatRed.B, kFloatRed.A);
    glClear(GL_COLOR_BUFFER_BIT);
    ASSERT_GL_NO_ERROR();

    // Blit from surface1 to surface2.
    EXPECT_EGL_TRUE(eglMakeCurrent(mDisplay, surface2, surface1, mContext));
    glBlitFramebuffer(0, 0, 64, 64, 0, 0, 64, 64, GL_COLOR_BUFFER_BIT, GL_NEAREST);
    ASSERT_GL_NO_ERROR();

    // Confirm surface1 has the clear color.
    EXPECT_EGL_TRUE(eglMakeCurrent(mDisplay, surface1, surface1, mContext));
    EXPECT_PIXEL_COLOR_EQ(32, 32, GLColor::red);

    // Confirm surface2 has the blited clear color.
    EXPECT_EGL_TRUE(eglMakeCurrent(mDisplay, surface2, surface2, mContext));
    EXPECT_PIXEL_COLOR_EQ(32, 32, GLColor::red);

    eglDestroySurface(mDisplay, surface1);
    eglDestroySurface(mDisplay, surface2);
}

// Verify bliting between two surfaces works correctly.
TEST_P(EGLSurfaceTest3, BlitBetweenSurfacesWithDeferredClear)
{
    initializeDisplay();
    ASSERT_NE(mDisplay, EGL_NO_DISPLAY);

    initializeSurfaceWithDefaultConfig(true);
    ASSERT_NE(mWindowSurface, EGL_NO_SURFACE);
    ASSERT_NE(mContext, EGL_NO_CONTEXT);

    EGLSurface surface1;
    EGLSurface surface2;

    const EGLint surfaceAttributes[] = {
        EGL_WIDTH, 64, EGL_HEIGHT, 64, EGL_NONE,
    };

    surface1 = eglCreatePbufferSurface(mDisplay, mConfig, surfaceAttributes);
    ASSERT_EGL_SUCCESS();
    surface2 = eglCreatePbufferSurface(mDisplay, mConfig, surfaceAttributes);
    ASSERT_EGL_SUCCESS();

    // Clear surface1.
    EXPECT_EGL_TRUE(eglMakeCurrent(mDisplay, surface1, surface1, mContext));

    // TODO(http://www.anglebug.com/6284): Failing with OpenGL ES backend on Android and Windows.
    // Must be after the eglMakeCurrent() so the renderer string is initialized.
    ANGLE_SKIP_TEST_IF(IsOpenGLES() && (IsAndroid() || IsWindows()));

    glClearColor(kFloatRed.R, kFloatRed.G, kFloatRed.B, kFloatRed.A);
    glClear(GL_COLOR_BUFFER_BIT);
    ASSERT_GL_NO_ERROR();
    // Force the clear to be flushed
    EXPECT_PIXEL_COLOR_EQ(32, 32, GLColor::red);

    // Clear to green, but don't read it back so the clear is deferred.
    glClearColor(kFloatGreen.R, kFloatGreen.G, kFloatGreen.B, kFloatGreen.A);
    glClear(GL_COLOR_BUFFER_BIT);
    ASSERT_GL_NO_ERROR();

    // Blit from surface1 to surface2.
    EXPECT_EGL_TRUE(eglMakeCurrent(mDisplay, surface2, surface1, mContext));
    glBlitFramebuffer(0, 0, 64, 64, 0, 0, 64, 64, GL_COLOR_BUFFER_BIT, GL_NEAREST);
    ASSERT_GL_NO_ERROR();

    // Confirm surface1 has the clear color.
    EXPECT_PIXEL_COLOR_EQ(32, 32, GLColor::green);

    // Confirm surface2 has the blited clear color.
    EXPECT_EGL_TRUE(eglMakeCurrent(mDisplay, surface2, surface2, mContext));
    EXPECT_PIXEL_COLOR_EQ(32, 32, GLColor::green);

    eglDestroySurface(mDisplay, surface1);
    eglDestroySurface(mDisplay, surface2);
}

// Verify switching between a surface with robust resource init and one without still clears alpha.
TEST_P(EGLSurfaceTest, RobustResourceInitAndEmulatedAlpha)
{
    // http://anglebug.com/5279
    ANGLE_SKIP_TEST_IF(IsNVIDIA() && isGLRenderer() && IsLinux());

    // http://anglebug.com/5280
    ANGLE_SKIP_TEST_IF(IsAndroid() && IsNexus5X() && isGLESRenderer());

    initializeDisplay();
    ASSERT_NE(mDisplay, EGL_NO_DISPLAY);

    ANGLE_SKIP_TEST_IF(
        !IsEGLDisplayExtensionEnabled(mDisplay, "EGL_ANGLE_robust_resource_initialization"));

    // Initialize and draw red to a Surface with robust resource init enabled.
    constexpr EGLint kRGBAAttributes[] = {EGL_RED_SIZE,     8,
                                          EGL_GREEN_SIZE,   8,
                                          EGL_BLUE_SIZE,    8,
                                          EGL_ALPHA_SIZE,   8,
                                          EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
                                          EGL_NONE};

    EGLint configCount   = 0;
    EGLConfig rgbaConfig = nullptr;
    ASSERT_EGL_TRUE(eglChooseConfig(mDisplay, kRGBAAttributes, &rgbaConfig, 1, &configCount));
    ASSERT_EQ(configCount, 1);
    ASSERT_NE(rgbaConfig, nullptr);

    std::vector<EGLint> robustInitAttribs;
    robustInitAttribs.push_back(EGL_ROBUST_RESOURCE_INITIALIZATION_ANGLE);
    robustInitAttribs.push_back(EGL_TRUE);

    initializeSurfaceWithAttribs(rgbaConfig, robustInitAttribs);
    ASSERT_EGL_SUCCESS();
    ASSERT_NE(mWindowSurface, EGL_NO_SURFACE);

    initializeSingleContext(&mContext);
    ASSERT_EGL_SUCCESS();
    ASSERT_NE(mContext, EGL_NO_CONTEXT);

    eglMakeCurrent(mDisplay, mWindowSurface, mWindowSurface, mContext);
    ASSERT_EGL_SUCCESS();
    eglSwapBuffers(mDisplay, mWindowSurface);

    // RGBA robust init setup complete. Draw red and verify.
    {
        ANGLE_GL_PROGRAM(program, essl1_shaders::vs::Simple(), essl1_shaders::fs::Red());
        glUseProgram(program);

        drawQuad(program, essl1_shaders::PositionAttrib(), 0.5f);
        ASSERT_GL_NO_ERROR();
        EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::red);

        eglSwapBuffers(mDisplay, mWindowSurface);
    }

    tearDownContextAndSurface();

    // Create second RGB surface with robust resource disabled.
    constexpr EGLint kRGBAttributes[] = {EGL_RED_SIZE,     8,
                                         EGL_GREEN_SIZE,   8,
                                         EGL_BLUE_SIZE,    8,
                                         EGL_ALPHA_SIZE,   0,
                                         EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
                                         EGL_NONE};

    configCount         = 0;
    EGLConfig rgbConfig = nullptr;
    ASSERT_EGL_TRUE(eglChooseConfig(mDisplay, kRGBAttributes, &rgbConfig, 1, &configCount));
    ASSERT_EQ(configCount, 1);
    ASSERT_NE(rgbConfig, nullptr);

    initializeSurface(rgbConfig);
    ASSERT_EGL_SUCCESS();
    ASSERT_NE(mWindowSurface, EGL_NO_SURFACE);

    initializeSingleContext(&mContext);
    ASSERT_EGL_SUCCESS();
    ASSERT_NE(mContext, EGL_NO_CONTEXT);

    eglMakeCurrent(mDisplay, mWindowSurface, mWindowSurface, mContext);
    ASSERT_EGL_SUCCESS();

    // RGB non-robust init setup complete. Draw red and verify.
    {
        ANGLE_GL_PROGRAM(program, essl1_shaders::vs::Simple(), essl1_shaders::fs::Red());
        glUseProgram(program);

        drawQuad(program, essl1_shaders::PositionAttrib(), 0.5f);
        ASSERT_GL_NO_ERROR();
        EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::red);

        eglSwapBuffers(mDisplay, mWindowSurface);
    }
}

void EGLSurfaceTest::drawQuadThenTearDown()
{
    initializeSingleContext(&mContext);

    eglMakeCurrent(mDisplay, mWindowSurface, mWindowSurface, mContext);
    ASSERT_EGL_SUCCESS();

    {
        ANGLE_GL_PROGRAM(greenProgram, essl1_shaders::vs::Simple(), essl1_shaders::fs::Green());
        drawQuad(greenProgram, essl1_shaders::PositionAttrib(), 0.5f);
        EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::green);
        eglSwapBuffers(mDisplay, mWindowSurface);
        ASSERT_EGL_SUCCESS();
    }

    tearDownContextAndSurface();
}

// Tests the EGL_ANGLE_create_surface_swap_interval extension if available.
TEST_P(EGLSurfaceTest, CreateSurfaceSwapIntervalANGLE)
{
    initializeDisplay();
    ASSERT_NE(mDisplay, EGL_NO_DISPLAY);

    mConfig = chooseDefaultConfig(true);
    ASSERT_NE(mConfig, nullptr);

    if (IsEGLDisplayExtensionEnabled(mDisplay, "EGL_ANGLE_create_surface_swap_interval"))
    {
        // Test error conditions.
        EGLint minSwapInterval = 0;
        eglGetConfigAttrib(mDisplay, mConfig, EGL_MIN_SWAP_INTERVAL, &minSwapInterval);
        ASSERT_EGL_SUCCESS();

        if (minSwapInterval > 0)
        {
            std::vector<EGLint> min1SwapAttribs = {EGL_SWAP_INTERVAL_ANGLE, minSwapInterval - 1};
            initializeWindowSurfaceWithAttribs(mConfig, min1SwapAttribs, EGL_BAD_ATTRIBUTE);
        }

        EGLint maxSwapInterval = 0;
        eglGetConfigAttrib(mDisplay, mConfig, EGL_MAX_SWAP_INTERVAL, &maxSwapInterval);
        ASSERT_EGL_SUCCESS();

        if (maxSwapInterval < std::numeric_limits<EGLint>::max())
        {
            std::vector<EGLint> max1SwapAttribs = {EGL_SWAP_INTERVAL_ANGLE, maxSwapInterval + 1};
            initializeWindowSurfaceWithAttribs(mConfig, max1SwapAttribs, EGL_BAD_ATTRIBUTE);
        }

        // Test valid min/max usage.
        {
            std::vector<EGLint> minSwapAttribs = {EGL_SWAP_INTERVAL_ANGLE, minSwapInterval};
            initializeWindowSurfaceWithAttribs(mConfig, minSwapAttribs, EGL_SUCCESS);
            drawQuadThenTearDown();
        }

        if (minSwapInterval != maxSwapInterval)
        {
            std::vector<EGLint> maxSwapAttribs = {EGL_SWAP_INTERVAL_ANGLE, maxSwapInterval};
            initializeWindowSurfaceWithAttribs(mConfig, maxSwapAttribs, EGL_SUCCESS);
            drawQuadThenTearDown();
        }
    }
    else
    {
        // Test extension unavailable error.
        std::vector<EGLint> swapInterval1Attribs = {EGL_SWAP_INTERVAL_ANGLE, 1};
        initializeWindowSurfaceWithAttribs(mConfig, swapInterval1Attribs, EGL_BAD_ATTRIBUTE);
    }
}

// Test that setting a surface's timestamp attribute works when the extension
// EGL_ANGLE_timestamp_surface_attribute is supported.
TEST_P(EGLSurfaceTest, TimestampSurfaceAttribute)
{
    initializeDisplay();
    ASSERT_NE(mDisplay, EGL_NO_DISPLAY);
    mConfig = chooseDefaultConfig(true);
    ASSERT_NE(mConfig, nullptr);
    initializeSurface(mConfig);
    ASSERT_NE(mWindowSurface, EGL_NO_SURFACE);
    initializeContext();
    ASSERT_NE(mContext, EGL_NO_CONTEXT);

    EXPECT_EGL_TRUE(eglMakeCurrent(mDisplay, mWindowSurface, mWindowSurface, mContext));
    ASSERT_EGL_SUCCESS() << "eglMakeCurrent failed.";

    const bool extensionSupported =
        IsEGLDisplayExtensionEnabled(mDisplay, "EGL_ANDROID_get_frame_timestamps") ||
        IsEGLDisplayExtensionEnabled(mDisplay, "EGL_ANGLE_timestamp_surface_attribute");

    EGLBoolean setSurfaceAttrib =
        eglSurfaceAttrib(mDisplay, mWindowSurface, EGL_TIMESTAMPS_ANDROID, EGL_TRUE);

    if (extensionSupported)
    {
        EXPECT_EGL_TRUE(setSurfaceAttrib);

        // Swap so the swapchain gets created.
        glClearColor(1.0, 1.0, 1.0, 1.0);
        glClear(GL_COLOR_BUFFER_BIT);
        EXPECT_EGL_TRUE(eglSwapBuffers(mDisplay, mWindowSurface));

        // Query to confirm the attribute persists across swaps.
        EGLint timestampEnabled = 0;
        EXPECT_EGL_TRUE(
            eglQuerySurface(mDisplay, mWindowSurface, EGL_TIMESTAMPS_ANDROID, &timestampEnabled));
        EXPECT_NE(timestampEnabled, 0);

        // Resize window and swap.
        mOSWindow->resize(256, 256);
        glClearColor(1.0, 1.0, 1.0, 1.0);
        glClear(GL_COLOR_BUFFER_BIT);
        EXPECT_EGL_TRUE(eglSwapBuffers(mDisplay, mWindowSurface));

        // Query to confirm the attribute persists across swapchain recreations.
        timestampEnabled = 0;
        EXPECT_EGL_TRUE(
            eglQuerySurface(mDisplay, mWindowSurface, EGL_TIMESTAMPS_ANDROID, &timestampEnabled));
        EXPECT_NE(timestampEnabled, 0);
    }
    else
    {
        EXPECT_EGL_FALSE(setSurfaceAttrib);
        EXPECT_EGL_ERROR(EGL_BAD_ATTRIBUTE);
    }

    EXPECT_EGL_TRUE(eglMakeCurrent(mDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT));
    ASSERT_EGL_SUCCESS() << "eglMakeCurrent - uncurrent failed.";
}

TEST_P(EGLSingleBufferTest, OnCreateWindowSurface)
{
    EGLConfig config = EGL_NO_CONFIG_KHR;
    ANGLE_SKIP_TEST_IF(!chooseConfig(&config, false));

    EGLContext context = EGL_NO_CONTEXT;
    EXPECT_EGL_TRUE(createContext(config, &context));
    ASSERT_EGL_SUCCESS() << "eglCreateContext failed.";

    EGLSurface surface = EGL_NO_SURFACE;
    OSWindow *osWindow = OSWindow::New();
    osWindow->initialize("EGLSingleBufferTest", kWidth, kHeight);
    EXPECT_EGL_TRUE(
        createWindowSurface(config, osWindow->getNativeWindow(), &surface, EGL_SINGLE_BUFFER));
    ASSERT_EGL_SUCCESS() << "eglCreateWindowSurface failed.";

    EXPECT_EGL_TRUE(eglMakeCurrent(mDisplay, surface, surface, context));
    ASSERT_EGL_SUCCESS() << "eglMakeCurrent failed.";

    EGLint actualRenderbuffer;
    EXPECT_EGL_TRUE(eglQueryContext(mDisplay, context, EGL_RENDER_BUFFER, &actualRenderbuffer));
    if (actualRenderbuffer == EGL_SINGLE_BUFFER)
    {
        EXPECT_EGL_TRUE(actualRenderbuffer == EGL_SINGLE_BUFFER);

        glClearColor(0.0, 1.0, 0.0, 1.0);
        glClear(GL_COLOR_BUFFER_BIT);
        glFlush();
        ASSERT_GL_NO_ERROR();
        // Flush should result in update of screen. Must be visually confirmed.
        // Pixel test for automation.
        EXPECT_PIXEL_COLOR_EQ(1, 1, GLColor::green);
    }
    else
    {
        std::cout << "SKIP test, no EGL_SINGLE_BUFFER support." << std::endl;
    }

    EXPECT_EGL_TRUE(eglMakeCurrent(mDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, context));
    ASSERT_EGL_SUCCESS() << "eglMakeCurrent - uncurrent failed.";

    eglDestroySurface(mDisplay, surface);
    surface = EGL_NO_SURFACE;
    osWindow->destroy();
    OSWindow::Delete(&osWindow);

    eglDestroyContext(mDisplay, context);
    context = EGL_NO_CONTEXT;
}

TEST_P(EGLSingleBufferTest, OnSetSurfaceAttrib)
{
    ANGLE_SKIP_TEST_IF(!IsEGLDisplayExtensionEnabled(mDisplay, "EGL_KHR_mutable_render_buffer"));

    EGLConfig config = EGL_NO_CONFIG_KHR;
    ANGLE_SKIP_TEST_IF(!chooseConfig(&config, true));

    EGLContext context = EGL_NO_CONTEXT;
    EXPECT_EGL_TRUE(createContext(config, &context));
    ASSERT_EGL_SUCCESS() << "eglCreateContext failed.";

    EGLSurface surface = EGL_NO_SURFACE;
    OSWindow *osWindow = OSWindow::New();
    osWindow->initialize("EGLSingleBufferTest", kWidth, kHeight);
    EXPECT_EGL_TRUE(
        createWindowSurface(config, osWindow->getNativeWindow(), &surface, EGL_BACK_BUFFER));
    ASSERT_EGL_SUCCESS() << "eglCreateWindowSurface failed.";

    EXPECT_EGL_TRUE(eglMakeCurrent(mDisplay, surface, surface, context));
    ASSERT_EGL_SUCCESS() << "eglMakeCurrent failed.";

    if (eglSurfaceAttrib(mDisplay, surface, EGL_RENDER_BUFFER, EGL_SINGLE_BUFFER))
    {
        // Transition into EGL_SINGLE_BUFFER mode.
        glClearColor(1.0, 1.0, 1.0, 1.0);
        glClear(GL_COLOR_BUFFER_BIT);
        EXPECT_EGL_TRUE(eglSwapBuffers(mDisplay, surface));

        EGLint actualRenderbuffer;
        EXPECT_EGL_TRUE(eglQueryContext(mDisplay, context, EGL_RENDER_BUFFER, &actualRenderbuffer));
        EXPECT_EGL_TRUE(actualRenderbuffer == EGL_SINGLE_BUFFER);

        glClearColor(0.0, 1.0, 0.0, 1.0);
        glClear(GL_COLOR_BUFFER_BIT);
        glFlush();
        // Flush should result in update of screen. Must be visually confirmed Green window.

        // Check color for automation.
        EXPECT_PIXEL_COLOR_EQ(1, 1, GLColor::green);

        // Switch back to EGL_BACK_BUFFEr and check.
        EXPECT_EGL_TRUE(eglSurfaceAttrib(mDisplay, surface, EGL_RENDER_BUFFER, EGL_BACK_BUFFER));
        glClearColor(1.0, 1.0, 1.0, 1.0);
        glClear(GL_COLOR_BUFFER_BIT);
        EXPECT_EGL_TRUE(eglSwapBuffers(mDisplay, surface));

        EXPECT_EGL_TRUE(eglQueryContext(mDisplay, context, EGL_RENDER_BUFFER, &actualRenderbuffer));
        EXPECT_EGL_TRUE(actualRenderbuffer == EGL_BACK_BUFFER);

        glClearColor(1.0, 0.0, 0.0, 1.0);
        glClear(GL_COLOR_BUFFER_BIT);
        EXPECT_PIXEL_COLOR_EQ(1, 1, GLColor::red);
    }
    else
    {
        std::cout << "EGL_SINGLE_BUFFER mode is not supported." << std::endl;
    }

    EXPECT_EGL_TRUE(eglMakeCurrent(mDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, context));
    ASSERT_EGL_SUCCESS() << "eglMakeCurrent - uncurrent failed.";

    eglDestroySurface(mDisplay, surface);
    surface = EGL_NO_SURFACE;
    osWindow->destroy();
    OSWindow::Delete(&osWindow);

    eglDestroyContext(mDisplay, context);
    context = EGL_NO_CONTEXT;
}

// Test that setting a surface to EGL_SINGLE_BUFFER after enabling
// EGL_FRONT_BUFFER_AUTO_REFRESH_ANDROID does not disable auto refresh
TEST_P(EGLAndroidAutoRefreshTest, Basic)
{
    ANGLE_SKIP_TEST_IF(!IsEGLDisplayExtensionEnabled(mDisplay, "EGL_KHR_mutable_render_buffer"));
    ANGLE_SKIP_TEST_IF(!IsAndroid());

    EGLConfig config = EGL_NO_CONFIG_KHR;
    ANGLE_SKIP_TEST_IF(!chooseConfig(&config, true));

    EGLContext context = EGL_NO_CONTEXT;
    EXPECT_EGL_TRUE(createContext(config, &context));
    ASSERT_EGL_SUCCESS() << "eglCreateContext failed.";

    EGLSurface surface = EGL_NO_SURFACE;
    OSWindow *osWindow = OSWindow::New();
    osWindow->initialize("EGLSingleBufferTest", kWidth, kHeight);
    EXPECT_EGL_TRUE(
        createWindowSurface(config, osWindow->getNativeWindow(), &surface, EGL_BACK_BUFFER));
    ASSERT_EGL_SUCCESS() << "eglCreateWindowSurface failed.";

    EXPECT_EGL_TRUE(eglMakeCurrent(mDisplay, surface, surface, context));
    ASSERT_EGL_SUCCESS() << "eglMakeCurrent failed.";

    EXPECT_EGL_TRUE(
        eglSurfaceAttrib(mDisplay, surface, EGL_FRONT_BUFFER_AUTO_REFRESH_ANDROID, EGL_TRUE));

    if (eglSurfaceAttrib(mDisplay, surface, EGL_RENDER_BUFFER, EGL_SINGLE_BUFFER))
    {
        // Transition into EGL_SINGLE_BUFFER mode.
        glClearColor(1.0, 1.0, 1.0, 1.0);
        glClear(GL_COLOR_BUFFER_BIT);
        EXPECT_EGL_TRUE(eglSwapBuffers(mDisplay, surface));

        EGLint actualRenderbuffer;
        EXPECT_EGL_TRUE(eglQueryContext(mDisplay, context, EGL_RENDER_BUFFER, &actualRenderbuffer));
        EXPECT_EGL_TRUE(actualRenderbuffer == EGL_SINGLE_BUFFER);

        glClearColor(0.0, 1.0, 0.0, 1.0);
        glClear(GL_COLOR_BUFFER_BIT);
        glFlush();
        // Flush should result in update of screen. Must be visually confirmed Green window.

        // Check color for automation.
        EXPECT_PIXEL_COLOR_EQ(1, 1, GLColor::green);

        // Switch back to EGL_BACK_BUFFER and check.
        EXPECT_EGL_TRUE(eglSurfaceAttrib(mDisplay, surface, EGL_RENDER_BUFFER, EGL_BACK_BUFFER));
        glClearColor(1.0, 1.0, 1.0, 1.0);
        glClear(GL_COLOR_BUFFER_BIT);
        EXPECT_EGL_TRUE(eglSwapBuffers(mDisplay, surface));

        EXPECT_EGL_TRUE(eglQueryContext(mDisplay, context, EGL_RENDER_BUFFER, &actualRenderbuffer));
        EXPECT_EGL_TRUE(actualRenderbuffer == EGL_BACK_BUFFER);

        glClearColor(1.0, 0.0, 0.0, 1.0);
        glClear(GL_COLOR_BUFFER_BIT);
        EXPECT_PIXEL_COLOR_EQ(1, 1, GLColor::red);
    }
    else
    {
        std::cout << "EGL_SINGLE_BUFFER mode is not supported." << std::endl;
    }

    EXPECT_EGL_TRUE(eglMakeCurrent(mDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, context));
    ASSERT_EGL_SUCCESS() << "eglMakeCurrent - uncurrent failed.";

    eglDestroySurface(mDisplay, surface);
    surface = EGL_NO_SURFACE;
    osWindow->destroy();
    OSWindow::Delete(&osWindow);

    eglDestroyContext(mDisplay, context);
    context = EGL_NO_CONTEXT;
}

}  // anonymous namespace

GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(EGLSingleBufferTest);
ANGLE_INSTANTIATE_TEST(EGLSingleBufferTest,
                       WithNoFixture(ES2_VULKAN()),
                       WithNoFixture(ES3_VULKAN()));

GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(EGLAndroidAutoRefreshTest);
ANGLE_INSTANTIATE_TEST(EGLAndroidAutoRefreshTest, WithNoFixture(ES3_VULKAN()));

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

GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(EGLFloatSurfaceTest);
ANGLE_INSTANTIATE_TEST(EGLFloatSurfaceTest,
                       WithNoFixture(ES2_OPENGL()),
                       WithNoFixture(ES3_OPENGL()),
                       WithNoFixture(ES2_VULKAN()),
                       WithNoFixture(ES3_VULKAN()));

GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(EGLSurfaceTest3);
ANGLE_INSTANTIATE_TEST(EGLSurfaceTest3,
                       WithNoFixture(ES3_D3D11()),
                       WithNoFixture(ES3_OPENGLES()),
                       WithNoFixture(ES3_VULKAN()),
                       WithNoFixture(ES3_VULKAN_SWIFTSHADER()));

#if defined(ANGLE_ENABLE_D3D11)
ANGLE_INSTANTIATE_TEST(EGLSurfaceTestD3D11, WithNoFixture(ES2_D3D11()), WithNoFixture(ES3_D3D11()));
#endif
