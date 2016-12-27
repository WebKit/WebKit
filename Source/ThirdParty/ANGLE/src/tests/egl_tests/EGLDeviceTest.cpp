//
// Copyright 2015 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#ifndef ANGLE_ENABLE_D3D9
#define ANGLE_ENABLE_D3D9
#endif

#ifndef ANGLE_ENABLE_D3D11
#define ANGLE_ENABLE_D3D11
#endif

#include <d3d11.h>

#include "com_utils.h"
#include "OSWindow.h"
#include "test_utils/ANGLETest.h"

using namespace angle;

class EGLDeviceCreationTest : public testing::Test
{
  protected:
    EGLDeviceCreationTest()
        : mD3D11Available(false),
          mD3D11Module(nullptr),
          mD3D11CreateDevice(nullptr),
          mDevice(nullptr),
          mDeviceContext(nullptr),
          mDeviceCreationD3D11ExtAvailable(false),
          mOSWindow(nullptr),
          mDisplay(EGL_NO_DISPLAY),
          mSurface(EGL_NO_SURFACE),
          mContext(EGL_NO_CONTEXT),
          mConfig(0)
    {
    }

    void SetUp() override
    {
        mD3D11Module = LoadLibrary(TEXT("d3d11.dll"));
        if (mD3D11Module == nullptr)
        {
            std::cout << "Unable to LoadLibrary D3D11" << std::endl;
            return;
        }

        mD3D11CreateDevice = reinterpret_cast<PFN_D3D11_CREATE_DEVICE>(
            GetProcAddress(mD3D11Module, "D3D11CreateDevice"));
        if (mD3D11CreateDevice == nullptr)
        {
            std::cout << "Could not retrieve D3D11CreateDevice from d3d11.dll" << std::endl;
            return;
        }

        mD3D11Available = true;

        const char *extensionString =
            static_cast<const char *>(eglQueryString(EGL_NO_DISPLAY, EGL_EXTENSIONS));
        if (strstr(extensionString, "EGL_ANGLE_device_creation"))
        {
            if (strstr(extensionString, "EGL_ANGLE_device_creation_d3d11"))
            {
                mDeviceCreationD3D11ExtAvailable = true;
            }
        }
    }

    void TearDown() override
    {
        SafeRelease(mDevice);
        SafeRelease(mDeviceContext);

        SafeDelete(mOSWindow);

        if (mSurface != EGL_NO_SURFACE)
        {
            eglDestroySurface(mDisplay, mSurface);
            mSurface = EGL_NO_SURFACE;
        }

        if (mContext != EGL_NO_CONTEXT)
        {
            eglDestroyContext(mDisplay, mContext);
            mContext = EGL_NO_CONTEXT;
        }

        if (mDisplay != EGL_NO_DISPLAY)
        {
            eglTerminate(mDisplay);
            mDisplay = EGL_NO_DISPLAY;
        }
    }

    void CreateD3D11Device()
    {
        ASSERT_TRUE(mD3D11Available);
        ASSERT_EQ(nullptr, mDevice);  // The device shouldn't be created twice

        HRESULT hr =
            mD3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, 0, 0, nullptr, 0,
                               D3D11_SDK_VERSION, &mDevice, &mFeatureLevel, &mDeviceContext);

