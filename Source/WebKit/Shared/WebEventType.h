/*
 * Copyright (C) 2023 Apple Inc. All rights reserved.
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

namespace WebKit {

enum class WebEventType : uint32_t {
    // WebMouseEvent
    MouseDown           = 1 << 0,
    MouseUp             = 1 << 1,
    MouseMove           = 1 << 2,
    MouseForceChanged   = 1 << 3,
    MouseForceDown      = 1 << 4,
    MouseForceUp        = 1 << 5,

    // WebWheelEvent
    Wheel               = 1 << 6,

    // WebKeyboardEvent
    KeyDown             = 1 << 7,
    KeyUp               = 1 << 8,
    RawKeyDown          = 1 << 9,
    Char                = 1 << 10,

#if ENABLE(TOUCH_EVENTS)
    // WebTouchEvent
    TouchStart          = 1 << 11,
    TouchMove           = 1 << 12,
    TouchEnd            = 1 << 13,
    TouchCancel         = 1 << 14,
#endif

#if ENABLE(MAC_GESTURE_EVENTS)
    GestureStart        = 1 << 15,
    GestureChange       = 1 << 16,
    GestureEnd          = 1 << 17
#endif
};

} // namespace WebKit
