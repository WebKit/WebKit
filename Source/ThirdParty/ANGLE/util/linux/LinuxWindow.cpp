//
// Copyright 2022 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// LinuxWindow.cpp: Implementation of OSWindow::New for Linux

#include "util/linux/LinuxWindow.h"

#include "common/linux/window_system.h"
#include "util/OSWindow.h"

#if defined(ANGLE_USE_WAYLAND)
#    include "wayland/WaylandWindow.h"
#endif

#if defined(ANGLE_USE_X11)
#    include "x11/X11Window.h"
#endif

#if defined(ANGLE_USE_X11) || defined(ANGLE_USE_WAYLAND)
namespace
{
// Maps a preferred window system to its EGL platform, returning 0 when that
// backend is not compiled in or its server cannot be reached.
EGLenum TryWindowSystem(angle::WindowSystem windowSystem)
{
    switch (windowSystem)
    {
#    if defined(ANGLE_USE_X11)
        case angle::WindowSystem::X11:
            return IsX11WindowAvailable() ? EGL_PLATFORM_X11_EXT : 0;
#    endif
#    if defined(ANGLE_USE_WAYLAND)
        case angle::WindowSystem::Wayland:
            return IsWaylandWindowAvailable() ? EGL_PLATFORM_WAYLAND_EXT : 0;
#    endif
        default:
            return 0;
    }
}

EGLenum ResolveNativeDisplayPlatformType()
{
#    if defined(ANGLE_USE_X11)
    constexpr bool kX11Compiled = true;
#    else
    constexpr bool kX11Compiled = false;
#    endif
#    if defined(ANGLE_USE_WAYLAND)
    constexpr bool kWaylandCompiled = true;
#    else
    constexpr bool kWaylandCompiled = false;
#    endif

    // Use the shared policy so this matches libANGLE's implicit selection in
    // GetPlatformTypeFromEnvironment(). Probe only the preferred backend and
    // return 0 if it is unreachable: that implicit path does not probe, so
    // falling back to the other backend here would pair a window with a
    // mismatched display for tests that pass no explicit platform type.
    return TryWindowSystem(angle::ChoosePreferredWindowSystem(
        kX11Compiled, kWaylandCompiled, angle::GetWindowSystemFromEnvironment()));
}
}  // namespace

EGLenum GetNativeDisplayPlatformType()
{
    // Queried repeatedly (dEQP platform setup, every OSWindow::New()) while the
    // answer cannot change during a run, so resolve once: this avoids redundant
    // server probes and guarantees every caller sees the same answer.
    static const EGLenum kResolved = ResolveNativeDisplayPlatformType();
    return kResolved;
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
