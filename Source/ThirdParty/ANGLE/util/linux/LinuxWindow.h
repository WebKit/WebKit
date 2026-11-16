//
// Copyright 2026 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// LinuxWindow.h: Linux-specific OSWindow helpers shared by OSWindow::New().

#ifndef UTIL_LINUX_WINDOW_H
#define UTIL_LINUX_WINDOW_H

#include <EGL/egl.h>

#include "util/util_export.h"

// Returns the native display platform type (EGL_PLATFORM_X11_EXT or
// EGL_PLATFORM_WAYLAND_EXT) that OSWindow::New() selects on this system, or 0 if
// no supported window system is available. This is the single source of truth
// for the X11-vs-Wayland choice, so callers that need to create a matching EGL
// display (e.g. the dEQP test runner) agree with the window OSWindow::New()
// creates.
ANGLE_UTIL_EXPORT EGLenum GetNativeDisplayPlatformType();

#endif  // UTIL_LINUX_WINDOW_H
