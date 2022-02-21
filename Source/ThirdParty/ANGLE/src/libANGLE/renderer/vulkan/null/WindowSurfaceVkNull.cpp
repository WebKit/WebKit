//
// Copyright 2021 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// WindowSurfaceVkNull.cpp:
//    Implements the class methods for WindowSurfaceVkNull.
//

#include "WindowSurfaceVkNull.h"
#include "libANGLE/renderer/vulkan/RendererVk.h"

namespace rx
{

WindowSurfaceVkNull::WindowSurfaceVkNull(const egl::SurfaceState &surfaceState,
                                         EGLNativeWindowType window)
    : WindowSurfaceVk(surfaceState, window)
{}

WindowSurfaceVkNull::~WindowSurfaceVkNull() {}

angle::Result WindowSurfaceVkNull::createSurfaceVk(vk::Context *context, gl::Extents *extentsOut)
{
    return angle::Result::Stop;
}

angle::Result WindowSurfaceVkNull::getCurrentWindowSize(vk::Context *context,
                                                        gl::Extents *extentsOut)
{
    return angle::Result::Stop;
}

}  // namespace rx
