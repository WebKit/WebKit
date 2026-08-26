//
// Copyright 2026 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// WindowSurfaceWgpuWayland.cpp:
//    Defines the class methods for WindowSurfaceWgpuWayland, implementing WindowSurfaceWgpu.
//

#include "libANGLE/renderer/wgpu/linux/wayland/WindowSurfaceWgpuWayland.h"

#include <wayland-client.h>
#include <wayland-egl-backend.h>

#include "libANGLE/Display.h"
#include "libANGLE/renderer/wgpu/DisplayWgpu.h"
#include "libANGLE/renderer/wgpu/wgpu_utils.h"

namespace rx
{

void WindowSurfaceWgpuWayland::ResizeCallback(wl_egl_window *eglWindow, void *payload)
{
    WindowSurfaceWgpuWayland *windowSurface = reinterpret_cast<WindowSurfaceWgpuWayland *>(payload);

    windowSurface->mExtents.width  = eglWindow->width;
    windowSurface->mExtents.height = eglWindow->height;
}

WindowSurfaceWgpuWayland::WindowSurfaceWgpuWayland(const egl::SurfaceState &surfaceState,
                                                   EGLNativeWindowType window)
    : WindowSurfaceWgpu(surfaceState, window)
{
    wl_egl_window *eglWindow   = reinterpret_cast<wl_egl_window *>(window);
    eglWindow->resize_callback = WindowSurfaceWgpuWayland::ResizeCallback;
    eglWindow->driver_private  = this;

    mExtents = gl::Extents(eglWindow->width, eglWindow->height, 1);
}

WindowSurfaceWgpuWayland::~WindowSurfaceWgpuWayland()
{
    // The wl_egl_window is owned by the application and can outlive this surface.
    // Unregister our callback so a later resize cannot dispatch ResizeCallback into
    // this destroyed object. Only clear if we are still the registered owner.
    wl_egl_window *eglWindow = reinterpret_cast<wl_egl_window *>(getNativeWindow());
    if (eglWindow != nullptr && eglWindow->driver_private == this)
    {
        eglWindow->resize_callback = nullptr;
        eglWindow->driver_private  = nullptr;
    }
}

angle::Result WindowSurfaceWgpuWayland::createWgpuSurface(const egl::Display *display,
                                                          webgpu::SurfaceHandle *outSurface)
{
    DisplayWgpu *displayWgpu        = webgpu::GetImpl(display);
    const DawnProcTable *wgpu       = displayWgpu->getProcs();
    webgpu::InstanceHandle instance = displayWgpu->getInstance();

    wl_egl_window *eglWindow = reinterpret_cast<wl_egl_window *>(getNativeWindow());

    // Dawn routes the Wayland surface through Vulkan WSI, and
    // VkWaylandSurfaceCreateInfoKHR requires that ::display and ::surface belong to the
    // same wl_display connection -- vkCreateSwapchainKHR calls wl_proxy_set_queue, which
    // asserts proxy->display == queue->display. Derive the display from the surface so
    // this holds regardless of how the EGL display connection was established.
    wl_display *surfaceDisplay = static_cast<wl_display *>(
        wl_proxy_get_display(reinterpret_cast<wl_proxy *>(eglWindow->surface)));

    WGPUSurfaceSourceWaylandSurface waylandDesc = WGPU_SURFACE_SOURCE_WAYLAND_SURFACE_INIT;
    waylandDesc.display                         = surfaceDisplay;
    waylandDesc.surface                         = eglWindow->surface;

    WGPUSurfaceDescriptor surfaceDesc = WGPU_SURFACE_DESCRIPTOR_INIT;
    surfaceDesc.nextInChain           = &waylandDesc.chain;

    webgpu::SurfaceHandle surface = webgpu::SurfaceHandle::Acquire(
        wgpu, wgpu->instanceCreateSurface(instance.get(), &surfaceDesc));
    *outSurface = surface;

    return angle::Result::Continue;
}

angle::Result WindowSurfaceWgpuWayland::getCurrentWindowSize(const egl::Display *display,
                                                             gl::Extents *outSize)
{
    *outSize = mExtents;
    return angle::Result::Continue;
}

WindowSurfaceWgpu *CreateWgpuWaylandWindowSurface(const egl::SurfaceState &surfaceState,
                                                  EGLNativeWindowType window)
{
    return new WindowSurfaceWgpuWayland(surfaceState, window);
}

}  // namespace rx
