//
// Copyright 2021-2022 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// WindowSurfaceVkWayland.h:
//    Defines the class interface for WindowSurfaceVkWayland, implementing WindowSurfaceVk.
//

#ifndef LIBANGLE_RENDERER_VULKAN_WAYLAND_WINDOWSURFACEVKWAYLAND_H_
#define LIBANGLE_RENDERER_VULKAN_WAYLAND_WINDOWSURFACEVKWAYLAND_H_

#include "libANGLE/renderer/vulkan/SurfaceVk.h"

struct wl_egl_window;
struct wl_display;

namespace rx
{

class WindowSurfaceVkWayland : public WindowSurfaceVk
{
  public:
    // Requests of new sizes from client go through this callback, but actual resize will happen
    // before the next operation which would provoke a backbuffer to be pulled.
    static void ResizeCallback(wl_egl_window *window, void *payload);

    // waylandDisplay is the wl_display the EGL display was initialized with, and it owns
    // the window's wl_surface for conformant EGL usage. Passing it explicitly avoids
    // wl_proxy_get_display(), which only exists in libwayland 1.20+ and otherwise breaks
    // builds against older system wayland headers (https://anglebug.com/534371626).
    WindowSurfaceVkWayland(const egl::SurfaceState &surfaceState,
                           EGLNativeWindowType window,
                           wl_display *waylandDisplay);

  private:
    angle::Result createSurfaceVk(vk::ErrorContext *context) override;
    angle::Result getCurrentWindowSize(vk::ErrorContext *context,
                                       gl::Extents *extentsOut) const override;

    gl::Extents mExtents;
    wl_display *mWaylandDisplay;
};

}  // namespace rx

#endif  // LIBANGLE_RENDERER_VULKAN_WAYLAND_WINDOWSURFACEVKWAYLAND_H_
