//
// Copyright 2021 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// WindowSurfaceVkNull.h:
//    Defines the class interface for WindowSurfaceVkNull, implementing WindowSurfaceVk.
//

#ifndef LIBANGLE_RENDERER_VULKAN_NULL_WINDOWSURFACEVKNULL_H_
#define LIBANGLE_RENDERER_VULKAN_NULL_WINDOWSURFACEVKNULL_H_

#include "libANGLE/renderer/vulkan/SurfaceVk.h"

namespace rx
{

class WindowSurfaceVkNull final : public WindowSurfaceVk
{
  public:
    WindowSurfaceVkNull(const egl::SurfaceState &surfaceState, EGLNativeWindowType window);
    ~WindowSurfaceVkNull() final;

  private:
    angle::Result createSurfaceVk(vk::Context *context, gl::Extents *extentsOut) override;
    angle::Result getCurrentWindowSize(vk::Context *context, gl::Extents *extentsOut) override;
};

}  // namespace rx

#endif  // LIBANGLE_RENDERER_VULKAN_NULL_WINDOWSURFACEVKNULL_H_
