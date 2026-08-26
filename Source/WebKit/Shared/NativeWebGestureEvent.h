/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
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

#if ENABLE(MAC_GESTURE_EVENTS)

#include "WebGestureEvent.h"

OBJC_CLASS NSEvent;
OBJC_CLASS NSView;

namespace WebKit {

class NativeWebGestureEvent final : public WebGestureEvent {
public:
    // Distinguishes magnify from rotate without needing a backing NSEvent.
    enum class Kind : uint8_t { Magnification, Rotation };

    struct Init {
        Kind kind;
        Phase phase;
        WebCore::FloatPoint locationInWindow;
        float gestureScale { 0 };
        float gestureRotation { 0 };
        MonotonicTime timestamp;
        bool allowsNativeZoom { true };
    };

    static std::optional<NativeWebGestureEvent> create(NSEvent *, NSView *);
    static std::optional<NativeWebGestureEvent> create(const Init&, NSView *);

    bool allowsNativeZoom() const { return m_allowsNativeZoom; }
    Kind kind() const { return m_kind; }
    NSEvent *nativeEvent() const { return m_nativeEvent.get(); }

private:
    static std::optional<NativeWebGestureEvent> create(const Init&, NSView *, NSEvent *);
    explicit NativeWebGestureEvent(WebEventType, const Init&, NSView *, NSEvent *);

    bool m_allowsNativeZoom { true };
    Kind m_kind;
    RetainPtr<NSEvent> m_nativeEvent;
};

} // namespace WebKit

#endif // ENABLE(MAC_GESTURE_EVENTS)
