//
// Copyright 2024 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// WindowSurfaceWgpuX11.cpp:
//    Defines the class interface for WindowSurfaceWgpuX11, implementing WindowSurfaceWgpu.
//

#include "libANGLE/renderer/wgpu/linux/x11/WindowSurfaceWgpuX11.h"

#include "libANGLE/Display.h"
#include "libANGLE/renderer/wgpu/DisplayWgpu.h"
#include "libANGLE/renderer/wgpu/wgpu_utils.h"

#include <X11/Xlib.h>
#include <X11/Xresource.h>
#include <X11/Xutil.h>

namespace rx
{

WindowSurfaceWgpuX11::WindowSurfaceWgpuX11(const egl::SurfaceState &surfaceState,
                                           EGLNativeWindowType window)
    : WindowSurfaceWgpu(surfaceState, window)
{}

angle::Result WindowSurfaceWgpuX11::createWgpuSurface(const egl::Display *display,
                                                      webgpu::SurfaceHandle *outSurface)
{
    DisplayWgpu *displayWgpu = webgpu::GetImpl(display);
    const DawnProcTable *wgpu = displayWgpu->getProcs();

    EGLNativeWindowType window = getNativeWindow();

    webgpu::InstanceHandle instance = displayWgpu->getInstance();

    WGPUSurfaceSourceXlibWindow x11Desc = WGPU_SURFACE_SOURCE_XLIB_WINDOW_INIT;
    x11Desc.display = reinterpret_cast<Display *>(display->getNativeDisplayId());
    x11Desc.window  = window;

    WGPUSurfaceDescriptor surfaceDesc = WGPU_SURFACE_DESCRIPTOR_INIT;
    surfaceDesc.nextInChain           = &x11Desc.chain;

    webgpu::SurfaceHandle surface = webgpu::SurfaceHandle::Acquire(
        wgpu, wgpu->instanceCreateSurface(instance.get(), &surfaceDesc));
    *outSurface           = surface;

    return angle::Result::Continue;
}

angle::Result WindowSurfaceWgpuX11::getCurrentWindowSize(const egl::Display *display,
                                                         gl::Extents *outSize)
{
    Window root;
    int x, y;
    unsigned int width, height, border, depth;
    if (XGetGeometry(reinterpret_cast<Display *>(display->getNativeDisplayId()), getNativeWindow(),
                     &root, &x, &y, &width, &height, &border, &depth) == 0)
    {
        ERR() << "Failed to get X11 window geometry.";
        return angle::Result::Stop;
    }

    *outSize = gl::Extents(width, height, 1);
    return angle::Result::Continue;
}

WindowSurfaceWgpu *CreateWgpuWindowSurface(const egl::SurfaceState &surfaceState,
                                           EGLNativeWindowType window)
{
    return new WindowSurfaceWgpuX11(surfaceState, window);
}
}  // namespace rx
