//
// Copyright (c) 2015 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// DisplayWGL.h: WGL implementation of egl::Display

#include "libANGLE/renderer/gl/wgl/DisplayWGL.h"

#include "common/debug.h"
#include "libANGLE/Config.h"
#include "libANGLE/Display.h"
#include "libANGLE/Surface.h"
#include "libANGLE/renderer/gl/RendererGL.h"
#include "libANGLE/renderer/gl/renderergl_utils.h"
#include "libANGLE/renderer/gl/wgl/DXGISwapChainWindowSurfaceWGL.h"
#include "libANGLE/renderer/gl/wgl/FunctionsWGL.h"
#include "libANGLE/renderer/gl/wgl/PbufferSurfaceWGL.h"
#include "libANGLE/renderer/gl/wgl/WindowSurfaceWGL.h"
#include "libANGLE/renderer/gl/wgl/wgl_utils.h"

#include "platform/Platform.h"

#include <EGL/eglext.h>
#include <string>
#include <sstream>

namespace rx
{

class FunctionsGLWindows : public FunctionsGL
{
  public:
    FunctionsGLWindows(HMODULE openGLModule, PFNWGLGETPROCADDRESSPROC getProcAddressWGL)
        : mOpenGLModule(openGLModule),
          mGetProcAddressWGL(getProcAddressWGL)
    {
        ASSERT(mOpenGLModule);
        ASSERT(mGetProcAddressWGL);
    }

    ~FunctionsGLWindows() override {}

  private:
    void *loadProcAddress(const std::string &function) override
    {
        void *proc = reinterpret_cast<void*>(mGetProcAddressWGL(function.c_str()));
        if (!proc)
        {
            proc = reinterpret_cast<void*>(GetProcAddress(mOpenGLModule, function.c_str()));
        }
        return proc;
    }

