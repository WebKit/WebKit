//
// Copyright 2026 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// window_system.h: Shared X11-vs-Wayland selection policy for Ozone/Linux.

#ifndef COMMON_LINUX_WINDOW_SYSTEM_H_
#define COMMON_LINUX_WINDOW_SYSTEM_H_

namespace angle
{
// Neutral (non-EGL) identifier for a Linux native window system. Kept free of
// the Xlib "None" identifier, which is a preprocessor macro on X11 builds.
enum class WindowSystem
{
    Unspecified,
    X11,
    Wayland,
};

// Returns true when WAYLAND_DISPLAY names a compositor socket, i.e. when the
// environment points this process at a Wayland display.
bool IsWaylandDisplay();

// Returns true when the environment describes a Wayland session, which stays
// true for clients that reach it through XWayland. Deliberately broader than
// IsWaylandDisplay() and not a basis for choosing a backend; rx::IsXWayland()
// uses it to gate workarounds for X11 clients of a Wayland compositor. Not
// cached; callers that query it repeatedly memoize.
bool IsWaylandSession();

// The window system the environment points this process at: Wayland when it
// names a compositor socket, otherwise X11.
WindowSystem GetWindowSystemFromEnvironment();

// Pure selection policy: the preferred window system if it is compiled in,
// otherwise whichever one is, preferring X11; Unspecified when neither is.
//
// No I/O, so every combination is unit-testable. Callers supply the preference
// (see GetWindowSystemFromEnvironment()) and, in the util layer, probe for
// availability. libANGLE consults this only when the caller passed no
// EGL_PLATFORM_ANGLE_NATIVE_PLATFORM_TYPE_ANGLE.
WindowSystem ChoosePreferredWindowSystem(bool x11Compiled,
                                         bool waylandCompiled,
                                         WindowSystem preferred);

}  // namespace angle

#endif  // COMMON_LINUX_WINDOW_SYSTEM_H_
