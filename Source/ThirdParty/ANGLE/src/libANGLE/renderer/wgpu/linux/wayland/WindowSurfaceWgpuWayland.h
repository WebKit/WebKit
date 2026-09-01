//
// Copyright 2026 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// WindowSurfaceWgpuWayland.h:
//    Defines the class interface for WindowSurfaceWgpuWayland, implementing WindowSurfaceWgpu.
//

#ifndef LIBANGLE_RENDERER_WGPU_LINUX_WAYLAND_WINDOWSURFACEWGPUWAYLAND_H_
#define LIBANGLE_RENDERER_WGPU_LINUX_WAYLAND_WINDOWSURFACEWGPUWAYLAND_H_

#include "libANGLE/renderer/wgpu/SurfaceWgpu.h"

struct wl_egl_window;

namespace rx
{
class WindowSurfaceWgpuWayland : public WindowSurfaceWgpu
{
  public:
    static void ResizeCallback(wl_egl_window *window, void *payload);

    WindowSurfaceWgpuWayland(const egl::SurfaceState &surfaceState, EGLNativeWindowType window);
    ~WindowSurfaceWgpuWayland() override;

  private:
    angle::Result createWgpuSurface(const egl::Display *display,
                                    webgpu::SurfaceHandle *outSurface) override;
    angle::Result getCurrentWindowSize(const egl::Display *display, gl::Extents *outSize) override;

    gl::Extents mExtents;
};

}  // namespace rx

#endif  // LIBANGLE_RENDERER_WGPU_LINUX_WAYLAND_WINDOWSURFACEWGPUWAYLAND_H_