    HMODULE mOpenGLModule;
    PFNWGLGETPROCADDRESSPROC mGetProcAddressWGL;
};

DisplayWGL::DisplayWGL()
    : DisplayGL(),
      mOpenGLModule(nullptr),
      mFunctionsWGL(nullptr),
      mFunctionsGL(nullptr),
      mHasRobustness(false),
      mWindowClass(0),
      mWindow(nullptr),
      mDeviceContext(nullptr),
      mPixelFormat(0),
      mWGLContext(nullptr),
      mUseDXGISwapChains(false),
      mDxgiModule(nullptr),
      mD3d11Module(nullptr),
      mD3D11DeviceHandle(nullptr),
      mD3D11Device(nullptr),
      mDisplay(nullptr)
{
}

DisplayWGL::~DisplayWGL()
{
}

egl::Error DisplayWGL::initialize(egl::Display *display)
{
    mDisplay = display;

    mOpenGLModule = LoadLibraryA("opengl32.dll");
    if (!mOpenGLModule)
    {
        return egl::Error(EGL_NOT_INITIALIZED, "Failed to load OpenGL library.");
    }

    mFunctionsWGL = new FunctionsWGL();
    mFunctionsWGL->initialize(mOpenGLModule, nullptr);

    // WGL can't grab extensions until it creates a context because it needs to load the driver's DLLs first.
    // Create a dummy context to load the driver and determine which GL versions are available.

    // Work around compile error from not defining "UNICODE" while Chromium does
    const LPSTR idcArrow = MAKEINTRESOURCEA(32512);

    std::string className = FormatString("ANGLE DisplayWGL 0x%0.8p Intermediate Window Class", mDisplay);

    WNDCLASSA intermediateClassDesc = { 0 };
    intermediateClassDesc.style = CS_OWNDC;
    intermediateClassDesc.lpfnWndProc = DefWindowProc;
    intermediateClassDesc.cbClsExtra = 0;
    intermediateClassDesc.cbWndExtra = 0;
    intermediateClassDesc.hInstance = GetModuleHandle(nullptr);
    intermediateClassDesc.hIcon = nullptr;
    intermediateClassDesc.hCursor = LoadCursorA(nullptr, idcArrow);
    intermediateClassDesc.hbrBackground = 0;
    intermediateClassDesc.lpszMenuName = nullptr;
    intermediateClassDesc.lpszClassName = className.c_str();
    mWindowClass = RegisterClassA(&intermediateClassDesc);
    if (!mWindowClass)
    {
        return egl::Error(EGL_NOT_INITIALIZED, "Failed to register intermediate OpenGL window class.");
    }

    HWND dummyWindow = CreateWindowExA(0,
                                       reinterpret_cast<const char *>(mWindowClass),
                                       "ANGLE Dummy Window",
                                       WS_OVERLAPPEDWINDOW,
                                       CW_USEDEFAULT,
                                       CW_USEDEFAULT,
                                       CW_USEDEFAULT,
                                       CW_USEDEFAULT,
                                       nullptr,
                                       nullptr,
                                       nullptr,
                                       nullptr);
    if (!dummyWindow)
    {
        return egl::Error(EGL_NOT_INITIALIZED, "Failed to create dummy OpenGL window.");
    }

    HDC dummyDeviceContext = GetDC(dummyWindow);
    if (!dummyDeviceContext)
    {
        return egl::Error(EGL_NOT_INITIALIZED, "Failed to get the device context of the dummy OpenGL window.");
    }

    const PIXELFORMATDESCRIPTOR pixelFormatDescriptor = wgl::GetDefaultPixelFormatDescriptor();

    int dummyPixelFormat = ChoosePixelFormat(dummyDeviceContext, &pixelFormatDescriptor);
    if (dummyPixelFormat == 0)
    {
        return egl::Error(EGL_NOT_INITIALIZED, "Could not find a compatible pixel format for the dummy OpenGL window.");
    }

    if (!SetPixelFormat(dummyDeviceContext, dummyPixelFormat, &pixelFormatDescriptor))
    {
        return egl::Error(EGL_NOT_INITIALIZED, "Failed to set the pixel format on the intermediate OpenGL window.");
    }

    HGLRC dummyWGLContext = mFunctionsWGL->createContext(dummyDeviceContext);
    if (!dummyDeviceContext)
    {
        return egl::Error(EGL_NOT_INITIALIZED, "Failed to create a WGL context for the dummy OpenGL window.");
    }

    if (!mFunctionsWGL->makeCurrent(dummyDeviceContext, dummyWGLContext))
    {
        return egl::Error(EGL_NOT_INITIALIZED, "Failed to make the dummy WGL context current.");
    }

    // Grab the GL version from this context and use it as the maximum version available.
    typedef const GLubyte* (GL_APIENTRYP PFNGLGETSTRINGPROC) (GLenum name);
    PFNGLGETSTRINGPROC getString = reinterpret_cast<PFNGLGETSTRINGPROC>(GetProcAddress(mOpenGLModule, "glGetString"));
    if (!getString)
    {
        return egl::Error(EGL_NOT_INITIALIZED, "Failed to get glGetString pointer.");
    }

    // Reinitialize the wgl functions to grab the extensions
    mFunctionsWGL->initialize(mOpenGLModule, dummyDeviceContext);

    bool hasWGLCreateContextRobustness =
        mFunctionsWGL->hasExtension("WGL_ARB_create_context_robustness");

    // Destroy the dummy window and context
    mFunctionsWGL->makeCurrent(dummyDeviceContext, nullptr);
    mFunctionsWGL->deleteContext(dummyWGLContext);
    ReleaseDC(dummyWindow, dummyDeviceContext);
    DestroyWindow(dummyWindow);

    const egl::AttributeMap &displayAttributes = display->getAttributeMap();
    EGLint requestedDisplayType = static_cast<EGLint>(displayAttributes.get(
        EGL_PLATFORM_ANGLE_TYPE_ANGLE, EGL_PLATFORM_ANGLE_TYPE_DEFAULT_ANGLE));
    if (requestedDisplayType == EGL_PLATFORM_ANGLE_TYPE_OPENGLES_ANGLE &&
        !mFunctionsWGL->hasExtension("WGL_EXT_create_context_es2_profile") &&
        !mFunctionsWGL->hasExtension("WGL_EXT_create_context_es_profile"))
    {
        return egl::Error(EGL_NOT_INITIALIZED,
                          "Cannot create an OpenGL ES platform on Windows without "
                          "the WGL_EXT_create_context_es(2)_profile extension.");
    }

    // Create the real intermediate context and windows
    mWindow = CreateWindowExA(0,
                              reinterpret_cast<const char *>(mWindowClass),
                              "ANGLE Intermediate Window",
                              WS_OVERLAPPEDWINDOW,
                              CW_USEDEFAULT,
                              CW_USEDEFAULT,
                              CW_USEDEFAULT,
                              CW_USEDEFAULT,
                              nullptr,
                              nullptr,
                              nullptr,
                              nullptr);
    if (!mWindow)
    {
        return egl::Error(EGL_NOT_INITIALIZED, "Failed to create intermediate OpenGL window.");
    }

    mDeviceContext = GetDC(mWindow);
    if (!mDeviceContext)
    {
        return egl::Error(EGL_NOT_INITIALIZED, "Failed to get the device context of the intermediate OpenGL window.");
    }

    if (mFunctionsWGL->choosePixelFormatARB)
    {
        std::vector<int> attribs = wgl::GetDefaultPixelFormatAttributes(false);

        UINT matchingFormats = 0;
        mFunctionsWGL->choosePixelFormatARB(mDeviceContext, &attribs[0], nullptr, 1u, &mPixelFormat,
                                            &matchingFormats);
    }

    if (mPixelFormat == 0)
    {
        mPixelFormat = ChoosePixelFormat(mDeviceContext, &pixelFormatDescriptor);
    }

    if (mPixelFormat == 0)
    {
        return egl::Error(EGL_NOT_INITIALIZED, "Could not find a compatible pixel format for the intermediate OpenGL window.");
    }

    if (!SetPixelFormat(mDeviceContext, mPixelFormat, &pixelFormatDescriptor))
    {
        return egl::Error(EGL_NOT_INITIALIZED, "Failed to set the pixel format on the intermediate OpenGL window.");
    }

    if (mFunctionsWGL->createContextAttribsARB)
    {
        int flags = 0;
        // TODO: allow debug contexts
        // TODO: handle robustness

        int mask = 0;

        if (requestedDisplayType == EGL_PLATFORM_ANGLE_TYPE_OPENGLES_ANGLE)
        {
            mask |= WGL_CONTEXT_ES_PROFILE_BIT_EXT;
        }
        else
        {
            // Request core profile
            mask |= WGL_CONTEXT_CORE_PROFILE_BIT_ARB;
        }

        std::vector<int> contextCreationAttributes;

        if (hasWGLCreateContextRobustness)
        {
            contextCreationAttributes.push_back(WGL_CONTEXT_RESET_NOTIFICATION_STRATEGY_ARB);
            contextCreationAttributes.push_back(WGL_LOSE_CONTEXT_ON_RESET_ARB);
        }

        // Don't request a specific version unless the user wants one.  WGL will return the highest version
        // that the driver supports if no version is requested.
        EGLint requestedMajorVersion = static_cast<EGLint>(
            displayAttributes.get(EGL_PLATFORM_ANGLE_MAX_VERSION_MAJOR_ANGLE, EGL_DONT_CARE));
        EGLint requestedMinorVersion = static_cast<EGLint>(
            displayAttributes.get(EGL_PLATFORM_ANGLE_MAX_VERSION_MINOR_ANGLE, EGL_DONT_CARE));
        if (requestedMajorVersion != EGL_DONT_CARE && requestedMinorVersion != EGL_DONT_CARE)
        {
            contextCreationAttributes.push_back(WGL_CONTEXT_MAJOR_VERSION_ARB);
            contextCreationAttributes.push_back(requestedMajorVersion);

            contextCreationAttributes.push_back(WGL_CONTEXT_MINOR_VERSION_ARB);
            contextCreationAttributes.push_back(requestedMinorVersion);
        }
        else
        {
            // the ES profile will give us ES version 1.1 unless a higher version is requested.
            // Requesting version 2.0 will give us the highest compatible version available (2.0,
            // 3.0, 3.1, etc).
            if (requestedDisplayType == EGL_PLATFORM_ANGLE_TYPE_OPENGLES_ANGLE)
            {
                contextCreationAttributes.push_back(WGL_CONTEXT_MAJOR_VERSION_ARB);
                contextCreationAttributes.push_back(2);

                contextCreationAttributes.push_back(WGL_CONTEXT_MINOR_VERSION_ARB);
                contextCreationAttributes.push_back(0);
            }
        }

        // Set the flag attributes
        if (flags != 0)
        {
            contextCreationAttributes.push_back(WGL_CONTEXT_FLAGS_ARB);
            contextCreationAttributes.push_back(flags);
        }

        // Set the mask attribute
        if (mask != 0)
        {
            contextCreationAttributes.push_back(WGL_CONTEXT_PROFILE_MASK_ARB);
            contextCreationAttributes.push_back(mask);
        }

        // Signal the end of the attributes
        contextCreationAttributes.push_back(0);
        contextCreationAttributes.push_back(0);

        mWGLContext = mFunctionsWGL->createContextAttribsARB(mDeviceContext, NULL, &contextCreationAttributes[0]);
    }

    // If wglCreateContextAttribsARB is unavailable or failed, try the standard wglCreateContext
    if (!mWGLContext)
    {
        // Don't have control over GL versions
        mWGLContext = mFunctionsWGL->createContext(mDeviceContext);
    }

    if (!mWGLContext)
    {
        return egl::Error(EGL_NOT_INITIALIZED, "Failed to create a WGL context for the intermediate OpenGL window.");
    }

    if (!mFunctionsWGL->makeCurrent(mDeviceContext, mWGLContext))
    {
        return egl::Error(EGL_NOT_INITIALIZED, "Failed to make the intermediate WGL context current.");
    }

    mFunctionsGL = new FunctionsGLWindows(mOpenGLModule, mFunctionsWGL->getProcAddress);
    mFunctionsGL->initialize();

    mHasRobustness = mFunctionsGL->getGraphicsResetStatus != nullptr;
    if (hasWGLCreateContextRobustness != mHasRobustness)
    {
        ANGLEPlatformCurrent()->logWarning(
            "WGL_ARB_create_context_robustness exists but unable to OpenGL context with "
            "robustness.");
    }

    // Intel OpenGL ES drivers are not currently supported due to bugs in the driver and ANGLE
    VendorID vendor = GetVendorID(mFunctionsGL);
    if (requestedDisplayType == EGL_PLATFORM_ANGLE_TYPE_OPENGLES_ANGLE && vendor == VENDOR_ID_INTEL)
    {
        return egl::Error(EGL_NOT_INITIALIZED, "Intel OpenGL ES drivers are not supported.");
    }

    // Create DXGI swap chains for windows that come from other processes.  Windows is unable to
    // SetPixelFormat on windows from other processes when a sandbox is enabled.
    HDC nativeDisplay = display->getNativeDisplayId();
    HWND nativeWindow = WindowFromDC(nativeDisplay);
    if (nativeWindow != nullptr)
    {
        DWORD currentProcessId = GetCurrentProcessId();
        DWORD windowProcessId;
        GetWindowThreadProcessId(nativeWindow, &windowProcessId);

        // AMD drivers advertise the WGL_NV_DX_interop and WGL_NV_DX_interop2 extensions but fail
        mUseDXGISwapChains = vendor != VENDOR_ID_AMD && (currentProcessId != windowProcessId);
    }
    else
    {
        mUseDXGISwapChains = false;
    }

    if (mUseDXGISwapChains)
    {
        egl::Error error = initializeD3DDevice();
        if (error.isError())
        {
            return error;
        }
    }

    return DisplayGL::initialize(display);
}

void DisplayWGL::terminate()
{
    DisplayGL::terminate();

    releaseD3DDevice(mD3D11DeviceHandle);

    mFunctionsWGL->makeCurrent(mDeviceContext, NULL);
    mFunctionsWGL->deleteContext(mWGLContext);
    mWGLContext = NULL;

    ReleaseDC(mWindow, mDeviceContext);
    mDeviceContext = NULL;

    DestroyWindow(mWindow);
    mWindow = NULL;

    UnregisterClassA(reinterpret_cast<const char*>(mWindowClass), NULL);
    mWindowClass = NULL;

    SafeDelete(mFunctionsWGL);
    SafeDelete(mFunctionsGL);

    FreeLibrary(mOpenGLModule);
    mOpenGLModule = nullptr;

    SafeRelease(mD3D11Device);

    if (mDxgiModule)
    {
        FreeLibrary(mDxgiModule);
        mDxgiModule = nullptr;
    }

    if (mD3d11Module)
    {
        FreeLibrary(mD3d11Module);
        mD3d11Module = nullptr;
    }

    ASSERT(mRegisteredD3DDevices.empty());
}

SurfaceImpl *DisplayWGL::createWindowSurface(const egl::SurfaceState &state,
                                             const egl::Config *configuration,
                                             EGLNativeWindowType window,
                                             const egl::AttributeMap &attribs)
{
    EGLint orientation = static_cast<EGLint>(attribs.get(EGL_SURFACE_ORIENTATION_ANGLE, 0));
    if (mUseDXGISwapChains)
    {
        return new DXGISwapChainWindowSurfaceWGL(state, getRenderer(), window, mD3D11Device,
                                                 mD3D11DeviceHandle, mWGLContext, mDeviceContext,
                                                 mFunctionsGL, mFunctionsWGL, orientation);
    }
    else
    {
        return new WindowSurfaceWGL(state, getRenderer(), window, mPixelFormat, mWGLContext,
                                    mFunctionsWGL, orientation);
    }
}

SurfaceImpl *DisplayWGL::createPbufferSurface(const egl::SurfaceState &state,
                                              const egl::Config *configuration,
                                              const egl::AttributeMap &attribs)
{
    EGLint width          = static_cast<EGLint>(attribs.get(EGL_WIDTH, 0));
    EGLint height         = static_cast<EGLint>(attribs.get(EGL_HEIGHT, 0));
    bool largest = (attribs.get(EGL_LARGEST_PBUFFER, EGL_FALSE) == EGL_TRUE);
    EGLenum textureFormat = static_cast<EGLenum>(attribs.get(EGL_TEXTURE_FORMAT, EGL_NO_TEXTURE));
    EGLenum textureTarget = static_cast<EGLenum>(attribs.get(EGL_TEXTURE_TARGET, EGL_NO_TEXTURE));

    return new PbufferSurfaceWGL(state, getRenderer(), width, height, textureFormat, textureTarget,
                                 largest, mPixelFormat, mDeviceContext, mWGLContext, mFunctionsWGL);
}

SurfaceImpl *DisplayWGL::createPbufferFromClientBuffer(const egl::SurfaceState &state,
                                                       const egl::Config *configuration,
                                                       EGLClientBuffer shareHandle,
                                                       const egl::AttributeMap &attribs)
{
    UNIMPLEMENTED();
    return nullptr;
}

SurfaceImpl *DisplayWGL::createPixmapSurface(const egl::SurfaceState &state,
                                             const egl::Config *configuration,
                                             NativePixmapType nativePixmap,
                                             const egl::AttributeMap &attribs)
{
    UNIMPLEMENTED();
    return nullptr;
}

egl::Error DisplayWGL::getDevice(DeviceImpl **device)
{
    UNIMPLEMENTED();
    return egl::Error(EGL_BAD_DISPLAY);
}

egl::ConfigSet DisplayWGL::generateConfigs()
{
    egl::ConfigSet configs;

    int minSwapInterval = 1;
    int maxSwapInterval = 1;
    if (mFunctionsWGL->swapIntervalEXT)
    {
        // No defined maximum swap interval in WGL_EXT_swap_control, use a reasonable number
        minSwapInterval = 0;
        maxSwapInterval = 8;
    }

    const gl::Version &maxVersion = getMaxSupportedESVersion();
    ASSERT(maxVersion >= gl::Version(2, 0));
    bool supportsES3 = maxVersion >= gl::Version(3, 0);

    PIXELFORMATDESCRIPTOR pixelFormatDescriptor;
    DescribePixelFormat(mDeviceContext, mPixelFormat, sizeof(pixelFormatDescriptor), &pixelFormatDescriptor);

    auto getAttrib = [this](int attrib)
    {
        return wgl::QueryWGLFormatAttrib(mDeviceContext, mPixelFormat, attrib, mFunctionsWGL);
    };

    const EGLint optimalSurfaceOrientation =
        mUseDXGISwapChains ? EGL_SURFACE_ORIENTATION_INVERT_Y_ANGLE : 0;

    egl::Config config;
    config.renderTargetFormat = GL_RGBA8; // TODO: use the bit counts to determine the format
    config.depthStencilFormat = GL_DEPTH24_STENCIL8; // TODO: use the bit counts to determine the format
    config.bufferSize = pixelFormatDescriptor.cColorBits;
    config.redSize = pixelFormatDescriptor.cRedBits;
    config.greenSize = pixelFormatDescriptor.cGreenBits;
    config.blueSize = pixelFormatDescriptor.cBlueBits;
    config.luminanceSize = 0;
    config.alphaSize = pixelFormatDescriptor.cAlphaBits;
    config.alphaMaskSize = 0;
    config.bindToTextureRGB   = (getAttrib(WGL_BIND_TO_TEXTURE_RGB_ARB) == TRUE);
    config.bindToTextureRGBA  = (getAttrib(WGL_BIND_TO_TEXTURE_RGBA_ARB) == TRUE);
    config.colorBufferType = EGL_RGB_BUFFER;
    config.configCaveat = EGL_NONE;
    config.conformant = EGL_OPENGL_ES2_BIT | (supportsES3 ? EGL_OPENGL_ES3_BIT_KHR : 0);
    config.depthSize = pixelFormatDescriptor.cDepthBits;
    config.level = 0;
    config.matchNativePixmap = EGL_NONE;
    config.maxPBufferWidth    = getAttrib(WGL_MAX_PBUFFER_WIDTH_ARB);
    config.maxPBufferHeight   = getAttrib(WGL_MAX_PBUFFER_HEIGHT_ARB);
    config.maxPBufferPixels   = getAttrib(WGL_MAX_PBUFFER_PIXELS_ARB);
    config.maxSwapInterval = maxSwapInterval;
    config.minSwapInterval = minSwapInterval;
    config.nativeRenderable = EGL_TRUE; // Direct rendering
    config.nativeVisualID = 0;
    config.nativeVisualType = EGL_NONE;
    config.renderableType = EGL_OPENGL_ES2_BIT | (supportsES3 ? EGL_OPENGL_ES3_BIT_KHR : 0);
    config.sampleBuffers = 0; // FIXME: enumerate multi-sampling
    config.samples = 0;
    config.stencilSize = pixelFormatDescriptor.cStencilBits;
    config.surfaceType =
        ((pixelFormatDescriptor.dwFlags & PFD_DRAW_TO_WINDOW) ? EGL_WINDOW_BIT : 0) |
        ((getAttrib(WGL_DRAW_TO_PBUFFER_ARB) == TRUE) ? EGL_PBUFFER_BIT : 0) |
        ((getAttrib(WGL_SWAP_METHOD_ARB) == WGL_SWAP_COPY_ARB) ? EGL_SWAP_BEHAVIOR_PRESERVED_BIT
                                                               : 0);
    config.optimalOrientation = optimalSurfaceOrientation;

    config.transparentType = EGL_NONE;
    config.transparentRedValue = 0;
    config.transparentGreenValue = 0;
    config.transparentBlueValue = 0;

    configs.add(config);

    return configs;
}

bool DisplayWGL::testDeviceLost()
{
    if (mHasRobustness)
    {
        return getRenderer()->getResetStatus() != GL_NO_ERROR;
    }

    return false;
}

egl::Error DisplayWGL::restoreLostDevice()
{
    return egl::Error(EGL_BAD_DISPLAY);
}

bool DisplayWGL::isValidNativeWindow(EGLNativeWindowType window) const
{
    return (IsWindow(window) == TRUE);
}

std::string DisplayWGL::getVendorString() const
{
    //UNIMPLEMENTED();
    return "";
}

const FunctionsGL *DisplayWGL::getFunctionsGL() const
{
    return mFunctionsGL;
}

egl::Error DisplayWGL::initializeD3DDevice()
{
    if (mD3D11Device != nullptr)
    {
        return egl::Error(EGL_SUCCESS);
    }

    mDxgiModule = LoadLibrary(TEXT("dxgi.dll"));
    if (!mDxgiModule)
    {
        return egl::Error(EGL_NOT_INITIALIZED, "Failed to load DXGI library.");
    }

    mD3d11Module = LoadLibrary(TEXT("d3d11.dll"));
    if (!mD3d11Module)
    {
        return egl::Error(EGL_NOT_INITIALIZED, "Failed to load d3d11 library.");
    }

    PFN_D3D11_CREATE_DEVICE d3d11CreateDevice = nullptr;
    d3d11CreateDevice = reinterpret_cast<PFN_D3D11_CREATE_DEVICE>(
        GetProcAddress(mD3d11Module, "D3D11CreateDevice"));
    if (d3d11CreateDevice == nullptr)
    {
        return egl::Error(EGL_NOT_INITIALIZED, "Could not retrieve D3D11CreateDevice address.");
    }

    HRESULT result = d3d11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, nullptr, 0,
                                       D3D11_SDK_VERSION, &mD3D11Device, nullptr, nullptr);
    if (FAILED(result))
    {
        return egl::Error(EGL_NOT_INITIALIZED, "Could not create D3D11 device, error: 0x%X",
                          result);
    }

