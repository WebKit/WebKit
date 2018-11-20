/*
 * Copyright (C) 2014 Igalia S.L.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "WebEvent.h"

struct wpe_input_axis_event;
struct wpe_input_keyboard_event;
struct wpe_input_pointer_event;
#if ENABLE(TOUCH_EVENTS)
struct wpe_input_touch_event;
#endif

namespace WebKit {

class WebEventFactory {
public:
    static WebKeyboardEvent createWebKeyboardEvent(struct wpe_input_keyboard_event*);
    static WebMouseEvent createWebMouseEvent(struct wpe_input_pointer_event*, float deviceScaleFactor);
    static WebWheelEvent createWebWheelEvent(struct wpe_input_axis_event*, float deviceScaleFactor);
#if ENABLE(TOUCH_EVENTS)
    static WebTouchEvent createWebTouchEvent(struct wpe_input_touch_event*, float deviceScaleFactor);
#endif
};

} // namespace WebKit
