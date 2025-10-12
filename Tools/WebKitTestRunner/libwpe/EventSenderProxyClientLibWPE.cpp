/*
 * Copyright (C) 2023 Igalia S.L.
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
#include "EventSenderProxyClientLibWPE.h"

#include "PlatformWebView.h"
#include "PlatformWebViewClientLibWPE.h"
#include "TestController.h"
#include <WPEToolingBackends/HeadlessViewBackend.h>
#include <WebCore/NotImplemented.h>
#include <wpe/wpe.h>
#include <wtf/MonotonicTime.h>
#include <wtf/UniqueArray.h>

namespace WTR {

// Key event location code defined in DOM Level 3.
enum KeyLocationCode {
    DOMKeyLocationStandard = 0x00,
    DOMKeyLocationLeft = 0x01,
    DOMKeyLocationRight = 0x02,
    DOMKeyLocationNumpad = 0x03
};

enum ButtonState {
    ButtonReleased = 0,
    ButtonPressed = 1
};

enum PointerAxis {
    VerticalScroll = 0,
    HorizontalScroll = 1
};

EventSenderProxyClientLibWPE::EventSenderProxyClientLibWPE(TestController& controller)
    : EventSenderProxyClient(controller)
    , m_buttonState(ButtonReleased)
{
}

EventSenderProxyClientLibWPE::~EventSenderProxyClientLibWPE() = default;

struct wpe_view_backend* viewBackend(TestController& controller)
{
    return static_cast<PlatformWebViewClientLibWPE*>(controller.mainWebView()->platformWindow())->backend()->backend();
}

static uint32_t secToMsTimestamp(double currentEventTime)
{
    return static_cast<uint32_t>(currentEventTime * 1000);
}

static unsigned senderButtonToWPEButton(unsigned senderButton)
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

static uint32_t modifierForButton(unsigned button)
{
    switch (button) {
    case 1:
        return wpe_input_pointer_modifier_button1;
    case 2:
        return wpe_input_pointer_modifier_button2;
    case 3:
        return wpe_input_pointer_modifier_button3;
    case 4:
        return wpe_input_pointer_modifier_button4;
    case 5:
        return wpe_input_pointer_modifier_button5;
    default:
        return 0;
    }

    RELEASE_ASSERT_NOT_REACHED();
}

static uint32_t wkEventModifiersToWPE(WKEventModifiers wkModifiers)
{
    uint32_t modifiers = 0;
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

void EventSenderProxyClientLibWPE::mouseDown(unsigned button, double time, WKEventModifiers wkModifiers, double x, double y, int /*clickCount*/, unsigned& mouseButtonsCurrentlyDown)
{
    m_buttonState = ButtonPressed;
    auto wpeButton = senderButtonToWPEButton(button);
    mouseButtonsCurrentlyDown |= modifierForButton(wpeButton);
    uint32_t modifiers = wkEventModifiersToWPE(wkModifiers);

    struct wpe_input_pointer_event event { wpe_input_pointer_event_type_button, secToMsTimestamp(time), static_cast<int>(x), static_cast<int>(y), wpeButton, m_buttonState, mouseButtonsCurrentlyDown | modifiers };
    wpe_view_backend_dispatch_pointer_event(viewBackend(m_testController), &event);
}

void EventSenderProxyClientLibWPE::mouseUp(unsigned button, double time, WKEventModifiers wkModifiers, double x, double y, unsigned& mouseButtonsCurrentlyDown)
{
    m_buttonState = ButtonReleased;
    auto wpeButton = senderButtonToWPEButton(button);
    mouseButtonsCurrentlyDown &= ~modifierForButton(wpeButton);
    uint32_t modifiers = wkEventModifiersToWPE(wkModifiers);

    struct wpe_input_pointer_event event { wpe_input_pointer_event_type_button, secToMsTimestamp(time), static_cast<int>(x), static_cast<int>(y), wpeButton, m_buttonState, mouseButtonsCurrentlyDown | modifiers };
    wpe_view_backend_dispatch_pointer_event(viewBackend(m_testController), &event);
}

void EventSenderProxyClientLibWPE::mouseMoveTo(double x, double y, double time, WKEventMouseButton clickButton, unsigned mouseButtonsCurrentlyDown)
{
    struct wpe_input_pointer_event event { wpe_input_pointer_event_type_motion, secToMsTimestamp(time), static_cast<int>(x), static_cast<int>(y), static_cast<uint32_t>(clickButton), m_buttonState, mouseButtonsCurrentlyDown };
    wpe_view_backend_dispatch_pointer_event(viewBackend(m_testController), &event);
}

