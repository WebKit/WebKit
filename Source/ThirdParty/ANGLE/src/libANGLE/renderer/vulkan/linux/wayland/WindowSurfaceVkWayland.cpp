//
// Copyright 2021-2022 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// WindowSurfaceVkWayland.cpp:
//    Implements the class methods for WindowSurfaceVkWayland.
//

#include "libANGLE/renderer/vulkan/linux/wayland/WindowSurfaceVkWayland.h"

#include "libANGLE/renderer/vulkan/vk_renderer.h"

#include <wayland-client.h>
#include <wayland-egl-backend.h>

namespace rx
{

void WindowSurfaceVkWayland::ResizeCallback(wl_egl_window *eglWindow, void *payload)
{
    WindowSurfaceVkWayland *windowSurface = reinterpret_cast<WindowSurfaceVkWayland *>(payload);

    windowSurface->mExtents.width  = eglWindow->width;
    windowSurface->mExtents.height = eglWindow->height;
}

WindowSurfaceVkWayland::WindowSurfaceVkWayland(const egl::SurfaceState &surfaceState,
                                               EGLNativeWindowType window)
    : WindowSurfaceVk(surfaceState, window)
{
    wl_egl_window *eglWindow   = reinterpret_cast<wl_egl_window *>(window);
    eglWindow->resize_callback = WindowSurfaceVkWayland::ResizeCallback;
    eglWindow->driver_private  = this;

    mExtents = gl::Extents(eglWindow->width, eglWindow->height, 1);
}

angle::Result WindowSurfaceVkWayland::createSurfaceVk(vk::ErrorContext *context)
{
    wl_egl_window *eglWindow = reinterpret_cast<wl_egl_window *>(mNativeWindowType);

    // VkWaylandSurfaceCreateInfoKHR::display and ::surface must share a
    // wl_display connection -- vkCreateSwapchainKHR calls wl_proxy_set_queue
    // which asserts proxy->display == queue->display. Derive the display
    // from the surface so this holds structurally regardless of how the EGL
    // display was initialized.
    wl_display *surfaceDisplay = static_cast<wl_display *>(
        wl_proxy_get_display(reinterpret_cast<wl_proxy *>(eglWindow->surface)));

    ANGLE_VK_CHECK(context,
                   vkGetPhysicalDeviceWaylandPresentationSupportKHR(
                       context->getRenderer()->getPhysicalDevice(),
                       context->getRenderer()->getQueueFamilyIndex(), surfaceDisplay),
                   VK_ERROR_INITIALIZATION_FAILED);

    VkWaylandSurfaceCreateInfoKHR createInfo = {};

    createInfo.sType   = VK_STRUCTURE_TYPE_WAYLAND_SURFACE_CREATE_INFO_KHR;
    createInfo.flags   = 0;
    createInfo.display = surfaceDisplay;
    createInfo.surface = eglWindow->surface;
    ANGLE_VK_TRY(context, vkCreateWaylandSurfaceKHR(context->getRenderer()->getInstance(),
                                                    &createInfo, nullptr, &mSurface));

    return angle::Result::Continue;
}

angle::Result WindowSurfaceVkWayland::getCurrentWindowSize(vk::ErrorContext *context,
                                                           gl::Extents *extentsOut) const
{
    *extentsOut = mExtents;
    return angle::Result::Continue;
}

}  // namespace rx
