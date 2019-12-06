//
// Copyright 2018 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// WGLWindow:
//   Implements initializing a WGL rendering context.
//

#include "util/windows/WGLWindow.h"

#include "common/string_utils.h"
#include "common/system_utils.h"
#include "util/OSWindow.h"

#include <iostream>

namespace
{
PIXELFORMATDESCRIPTOR GetDefaultPixelFormatDescriptor()
{
    PIXELFORMATDESCRIPTOR pixelFormatDescriptor = {};
    pixelFormatDescriptor.nSize                 = sizeof(pixelFormatDescriptor);
    pixelFormatDescriptor.nVersion              = 1;
    pixelFormatDescriptor.dwFlags =
        PFD_DRAW_TO_WINDOW | PFD_GENERIC_ACCELERATED | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    pixelFormatDescriptor.iPixelType   = PFD_TYPE_RGBA;
    pixelFormatDescriptor.cColorBits   = 24;
    pixelFormatDescriptor.cAlphaBits   = 8;
    pixelFormatDescriptor.cDepthBits   = 24;
    pixelFormatDescriptor.cStencilBits = 8;
    pixelFormatDescriptor.iLayerType   = PFD_MAIN_PLANE;

    return pixelFormatDescriptor;
}

PFNWGLGETPROCADDRESSPROC gCurrentWGLGetProcAddress = nullptr;
HMODULE gCurrentModule                             = nullptr;

angle::GenericProc WINAPI GetProcAddressWithFallback(const char *name)
{
    angle::GenericProc proc = reinterpret_cast<angle::GenericProc>(gCurrentWGLGetProcAddress(name));
    if (proc)
    {
        return proc;
    }

    return reinterpret_cast<angle::GenericProc>(GetProcAddress(gCurrentModule, name));
}

bool HasExtension(const std::vector<std::string> &extensions, const char *ext)
{
    return std::find(extensions.begin(), extensions.end(), ext) != extensions.end();
}

void DumpLastWindowsError()
{
    std::cerr << "Last Windows error code: 0x" << std::hex << GetLastError() << std::endl;
}
}  // namespace

WGLWindow::WGLWindow(int glesMajorVersion, int glesMinorVersion)
    : GLWindowBase(glesMajorVersion, glesMinorVersion),
      mDeviceContext(nullptr),
      mWGLContext(nullptr),
      mWindow(nullptr)
{}

WGLWindow::~WGLWindow() {}