void EventSenderProxyClientLibWPE::mouseScrollBy(int horizontal, int vertical, double time, double x, double y)
{
    if (horizontal) {
        struct wpe_input_axis_event event = { wpe_input_axis_event_type_motion, secToMsTimestamp(time), static_cast<int>(x), static_cast<int>(y), HorizontalScroll, horizontal, 0 };
        wpe_view_backend_dispatch_axis_event(viewBackend(m_testController), &event);
    }
    if (vertical) {
        struct wpe_input_axis_event event = { wpe_input_axis_event_type_motion, secToMsTimestamp(time), static_cast<int>(x), static_cast<int>(y), VerticalScroll, vertical, 0 };
        wpe_view_backend_dispatch_axis_event(viewBackend(m_testController), &event);
    }
}

static uint32_t wpeKeySymForKeyRef(WKStringRef keyRef, unsigned location, uint32_t* modifiers)
{
    if (location == DOMKeyLocationNumpad) {
        if (WKStringIsEqualToUTF8CString(keyRef, "leftArrow"))
            return WPE_KEY_KP_Left;
        if (WKStringIsEqualToUTF8CString(keyRef, "rightArrow"))
            return WPE_KEY_KP_Right;
        if (WKStringIsEqualToUTF8CString(keyRef, "upArrow"))
            return WPE_KEY_KP_Up;
        if (WKStringIsEqualToUTF8CString(keyRef, "downArrow"))
            return WPE_KEY_KP_Down;
        if (WKStringIsEqualToUTF8CString(keyRef, "pageUp"))
            return WPE_KEY_KP_Page_Up;
        if (WKStringIsEqualToUTF8CString(keyRef, "pageDown"))
            return WPE_KEY_KP_Page_Down;
        if (WKStringIsEqualToUTF8CString(keyRef, "home"))
            return WPE_KEY_KP_Home;
        if (WKStringIsEqualToUTF8CString(keyRef, "end"))
            return WPE_KEY_KP_End;
        if (WKStringIsEqualToUTF8CString(keyRef, "insert"))
            return WPE_KEY_KP_Insert;
        if (WKStringIsEqualToUTF8CString(keyRef, "delete"))
            return WPE_KEY_KP_Delete;

        return WPE_KEY_VoidSymbol;
    }

    if (WKStringIsEqualToUTF8CString(keyRef, "leftControl"))
        return WPE_KEY_Control_L;
    if (WKStringIsEqualToUTF8CString(keyRef, "rightControl"))
        return WPE_KEY_Control_R;
    if (WKStringIsEqualToUTF8CString(keyRef, "leftShift"))
        return WPE_KEY_Shift_L;
    if (WKStringIsEqualToUTF8CString(keyRef, "rightShift"))
        return WPE_KEY_Shift_R;
    if (WKStringIsEqualToUTF8CString(keyRef, "leftMeta"))
        return WPE_KEY_Meta_L;
    if (WKStringIsEqualToUTF8CString(keyRef, "rightMeta"))
        return WPE_KEY_Meta_R;
    if (WKStringIsEqualToUTF8CString(keyRef, "leftAlt"))
        return WPE_KEY_Alt_L;
    if (WKStringIsEqualToUTF8CString(keyRef, "rightAlt"))
        return WPE_KEY_Alt_R;
    if (WKStringIsEqualToUTF8CString(keyRef, "leftArrow"))
        return WPE_KEY_Left;
    if (WKStringIsEqualToUTF8CString(keyRef, "rightArrow"))
        return WPE_KEY_Right;
    if (WKStringIsEqualToUTF8CString(keyRef, "upArrow"))
        return WPE_KEY_Up;
    if (WKStringIsEqualToUTF8CString(keyRef, "downArrow"))
        return WPE_KEY_Down;
    if (WKStringIsEqualToUTF8CString(keyRef, "pageUp"))
        return WPE_KEY_Page_Up;
    if (WKStringIsEqualToUTF8CString(keyRef, "pageDown"))
        return WPE_KEY_Page_Down;
    if (WKStringIsEqualToUTF8CString(keyRef, "home"))
        return WPE_KEY_Home;
    if (WKStringIsEqualToUTF8CString(keyRef, "end"))
        return WPE_KEY_End;
    if (WKStringIsEqualToUTF8CString(keyRef, "insert"))
        return WPE_KEY_Insert;
    if (WKStringIsEqualToUTF8CString(keyRef, "delete"))
        return WPE_KEY_Delete;
    if (WKStringIsEqualToUTF8CString(keyRef, "printScreen"))
        return WPE_KEY_Print;
    if (WKStringIsEqualToUTF8CString(keyRef, "menu"))
        return WPE_KEY_Menu;
    if (WKStringIsEqualToUTF8CString(keyRef, "F1"))
        return WPE_KEY_F1;
    if (WKStringIsEqualToUTF8CString(keyRef, "F2"))
        return WPE_KEY_F2;
    if (WKStringIsEqualToUTF8CString(keyRef, "F3"))
        return WPE_KEY_F3;
    if (WKStringIsEqualToUTF8CString(keyRef, "F4"))
        return WPE_KEY_F4;
    if (WKStringIsEqualToUTF8CString(keyRef, "F5"))
        return WPE_KEY_F5;
    if (WKStringIsEqualToUTF8CString(keyRef, "F6"))
        return WPE_KEY_F6;
    if (WKStringIsEqualToUTF8CString(keyRef, "F7"))
        return WPE_KEY_F7;
    if (WKStringIsEqualToUTF8CString(keyRef, "F8"))
        return WPE_KEY_F8;
    if (WKStringIsEqualToUTF8CString(keyRef, "F9"))
        return WPE_KEY_F9;
    if (WKStringIsEqualToUTF8CString(keyRef, "F10"))
        return WPE_KEY_F10;
    if (WKStringIsEqualToUTF8CString(keyRef, "F11"))
        return WPE_KEY_F11;
    if (WKStringIsEqualToUTF8CString(keyRef, "F12"))
        return WPE_KEY_F12;
    if (WKStringIsEqualToUTF8CString(keyRef, "escape"))
        return WPE_KEY_Escape;

    size_t bufferSize = WKStringGetMaximumUTF8CStringSize(keyRef);
    auto buffer = makeUniqueArray<char>(bufferSize);
    WKStringGetUTF8CString(keyRef, buffer.get(), bufferSize);
    char charCode = buffer.get()[0];

    if (charCode == '\n' || charCode == '\r')
        return WPE_KEY_Return;
    if (charCode == '\t')
        return WPE_KEY_Tab;
    if (charCode == '\x8')
        return WPE_KEY_BackSpace;
    if (charCode == 0x001B)
        return WPE_KEY_Escape;

    if (WTF::isASCIIUpper(charCode))
        *modifiers |= wpe_input_keyboard_modifier_shift;

    return wpe_unicode_to_key_code(static_cast<uint32_t>(charCode));
}

