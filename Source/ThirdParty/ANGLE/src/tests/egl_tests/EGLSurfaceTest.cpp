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

#include "OSWindow.h"
#include "test_utils/ANGLETest.h"

#if defined(ANGLE_ENABLE_D3D11)
#define INITGUID
#include <guiddef.h>

#include <d3d11.h>
#include <dcomp.h>
#endif

namespace
{

class EGLSurfaceTest : public testing::Test
{
  protected:
    EGLSurfaceTest()
        : mDisplay(EGL_NO_DISPLAY),
          mWindowSurface(EGL_NO_SURFACE),
          mPbufferSurface(EGL_NO_SURFACE),
          mContext(EGL_NO_CONTEXT),
          mSecondContext(EGL_NO_CONTEXT),
          mOSWindow(nullptr)
    {
    }

    void SetUp() override
    {
        mOSWindow = CreateOSWindow();
        mOSWindow->initialize("EGLSurfaceTest", 64, 64);
    }

    // Release any resources created in the test body
    void TearDown() override
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
        SafeDelete(mOSWindow);

        ASSERT_TRUE(mWindowSurface == EGL_NO_SURFACE && mContext == EGL_NO_CONTEXT);
    }

    void initializeDisplay(EGLenum platformType)
    {
        PFNEGLGETPLATFORMDISPLAYEXTPROC eglGetPlatformDisplayEXT = reinterpret_cast<PFNEGLGETPLATFORMDISPLAYEXTPROC>(eglGetProcAddress("eglGetPlatformDisplayEXT"));
        ASSERT_TRUE(eglGetPlatformDisplayEXT != nullptr);

        std::vector<EGLint> displayAttributes;
        displayAttributes.push_back(EGL_PLATFORM_ANGLE_TYPE_ANGLE);
        displayAttributes.push_back(platformType);
        displayAttributes.push_back(EGL_PLATFORM_ANGLE_MAX_VERSION_MAJOR_ANGLE);
        displayAttributes.push_back(EGL_DONT_CARE);
        displayAttributes.push_back(EGL_PLATFORM_ANGLE_MAX_VERSION_MINOR_ANGLE);
        displayAttributes.push_back(EGL_DONT_CARE);

        if (platformType == EGL_PLATFORM_ANGLE_TYPE_D3D9_ANGLE || platformType == EGL_PLATFORM_ANGLE_TYPE_D3D11_ANGLE)
        {
            displayAttributes.push_back(EGL_PLATFORM_ANGLE_DEVICE_TYPE_ANGLE);
            displayAttributes.push_back(EGL_PLATFORM_ANGLE_DEVICE_TYPE_HARDWARE_ANGLE);
        }
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
        EGLint contextAttibutes[] = {EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE};

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
        mWindowSurface = eglCreateWindowSurface(mDisplay, mConfig, mOSWindow->getNativeWindow(), &surfaceAttributes[0]);
        ASSERT_TRUE(eglGetError() == EGL_SUCCESS);

        mPbufferSurface = eglCreatePbufferSurface(mDisplay, mConfig, &surfaceAttributes[0]);
        initializeContext();
    }

    void initializeSurfaceWithDefaultConfig()
    {
        const EGLint configAttributes[] =
        {
            EGL_RED_SIZE, EGL_DONT_CARE,
            EGL_GREEN_SIZE, EGL_DONT_CARE,
            EGL_BLUE_SIZE, EGL_DONT_CARE,
            EGL_ALPHA_SIZE, EGL_DONT_CARE,
            EGL_DEPTH_SIZE, EGL_DONT_CARE,
            EGL_STENCIL_SIZE, EGL_DONT_CARE,
            EGL_SAMPLE_BUFFERS, 0,
            EGL_NONE
        };

        EGLint configCount;
        EGLConfig config;
        ASSERT_TRUE(eglChooseConfig(mDisplay, configAttributes, &config, 1, &configCount) || (configCount != 1) == EGL_TRUE);

        initializeSurface(config);
    }

    GLuint createProgram()
    {
        const std::string testVertexShaderSource =
            R"(attribute highp vec4 position;

            void main(void)
            {
                gl_Position = position;
            })";

