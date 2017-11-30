//
// Copyright 2016 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// DisplayVkWin32.cpp:
//    Implements the class methods for DisplayVkWin32.
//

#include "libANGLE/renderer/vulkan/win32/DisplayVkWin32.h"

#include <vulkan/vulkan.h>

#include "libANGLE/renderer/vulkan/win32/WindowSurfaceVkWin32.h"

namespace rx
{

DisplayVkWin32::DisplayVkWin32(const egl::DisplayState &state) : DisplayVk(state)
{
}

bool DisplayVkWin32::isValidNativeWindow(EGLNativeWindowType window) const
{
    return (IsWindow(window) == TRUE);
}

SurfaceImpl *DisplayVkWin32::createWindowSurfaceVk(const egl::SurfaceState &state,
                                                   EGLNativeWindowType window,
                                                   EGLint width,
                                                   EGLint height)
{
    return new WindowSurfaceVkWin32(state, window, width, height);
}

const char *DisplayVkWin32::getWSIName() const
{
    return VK_KHR_WIN32_SURFACE_EXTENSION_NAME;
}

}  // namespace rx
