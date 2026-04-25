//
// Copyright 2024 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// WindowSurfaceVkWin32.cpp:
//    Defines the class interface for WindowSurfaceWgpuWin32, implementing WindowSurfaceWgpu.
//

#include "libANGLE/renderer/wgpu/win32/WindowSurfaceWgpuWin32.h"

#include "libANGLE/Display.h"
#include "libANGLE/renderer/wgpu/DisplayWgpu.h"
#include "libANGLE/renderer/wgpu/wgpu_utils.h"

namespace rx
{
WindowSurfaceWgpuWin32::WindowSurfaceWgpuWin32(const egl::SurfaceState &surfaceState,
                                               EGLNativeWindowType window)
    : WindowSurfaceWgpu(surfaceState, window)
{}

angle::Result WindowSurfaceWgpuWin32::createWgpuSurface(const egl::Display *display,
                                                        webgpu::SurfaceHandle *outSurface)
{
    DisplayWgpu *displayWgpu = webgpu::GetImpl(display);
    const DawnProcTable *wgpu       = displayWgpu->getProcs();
    webgpu::InstanceHandle instance = displayWgpu->getInstance();

    WGPUSurfaceSourceWindowsHWND hwndDesc = WGPU_SURFACE_SOURCE_WINDOWS_HWND_INIT;
    hwndDesc.hinstance                    = GetModuleHandle(nullptr);
    hwndDesc.hwnd                         = getNativeWindow();

    WGPUSurfaceDescriptor surfaceDesc = WGPU_SURFACE_DESCRIPTOR_INIT;
    surfaceDesc.nextInChain           = &hwndDesc.chain;

    webgpu::SurfaceHandle surface = webgpu::SurfaceHandle::Acquire(
        wgpu, wgpu->instanceCreateSurface(instance.get(), &surfaceDesc));
    *outSurface           = surface;

    return angle::Result::Continue;
}

angle::Result WindowSurfaceWgpuWin32::getCurrentWindowSize(const egl::Display *display,
                                                           gl::Extents *outSize)
{
    RECT rect;
    if (!GetClientRect(getNativeWindow(), &rect))
    {
        // TODO: generate a proper error + msg
        return angle::Result::Stop;
    }

    *outSize = gl::Extents(rect.right - rect.left, rect.bottom - rect.top, 1);
    return angle::Result::Continue;
}

WindowSurfaceWgpu *CreateWgpuWindowSurface(const egl::SurfaceState &surfaceState,
                                           EGLNativeWindowType window)
{
    return new WindowSurfaceWgpuWin32(surfaceState, window);
}
}  // namespace rx