        const std::string testFragmentShaderSource =
            R"(void main(void)
            {
                gl_FragColor = vec4(1.0, 0.0, 0.0, 1.0);
            })";

        return CompileProgram(testVertexShaderSource, testFragmentShaderSource);
    }

    void drawWithProgram(GLuint program)
    {
        glClearColor(0, 0, 0, 1);
        glClear(GL_COLOR_BUFFER_BIT);

        GLint positionLocation = glGetAttribLocation(program, "position");

        glUseProgram(program);

        const GLfloat vertices[] =
        {
            -1.0f,  1.0f, 0.5f,
            -1.0f, -1.0f, 0.5f,
             1.0f, -1.0f, 0.5f,

            -1.0f,  1.0f, 0.5f,
             1.0f, -1.0f, 0.5f,
             1.0f,  1.0f, 0.5f,
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

        mWindowSurface = eglCreateWindowSurface(mDisplay, mConfig, mOSWindow->getNativeWindow(), &surfaceAttributes[0]);
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

// Test a surface bug where we could have two Window surfaces active
// at one time, blocking message loops. See http://crbug.com/475085
TEST_F(EGLSurfaceTest, MessageLoopBug)
{
    const char *extensionsString = eglQueryString(EGL_NO_DISPLAY, EGL_EXTENSIONS);
    if (strstr(extensionsString, "EGL_ANGLE_platform_angle_d3d") == nullptr)
    {
        std::cout << "D3D Platform not supported in ANGLE" << std::endl;
        return;
    }

    initializeDisplay(EGL_PLATFORM_ANGLE_TYPE_D3D11_ANGLE);
    initializeSurfaceWithDefaultConfig();

    runMessageLoopTest(EGL_NO_SURFACE, EGL_NO_CONTEXT);
}

// Tests the message loop bug, but with setting a second context
// instead of null.
TEST_F(EGLSurfaceTest, MessageLoopBugContext)
{
    const char *extensionsString = eglQueryString(EGL_NO_DISPLAY, EGL_EXTENSIONS);
    if (strstr(extensionsString, "EGL_ANGLE_platform_angle_d3d") == nullptr)
    {
        std::cout << "D3D Platform not supported in ANGLE" << std::endl;
        return;
    }

    initializeDisplay(EGL_PLATFORM_ANGLE_TYPE_D3D11_ANGLE);
    initializeSurfaceWithDefaultConfig();

    runMessageLoopTest(mPbufferSurface, mSecondContext);
}

// Test a bug where calling makeCurrent twice would release the surface
TEST_F(EGLSurfaceTest, MakeCurrentTwice)
{
#if defined(ANGLE_PLATFORM_APPLE) && !defined(ANGLE_STANDALONE_BUILD)
    // TODO(cwallez) Make context creation return at least an OpenGL ES 2 context on
    // the Mac trybots.
    std::cout << "Test skipped temporarily skipped on the Mac trybots" << std::endl;
    return;
#endif

    initializeDisplay(EGL_PLATFORM_ANGLE_TYPE_DEFAULT_ANGLE);
    initializeSurfaceWithDefaultConfig();

    eglMakeCurrent(mDisplay, mWindowSurface, mWindowSurface, mContext);
    ASSERT_TRUE(eglGetError() == EGL_SUCCESS);

    eglMakeCurrent(mDisplay, mWindowSurface, mWindowSurface, mContext);
    ASSERT_TRUE(eglGetError() == EGL_SUCCESS);

    // Simple operation to test the FBO is set appropriately
    glClear(GL_COLOR_BUFFER_BIT);
}

// Test that the D3D window surface is correctly resized after calling swapBuffers
TEST_F(EGLSurfaceTest, ResizeD3DWindow)
{
    const char *extensionsString = eglQueryString(EGL_NO_DISPLAY, EGL_EXTENSIONS);
    if (strstr(extensionsString, "EGL_ANGLE_platform_angle_d3d") == nullptr)
    {
        std::cout << "D3D Platform not supported in ANGLE" << std::endl;
        return;
    }

    initializeDisplay(EGL_PLATFORM_ANGLE_TYPE_D3D11_ANGLE);
    initializeSurfaceWithDefaultConfig();
    initializeContext();

    eglMakeCurrent(mDisplay, mWindowSurface, mWindowSurface, mContext);
    eglSwapBuffers(mDisplay, mWindowSurface);
    ASSERT_EGL_SUCCESS();

    EGLint height;
    eglQuerySurface(mDisplay, mWindowSurface, EGL_HEIGHT, &height);
    ASSERT_EGL_SUCCESS();
    ASSERT_EQ(64, height);  // initial size

    // set window's height to 0
    mOSWindow->resize(64, 0);

    eglSwapBuffers(mDisplay, mWindowSurface);
    ASSERT_EGL_SUCCESS();

    eglQuerySurface(mDisplay, mWindowSurface, EGL_HEIGHT, &height);
    ASSERT_EGL_SUCCESS();
    ASSERT_EQ(0, height);

    // restore window's height
    mOSWindow->resize(64, 64);

    eglSwapBuffers(mDisplay, mWindowSurface);
    ASSERT_EGL_SUCCESS();

    eglQuerySurface(mDisplay, mWindowSurface, EGL_HEIGHT, &height);
    ASSERT_EGL_SUCCESS();
    ASSERT_EQ(64, height);
}

// Test creating a surface that supports a EGLConfig with 16bit
// support GL_RGB565
TEST_F(EGLSurfaceTest, CreateWithEGLConfig5650Support)
{
    if (!ANGLETest::eglDisplayExtensionEnabled(EGL_NO_DISPLAY, "EGL_ANGLE_platform_angle_d3d"))
    {
        std::cout << "D3D Platform not supported in ANGLE" << std::endl;
        return;
    }

    const EGLint configAttributes[] =
    {
        EGL_RED_SIZE, 5,
        EGL_GREEN_SIZE, 6,
        EGL_BLUE_SIZE, 5,
        EGL_ALPHA_SIZE, 0,
        EGL_DEPTH_SIZE, 0,
        EGL_STENCIL_SIZE, 0,
        EGL_SAMPLE_BUFFERS, 0,
        EGL_NONE
    };

    initializeDisplay(EGL_PLATFORM_ANGLE_TYPE_D3D11_ANGLE);
    EGLConfig config;
    if (EGLWindow::FindEGLConfig(mDisplay, configAttributes, &config) == EGL_FALSE)
    {
        std::cout << "EGLConfig for a GL_RGB565 surface is not supported, skipping test" << std::endl;
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
TEST_F(EGLSurfaceTest, CreateWithEGLConfig4444Support)
{
    if (!ANGLETest::eglDisplayExtensionEnabled(EGL_NO_DISPLAY, "EGL_ANGLE_platform_angle_d3d"))
    {
        std::cout << "D3D Platform not supported in ANGLE" << std::endl;
        return;
    }

    const EGLint configAttributes[] =
    {
        EGL_RED_SIZE, 4,
        EGL_GREEN_SIZE, 4,
        EGL_BLUE_SIZE, 4,
        EGL_ALPHA_SIZE, 4,
        EGL_DEPTH_SIZE, 0,
        EGL_STENCIL_SIZE, 0,
        EGL_SAMPLE_BUFFERS, 0,
        EGL_NONE
    };

    initializeDisplay(EGL_PLATFORM_ANGLE_TYPE_D3D11_ANGLE);
    EGLConfig config;
    if (EGLWindow::FindEGLConfig(mDisplay, configAttributes, &config) == EGL_FALSE)
    {
        std::cout << "EGLConfig for a GL_RGBA4 surface is not supported, skipping test" << std::endl;
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
TEST_F(EGLSurfaceTest, CreateWithEGLConfig5551Support)
{
    if (!ANGLETest::eglDisplayExtensionEnabled(EGL_NO_DISPLAY, "EGL_ANGLE_platform_angle_d3d"))
    {
        std::cout << "D3D Platform not supported in ANGLE" << std::endl;
        return;
    }

    const EGLint configAttributes[] =
    {
        EGL_RED_SIZE, 5,
        EGL_GREEN_SIZE, 5,
        EGL_BLUE_SIZE, 5,
        EGL_ALPHA_SIZE, 1,
        EGL_DEPTH_SIZE, 0,
        EGL_STENCIL_SIZE, 0,
        EGL_SAMPLE_BUFFERS, 0,
        EGL_NONE
    };

    initializeDisplay(EGL_PLATFORM_ANGLE_TYPE_D3D11_ANGLE);
    EGLConfig config;
    if (EGLWindow::FindEGLConfig(mDisplay, configAttributes, &config) == EGL_FALSE)
    {
        std::cout << "EGLConfig for a GL_RGB5_A1 surface is not supported, skipping test" << std::endl;
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
TEST_F(EGLSurfaceTest, CreateWithEGLConfig8880Support)
{
    if (!ANGLETest::eglDisplayExtensionEnabled(EGL_NO_DISPLAY, "EGL_ANGLE_platform_angle_d3d"))
    {
        std::cout << "D3D Platform not supported in ANGLE" << std::endl;
        return;
    }

    const EGLint configAttributes[] =
    {
        EGL_RED_SIZE, 8,
        EGL_GREEN_SIZE, 8,
        EGL_BLUE_SIZE, 8,
        EGL_ALPHA_SIZE, 0,
        EGL_DEPTH_SIZE, 0,
        EGL_STENCIL_SIZE, 0,
        EGL_SAMPLE_BUFFERS, 0,
        EGL_NONE
    };

    initializeDisplay(EGL_PLATFORM_ANGLE_TYPE_D3D11_ANGLE);
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

#if defined(ANGLE_ENABLE_D3D11)
// Test that rendering to an IDCompositionSurface using a pbuffer works.
TEST_F(EGLSurfaceTest, CreateDirectCompositionSurface)
{
    if (!ANGLETest::eglDisplayExtensionEnabled(EGL_NO_DISPLAY, "EGL_ANGLE_platform_angle_d3d"))
    {
        std::cout << "D3D Platform not supported in ANGLE" << std::endl;
        return;
    }
    initializeDisplay(EGL_PLATFORM_ANGLE_TYPE_D3D11_ANGLE);

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

#endif
}