    egl::Error error = registerD3DDevice(mD3D11Device, &mD3D11DeviceHandle);
    if (error.isError())
    {
        return error;
    }

    return egl::Error(EGL_SUCCESS);
}

void DisplayWGL::generateExtensions(egl::DisplayExtensions *outExtensions) const
{
    // Only enable the surface orientation  and post sub buffer for DXGI swap chain surfaces, they
    // prefer to swap with inverted Y.
    outExtensions->postSubBuffer      = mUseDXGISwapChains;
    outExtensions->surfaceOrientation = mUseDXGISwapChains;

    outExtensions->createContextRobustness = mHasRobustness;
}

void DisplayWGL::generateCaps(egl::Caps *outCaps) const
{
    outCaps->textureNPOT = true;
}

egl::Error DisplayWGL::waitClient() const
{
    // Unimplemented as this is not needed for WGL
    return egl::Error(EGL_SUCCESS);
}

egl::Error DisplayWGL::waitNative(EGLint engine,
                                  egl::Surface *drawSurface,
                                  egl::Surface *readSurface) const
{
    // Unimplemented as this is not needed for WGL
    return egl::Error(EGL_SUCCESS);
}

egl::Error DisplayWGL::getDriverVersion(std::string *version) const
{
    *version = "";
    return egl::Error(EGL_SUCCESS);
}

