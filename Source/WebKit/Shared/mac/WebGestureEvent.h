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

#ifndef WebGestureEvent_h
#define WebGestureEvent_h

#if ENABLE(MAC_GESTURE_EVENTS)

#include "WebEvent.h"
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

class WebGestureEvent : public WebEvent {
public:
    WebGestureEvent() { }
    WebGestureEvent(WebEvent::Type type, OptionSet<WebEvent::Modifier> modifiers, WallTime timestamp, WebCore::IntPoint position, float gestureScale, float gestureRotation)
        : WebEvent(type, modifiers, timestamp)
        , m_position(position)
        , m_gestureScale(gestureScale)
        , m_gestureRotation(gestureRotation)
    {
        ASSERT(isGestureEventType(type));
    }

    WebCore::IntPoint position() const { return m_position; }

    float gestureScale() const { return m_gestureScale; }
    float gestureRotation() const { return m_gestureRotation; }

    void encode(IPC::Encoder&) const;
    static bool decode(IPC::Decoder&, WebGestureEvent&);
    
private:
    bool isGestureEventType(Type) const;

    WebCore::IntPoint m_position;
    float m_gestureScale;
    float m_gestureRotation;
};

} // namespace WebKit

#endif // ENABLE(MAC_GESTURE_EVENTS)

#endif // WebGestureEvent_h
