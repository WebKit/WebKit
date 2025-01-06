/*
 * Copyright (C) 2023 Igalia S.L.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "WPEDisplayWayland.h"
#include "WPEScreen.h"
#include "WPEWaylandCursor.h"
#include "WPEWaylandSeat.h"

struct xdg_wm_base* wpeDisplayWaylandGetXDGWMBase(WPEDisplayWayland*);
WPE::WaylandSeat* wpeDisplayWaylandGetSeat(WPEDisplayWayland*);
WPE::WaylandCursor* wpeDisplayWaylandGetCursor(WPEDisplayWayland*);
WPEScreen* wpeDisplayWaylandFindScreen(WPEDisplayWayland*, struct wl_output*);
struct zwp_linux_dmabuf_v1* wpeDisplayWaylandGetLinuxDMABuf(WPEDisplayWayland*);
struct zwp_linux_explicit_synchronization_v1* wpeDisplayWaylandGetLinuxExplicitSync(WPEDisplayWayland*);
struct zwp_text_input_v1* wpeDisplayWaylandGetTextInputV1(WPEDisplayWayland*);
struct zwp_text_input_v3* wpeDisplayWaylandGetTextInputV3(WPEDisplayWayland*);
struct zwp_pointer_constraints_v1* wpeDisplayWaylandGetPointerConstraints(WPEDisplayWayland*);
struct zwp_relative_pointer_manager_v1* wpeDisplayWaylandGetRelativePointerManager(WPEDisplayWayland*);
