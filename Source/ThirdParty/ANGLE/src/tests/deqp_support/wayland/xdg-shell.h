//
// Copyright 2026 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// dEQP's Linux Wayland platform (tcuLnxWayland.hpp) includes "xdg-shell.h".
// ANGLE generates Wayland protocol headers with the newer
// "xdg-shell-client-protocol.h" naming, so forward the dEQP include to the
// generated client header.
#ifndef ANGLE_TESTS_DEQP_SUPPORT_WAYLAND_XDG_SHELL_H_
#define ANGLE_TESTS_DEQP_SUPPORT_WAYLAND_XDG_SHELL_H_

#include "xdg-shell-client-protocol.h"

#endif  // ANGLE_TESTS_DEQP_SUPPORT_WAYLAND_XDG_SHELL_H_
