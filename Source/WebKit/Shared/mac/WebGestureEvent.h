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

#include "WebEvent.h"
#include "WebEventPhase.h"
#include <WebCore/FloatPoint.h>
#include <WebCore/FloatSize.h>
#include <WebCore/IntPoint.h>
#include <WebCore/IntSize.h>
#include <wtf/text/WTFString.h>

namespace IPC {
class Decoder;
class Encoder;
}

namespace WebKit {

// Field order matches WebEvent.serialization.in.
struct WebGestureEventData {
    WebCore::IntPoint position;
    float gestureScale { 0 };
    float gestureRotation { 0 };
    WebEventPhase phase { WebEventPhase::None };
};

struct WebGestureEventInit {
    WebEventData event;
    WebGestureEventData gesture;
};

class WebGestureEvent : public WebEvent {
    WTF_MAKE_TZONE_ALLOCATED(WebGestureEvent);
public:
    using Phase = WebEventPhase;

    static Ref<WebGestureEvent> create(WebEventData&&, WebGestureEventData&&);
    static Ref<WebGestureEvent> create(WebGestureEventInit&&);

    WebCore::IntPoint position() const { return m_data.position; }

    float gestureScale() const { return m_data.gestureScale; }
    float gestureRotation() const { return m_data.gestureRotation; }
    Phase phase() const { return m_data.phase; }

    const WebGestureEventData& gestureData() const LIFETIME_BOUND { return m_data; }

protected:
    WebGestureEvent(WebEventData&&, WebGestureEventData&&);

private:
    static bool isGestureEventType(WebEventType);

    WebGestureEventData m_data;
};

} // namespace WebKit

#endif // ENABLE(MAC_GESTURE_EVENTS)
