/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
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

#include "config.h"
#include "WebEvent.h"

#include "Decoder.h"
#include "Encoder.h"
#include "WebCoreArgumentCoders.h"

namespace WebKit {

WebEvent::WebEvent(WebEventType type, OptionSet<WebEventModifier> modifiers, WallTime timestamp, WTF::UUID authorizationToken)
    : m_type(type)
    , m_modifiers(modifiers)
    , m_timestamp(timestamp)
    , m_authorizationToken(authorizationToken)
{
}

WebEvent::WebEvent(WebEventType type, OptionSet<WebEventModifier> modifiers, WallTime timestamp)
    : m_type(type)
    , m_modifiers(modifiers)
    , m_timestamp(timestamp)
    , m_authorizationToken(WTF::UUID::createVersion4())
{
}

TextStream& operator<<(TextStream& ts, WebEventType eventType)
{
    switch (eventType) {
    case WebEventType::MouseDown: ts << "MouseDown"; break;
    case WebEventType::MouseUp: ts << "MouseUp"; break;
    case WebEventType::MouseMove: ts << "MouseMove"; break;
    case WebEventType::MouseForceChanged: ts << "MouseForceChanged"; break;
    case WebEventType::MouseForceDown: ts << "MouseForceDown"; break;
    case WebEventType::MouseForceUp: ts << "MouseForceUp"; break;
    case WebEventType::Wheel: ts << "Wheel"; break;
    case WebEventType::KeyDown: ts << "KeyDown"; break;
    case WebEventType::KeyUp: ts << "KeyUp"; break;
    case WebEventType::RawKeyDown: ts << "RawKeyDown"; break;
    case WebEventType::Char: ts << "Char"; break;

#if ENABLE(TOUCH_EVENTS)
    case WebEventType::TouchStart: ts << "TouchStart"; break;
    case WebEventType::TouchMove: ts << "TouchMove"; break;
    case WebEventType::TouchEnd: ts << "TouchEnd"; break;
    case WebEventType::TouchCancel: ts << "TouchCancel"; break;
#endif

#if ENABLE(MAC_GESTURE_EVENTS)
    case WebEventType::GestureStart: ts << "GestureStart"; break;
    case WebEventType::GestureChange: ts << "GestureChange"; break;
    case WebEventType::GestureEnd: ts << "GestureEnd"; break;
#endif
    }

    return ts;
}

} // namespace WebKit