        ASSERT_TRUE(SUCCEEDED(hr));
        ASSERT_GE(mFeatureLevel, D3D_FEATURE_LEVEL_9_3);
    }

    void CreateD3D11FL9_3Device()
    {
        ASSERT_TRUE(mD3D11Available);
        ASSERT_EQ(nullptr, mDevice);

        D3D_FEATURE_LEVEL fl93 = D3D_FEATURE_LEVEL_9_3;

        HRESULT hr =
            mD3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, 0, 0, &fl93, 1, D3D11_SDK_VERSION,
                               &mDevice, &mFeatureLevel, &mDeviceContext);

        ASSERT_TRUE(SUCCEEDED(hr));
    }

    void CreateWindowSurface()
    {
        EGLint majorVersion, minorVersion;
        ASSERT_EGL_TRUE(eglInitialize(mDisplay, &majorVersion, &minorVersion));

        eglBindAPI(EGL_OPENGL_ES_API);
        ASSERT_EGL_SUCCESS();

        // Choose a config
        const EGLint configAttributes[] = {EGL_NONE};
        EGLint configCount = 0;
        ASSERT_EGL_TRUE(eglChooseConfig(mDisplay, configAttributes, &mConfig, 1, &configCount));

        // Create an OS Window
        mOSWindow = CreateOSWindow();
        mOSWindow->initialize("EGLSurfaceTest", 64, 64);

        // Create window surface
        mSurface = eglCreateWindowSurface(mDisplay, mConfig, mOSWindow->getNativeWindow(), nullptr);
        ASSERT_EGL_SUCCESS();

        // Create EGL context
        EGLint contextAttibutes[] = {EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE};
        mContext = eglCreateContext(mDisplay, mConfig, nullptr, contextAttibutes);
        ASSERT_EGL_SUCCESS();

        // Make the surface current
        eglMakeCurrent(mDisplay, mSurface, mSurface, mContext);
        ASSERT_EGL_SUCCESS();
    }

    // This triggers a D3D device lost on current Windows systems
    // This behavior could potentially change in the future
    void trigger9_3DeviceLost()
    {
        ID3D11Buffer *gsBuffer       = nullptr;
        D3D11_BUFFER_DESC bufferDesc = {0};
        bufferDesc.ByteWidth         = 64;
        bufferDesc.Usage             = D3D11_USAGE_DEFAULT;
        bufferDesc.BindFlags         = D3D11_BIND_CONSTANT_BUFFER;

        HRESULT result = mDevice->CreateBuffer(&bufferDesc, nullptr, &gsBuffer);
        ASSERT_TRUE(SUCCEEDED(result));

        mDeviceContext->GSSetConstantBuffers(0, 1, &gsBuffer);
        SafeRelease(gsBuffer);
        gsBuffer = nullptr;

        result = mDevice->GetDeviceRemovedReason();
        ASSERT_TRUE(FAILED(result));
    }

    bool mD3D11Available;
    HMODULE mD3D11Module;
    PFN_D3D11_CREATE_DEVICE mD3D11CreateDevice;

    ID3D11Device *mDevice;
    ID3D11DeviceContext *mDeviceContext;
    D3D_FEATURE_LEVEL mFeatureLevel;

    bool mDeviceCreationD3D11ExtAvailable;

    OSWindow *mOSWindow;

    EGLDisplay mDisplay;
    EGLSurface mSurface;
    EGLContext mContext;
    EGLConfig mConfig;
};

// Test that creating a EGLDeviceEXT from D3D11 device works, and it can be queried to retrieve
// D3D11 device
TEST_F(EGLDeviceCreationTest, BasicD3D11Device)
{
    if (!mDeviceCreationD3D11ExtAvailable || !mD3D11Available)
    {
        std::cout << "EGLDevice creation and/or D3D11 not available, skipping test" << std::endl;
        return;
    }

    CreateD3D11Device();

    EGLDeviceEXT eglDevice =
        eglCreateDeviceANGLE(EGL_D3D11_DEVICE_ANGLE, reinterpret_cast<void *>(mDevice), nullptr);
    ASSERT_NE(EGL_NO_DEVICE_EXT, eglDevice);
    ASSERT_EGL_SUCCESS();

    EGLAttrib deviceAttrib;
    eglQueryDeviceAttribEXT(eglDevice, EGL_D3D11_DEVICE_ANGLE, &deviceAttrib);
    ASSERT_EGL_SUCCESS();

    ID3D11Device *queriedDevice = reinterpret_cast<ID3D11Device *>(deviceAttrib);
    ASSERT_EQ(mFeatureLevel, queriedDevice->GetFeatureLevel());

    eglReleaseDeviceANGLE(eglDevice);
}

