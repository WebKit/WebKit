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
#include "libANGLE/renderer/gl/renderergl_utils.h"
#include "libANGLE/renderer/gl/wgl/FunctionsWGL.h"
#include "libANGLE/renderer/gl/wgl/PbufferSurfaceWGL.h"
#include "libANGLE/renderer/gl/wgl/WindowSurfaceWGL.h"
#include "libANGLE/renderer/gl/wgl/wgl_utils.h"

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

    virtual ~FunctionsGLWindows()
    {
    }

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
      mWindowClass(0),
      mWindow(nullptr),
      mDeviceContext(nullptr),
      mPixelFormat(0),
      mWGLContext(nullptr),
      mDisplay(nullptr)
{
}

DisplayWGL::~DisplayWGL()
{
}

static LRESULT CALLBACK IntermediateWindowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
      case WM_ERASEBKGND:
        // Prevent windows from erasing the background.
        return 1;
      case WM_PAINT:
        // Do not paint anything.
        PAINTSTRUCT paint;
        if (BeginPaint(window, &paint))
        {
            EndPaint(window, &paint);
        }
        return 0;
    }

    return DefWindowProc(window, message, wParam, lParam);
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

    WNDCLASSA intermediateClassDesc = { 0 };
    intermediateClassDesc.style = CS_OWNDC;
    intermediateClassDesc.lpfnWndProc = IntermediateWindowProc;
    intermediateClassDesc.cbClsExtra = 0;
    intermediateClassDesc.cbWndExtra = 0;
    intermediateClassDesc.hInstance = GetModuleHandle(NULL);
    intermediateClassDesc.hIcon = NULL;
    intermediateClassDesc.hCursor = LoadCursorA(NULL, idcArrow);
    intermediateClassDesc.hbrBackground = 0;
    intermediateClassDesc.lpszMenuName = NULL;
    intermediateClassDesc.lpszClassName = "ANGLE Intermediate Window";
    mWindowClass = RegisterClassA(&intermediateClassDesc);
    if (!mWindowClass)
    {
        return egl::Error(EGL_NOT_INITIALIZED, "Failed to register intermediate OpenGL window class.");
    }

    HWND dummyWindow = CreateWindowExA(WS_EX_NOPARENTNOTIFY,
                                       reinterpret_cast<const char *>(mWindowClass),
                                       "ANGLE Dummy Window",
                                       WS_OVERLAPPEDWINDOW,
                                       CW_USEDEFAULT,
                                       CW_USEDEFAULT,
                                       CW_USEDEFAULT,
                                       CW_USEDEFAULT,
                                       NULL,
                                       NULL,
                                       NULL,
                                       NULL);
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

    // Destroy the dummy window and context
    mFunctionsWGL->makeCurrent(dummyDeviceContext, NULL);
    mFunctionsWGL->deleteContext(dummyWGLContext);
    ReleaseDC(dummyWindow, dummyDeviceContext);
    DestroyWindow(dummyWindow);

    // Create the real intermediate context and windows
    HDC parentHDC = display->getNativeDisplayId();
    HWND parentWindow = WindowFromDC(parentHDC);

    mWindow = CreateWindowExA(WS_EX_NOPARENTNOTIFY,
                              reinterpret_cast<const char *>(mWindowClass),
                              "ANGLE Intermediate Window",
                              WS_OVERLAPPEDWINDOW,
                              CW_USEDEFAULT,
                              CW_USEDEFAULT,
                              CW_USEDEFAULT,
                              CW_USEDEFAULT,
                              parentWindow,
                              NULL,
                              NULL,
                              NULL);
    if (!mWindow)
    {
        return egl::Error(EGL_NOT_INITIALIZED, "Failed to create intermediate OpenGL window.");
    }

    mDeviceContext = GetDC(mWindow);
    if (!mDeviceContext)
    {
        return egl::Error(EGL_NOT_INITIALIZED, "Failed to get the device context of the intermediate OpenGL window.");
    }

    mPixelFormat = ChoosePixelFormat(mDeviceContext, &pixelFormatDescriptor);
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
        // Request core profile, TODO: Don't request core if requested GL version is less than 3.0
        mask |= WGL_CONTEXT_CORE_PROFILE_BIT_ARB;

        std::vector<int> contextCreationAttibutes;

        // TODO: create a context version based on the requested version and validate the version numbers
        contextCreationAttibutes.push_back(WGL_CONTEXT_MAJOR_VERSION_ARB);
        contextCreationAttibutes.push_back(3);

        contextCreationAttibutes.push_back(WGL_CONTEXT_MINOR_VERSION_ARB);
        contextCreationAttibutes.push_back(1);

        // Set the flag attributes
        if (flags != 0)
        {
            contextCreationAttibutes.push_back(WGL_CONTEXT_FLAGS_ARB);
            contextCreationAttibutes.push_back(flags);
        }

        // Set the mask attribute
        if (mask != 0)
        {
            contextCreationAttibutes.push_back(WGL_CONTEXT_PROFILE_MASK_ARB);
            contextCreationAttibutes.push_back(mask);
        }

        // Signal the end of the attributes
        contextCreationAttibutes.push_back(0);
        contextCreationAttibutes.push_back(0);

        mWGLContext = mFunctionsWGL->createContextAttribsARB(mDeviceContext, NULL, &contextCreationAttibutes[0]);
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

    return DisplayGL::initialize(display);
}

