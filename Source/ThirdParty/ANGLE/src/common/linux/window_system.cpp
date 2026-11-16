//
// Copyright 2026 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// window_system.cpp: Shared X11-vs-Wayland selection policy for Ozone/Linux.

#include "common/linux/window_system.h"

#include "common/system_utils.h"

namespace angle
{
bool IsWaylandDisplay()
{
    return !GetEnvironmentVar("WAYLAND_DISPLAY").empty();
}

bool IsWaylandSession()
{
    // Any one of these is enough, as none is always present: XDG_SESSION_TYPE is
    // only set by pam_systemd so it is absent under su and cron, and
    // DESKTOP_SESSION is a display-manager convention whose value varies by
    // distribution. Both describe the login session and stay true under
    // XWayland, which is why they inform driver workarounds but not selection.
    return IsWaylandDisplay() || GetEnvironmentVar("XDG_SESSION_TYPE") == "wayland" ||
           GetEnvironmentVar("DESKTOP_SESSION").find("wayland") != std::string::npos;
}

WindowSystem GetWindowSystemFromEnvironment()
{
    // A named compositor socket means connect to it directly rather than going
    // through XWayland.
    return IsWaylandDisplay() ? WindowSystem::Wayland : WindowSystem::X11;
}

WindowSystem ChoosePreferredWindowSystem(bool x11Compiled,
                                         bool waylandCompiled,
                                         WindowSystem preferred)
{
    if (preferred == WindowSystem::X11 && x11Compiled)
    {
        return WindowSystem::X11;
    }
    if (preferred == WindowSystem::Wayland && waylandCompiled)
    {
        return WindowSystem::Wayland;
    }

    // The preference is not compiled in, so take whichever backend is.
    if (x11Compiled)
    {
        return WindowSystem::X11;
    }
    if (waylandCompiled)
    {
        return WindowSystem::Wayland;
    }
    return WindowSystem::Unspecified;
}
}  // namespace angle