egl::Error DisplayWGL::registerD3DDevice(IUnknown *device, HANDLE *outHandle)
{
    ASSERT(device != nullptr);
    ASSERT(outHandle != nullptr);

    auto iter = mRegisteredD3DDevices.find(device);
    if (iter != mRegisteredD3DDevices.end())
    {
        iter->second.refCount++;
        *outHandle = iter->second.handle;
        return egl::Error(EGL_SUCCESS);
    }

    HANDLE handle = mFunctionsWGL->dxOpenDeviceNV(device);
    if (!handle)
    {
        return egl::Error(EGL_BAD_PARAMETER, "Failed to open D3D device.");
    }

    device->AddRef();

    D3DObjectHandle newDeviceInfo;
    newDeviceInfo.handle          = handle;
    newDeviceInfo.refCount        = 1;
    mRegisteredD3DDevices[device] = newDeviceInfo;

    *outHandle = handle;
    return egl::Error(EGL_SUCCESS);
}

void DisplayWGL::releaseD3DDevice(HANDLE deviceHandle)
{
    for (auto iter = mRegisteredD3DDevices.begin(); iter != mRegisteredD3DDevices.end(); iter++)
    {
        if (iter->second.handle == deviceHandle)
        {
            iter->second.refCount--;
            if (iter->second.refCount == 0)
            {
                mFunctionsWGL->dxCloseDeviceNV(iter->second.handle);
                iter->first->Release();
                mRegisteredD3DDevices.erase(iter);
                break;
            }
        }
    }
}
}