class KeyboardEvent
{
public:
    KeyboardEvent(WKStringRef keyRef, WKEventModifiers wkModifiers, unsigned location, double time)
        : m_modifiers { wkEventModifiersToWPE(wkModifiers) }
        , m_keySym { wpeKeySymForKeyRef(keyRef, location, &m_modifiers) }
        , event { createInputKeyboardEvent(m_modifiers, m_keySym, time) }
    {
    }

    KeyboardEvent(WKStringRef keyRef, WKEventModifiers wkModifiers, unsigned location)
        : KeyboardEvent(keyRef, wkModifiers, location, secToMsTimestamp(MonotonicTime::now().secondsSinceEpoch().value()))
    {
    }

    ~KeyboardEvent()
    {
#if defined(WPE_ENABLE_XKB) && WPE_ENABLE_XKB
        free(entries);
#endif
    }

private:
    struct wpe_input_keyboard_event createInputKeyboardEvent(uint32_t modifiers, uint32_t keySym, double time);

    uint32_t m_modifiers;
    uint32_t m_keySym;

#if defined(WPE_ENABLE_XKB) && WPE_ENABLE_XKB
    struct wpe_input_xkb_keymap_entry* entries { nullptr };
#endif

public:
    struct wpe_input_keyboard_event event;
};

struct wpe_input_keyboard_event KeyboardEvent::createInputKeyboardEvent(uint32_t modifiers, uint32_t keySym, double time)
{
#if defined(WPE_ENABLE_XKB) && WPE_ENABLE_XKB
    uint32_t entriesCount;
    wpe_input_xkb_context_get_entries_for_key_code(wpe_input_xkb_context_get_default(), keySym, &entries, &entriesCount);
    return wpe_input_keyboard_event { secToMsTimestamp(time), keySym, entriesCount ? entries[0].hardware_key_code : 0, true, modifiers };
#else
    return wpe_input_keyboard_event { secToMsTimestamp(time), keySym, 0, true, modifiers };
#endif
}

