//
// Copyright 2022 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// LinuxWindow.cpp: Implementation of OSWindow::New for Linux

#include "util/linux/LinuxWindow.h"
#include "util/OSWindow.h"

#if defined(ANGLE_USE_WAYLAND)
#    include "wayland/WaylandWindow.h"
#endif

#if defined(ANGLE_USE_X11)
#    include "x11/X11Window.h"
#endif

#if defined(ANGLE_USE_X11) || defined(ANGLE_USE_WAYLAND)
EGLenum GetNativeDisplayPlatformType()
{
#    if defined(ANGLE_USE_X11)
    // Prefer X11
    if (IsX11WindowAvailable())
    {
        return EGL_PLATFORM_X11_EXT;
    }
#    endif

#    if defined(ANGLE_USE_WAYLAND)
    if (IsWaylandWindowAvailable())
    {
        return EGL_PLATFORM_WAYLAND_EXT;
    }
#    endif

    return 0;
}

// static
OSWindow *OSWindow::New(void *nativeDisplay)
{
    switch (GetNativeDisplayPlatformType())
    {
#    if defined(ANGLE_USE_X11)
        case EGL_PLATFORM_X11_EXT:
            return CreateX11Window();
#    endif

#    if defined(ANGLE_USE_WAYLAND)
        case EGL_PLATFORM_WAYLAND_EXT:
            return CreateWaylandWindow(nativeDisplay);
#    endif

        default:
            return nullptr;
    }
}
#endif