// Test that creating a EGLDeviceEXT from D3D11 device works, and it can be queried to retrieve
// D3D11 device
TEST_F(EGLDeviceCreationTest, BasicD3D11DeviceViaFuncPointer)
{
    if (!mDeviceCreationD3D11ExtAvailable || !mD3D11Available)
    {
        std::cout << "EGLDevice creation and/or D3D11 not available, skipping test" << std::endl;
        return;
    }

    CreateD3D11Device();

    PFNEGLCREATEDEVICEANGLEPROC createDeviceANGLE =
        (PFNEGLCREATEDEVICEANGLEPROC)eglGetProcAddress("eglCreateDeviceANGLE");
    PFNEGLRELEASEDEVICEANGLEPROC releaseDeviceANGLE =
        (PFNEGLRELEASEDEVICEANGLEPROC)eglGetProcAddress("eglReleaseDeviceANGLE");

    EGLDeviceEXT eglDevice =
        createDeviceANGLE(EGL_D3D11_DEVICE_ANGLE, reinterpret_cast<void *>(mDevice), nullptr);
    ASSERT_NE(EGL_NO_DEVICE_EXT, eglDevice);
    ASSERT_EGL_SUCCESS();

    EGLAttrib deviceAttrib;
    eglQueryDeviceAttribEXT(eglDevice, EGL_D3D11_DEVICE_ANGLE, &deviceAttrib);
    ASSERT_EGL_SUCCESS();

    ID3D11Device *queriedDevice = reinterpret_cast<ID3D11Device *>(deviceAttrib);
    ASSERT_EQ(mFeatureLevel, queriedDevice->GetFeatureLevel());

    releaseDeviceANGLE(eglDevice);
}