void DisplayWGL::terminate()
{
    DisplayGL::terminate();

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
}

egl::Error DisplayWGL::createWindowSurface(const egl::Config *configuration, EGLNativeWindowType window,
                                           const egl::AttributeMap &attribs, SurfaceImpl **outSurface)
{
    WindowSurfaceWGL *surface = new WindowSurfaceWGL(window, mWindowClass, mPixelFormat, mWGLContext, mFunctionsWGL);
    egl::Error error = surface->initialize();
    if (error.isError())
    {
        SafeDelete(surface);
        return error;
    }

    *outSurface = surface;
    return egl::Error(EGL_SUCCESS);
}

egl::Error DisplayWGL::createPbufferSurface(const egl::Config *configuration, const egl::AttributeMap &attribs,
                                            SurfaceImpl **outSurface)
{
    EGLint width = attribs.get(EGL_WIDTH, 0);
    EGLint height = attribs.get(EGL_HEIGHT, 0);
    bool largest = (attribs.get(EGL_LARGEST_PBUFFER, EGL_FALSE) == EGL_TRUE);
    EGLenum textureFormat = attribs.get(EGL_TEXTURE_FORMAT, EGL_NO_TEXTURE);
    EGLenum textureTarget = attribs.get(EGL_TEXTURE_TARGET, EGL_NO_TEXTURE);

    PbufferSurfaceWGL *surface = new PbufferSurfaceWGL(width, height, textureFormat, textureTarget, largest,
                                                       mPixelFormat, mDeviceContext, mWGLContext, mFunctionsWGL);
    egl::Error error = surface->initialize();
    if (error.isError())
    {
        SafeDelete(surface);
        return error;
    }

    *outSurface = surface;
    return egl::Error(EGL_SUCCESS);
}

egl::Error DisplayWGL::createPbufferFromClientBuffer(const egl::Config *configuration, EGLClientBuffer shareHandle,
                                                     const egl::AttributeMap &attribs, SurfaceImpl **outSurface)
{
    UNIMPLEMENTED();
    return egl::Error(EGL_BAD_DISPLAY);
}

egl::Error DisplayWGL::createPixmapSurface(const egl::Config *configuration, NativePixmapType nativePixmap,
                                           const egl::AttributeMap &attribs, SurfaceImpl **outSurface)
{
    UNIMPLEMENTED();
    return egl::Error(EGL_BAD_DISPLAY);
}

static int QueryWGLFormatAttrib(HDC dc, int format, int attribName, const FunctionsWGL *functions)
{
    int result = 0;
    if (functions->getPixelFormatAttribivARB == nullptr ||
        !functions->getPixelFormatAttribivARB(dc, format, 0, 1, &attribName, &result))
    {
        return 0;
    }
    return result;
}

