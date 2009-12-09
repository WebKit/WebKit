/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "WebInputEventConversion.h"

#include "EventNames.h"
#include "KeyboardCodes.h"
#include "KeyboardEvent.h"
#include "MouseEvent.h"
#include "PlatformKeyboardEvent.h"
#include "PlatformMouseEvent.h"
#include "PlatformWheelEvent.h"
#include "ScrollView.h"
#include "WebInputEvent.h"
#include "Widget.h"

using namespace WebCore;

namespace WebKit {

// MakePlatformMouseEvent -----------------------------------------------------

PlatformMouseEventBuilder::PlatformMouseEventBuilder(Widget* widget, const WebMouseEvent& e)
{
    // FIXME: widget is always toplevel, unless it's a popup.  We may be able
    // to get rid of this once we abstract popups into a WebKit API.
    m_position = widget->convertFromContainingWindow(IntPoint(e.x, e.y));
    m_globalPosition = IntPoint(e.globalX, e.globalY);
    m_button = static_cast<MouseButton>(e.button);
    m_shiftKey = (e.modifiers & WebInputEvent::ShiftKey);
    m_ctrlKey = (e.modifiers & WebInputEvent::ControlKey);
    m_altKey = (e.modifiers & WebInputEvent::AltKey);
    m_metaKey = (e.modifiers & WebInputEvent::MetaKey);
    m_modifierFlags = e.modifiers;
    m_timestamp = e.timeStampSeconds;
    m_clickCount = e.clickCount;

    switch (e.type) {
    case WebInputEvent::MouseMove:
    case WebInputEvent::MouseLeave:  // synthesize a move event
        m_eventType = MouseEventMoved;
        break;

    case WebInputEvent::MouseDown:
        m_eventType = MouseEventPressed;
        break;

    case WebInputEvent::MouseUp:
        m_eventType = MouseEventReleased;
        break;

    default:
        ASSERT_NOT_REACHED();
    }
}

// PlatformWheelEventBuilder --------------------------------------------------

PlatformWheelEventBuilder::PlatformWheelEventBuilder(Widget* widget, const WebMouseWheelEvent& e)
{
    m_position = widget->convertFromContainingWindow(IntPoint(e.x, e.y));
    m_globalPosition = IntPoint(e.globalX, e.globalY);
    m_deltaX = e.deltaX;
    m_deltaY = e.deltaY;
    m_wheelTicksX = e.wheelTicksX;
    m_wheelTicksY = e.wheelTicksY;
    m_isAccepted = false;
    m_granularity = e.scrollByPage ?
        ScrollByPageWheelEvent : ScrollByPixelWheelEvent;
    m_shiftKey = (e.modifiers & WebInputEvent::ShiftKey);
    m_ctrlKey = (e.modifiers & WebInputEvent::ControlKey);
    m_altKey = (e.modifiers & WebInputEvent::AltKey);
    m_metaKey = (e.modifiers & WebInputEvent::MetaKey);
}

// MakePlatformKeyboardEvent --------------------------------------------------

static inline const PlatformKeyboardEvent::Type toPlatformKeyboardEventType(WebInputEvent::Type type)
{
    switch (type) {
    case WebInputEvent::KeyUp:
        return PlatformKeyboardEvent::KeyUp;
    case WebInputEvent::KeyDown:
        return PlatformKeyboardEvent::KeyDown;
    case WebInputEvent::RawKeyDown:
        return PlatformKeyboardEvent::RawKeyDown;
    case WebInputEvent::Char:
        return PlatformKeyboardEvent::Char;
    default:
        ASSERT_NOT_REACHED();
    }
    return PlatformKeyboardEvent::KeyDown;
}

PlatformKeyboardEventBuilder::PlatformKeyboardEventBuilder(const WebKeyboardEvent& e)
{
    m_type = toPlatformKeyboardEventType(e.type);
    m_text = String(e.text);
    m_unmodifiedText = String(e.unmodifiedText);
    m_keyIdentifier = String(e.keyIdentifier);
    m_autoRepeat = (e.modifiers & WebInputEvent::IsAutoRepeat);
    m_windowsVirtualKeyCode = e.windowsKeyCode;
    m_nativeVirtualKeyCode = e.nativeKeyCode;
    m_isKeypad = (e.modifiers & WebInputEvent::IsKeyPad);
    m_shiftKey = (e.modifiers & WebInputEvent::ShiftKey);
    m_ctrlKey = (e.modifiers & WebInputEvent::ControlKey);
    m_altKey = (e.modifiers & WebInputEvent::AltKey);
    m_metaKey = (e.modifiers & WebInputEvent::MetaKey);
    m_isSystemKey = e.isSystemKey;
}

void PlatformKeyboardEventBuilder::setKeyType(Type type)
{
    // According to the behavior of Webkit in Windows platform,
    // we need to convert KeyDown to RawKeydown and Char events
    // See WebKit/WebKit/Win/WebView.cpp
    ASSERT(m_type == KeyDown);
    ASSERT(type == RawKeyDown || type == Char);
    m_type = type;

    if (type == RawKeyDown) {
        m_text = String();
        m_unmodifiedText = String();
    } else {
        m_keyIdentifier = String();
        m_windowsVirtualKeyCode = 0;
    }
}

// Please refer to bug http://b/issue?id=961192, which talks about Webkit
// keyboard event handling changes. It also mentions the list of keys
// which don't have associated character events.
bool PlatformKeyboardEventBuilder::isCharacterKey() const
{
    switch (windowsVirtualKeyCode()) {
    case VKEY_BACK:
    case VKEY_ESCAPE:
        return false;
    }
    return true;
}

static int getWebInputModifiers(const UIEventWithKeyState& event)
{
    int modifiers = 0;
    if (event.ctrlKey())
        modifiers |= WebInputEvent::ControlKey;
    if (event.shiftKey())
        modifiers |= WebInputEvent::ShiftKey;
    if (event.altKey())
        modifiers |= WebInputEvent::AltKey;
    if (event.metaKey())
        modifiers |= WebInputEvent::MetaKey;
    return modifiers;
}

WebMouseEventBuilder::WebMouseEventBuilder(const ScrollView* view, const MouseEvent& event)
{
    if (event.type() == eventNames().mousemoveEvent)
        type = WebInputEvent::MouseMove;
    else if (event.type() == eventNames().mouseoutEvent)
        type = WebInputEvent::MouseLeave;
    else if (event.type() == eventNames().mouseoverEvent)
        type = WebInputEvent::MouseEnter;
    else if (event.type() == eventNames().mousedownEvent)
        type = WebInputEvent::MouseDown;
    else if (event.type() == eventNames().mouseupEvent)
        type = WebInputEvent::MouseUp;
    else
        return; // Skip all other mouse events.
    timeStampSeconds = event.timeStamp() * 1.0e-3;
    switch (event.button()) {
    case LeftButton:
        button = WebMouseEvent::ButtonLeft;
        break;
    case MiddleButton:
        button = WebMouseEvent::ButtonMiddle;
        break;
    case RightButton:
        button = WebMouseEvent::ButtonRight;
        break;
    }
    modifiers = getWebInputModifiers(event);
    if (event.buttonDown()) {
        switch (event.button()) {
        case LeftButton:
            modifiers |= WebInputEvent::LeftButtonDown;
            break;
        case MiddleButton:
            modifiers |= WebInputEvent::MiddleButtonDown;
            break;
        case RightButton:
            modifiers |= WebInputEvent::RightButtonDown;
            break;
        }
    }
    IntPoint p = view->contentsToWindow(IntPoint(event.pageX(), event.pageY()));
    globalX = event.screenX();
    globalY = event.screenY();
    windowX = p.x();
    windowY = p.y();
    x = event.offsetX();
    y = event.offsetY();
    clickCount = event.detail();
}

WebKeyboardEventBuilder::WebKeyboardEventBuilder(const KeyboardEvent& event)
{
    if (event.type() == eventNames().keydownEvent)
        type = KeyDown;
    else if (event.type() == eventNames().keyupEvent)
        type = WebInputEvent::KeyUp;
    else if (event.type() == eventNames().keypressEvent)
        type = WebInputEvent::Char;
    else
        return; // Skip all other keyboard events.
    modifiers = getWebInputModifiers(event);
    timeStampSeconds = event.timeStamp() * 1.0e-3;
    windowsKeyCode = event.keyCode();
    nativeKeyCode = event.keyEvent()->nativeVirtualKeyCode();
    unsigned int numChars = std::min(event.keyEvent()->text().length(),
        static_cast<unsigned int>(WebKeyboardEvent::textLengthCap));
    for (unsigned int i = 0; i < numChars; i++) {
        text[i] = event.keyEvent()->text()[i];
        unmodifiedText[i] = event.keyEvent()->unmodifiedText()[i];
    }
}

} // namespace WebKit