// Internally initializes GL resources.
bool WGLWindow::initializeGL(OSWindow *osWindow,
                             angle::Library *glWindowingLibrary,
                             const EGLPlatformParameters &platformParams,
                             const ConfigParameters &configParams)
{
    glWindowingLibrary->getAs("wglGetProcAddress", &gCurrentWGLGetProcAddress);

    if (!gCurrentWGLGetProcAddress)
    {
        std::cerr << "Error loading wglGetProcAddress." << std::endl;
        return false;
    }

    gCurrentModule = reinterpret_cast<HMODULE>(glWindowingLibrary->getNative());
    angle::LoadWGL(GetProcAddressWithFallback);

    mWindow                                           = osWindow->getNativeWindow();
    mDeviceContext                                    = GetDC(mWindow);
    const PIXELFORMATDESCRIPTOR pixelFormatDescriptor = GetDefaultPixelFormatDescriptor();

    int pixelFormat = ChoosePixelFormat(mDeviceContext, &pixelFormatDescriptor);
    if (pixelFormat == 0)
    {
        std::cerr << "Could not find a compatible pixel format." << std::endl;
        DumpLastWindowsError();
        return false;
    }

    // According to the Windows docs, it is an error to set a pixel format twice.
    int currentPixelFormat = GetPixelFormat(mDeviceContext);
    if (currentPixelFormat != pixelFormat)
    {
        if (SetPixelFormat(mDeviceContext, pixelFormat, &pixelFormatDescriptor) != TRUE)
        {
            std::cerr << "Failed to set the pixel format." << std::endl;
            DumpLastWindowsError();
            return false;
        }
    }

    mWGLContext = _wglCreateContext(mDeviceContext);
    if (!mWGLContext)
    {
        std::cerr << "Failed to create a WGL context." << std::endl;
        return false;
    }

    if (!makeCurrent())
    {
        return false;
    }

    // Reload entry points to capture extensions.
    angle::LoadWGL(GetProcAddressWithFallback);

    if (!_wglGetExtensionsStringARB)
    {
        std::cerr << "Driver does not expose wglGetExtensionsStringARB." << std::endl;
        return false;
    }

    const char *extensionsString = _wglGetExtensionsStringARB(mDeviceContext);

    std::vector<std::string> extensions;
    angle::SplitStringAlongWhitespace(extensionsString, &extensions);

    if (!HasExtension(extensions, "WGL_EXT_create_context_es2_profile"))
    {
        std::cerr << "Driver does not expose WGL_EXT_create_context_es2_profile." << std::endl;
        return false;
    }

    if (configParams.webGLCompatibility.valid() || configParams.robustResourceInit.valid())
    {
        std::cerr << "WGLWindow does not support the requested feature set." << std::endl;
        return false;
    }

    // Tear down the context and create another with ES2 compatibility.
    _wglDeleteContext(mWGLContext);

    // This could be extended to cover ES1 compatiblity.
    int kCreateAttribs[] = {WGL_CONTEXT_MAJOR_VERSION_ARB,
                            mClientMajorVersion,
                            WGL_CONTEXT_MINOR_VERSION_ARB,
                            mClientMinorVersion,
                            WGL_CONTEXT_PROFILE_MASK_ARB,
                            WGL_CONTEXT_ES2_PROFILE_BIT_EXT,
                            0,
                            0};

    mWGLContext = _wglCreateContextAttribsARB(mDeviceContext, nullptr, kCreateAttribs);
    if (!mWGLContext)
    {
        std::cerr << "Failed to create an ES2 compatible WGL context." << std::endl;
        return false;
    }

    if (!makeCurrent())
    {
        return false;
    }

    mPlatform     = platformParams;
    mConfigParams = configParams;

    angle::LoadGLES(GetProcAddressWithFallback);
    return true;
}

void WGLWindow::destroyGL()
{
    if (mWGLContext)
    {
        _wglDeleteContext(mWGLContext);
        mWGLContext = nullptr;
    }

    if (mDeviceContext)
    {
        ReleaseDC(mWindow, mDeviceContext);
        mDeviceContext = nullptr;
    }
}

bool WGLWindow::isGLInitialized() const
{
    return mWGLContext != nullptr;
}

bool WGLWindow::makeCurrent()
{
    if (_wglMakeCurrent(mDeviceContext, mWGLContext) == FALSE)
    {
        std::cerr << "Error during wglMakeCurrent.\n";
        return false;
    }

    return true;
}

bool WGLWindow::setSwapInterval(EGLint swapInterval)
{
    if (!_wglSwapIntervalEXT || _wglSwapIntervalEXT(swapInterval) == FALSE)
    {
        std::cerr << "Error during wglSwapIntervalEXT.\n";
        return false;
    }
    return true;
}

void WGLWindow::swap()
{
    if (SwapBuffers(mDeviceContext) == FALSE)
    {
        std::cerr << "Error during SwapBuffers.\n";
    }
}

bool WGLWindow::hasError() const
{
    return GetLastError() != S_OK;
}

// static
WGLWindow *WGLWindow::New(int glesMajorVersion, int glesMinorVersion)
{
    return new WGLWindow(glesMajorVersion, glesMinorVersion);
}

// static
void WGLWindow::Delete(WGLWindow **window)
{
    delete *window;
    *window = nullptr;
}