void EventSenderProxyClientLibWPE::keyDown(WKStringRef keyRef, double time, WKEventModifiers wkModifiers, unsigned location)
{
    KeyboardEvent keyboardEvent(keyRef, wkModifiers, location, time);

    wpe_view_backend_dispatch_keyboard_event(viewBackend(m_testController), &keyboardEvent.event);
    keyboardEvent.event.pressed = false;
    wpe_view_backend_dispatch_keyboard_event(viewBackend(m_testController), &keyboardEvent.event);
}

void EventSenderProxyClientLibWPE::rawKeyDown(WKStringRef key, WKEventModifiers wkModifiers, unsigned keyLocation)
{
    KeyboardEvent keyboardEvent(key, wkModifiers, keyLocation);

    wpe_view_backend_dispatch_keyboard_event(viewBackend(m_testController), &keyboardEvent.event);
}

void EventSenderProxyClientLibWPE::rawKeyUp(WKStringRef key, WKEventModifiers wkModifiers, unsigned keyLocation)
{
    KeyboardEvent keyboardEvent(key, wkModifiers, keyLocation);

    wpe_view_backend_dispatch_keyboard_event(viewBackend(m_testController), &keyboardEvent.event);
}

#if ENABLE(TOUCH_EVENTS)
void EventSenderProxyClientLibWPE::addTouchPoint(int x, int y, double time)
{
    struct wpe_input_touch_event_raw rawEvent { wpe_input_touch_event_type_down, secToMsTimestamp(time), static_cast<int>(m_touchEvents.size()), static_cast<int32_t>(x), static_cast<int32_t>(y) };
    m_touchEvents.append(rawEvent);
    m_updatedTouchEvents.add(rawEvent.id);
}

void EventSenderProxyClientLibWPE::updateTouchPoint(int index, int x, int y, double time)
{
    ASSERT(index >= 0 && static_cast<size_t>(index) <= m_touchEvents.size());

    auto& rawEvent = m_touchEvents[index];
    rawEvent.x = x;
    rawEvent.y = y;
    rawEvent.time = secToMsTimestamp(time);
    rawEvent.type = wpe_input_touch_event_type_motion;
    m_updatedTouchEvents.add(index);
}

void EventSenderProxyClientLibWPE::touchStart(double time)
{
    prepareAndDispatchTouchEvent(wpe_input_touch_event_type_down, time);
}

void EventSenderProxyClientLibWPE::touchMove(double time)
{
    prepareAndDispatchTouchEvent(wpe_input_touch_event_type_motion, time);
}

void EventSenderProxyClientLibWPE::touchEnd(double time)
{
    prepareAndDispatchTouchEvent(wpe_input_touch_event_type_up, time);
}

void EventSenderProxyClientLibWPE::touchCancel(double)
{
    notImplemented();
}

void EventSenderProxyClientLibWPE::clearTouchPoints()
{
    m_touchEvents.clear();
    m_updatedTouchEvents.clear();
}

void EventSenderProxyClientLibWPE::releaseTouchPoint(int index, double time)
{
    ASSERT(index >= 0 && static_cast<size_t>(index) <= m_touchEvents.size());

    auto& rawEvent = m_touchEvents[index];
    rawEvent.time = secToMsTimestamp(time);
    rawEvent.type = wpe_input_touch_event_type_up;
    m_updatedTouchEvents.add(index);
}

void EventSenderProxyClientLibWPE::cancelTouchPoint(int, double)
{
    notImplemented();
}

Vector<struct wpe_input_touch_event_raw> EventSenderProxyClientLibWPE::getUpdatedTouchEvents()
{
    Vector<wpe_input_touch_event_raw> events;
    for (auto id : m_updatedTouchEvents)
        events.append(m_touchEvents[id]);
    return events;
}

void EventSenderProxyClientLibWPE::removeUpdatedTouchEvents()
{
    for (auto id : m_updatedTouchEvents)
        m_touchEvents[id].type = wpe_input_touch_event_type_null;
    m_touchEvents.removeAllMatching([] (auto current) {
        return current.type == wpe_input_touch_event_type_null;
        });
}

void EventSenderProxyClientLibWPE::prepareAndDispatchTouchEvent(uint32_t eventType, double time)
{
    auto updatedEvents = getUpdatedTouchEvents();
    struct wpe_input_touch_event event = { updatedEvents.span().data(), updatedEvents.size(), static_cast<enum wpe_input_touch_event_type>(eventType), 0, secToMsTimestamp(time), 0 };
    wpe_view_backend_dispatch_touch_event(viewBackend(m_testController), &event);
    if (eventType == wpe_input_touch_event_type_up)
        removeUpdatedTouchEvents();
    m_updatedTouchEvents.clear();
}
#endif // ENABLE(TOUCH_EVENTS)

} // namespace WTR