// Test that creating a EGLDeviceEXT from D3D11 device works, and can be used for rendering
TEST_F(EGLDeviceCreationTest, RenderingUsingD3D11Device)
{
    if (!mD3D11Available)
    {
        std::cout << "D3D11 not available, skipping test" << std::endl;
        return;
    }

    CreateD3D11Device();

    EGLDeviceEXT eglDevice =
        eglCreateDeviceANGLE(EGL_D3D11_DEVICE_ANGLE, reinterpret_cast<void *>(mDevice), nullptr);
    ASSERT_EGL_SUCCESS();

    // Create an EGLDisplay using the EGLDevice
    mDisplay = eglGetPlatformDisplayEXT(EGL_PLATFORM_DEVICE_EXT, eglDevice, nullptr);
    ASSERT_NE(EGL_NO_DISPLAY, mDisplay);

    // Create a surface using the display
    CreateWindowSurface();

    // Perform some very basic rendering
    glClearColor(1.0f, 0.0f, 1.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    EXPECT_PIXEL_EQ(32, 32, 255, 0, 255, 255);

    // Note that we must call TearDown() before we release the EGL device, since the display
    // depends on the device
    TearDown();

    eglReleaseDeviceANGLE(eglDevice);
}

// Test that ANGLE doesn't try to recreate a D3D11 device if the inputted one is lost
TEST_F(EGLDeviceCreationTest, D3D11DeviceRecovery)
{
    if (!mD3D11Available)
    {
        std::cout << "D3D11 not available, skipping test" << std::endl;
        return;
    }

    // Force Feature Level 9_3 so we can easily trigger a device lost later
    CreateD3D11FL9_3Device();

    EGLDeviceEXT eglDevice =
        eglCreateDeviceANGLE(EGL_D3D11_DEVICE_ANGLE, reinterpret_cast<void *>(mDevice), nullptr);
    ASSERT_EGL_SUCCESS();

    // Create an EGLDisplay using the EGLDevice
    mDisplay = eglGetPlatformDisplayEXT(EGL_PLATFORM_DEVICE_EXT, eglDevice, nullptr);
    ASSERT_TRUE(mDisplay != EGL_NO_DISPLAY);

    // Create a surface using the display
    CreateWindowSurface();

    // Perform some very basic rendering
    glClearColor(1.0f, 0.0f, 1.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    EXPECT_PIXEL_EQ(32, 32, 255, 0, 255, 255);
    ASSERT_GL_NO_ERROR();

    // ANGLE's SwapChain11::initPassThroughResources doesn't handle device lost before
    // eglSwapBuffers, so we must call eglSwapBuffers before we lose the device.
    ASSERT_EGL_TRUE(eglSwapBuffers(mDisplay, mSurface));

    // Trigger a lost device
    trigger9_3DeviceLost();

    // Destroy the old EGL Window Surface
    if (mSurface != EGL_NO_SURFACE)
    {
        eglDestroySurface(mDisplay, mSurface);
        mSurface = EGL_NO_SURFACE;
    }

    // Try to create a new window surface. In certain configurations this will recreate the D3D11
    // device. We want to test that it doesn't recreate the D3D11 device when EGLDeviceEXT is
    // used. The window surface creation should fail if a new D3D11 device isn't created.
    mSurface = eglCreateWindowSurface(mDisplay, mConfig, mOSWindow->getNativeWindow(), nullptr);
    ASSERT_EQ(EGL_NO_SURFACE, mSurface);
    ASSERT_EGL_ERROR(EGL_BAD_ALLOC);

    // Get the D3D11 device out of the EGLDisplay again. It should be the same one as above.
    EGLAttrib device       = 0;
    EGLAttrib newEglDevice = 0;
    ASSERT_EGL_TRUE(eglQueryDisplayAttribEXT(mDisplay, EGL_DEVICE_EXT, &newEglDevice));
    ASSERT_EGL_TRUE(eglQueryDeviceAttribEXT(reinterpret_cast<EGLDeviceEXT>(newEglDevice),
                                                EGL_D3D11_DEVICE_ANGLE, &device));
    ID3D11Device *newDevice = reinterpret_cast<ID3D11Device *>(device);

    ASSERT_EQ(reinterpret_cast<EGLDeviceEXT>(newEglDevice), eglDevice);
    ASSERT_EQ(newDevice, mDevice);

    // Note that we must call TearDown() before we release the EGL device, since the display
    // depends on the device
    TearDown();

    eglReleaseDeviceANGLE(eglDevice);
}

// Test that calling eglGetPlatformDisplayEXT with the same device returns the same display
TEST_F(EGLDeviceCreationTest, getPlatformDisplayTwice)
{
    if (!mD3D11Available)
    {
        std::cout << "D3D11 not available, skipping test" << std::endl;
        return;
    }

    CreateD3D11Device();

    EGLDeviceEXT eglDevice =
        eglCreateDeviceANGLE(EGL_D3D11_DEVICE_ANGLE, reinterpret_cast<void *>(mDevice), nullptr);
    ASSERT_EGL_SUCCESS();

    // Create an EGLDisplay using the EGLDevice
    EGLDisplay display1 = eglGetPlatformDisplayEXT(EGL_PLATFORM_DEVICE_EXT, eglDevice, nullptr);
    ASSERT_NE(EGL_NO_DISPLAY, display1);

    EGLDisplay display2 = eglGetPlatformDisplayEXT(EGL_PLATFORM_DEVICE_EXT, eglDevice, nullptr);
    ASSERT_NE(EGL_NO_DISPLAY, display2);

    ASSERT_EQ(display1, display2);

    eglTerminate(display1);
    eglReleaseDeviceANGLE(eglDevice);
}

// Test that creating a EGLDeviceEXT from an invalid D3D11 device fails
TEST_F(EGLDeviceCreationTest, InvalidD3D11Device)
{
    if (!mDeviceCreationD3D11ExtAvailable || !mD3D11Available)
    {
        std::cout << "EGLDevice creation and/or D3D11 not available, skipping test" << std::endl;
        return;
    }

    CreateD3D11Device();

    // Use mDeviceContext instead of mDevice
    EGLDeviceEXT eglDevice = eglCreateDeviceANGLE(
        EGL_D3D11_DEVICE_ANGLE, reinterpret_cast<void *>(mDeviceContext), nullptr);
    EXPECT_EQ(EGL_NO_DEVICE_EXT, eglDevice);
    EXPECT_EGL_ERROR(EGL_BAD_ATTRIBUTE);
}

// Test that EGLDeviceEXT holds a ref to the D3D11 device
TEST_F(EGLDeviceCreationTest, D3D11DeviceReferenceCounting)
{
    if (!mDeviceCreationD3D11ExtAvailable || !mD3D11Available)
    {
        std::cout << "EGLDevice creation and/or D3D11 not available, skipping test" << std::endl;
        return;
    }

    CreateD3D11Device();

    EGLDeviceEXT eglDevice =
        eglCreateDeviceANGLE(EGL_D3D11_DEVICE_ANGLE, reinterpret_cast<void *>(mDevice), nullptr);
    ASSERT_NE(EGL_NO_DEVICE_EXT, eglDevice);
    ASSERT_EGL_SUCCESS();

    // Now release our D3D11 device/context
    SafeRelease(mDevice);
    SafeRelease(mDeviceContext);

    EGLAttrib deviceAttrib;
    eglQueryDeviceAttribEXT(eglDevice, EGL_D3D11_DEVICE_ANGLE, &deviceAttrib);
    ASSERT_EGL_SUCCESS();

    ID3D11Device *queriedDevice = reinterpret_cast<ID3D11Device *>(deviceAttrib);
    ASSERT_EQ(mFeatureLevel, queriedDevice->GetFeatureLevel());

    eglReleaseDeviceANGLE(eglDevice);
}

// Test that creating a EGLDeviceEXT from a D3D9 device fails
TEST_F(EGLDeviceCreationTest, AnyD3D9Device)
{
    if (!mDeviceCreationD3D11ExtAvailable)
    {
        std::cout << "EGLDevice creation not available, skipping test" << std::endl;
        return;
    }

    std::string fakeD3DDevice = "This is a string, not a D3D device";

    EGLDeviceEXT eglDevice = eglCreateDeviceANGLE(
        EGL_D3D9_DEVICE_ANGLE, reinterpret_cast<void *>(&fakeD3DDevice), nullptr);
    EXPECT_EQ(EGL_NO_DEVICE_EXT, eglDevice);
    EXPECT_EGL_ERROR(EGL_BAD_ATTRIBUTE);
}

class EGLDeviceQueryTest : public ANGLETest
{
  protected:
    EGLDeviceQueryTest()
    {
        mQueryDisplayAttribEXT = nullptr;
        mQueryDeviceAttribEXT  = nullptr;
        mQueryDeviceStringEXT  = nullptr;
    }

    void SetUp() override
    {
        ANGLETest::SetUp();

        const char *extensionString =
            static_cast<const char *>(eglQueryString(getEGLWindow()->getDisplay(), EGL_EXTENSIONS));
        if (strstr(extensionString, "EGL_EXT_device_query"))
        {
            mQueryDisplayAttribEXT =
                (PFNEGLQUERYDISPLAYATTRIBEXTPROC)eglGetProcAddress("eglQueryDisplayAttribEXT");
            mQueryDeviceAttribEXT =
                (PFNEGLQUERYDEVICEATTRIBEXTPROC)eglGetProcAddress("eglQueryDeviceAttribEXT");
            mQueryDeviceStringEXT =
                (PFNEGLQUERYDEVICESTRINGEXTPROC)eglGetProcAddress("eglQueryDeviceStringEXT");
        }

        if (!mQueryDeviceStringEXT)
        {
            FAIL() << "ANGLE extension EGL_EXT_device_query export eglQueryDeviceStringEXT was not "
                      "found";
        }

        if (!mQueryDisplayAttribEXT)
        {
            FAIL() << "ANGLE extension EGL_EXT_device_query export eglQueryDisplayAttribEXT was "
                      "not found";
        }

        if (!mQueryDeviceAttribEXT)
        {
            FAIL() << "ANGLE extension EGL_EXT_device_query export eglQueryDeviceAttribEXT was not "
                      "found";
        }

        EGLAttrib angleDevice = 0;
        EXPECT_EGL_TRUE(
            mQueryDisplayAttribEXT(getEGLWindow()->getDisplay(), EGL_DEVICE_EXT, &angleDevice));
        extensionString = static_cast<const char *>(
            mQueryDeviceStringEXT(reinterpret_cast<EGLDeviceEXT>(angleDevice), EGL_EXTENSIONS));
        if (strstr(extensionString, "EGL_ANGLE_device_d3d") == NULL)
        {
            FAIL() << "ANGLE extension EGL_ANGLE_device_d3d was not found";
        }
    }

    void TearDown() override { ANGLETest::TearDown(); }

    PFNEGLQUERYDISPLAYATTRIBEXTPROC mQueryDisplayAttribEXT;
    PFNEGLQUERYDEVICEATTRIBEXTPROC mQueryDeviceAttribEXT;
    PFNEGLQUERYDEVICESTRINGEXTPROC mQueryDeviceStringEXT;
};

// This test attempts to obtain a D3D11 device and a D3D9 device using the eglQueryDeviceAttribEXT
// function.
// If the test is configured to use D3D11 then it should succeed to obtain a D3D11 device.
// If the test is confitured to use D3D9, then it should succeed to obtain a D3D9 device.
TEST_P(EGLDeviceQueryTest, QueryDevice)
{
    EGLAttrib device      = 0;
    EGLAttrib angleDevice = 0;
    if (getPlatformRenderer() == EGL_PLATFORM_ANGLE_TYPE_D3D11_ANGLE)
    {
        EXPECT_EGL_TRUE(
            mQueryDisplayAttribEXT(getEGLWindow()->getDisplay(), EGL_DEVICE_EXT, &angleDevice));
        EXPECT_EGL_TRUE(mQueryDeviceAttribEXT(reinterpret_cast<EGLDeviceEXT>(angleDevice),
                                              EGL_D3D11_DEVICE_ANGLE, &device));
        ID3D11Device *d3d11Device = reinterpret_cast<ID3D11Device *>(device);
        IDXGIDevice *dxgiDevice = DynamicCastComObject<IDXGIDevice>(d3d11Device);
        EXPECT_TRUE(dxgiDevice != nullptr);
        SafeRelease(dxgiDevice);
    }

    if (getPlatformRenderer() == EGL_PLATFORM_ANGLE_TYPE_D3D9_ANGLE)
    {
        EXPECT_EGL_TRUE(
            mQueryDisplayAttribEXT(getEGLWindow()->getDisplay(), EGL_DEVICE_EXT, &angleDevice));
        EXPECT_EGL_TRUE(mQueryDeviceAttribEXT(reinterpret_cast<EGLDeviceEXT>(angleDevice),
                                              EGL_D3D9_DEVICE_ANGLE, &device));
        IDirect3DDevice9 *d3d9Device = reinterpret_cast<IDirect3DDevice9 *>(device);
        IDirect3D9 *d3d9 = nullptr;
        EXPECT_EQ(S_OK, d3d9Device->GetDirect3D(&d3d9));
        EXPECT_TRUE(d3d9 != nullptr);
        SafeRelease(d3d9);
    }
}

// This test attempts to obtain a D3D11 device from a D3D9 configured system and a D3D9 device from
// a D3D11 configured system using the eglQueryDeviceAttribEXT function.
// If the test is configured to use D3D11 then it should fail to obtain a D3D11 device.
// If the test is confitured to use D3D9, then it should fail to obtain a D3D9 device.
TEST_P(EGLDeviceQueryTest, QueryDeviceBadAttribute)
{
    EGLAttrib device      = 0;
    EGLAttrib angleDevice = 0;
    if (getPlatformRenderer() == EGL_PLATFORM_ANGLE_TYPE_D3D11_ANGLE)
    {
        EXPECT_EGL_TRUE(
            mQueryDisplayAttribEXT(getEGLWindow()->getDisplay(), EGL_DEVICE_EXT, &angleDevice));
        EXPECT_EGL_FALSE(mQueryDeviceAttribEXT(reinterpret_cast<EGLDeviceEXT>(angleDevice),
                                               EGL_D3D9_DEVICE_ANGLE, &device));
    }

    if (getPlatformRenderer() == EGL_PLATFORM_ANGLE_TYPE_D3D9_ANGLE)
    {
        EXPECT_EGL_TRUE(
            mQueryDisplayAttribEXT(getEGLWindow()->getDisplay(), EGL_DEVICE_EXT, &angleDevice));
        EXPECT_EGL_FALSE(mQueryDeviceAttribEXT(reinterpret_cast<EGLDeviceEXT>(angleDevice),
                                               EGL_D3D11_DEVICE_ANGLE, &device));
    }
}

// Ensure that:
//    - calling getPlatformDisplayEXT using ANGLE_Platform with some parameters
//    - extracting the EGLDeviceEXT from the EGLDisplay
//    - calling getPlatformDisplayEXT with this EGLDeviceEXT
// results in the same EGLDisplay being returned from getPlatformDisplayEXT both times
TEST_P(EGLDeviceQueryTest, getPlatformDisplayDeviceReuse)
{
    EGLAttrib eglDevice = 0;
    EXPECT_EGL_TRUE(
              eglQueryDisplayAttribEXT(getEGLWindow()->getDisplay(), EGL_DEVICE_EXT, &eglDevice));

    EGLDisplay display2 = eglGetPlatformDisplayEXT(
        EGL_PLATFORM_DEVICE_EXT, reinterpret_cast<EGLDeviceEXT>(eglDevice), nullptr);
    EXPECT_EQ(getEGLWindow()->getDisplay(), display2);
}

// Use this to select which configurations (e.g. which renderer, which GLES major version) these
// tests should be run against.
ANGLE_INSTANTIATE_TEST(EGLDeviceQueryTest, ES2_D3D9(), ES2_D3D11());
