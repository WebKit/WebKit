//
// Copyright 2016 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// WindowSurfaceVkWin32.cpp:
//    Implements the class methods for WindowSurfaceVkWin32.
//

#include "libANGLE/renderer/vulkan/win32/WindowSurfaceVkWin32.h"

#include "libANGLE/renderer/vulkan/RendererVk.h"

namespace rx
{

WindowSurfaceVkWin32::WindowSurfaceVkWin32(const egl::SurfaceState &surfaceState,
                                           EGLNativeWindowType window,
                                           EGLint width,
                                           EGLint height)
    : WindowSurfaceVk(surfaceState, window, width, height)
{
}

vk::ErrorOrResult<gl::Extents> WindowSurfaceVkWin32::createSurfaceVk(RendererVk *renderer)
{
    VkWin32SurfaceCreateInfoKHR createInfo;

    createInfo.sType     = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
    createInfo.pNext     = nullptr;
    createInfo.flags     = 0;
    createInfo.hinstance = GetModuleHandle(nullptr);
    createInfo.hwnd      = mNativeWindowType;
    ANGLE_VK_TRY(vkCreateWin32SurfaceKHR(renderer->getInstance(), &createInfo, nullptr, &mSurface));

    RECT rect;
    ANGLE_VK_CHECK(GetClientRect(mNativeWindowType, &rect) == TRUE, VK_ERROR_INITIALIZATION_FAILED);

    return gl::Extents(rect.right - rect.left, rect.bottom - rect.top, 0);
}

}  // namespace rx
