//
// Copyright (c) 2015 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// WindowSurfaceWGL.cpp: WGL implementation of egl::Surface for windows

#include "libANGLE/renderer/gl/wgl/WindowSurfaceWGL.h"

#include "common/debug.h"
#include "libANGLE/renderer/gl/wgl/FunctionsWGL.h"
#include "libANGLE/renderer/gl/wgl/wgl_utils.h"

namespace rx
{

WindowSurfaceWGL::WindowSurfaceWGL(EGLNativeWindowType window, ATOM windowClass, int pixelFormat, HGLRC wglContext, const FunctionsWGL *functions)
    : SurfaceGL(),
      mWindowClass(windowClass),
      mPixelFormat(pixelFormat),
      mShareWGLContext(wglContext),
      mParentWindow(window),
      mChildWindow(nullptr),
      mChildDeviceContext(nullptr),
      mFunctionsWGL(functions)
{
}

WindowSurfaceWGL::~WindowSurfaceWGL()
{
    mWindowClass = 0;
    mPixelFormat = 0;
    mShareWGLContext = nullptr;

    ReleaseDC(mChildWindow, mChildDeviceContext);
    mChildDeviceContext = nullptr;

    DestroyWindow(mChildWindow);
    mChildWindow = nullptr;
}

egl::Error WindowSurfaceWGL::initialize()
{
    // Create a child window of the supplied window to draw to.
    RECT rect;
    if (!GetClientRect(mParentWindow, &rect))
    {
        return egl::Error(EGL_BAD_NATIVE_WINDOW, "Failed to get the size of the native window.");
    }

    mChildWindow = CreateWindowExA(WS_EX_NOPARENTNOTIFY,
                                   reinterpret_cast<const char*>(mWindowClass),
                                   "ANGLE Intermediate Surface Window",
                                   WS_CHILDWINDOW | WS_DISABLED | WS_VISIBLE,
                                   0,
                                   0,
                                   rect.right - rect.left,
                                   rect.bottom - rect.top,
                                   mParentWindow,
                                   NULL,
                                   NULL,
                                   NULL);
    if (!mChildWindow)
    {
        return egl::Error(EGL_BAD_NATIVE_WINDOW, "Failed to create a child window.");
    }

    mChildDeviceContext = GetDC(mChildWindow);
    if (!mChildDeviceContext)
    {
        return egl::Error(EGL_BAD_NATIVE_WINDOW, "Failed to get the device context of the child window.");
    }

    const PIXELFORMATDESCRIPTOR pixelFormatDescriptor = wgl::GetDefaultPixelFormatDescriptor();

    if (!SetPixelFormat(mChildDeviceContext, mPixelFormat, &pixelFormatDescriptor))
    {
        return egl::Error(EGL_BAD_NATIVE_WINDOW, "Failed to set the pixel format on the child window.");
    }

    return egl::Error(EGL_SUCCESS);
}

egl::Error WindowSurfaceWGL::makeCurrent()
{
    if (!mFunctionsWGL->makeCurrent(mChildDeviceContext, mShareWGLContext))
    {
        // TODO: What error type here?
        return egl::Error(EGL_CONTEXT_LOST, "Failed to make the WGL context current.");
    }

    return egl::Error(EGL_SUCCESS);
}

egl::Error WindowSurfaceWGL::swap()
{
    // Resize the child window to the interior of the parent window.
    RECT rect;
    if (!GetClientRect(mParentWindow, &rect))
    {
        // TODO: What error type here?
        return egl::Error(EGL_CONTEXT_LOST, "Failed to get the size of the native window.");
    }

    if (!MoveWindow(mChildWindow, 0, 0, rect.right - rect.left, rect.bottom - rect.top, FALSE))
    {
        // TODO: What error type here?
        return egl::Error(EGL_CONTEXT_LOST, "Failed to move the child window.");
    }

    if (!mFunctionsWGL->swapBuffers(mChildDeviceContext))
    {
        // TODO: What error type here?
        return egl::Error(EGL_CONTEXT_LOST, "Failed to swap buffers on the child window.");
    }

    return egl::Error(EGL_SUCCESS);
}

egl::Error WindowSurfaceWGL::postSubBuffer(EGLint x, EGLint y, EGLint width, EGLint height)
{
    UNIMPLEMENTED();
    return egl::Error(EGL_SUCCESS);
}

egl::Error WindowSurfaceWGL::querySurfacePointerANGLE(EGLint attribute, void **value)
{
    UNIMPLEMENTED();
    return egl::Error(EGL_SUCCESS);
}

egl::Error WindowSurfaceWGL::bindTexImage(EGLint buffer)
{
    UNIMPLEMENTED();
    return egl::Error(EGL_SUCCESS);
}

egl::Error WindowSurfaceWGL::releaseTexImage(EGLint buffer)
{
    UNIMPLEMENTED();
    return egl::Error(EGL_SUCCESS);
}

void WindowSurfaceWGL::setSwapInterval(EGLint interval)
{
    if (mFunctionsWGL->swapIntervalEXT)
    {
        mFunctionsWGL->swapIntervalEXT(interval);
    }
}

EGLint WindowSurfaceWGL::getWidth() const
{
    RECT rect;
    if (!GetClientRect(mParentWindow, &rect))
    {
        return 0;
    }
    return rect.right - rect.left;
}

EGLint WindowSurfaceWGL::getHeight() const
{
    RECT rect;
    if (!GetClientRect(mParentWindow, &rect))
    {
        return 0;
    }
    return rect.bottom - rect.top;
}

EGLint WindowSurfaceWGL::isPostSubBufferSupported() const
{
    // PostSubBuffer extension not exposed on WGL.
    UNIMPLEMENTED();
    return EGL_FALSE;
}

}
