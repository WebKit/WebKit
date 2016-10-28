//
// Copyright (c) 2015 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// WindowSurfaceGLX.cpp: GLX implementation of egl::Surface for windows

#include "libANGLE/renderer/gl/glx/WindowSurfaceGLX.h"

#include "common/debug.h"

#include "libANGLE/renderer/gl/glx/DisplayGLX.h"
#include "libANGLE/renderer/gl/glx/FunctionsGLX.h"

namespace rx
{

static int IgnoreX11Errors(Display *, XErrorEvent *)
{
    return 0;
}

WindowSurfaceGLX::WindowSurfaceGLX(const egl::SurfaceState &state,
                                   const FunctionsGLX &glx,
                                   DisplayGLX *glxDisplay,
                                   RendererGL *renderer,
                                   Window window,
                                   Display *display,
                                   glx::Context context,
                                   glx::FBConfig fbConfig)
    : SurfaceGLX(state, renderer),
      mParent(window),
      mWindow(0),
      mDisplay(display),
      mGLX(glx),
      mGLXDisplay(glxDisplay),
      mContext(context),
      mFBConfig(fbConfig),
      mGLXWindow(0)
{
}

WindowSurfaceGLX::~WindowSurfaceGLX()
{
    if (mGLXWindow)
    {
        mGLX.destroyWindow(mGLXWindow);
    }

    if (mWindow)
    {
        // When destroying the window, it may happen that the window has already been
        // destroyed by the application (this happens in Chromium). There is no way to
        // atomically check that a window exists and to destroy it so instead we call
        // XDestroyWindow, ignoring any errors.
        auto oldErrorHandler = XSetErrorHandler(IgnoreX11Errors);
        XDestroyWindow(mDisplay, mWindow);
        XSync(mDisplay, False);
        XSetErrorHandler(oldErrorHandler);
    }

    mGLXDisplay->syncXCommands();
}

egl::Error WindowSurfaceGLX::initialize()
{
    // Check that the window's visual ID is valid, as part of the AMGLE_x11_visual
    // extension.
    {
        XWindowAttributes windowAttributes;
        XGetWindowAttributes(mDisplay, mParent, &windowAttributes);
        unsigned long visualId = windowAttributes.visual->visualid;

        if (!mGLXDisplay->isValidWindowVisualId(visualId))
        {
            return egl::Error(EGL_BAD_MATCH,
                              "The visual of native_window doesn't match the visual given with "
                              "ANGLE_X11_VISUAL_ID");
        }
    }

    // The visual of the X window, GLX window and GLX context must match,
    // however we received a user-created window that can have any visual
    // and wouldn't work with our GLX context. To work in all cases, we
    // create a child window with the right visual that covers all of its
    // parent.
    XVisualInfo *visualInfo = mGLX.getVisualFromFBConfig(mFBConfig);
    if (!visualInfo)
    {
        return egl::Error(EGL_BAD_NATIVE_WINDOW, "Failed to get the XVisualInfo for the child window.");
    }
    Visual* visual = visualInfo->visual;

    if (!getWindowDimensions(mParent, &mParentWidth, &mParentHeight))
    {
        return egl::Error(EGL_BAD_NATIVE_WINDOW, "Failed to get the parent window's dimensions.");
    }

    // The depth, colormap and visual must match otherwise we get a X error
    // so we specify the colormap attribute. Also we do not want the window
    // to be taken into account for input so we specify the event and
    // do-not-propagate masks to 0 (the defaults). Finally we specify the
    // border pixel attribute so that we can use a different visual depth
    // than our parent (seems like X uses that as a condition to render
    // the subwindow in a different buffer)
    XSetWindowAttributes attributes;
    unsigned long attributeMask = CWColormap | CWBorderPixel;

    Colormap colormap = XCreateColormap(mDisplay, mParent, visual, AllocNone);
    if(!colormap)
    {
        XFree(visualInfo);
        return egl::Error(EGL_BAD_NATIVE_WINDOW, "Failed to create the Colormap for the child window.");
    }
    attributes.colormap = colormap;
    attributes.border_pixel = 0;

    //TODO(cwallez) set up our own error handler to see if the call failed
    mWindow = XCreateWindow(mDisplay, mParent, 0, 0, mParentWidth, mParentHeight,
                            0, visualInfo->depth, InputOutput, visual, attributeMask, &attributes);
    mGLXWindow = mGLX.createWindow(mFBConfig, mWindow, nullptr);

    XMapWindow(mDisplay, mWindow);
    XFlush(mDisplay);

    XFree(visualInfo);
    XFreeColormap(mDisplay, colormap);

    mGLXDisplay->syncXCommands();

    return egl::Error(EGL_SUCCESS);
}

egl::Error WindowSurfaceGLX::makeCurrent()
{
    if (mGLX.makeCurrent(mGLXWindow, mContext) != True)
    {
        return egl::Error(EGL_BAD_DISPLAY);
    }
    return egl::Error(EGL_SUCCESS);
}

egl::Error WindowSurfaceGLX::swap()
{
    // We need to swap before resizing as some drivers clobber the back buffer
    // when the window is resized.
    mGLXDisplay->setSwapInterval(mGLXWindow, &mSwapControl);
    mGLX.swapBuffers(mGLXWindow);

    egl::Error error = checkForResize();
    if (error.isError())
    {
        return error;
    }

    return egl::Error(EGL_SUCCESS);
}

egl::Error WindowSurfaceGLX::postSubBuffer(EGLint x, EGLint y, EGLint width, EGLint height)
{
    UNIMPLEMENTED();
    return egl::Error(EGL_SUCCESS);
}

egl::Error WindowSurfaceGLX::querySurfacePointerANGLE(EGLint attribute, void **value)
{
    UNIMPLEMENTED();
    return egl::Error(EGL_SUCCESS);
}

egl::Error WindowSurfaceGLX::bindTexImage(gl::Texture *texture, EGLint buffer)
{
    UNIMPLEMENTED();
    return egl::Error(EGL_SUCCESS);
}

egl::Error WindowSurfaceGLX::releaseTexImage(EGLint buffer)
{
    UNIMPLEMENTED();
    return egl::Error(EGL_SUCCESS);
}

void WindowSurfaceGLX::setSwapInterval(EGLint interval)
{
    mSwapControl.targetSwapInterval = interval;
}

EGLint WindowSurfaceGLX::getWidth() const
{
    // The size of the window is always the same as the cached size of its parent.
    return mParentWidth;
}

EGLint WindowSurfaceGLX::getHeight() const
{
    // The size of the window is always the same as the cached size of its parent.
    return mParentHeight;
}

EGLint WindowSurfaceGLX::isPostSubBufferSupported() const
{
    UNIMPLEMENTED();
    return EGL_FALSE;
}

EGLint WindowSurfaceGLX::getSwapBehavior() const
{
    return EGL_BUFFER_PRESERVED;
}

egl::Error WindowSurfaceGLX::checkForResize()
{
    // TODO(cwallez) set up our own error handler to see if the call failed
    unsigned int newParentWidth, newParentHeight;
    if (!getWindowDimensions(mParent, &newParentWidth, &newParentHeight))
    {
        return egl::Error(EGL_BAD_CURRENT_SURFACE,
                          "Failed to retrieve the size of the parent window.");
    }

    if (mParentWidth != newParentWidth || mParentHeight != newParentHeight)
    {
        mParentWidth  = newParentWidth;
        mParentHeight = newParentHeight;

        mGLX.waitGL();
        XResizeWindow(mDisplay, mWindow, mParentWidth, mParentHeight);
        mGLX.waitX();
        XSync(mDisplay, False);
    }

    return egl::Error(EGL_SUCCESS);
}

bool WindowSurfaceGLX::getWindowDimensions(Window window, unsigned int *width, unsigned int *height) const
{
    Window root;
    int x, y;
    unsigned int border, depth;
    return XGetGeometry(mDisplay, window, &root, &x, &y, width, height, &border, &depth) != 0;
}

}
