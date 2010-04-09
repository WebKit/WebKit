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

#include "WebEventConversion.h"

#include "WebEvent.h"
#include <WebCore/IntPoint.h>

namespace WebKit {

class WebKit2PlatformMouseEvent : public WebCore::PlatformMouseEvent {
public:
    WebKit2PlatformMouseEvent(const WebMouseEvent& webEvent)
    {
        switch (webEvent.type()) {
        case WebEvent::MouseDown:
            m_eventType = WebCore::MouseEventPressed;
            break;
        case WebEvent::MouseUp:
            m_eventType = WebCore::MouseEventReleased;
            break;
        case WebEvent::MouseMove:
            m_eventType = WebCore::MouseEventMoved;
            break;
        default:
            ASSERT_NOT_REACHED();
        }

        switch (webEvent.button()) {
        case WebMouseEvent::NoButton:
            m_button = WebCore::NoButton;
            break;
        case WebMouseEvent::LeftButton:
            m_button = WebCore::LeftButton;
            break;
        case WebMouseEvent::MiddleButton:
            m_button = WebCore::MiddleButton;
            break;
        case WebMouseEvent::RightButton:
            m_button = WebCore::RightButton;
            break;
        default:
            ASSERT_NOT_REACHED();
        }

        m_position = WebCore::IntPoint(webEvent.positionX(), webEvent.positionY());
        m_globalPosition = WebCore::IntPoint(webEvent.globalPositionX(), webEvent.globalPositionY());
        m_clickCount = webEvent.clickCount();
        m_timestamp = webEvent.timestamp();
        m_shiftKey = webEvent.shiftKey();
        m_ctrlKey = webEvent.controlKey();
        m_altKey = webEvent.altKey();
        m_metaKey = webEvent.metaKey();

        m_modifierFlags = 0;
    }
};

WebCore::PlatformMouseEvent platform(const WebMouseEvent& webEvent)
{
    return WebKit2PlatformMouseEvent(webEvent);
}

class WebKit2PlatformWheelEvent : public WebCore::PlatformWheelEvent {
public:
    WebKit2PlatformWheelEvent(const WebWheelEvent& webEvent)
    {
        m_position = WebCore::IntPoint(webEvent.positionX(), webEvent.positionY());
        m_globalPosition = WebCore::IntPoint(webEvent.globalPositionX(), webEvent.globalPositionY());
        m_deltaX = webEvent.deltaX();
        m_deltaY = webEvent.deltaY();
        m_wheelTicksX = webEvent.wheelTicksX();
        m_wheelTicksY = webEvent.wheelTicksY();
        m_granularity = (webEvent.granularity() == WebWheelEvent::ScrollByPageWheelEvent) ? WebCore::ScrollByPageWheelEvent : WebCore::ScrollByPixelWheelEvent;
        m_shiftKey = webEvent.shiftKey();
        m_ctrlKey = webEvent.controlKey();
        m_altKey = webEvent.altKey();
        m_metaKey = webEvent.metaKey();
    }
};

WebCore::PlatformWheelEvent platform(const WebWheelEvent& webEvent)
{
    return WebKit2PlatformWheelEvent(webEvent);
}

class WebKit2PlatformKeyboardEvent : public WebCore::PlatformKeyboardEvent {
public:
    WebKit2PlatformKeyboardEvent(const WebKeyboardEvent& webEvent)
    {
        switch (webEvent.type()) {
        case WebEvent::KeyDown:
            m_type = PlatformKeyboardEvent::KeyDown;
            break;
        case WebEvent::KeyUp:
            m_type = PlatformKeyboardEvent::KeyUp;
            break;
        case WebEvent::RawKeyDown:
            m_type = PlatformKeyboardEvent::RawKeyDown;
            break;
        case WebEvent::Char:
            m_type = PlatformKeyboardEvent::Char;
            break;
        default:
            ASSERT_NOT_REACHED();
        }
        m_text = webEvent.text();
        m_unmodifiedText = webEvent.unmodifiedText();
        m_keyIdentifier = webEvent.keyIdentifier();
        m_windowsVirtualKeyCode = webEvent.windowsVirtualKeyCode();
        m_nativeVirtualKeyCode = webEvent.nativeVirtualKeyCode();
        m_autoRepeat = webEvent.isAutoRepeat();
        m_isKeypad = webEvent.isKeypad();
        m_shiftKey = webEvent.shiftKey();
        m_ctrlKey = webEvent.controlKey();
        m_altKey = webEvent.altKey();
        m_metaKey = webEvent.metaKey();
        
#if PLATFORM(WIN)
        // FIXME: We should make m_isSystemKey available (and false) on all platforms
        // to avoid this #define. 
        m_isSystemKey = webEvent.isSystemKey();
#endif
    }
};

WebCore::PlatformKeyboardEvent platform(const WebKeyboardEvent& webEvent)
{
    return WebKit2PlatformKeyboardEvent(webEvent);
}

} // namespace WebKit
