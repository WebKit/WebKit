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
#include "WebKeyboardEvent.h"
#include <WebCore/WindowsKeyboardCodes.h>
#include <wtf/TZoneMallocInlines.h>
#include <wtf/text/TextStream.h>

namespace WebKit {

WTF_MAKE_TZONE_ALLOCATED_IMPL(WebEvent);

WebEvent::WebEvent(WebEventType type, OptionSet<WebEventModifier> modifiers, MonotonicTime timestamp, WTF::UUID authorizationToken)
    : m_type(type)
    , m_modifiers(modifiers)
    , m_timestamp(timestamp)
    , m_authorizationToken(authorizationToken)
{
}

WebEvent::WebEvent(WebEventType type, OptionSet<WebEventModifier> modifiers, MonotonicTime timestamp)
    : m_type(type)
    , m_modifiers(modifiers)
    , m_timestamp(timestamp)
    , m_authorizationToken(WTF::UUID::createVersion4())
{
}

// https://html.spec.whatwg.org/multipage/interaction.html#activation-triggering-input-event
bool WebEvent::isActivationTriggeringEvent() const
{
    switch (type()) {
    case WebEventType::MouseDown:
#if ENABLE(TOUCH_EVENTS)
    case WebEventType::TouchEnd:
#endif
        return true;
    case WebEventType::KeyDown:
        return downcast<WebKeyboardEvent>(*this).windowsVirtualKeyCode() != VK_ESCAPE;
    default:
        break;
    }
    return false;
}

TextStream& operator<<(TextStream& ts, WebEventType eventType)
{
    switch (eventType) {
    case WebEventType::MouseDown: ts << "MouseDown"_s; break;
    case WebEventType::MouseUp: ts << "MouseUp"_s; break;
    case WebEventType::MouseMove: ts << "MouseMove"_s; break;
    case WebEventType::MouseForceChanged: ts << "MouseForceChanged"_s; break;
    case WebEventType::MouseForceDown: ts << "MouseForceDown"_s; break;
    case WebEventType::MouseForceUp: ts << "MouseForceUp"_s; break;
    case WebEventType::Wheel: ts << "Wheel"_s; break;
    case WebEventType::KeyDown: ts << "KeyDown"_s; break;
    case WebEventType::KeyUp: ts << "KeyUp"_s; break;
    case WebEventType::RawKeyDown: ts << "RawKeyDown"_s; break;
    case WebEventType::Char: ts << "Char"_s; break;

#if ENABLE(TOUCH_EVENTS)
    case WebEventType::TouchStart: ts << "TouchStart"_s; break;
    case WebEventType::TouchMove: ts << "TouchMove"_s; break;
    case WebEventType::TouchEnd: ts << "TouchEnd"_s; break;
    case WebEventType::TouchCancel: ts << "TouchCancel"_s; break;
#endif

#if ENABLE(MAC_GESTURE_EVENTS)
    case WebEventType::GestureStart: ts << "GestureStart"_s; break;
    case WebEventType::GestureChange: ts << "GestureChange"_s; break;
    case WebEventType::GestureEnd: ts << "GestureEnd"_s; break;
#endif
    }

    return ts;
}

} // namespace WebKit
