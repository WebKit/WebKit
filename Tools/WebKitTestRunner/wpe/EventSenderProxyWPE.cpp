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

#include "config.h"
#include "EventSenderProxy.h"

#include "HeadlessViewBackend.h"
#include "PlatformWebView.h"
#include "TestController.h"
#include <WebCore/NotImplemented.h>
#include <wpe/wpe.h>
#include <xkbcommon/xkbcommon-keysyms.h>
#include <xkbcommon/xkbcommon.h>

namespace WTR {

// Key event location code defined in DOM Level 3.
enum KeyLocationCode {
    DOMKeyLocationStandard      = 0x00,
    DOMKeyLocationLeft          = 0x01,
    DOMKeyLocationRight         = 0x02,
    DOMKeyLocationNumpad        = 0x03
};

enum ButtonState {
    ButtonReleased = 0,
    ButtonPressed = 1
};

enum PointerAxis {
    VerticalScroll = 0,
    HorizontalScroll = 1
};

EventSenderProxy::EventSenderProxy(TestController* testController)
    : m_testController(testController)
    , m_time(0)
    , m_leftMouseButtonDown(false)
    , m_clickCount(0)
    , m_clickTime(0)
    , m_clickButton(kWKEventMouseButtonNoButton)
    , m_buttonState(ButtonReleased)
{
    m_viewBackend = m_testController->mainWebView()->platformWindow()->backend();
}

EventSenderProxy::~EventSenderProxy()
{
}

unsigned senderButtonToWPEButton(unsigned senderButton)
{
    // Tests using the EventSender have a different numbering ordering than the one
    // that the WPE port expects. Shuffle these here.
    switch (senderButton) {
    case 0:
        return 1;
    case 1:
        return 3;
    case 2:
        return 2;
    default:
        return senderButton;
    }
}

void EventSenderProxy::mouseDown(unsigned button, WKEventModifiers wkModifiers)
{
    m_clickButton = button;
    m_clickPosition = m_position;
    m_clickTime = m_time;
    m_buttonState = ButtonPressed;

    struct wpe_input_pointer_event event { wpe_input_pointer_event_type_button, static_cast<uint32_t>(m_time), static_cast<int>(m_position.x), static_cast<int>(m_position.y), senderButtonToWPEButton(button), m_buttonState};
    wpe_view_backend_dispatch_pointer_event(m_viewBackend, &event);
}

void EventSenderProxy::mouseUp(unsigned button, WKEventModifiers wkModifiers)
{
    m_buttonState = ButtonReleased;
    m_clickButton = kWKEventMouseButtonNoButton;

    struct wpe_input_pointer_event event { wpe_input_pointer_event_type_button, static_cast<uint32_t>(m_time), static_cast<int>(m_position.x), static_cast<int>(m_position.y), senderButtonToWPEButton(button), m_buttonState};
    wpe_view_backend_dispatch_pointer_event(m_viewBackend, &event);
}

void EventSenderProxy::mouseMoveTo(double x, double y)
{
    m_position.x = x;
    m_position.y = y;

    struct wpe_input_pointer_event event { wpe_input_pointer_event_type_motion, static_cast<uint32_t>(m_time), static_cast<int>(m_position.x), static_cast<int>(m_position.y), static_cast<uint32_t>(m_clickButton), m_buttonState};
    wpe_view_backend_dispatch_pointer_event(m_viewBackend, &event);
}

void EventSenderProxy::mouseScrollBy(int horizontal, int vertical)
{
    // Copy behaviour of GTK+ - just return in case of (0,0) mouse scroll
    if (!horizontal && !vertical)
        return;

    if (horizontal) {
        struct wpe_input_axis_event event = { wpe_input_axis_event_type_motion, static_cast<uint32_t>(m_time), static_cast<int>(m_position.x), static_cast<int>(m_position.y), HorizontalScroll, horizontal};
        wpe_view_backend_dispatch_axis_event(m_viewBackend, &event);
    }
    if (vertical) {
        struct wpe_input_axis_event event =  { wpe_input_axis_event_type_motion, static_cast<uint32_t>(m_time), static_cast<int>(m_position.x), static_cast<int>(m_position.y), VerticalScroll, vertical};
        wpe_view_backend_dispatch_axis_event(m_viewBackend, &event);
    }
}

void EventSenderProxy::mouseScrollByWithWheelAndMomentumPhases(int horizontal, int vertical, int, int)
{
    mouseScrollBy(horizontal, vertical);
}

void EventSenderProxy::continuousMouseScrollBy(int, int, bool)
{
}

void EventSenderProxy::leapForward(int milliseconds)
{
    m_time += milliseconds / 1000.0;
}

static uint8_t wkEventModifiersToWPE(WKEventModifiers wkModifiers)
{
    uint8_t modifiers = 0;
    if (wkModifiers & kWKEventModifiersShiftKey)
        modifiers |=  wpe_input_keyboard_modifier_shift;
    if (wkModifiers & kWKEventModifiersControlKey)
        modifiers |= wpe_input_keyboard_modifier_control;
    if (wkModifiers & kWKEventModifiersAltKey)
        modifiers |= wpe_input_keyboard_modifier_alt;
    if (wkModifiers & kWKEventModifiersMetaKey)
        modifiers |= wpe_input_keyboard_modifier_meta;

    return modifiers;
}

int getXKBKeySymForKeyRef(WKStringRef keyRef, unsigned location, uint8_t* modifiers)
{
    if (location == DOMKeyLocationNumpad) {
        if (WKStringIsEqualToUTF8CString(keyRef, "leftArrow"))
            return XKB_KEY_KP_Left;
        if (WKStringIsEqualToUTF8CString(keyRef, "rightArror"))
            return XKB_KEY_KP_Right;
        if (WKStringIsEqualToUTF8CString(keyRef, "upArrow"))
            return XKB_KEY_KP_Up;
        if (WKStringIsEqualToUTF8CString(keyRef, "downArrow"))
            return XKB_KEY_KP_Down;
        if (WKStringIsEqualToUTF8CString(keyRef, "pageUp"))
            return XKB_KEY_KP_Page_Up;
        if (WKStringIsEqualToUTF8CString(keyRef, "pageDown"))
            return XKB_KEY_KP_Page_Down;
        if (WKStringIsEqualToUTF8CString(keyRef, "home"))
            return XKB_KEY_KP_Home;
        if (WKStringIsEqualToUTF8CString(keyRef, "end"))
            return XKB_KEY_KP_End;
        if (WKStringIsEqualToUTF8CString(keyRef, "insert"))
            return XKB_KEY_KP_Insert;
        if (WKStringIsEqualToUTF8CString(keyRef, "delete"))
            return XKB_KEY_KP_Delete;

        return XKB_KEY_VoidSymbol;
    }

    if (WKStringIsEqualToUTF8CString(keyRef, "leftControl"))
        return XKB_KEY_Control_L;
    if (WKStringIsEqualToUTF8CString(keyRef, "rightControl"))
        return XKB_KEY_Control_R;
    if (WKStringIsEqualToUTF8CString(keyRef, "leftShift"))
        return XKB_KEY_Shift_L;
    if (WKStringIsEqualToUTF8CString(keyRef, "rightShift"))
        return XKB_KEY_Shift_R;
    if (WKStringIsEqualToUTF8CString(keyRef, "leftAlt"))
        return XKB_KEY_Alt_L;
    if (WKStringIsEqualToUTF8CString(keyRef, "rightAlt"))
        return XKB_KEY_Alt_R;
    if (WKStringIsEqualToUTF8CString(keyRef, "leftArrow"))
        return XKB_KEY_Left;
    if (WKStringIsEqualToUTF8CString(keyRef, "rightArrow"))
        return XKB_KEY_Right;
    if (WKStringIsEqualToUTF8CString(keyRef, "upArrow"))
        return XKB_KEY_Up;
    if (WKStringIsEqualToUTF8CString(keyRef, "downArrow"))
        return XKB_KEY_Down;
    if (WKStringIsEqualToUTF8CString(keyRef, "pageUp"))
        return XKB_KEY_Page_Up;
    if (WKStringIsEqualToUTF8CString(keyRef, "pageDown"))
        return XKB_KEY_Page_Down;
    if (WKStringIsEqualToUTF8CString(keyRef, "home"))
        return XKB_KEY_Home;
    if (WKStringIsEqualToUTF8CString(keyRef, "end"))
        return XKB_KEY_End;
    if (WKStringIsEqualToUTF8CString(keyRef, "insert"))
        return XKB_KEY_Insert;
    if (WKStringIsEqualToUTF8CString(keyRef, "delete"))
        return XKB_KEY_Delete;
    if (WKStringIsEqualToUTF8CString(keyRef, "printScreen"))
        return XKB_KEY_Print;
    if (WKStringIsEqualToUTF8CString(keyRef, "menu"))
        return XKB_KEY_Menu;
    if (WKStringIsEqualToUTF8CString(keyRef, "F1"))
        return XKB_KEY_F1;
    if (WKStringIsEqualToUTF8CString(keyRef, "F2"))
        return XKB_KEY_F2;
    if (WKStringIsEqualToUTF8CString(keyRef, "F3"))
        return XKB_KEY_F3;
    if (WKStringIsEqualToUTF8CString(keyRef, "F4"))
        return XKB_KEY_F4;
    if (WKStringIsEqualToUTF8CString(keyRef, "F5"))
        return XKB_KEY_F5;
    if (WKStringIsEqualToUTF8CString(keyRef, "F6"))
        return XKB_KEY_F6;
    if (WKStringIsEqualToUTF8CString(keyRef, "F7"))
        return XKB_KEY_F7;
    if (WKStringIsEqualToUTF8CString(keyRef, "F8"))
        return XKB_KEY_F8;
    if (WKStringIsEqualToUTF8CString(keyRef, "F9"))
        return XKB_KEY_F9;
    if (WKStringIsEqualToUTF8CString(keyRef, "F10"))
        return XKB_KEY_F10;
    if (WKStringIsEqualToUTF8CString(keyRef, "F11"))
        return XKB_KEY_F11;
    if (WKStringIsEqualToUTF8CString(keyRef, "F12"))
        return XKB_KEY_F12;

    size_t bufferSize = WKStringGetMaximumUTF8CStringSize(keyRef);
    auto buffer = std::make_unique<char[]>(bufferSize);
    WKStringGetUTF8CString(keyRef, buffer.get(), bufferSize);
    char charCode = buffer.get()[0];

    if (charCode == '\n' || charCode == '\r')
        return XKB_KEY_Return;
    if (charCode == '\t')
        return XKB_KEY_Tab;
    if (charCode == '\x8')
        return XKB_KEY_BackSpace;
    if (charCode == 0x001B)
        return XKB_KEY_Escape;

    if (WTF::isASCIIUpper(charCode))
        *modifiers |= wpe_input_keyboard_modifier_shift;

    // Not sure if this is correct.
    return charCode;
}

void EventSenderProxy::keyDown(WKStringRef keyRef, WKEventModifiers wkModifiers, unsigned location)
{
    uint8_t modifiers = wkEventModifiersToWPE(wkModifiers);
    uint32_t keySym = getXKBKeySymForKeyRef(keyRef, location, &modifiers);
    uint32_t unicode = xkb_keysym_to_utf32(keySym);
    struct wpe_input_keyboard_event event { static_cast<uint32_t>(m_time), keySym, unicode, true, modifiers};
    wpe_view_backend_dispatch_keyboard_event(m_viewBackend, &event);
    event.pressed = false;
    wpe_view_backend_dispatch_keyboard_event(m_viewBackend, &event);
}

void EventSenderProxy::addTouchPoint(int x, int y)
{
    struct wpe_input_touch_event_raw rawEvent { wpe_input_touch_event_type_down, static_cast<uint32_t>(m_time), static_cast<int>(m_touchEvents.size()), static_cast<int32_t>(x), static_cast<int32_t>(y) };
    m_touchEvents.append(rawEvent);
    m_updatedTouchEvents.add(rawEvent.id);
}

void EventSenderProxy::updateTouchPoint(int index, int x, int y)
{
    ASSERT(index >= 0 && static_cast<size_t>(index) <= m_touchEvents.size());

    auto& rawEvent = m_touchEvents[index];
    rawEvent.x = x;
    rawEvent.y = y;
    rawEvent.time = m_time;
    rawEvent.type = wpe_input_touch_event_type_motion;
    m_updatedTouchEvents.add(index);
}

void EventSenderProxy::setTouchModifier(WKEventModifiers, bool)
{
    notImplemented();
}

void EventSenderProxy::setTouchPointRadius(int, int)
{
    notImplemented();
}

Vector<struct wpe_input_touch_event_raw> EventSenderProxy::getUpdatedTouchEvents()
{
    Vector<wpe_input_touch_event_raw> events;
    for (auto id : m_updatedTouchEvents)
        events.append(m_touchEvents[id]);
    return events;
}

void EventSenderProxy::removeUpdatedTouchEvents()
{
    for (auto id : m_updatedTouchEvents)
        m_touchEvents[id].type = wpe_input_touch_event_type_null;
    m_touchEvents.removeAllMatching([] (auto current) {
        return current.type == wpe_input_touch_event_type_null;
        });
}

void EventSenderProxy::prepareAndDispatchTouchEvent(enum wpe_input_touch_event_type eventType)
{
    auto updatedEvents = getUpdatedTouchEvents();
    struct wpe_input_touch_event event = { updatedEvents.data(), updatedEvents.size(), eventType, 0, static_cast<uint32_t>(m_time) };
    wpe_view_backend_dispatch_touch_event(m_viewBackend, &event);
    if (eventType == wpe_input_touch_event_type_up)
        removeUpdatedTouchEvents();
    m_updatedTouchEvents.clear();
}

void EventSenderProxy::touchStart()
{
    prepareAndDispatchTouchEvent(wpe_input_touch_event_type_down);
}

void EventSenderProxy::touchMove()
{
    prepareAndDispatchTouchEvent(wpe_input_touch_event_type_motion);
}

void EventSenderProxy::touchEnd()
{
    prepareAndDispatchTouchEvent(wpe_input_touch_event_type_up);
}

void EventSenderProxy::touchCancel()
{
    notImplemented();
}

void EventSenderProxy::clearTouchPoints()
{
    m_touchEvents.clear();
    m_updatedTouchEvents.clear();
}

void EventSenderProxy::releaseTouchPoint(int index)
{
    ASSERT(index >= 0 && static_cast<size_t>(index) <= m_touchEvents.size());

    auto& rawEvent = m_touchEvents[index];
    rawEvent.time = m_time;
    rawEvent.type = wpe_input_touch_event_type_up;
    m_updatedTouchEvents.add(index);
}

void EventSenderProxy::cancelTouchPoint(int)
{
    notImplemented();
}

} // namespace WTR