egl::Error DisplayWGL::getDevice(DeviceImpl **device)
{
    UNIMPLEMENTED();
    return egl::Error(EGL_BAD_DISPLAY);
}

egl::ConfigSet DisplayWGL::generateConfigs() const
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

    PIXELFORMATDESCRIPTOR pixelFormatDescriptor;
    DescribePixelFormat(mDeviceContext, mPixelFormat, sizeof(pixelFormatDescriptor), &pixelFormatDescriptor);

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
    config.bindToTextureRGB = (QueryWGLFormatAttrib(mDeviceContext, mPixelFormat, WGL_BIND_TO_TEXTURE_RGB_ARB, mFunctionsWGL) == TRUE);
    config.bindToTextureRGBA = (QueryWGLFormatAttrib(mDeviceContext, mPixelFormat, WGL_BIND_TO_TEXTURE_RGBA_ARB, mFunctionsWGL) == TRUE);
    config.colorBufferType = EGL_RGB_BUFFER;
    config.configCaveat = EGL_NONE;
    config.configID = mPixelFormat;
    config.conformant = EGL_OPENGL_ES2_BIT | EGL_OPENGL_ES3_BIT_KHR; // TODO: determine the GL version and what ES versions it supports
    config.depthSize = pixelFormatDescriptor.cDepthBits;
    config.level = 0;
    config.matchNativePixmap = EGL_NONE;
    config.maxPBufferWidth = QueryWGLFormatAttrib(mDeviceContext, mPixelFormat, WGL_MAX_PBUFFER_WIDTH_ARB, mFunctionsWGL);
    config.maxPBufferHeight = QueryWGLFormatAttrib(mDeviceContext, mPixelFormat, WGL_MAX_PBUFFER_HEIGHT_ARB, mFunctionsWGL);
    config.maxPBufferPixels = QueryWGLFormatAttrib(mDeviceContext, mPixelFormat, WGL_MAX_PBUFFER_PIXELS_ARB, mFunctionsWGL);
    config.maxSwapInterval = maxSwapInterval;
    config.minSwapInterval = minSwapInterval;
    config.nativeRenderable = EGL_TRUE; // Direct rendering
    config.nativeVisualID = 0;
    config.nativeVisualType = EGL_NONE;
    config.renderableType = EGL_OPENGL_ES2_BIT | EGL_OPENGL_ES3_BIT_KHR; // TODO
    config.sampleBuffers = 0; // FIXME: enumerate multi-sampling
    config.samples = 0;
    config.stencilSize = pixelFormatDescriptor.cStencilBits;
    config.surfaceType = ((pixelFormatDescriptor.dwFlags & PFD_DRAW_TO_WINDOW) ? EGL_WINDOW_BIT : 0) |
                         ((QueryWGLFormatAttrib(mDeviceContext, mPixelFormat, WGL_DRAW_TO_PBUFFER_ARB, mFunctionsWGL) == TRUE) ? EGL_PBUFFER_BIT : 0) |
                         EGL_SWAP_BEHAVIOR_PRESERVED_BIT;
    config.transparentType = EGL_NONE;
    config.transparentRedValue = 0;
    config.transparentGreenValue = 0;
    config.transparentBlueValue = 0;

    configs.add(config);

    return configs;
}

bool DisplayWGL::isDeviceLost() const
{
    //UNIMPLEMENTED();
    return false;
}

bool DisplayWGL::testDeviceLost()
{
    //UNIMPLEMENTED();
    return false;
}

egl::Error DisplayWGL::restoreLostDevice()
{
    UNIMPLEMENTED();
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

void DisplayWGL::generateExtensions(egl::DisplayExtensions *outExtensions) const
{
    //UNIMPLEMENTED();
}

void DisplayWGL::generateCaps(egl::Caps *outCaps) const
{
    outCaps->textureNPOT = true;
}

}
