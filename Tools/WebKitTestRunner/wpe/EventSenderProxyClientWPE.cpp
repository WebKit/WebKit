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
#include "EventSenderProxyClientWPE.h"

#if ENABLE(WPE_PLATFORM)

#include "PlatformWebView.h"
#include "PlatformWebViewClientWPE.h"
#include "TestController.h"
#include <wpe/wpe-platform.h>
#include <wtf/MonotonicTime.h>
#include <wtf/UniqueArray.h>
#include <wtf/glib/GUniquePtr.h>

namespace WTR {

// Key event location code defined in DOM Level 3.
enum KeyLocationCode {
    DOMKeyLocationStandard = 0x00,
    DOMKeyLocationLeft = 0x01,
    DOMKeyLocationRight = 0x02,
    DOMKeyLocationNumpad = 0x03
};

EventSenderProxyClientWPE::EventSenderProxyClientWPE(TestController& controller)
    : EventSenderProxyClient(controller)
{
}

EventSenderProxyClientWPE::~EventSenderProxyClientWPE() = default;

static uint32_t secToMsTimestamp(double currentEventTime)
{
    return static_cast<uint32_t>(currentEventTime * 1000);
}

static unsigned wkEventModifiersToWPE(WKEventModifiers wkModifiers)
{
    unsigned modifiers = 0;
    if (wkModifiers & kWKEventModifiersControlKey)
        modifiers |= WPE_MODIFIER_KEYBOARD_CONTROL;
    if (wkModifiers & kWKEventModifiersShiftKey)
        modifiers |= WPE_MODIFIER_KEYBOARD_SHIFT;
    if (wkModifiers & kWKEventModifiersAltKey)
        modifiers |= WPE_MODIFIER_KEYBOARD_ALT;
    if (wkModifiers & kWKEventModifiersMetaKey)
        modifiers |= WPE_MODIFIER_KEYBOARD_META;
    if (wkModifiers & kWKEventModifiersCapsLockKey)
        modifiers |= WPE_MODIFIER_KEYBOARD_CAPS_LOCK;
    return modifiers;
}

static unsigned applyKeyvalModifiers(unsigned keyval, unsigned modifiers)
{
    unsigned updatedModifiers = modifiers;
    switch (keyval) {
    case WPE_KEY_Control_L:
    case WPE_KEY_Control_R:
        updatedModifiers |= WPE_MODIFIER_KEYBOARD_CONTROL;
        break;
    case WPE_KEY_Shift_L:
    case WPE_KEY_Shift_R:
        updatedModifiers |= WPE_MODIFIER_KEYBOARD_SHIFT;
        break;
    case WPE_KEY_Alt_L:
    case WPE_KEY_Alt_R:
        updatedModifiers |= WPE_MODIFIER_KEYBOARD_ALT;
        break;
    case WPE_KEY_Meta_L:
    case WPE_KEY_Meta_R:
        updatedModifiers |= WPE_MODIFIER_KEYBOARD_META;
        break;
    case WPE_KEY_Caps_Lock:
        updatedModifiers |= WPE_MODIFIER_KEYBOARD_CAPS_LOCK;
        break;
    }
    return updatedModifiers;
}

static unsigned eventSenderButtonToWPEButton(unsigned button)
{
    int mouseButton = 3;
    if (button <= 2)
        mouseButton = button + 1;
    // fast/events/mouse-click-events expects the 4th button to be treated as the middle button.
    else if (button == 3)
        mouseButton = 2;

    return mouseButton;
}

static unsigned modifierForButton(unsigned button)
{
    switch (button) {
    case 1:
        return WPE_MODIFIER_POINTER_BUTTON1;
    case 2:
        return WPE_MODIFIER_POINTER_BUTTON2;
    case 3:
        return WPE_MODIFIER_POINTER_BUTTON3;
    case 4:
        return WPE_MODIFIER_POINTER_BUTTON4;
    case 5:
        return WPE_MODIFIER_POINTER_BUTTON5;
    default:
        break;
    }

    return 0;
}

void EventSenderProxyClientWPE::mouseDown(unsigned button, double time, WKEventModifiers wkModifiers, double x, double y, int clickCount, unsigned& mouseButtonsCurrentlyDown)
{
    auto wpeButton = eventSenderButtonToWPEButton(button);
    mouseButtonsCurrentlyDown |= modifierForButton(wpeButton);
    auto modifiers = static_cast<WPEModifiers>(wkEventModifiersToWPE(wkModifiers) | mouseButtonsCurrentlyDown);
    auto timestamp = secToMsTimestamp(time);
    auto* view = WKViewGetView(m_testController.mainWebView()->platformView());
    auto* event = wpe_event_pointer_button_new(WPE_EVENT_POINTER_DOWN, view, WPE_INPUT_SOURCE_MOUSE, timestamp, modifiers, wpeButton, x, y, clickCount);
    wpe_view_event(view, event);
    wpe_event_unref(event);
}

void EventSenderProxyClientWPE::mouseUp(unsigned button, double time, WKEventModifiers wkModifiers, double x, double y, unsigned& mouseButtonsCurrentlyDown)
{
    auto wpeButton = eventSenderButtonToWPEButton(button);
    mouseButtonsCurrentlyDown &= ~modifierForButton(wpeButton);
    auto modifiers = static_cast<WPEModifiers>(wkEventModifiersToWPE(wkModifiers) | mouseButtonsCurrentlyDown);
    auto* view = WKViewGetView(m_testController.mainWebView()->platformView());
    auto* event = wpe_event_pointer_button_new(WPE_EVENT_POINTER_UP, view, WPE_INPUT_SOURCE_MOUSE, secToMsTimestamp(time), modifiers, wpeButton, x, y, 0);
    wpe_view_event(view, event);
    wpe_event_unref(event);
}

void EventSenderProxyClientWPE::mouseMoveTo(double x, double y, double time, WKEventMouseButton, unsigned mouseButtonsCurrentlyDown)
{
    auto* view = WKViewGetView(m_testController.mainWebView()->platformView());
    auto* event = wpe_event_pointer_move_new(WPE_EVENT_POINTER_MOVE, view, WPE_INPUT_SOURCE_MOUSE, secToMsTimestamp(time),
        static_cast<WPEModifiers>(mouseButtonsCurrentlyDown), x, y, 0, 0);
    wpe_view_event(view, event);
    wpe_event_unref(event);
}

void EventSenderProxyClientWPE::mouseScrollBy(int horizontal, int vertical, double time, double x, double y)
{
    auto* view = WKViewGetView(m_testController.mainWebView()->platformView());
    auto* event = wpe_event_scroll_new(view, WPE_INPUT_SOURCE_MOUSE, secToMsTimestamp(time), static_cast<WPEModifiers>(0), horizontal, vertical, FALSE, FALSE, x, y);
    wpe_view_event(view, event);
    wpe_event_unref(event);
}

static unsigned wpeKeyvalForKeyRef(WKStringRef keyRef, unsigned location, unsigned& modifiers)
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
        modifiers |= WPE_MODIFIER_KEYBOARD_SHIFT;

    return wpe_unicode_to_keyval(static_cast<uint32_t>(charCode));
}

void EventSenderProxyClientWPE::keyDown(WKStringRef keyRef, double time, WKEventModifiers wkModifiers, unsigned location)
{
    unsigned modifiers = wkEventModifiersToWPE(wkModifiers);
    auto keyval = wpeKeyvalForKeyRef(keyRef, location, modifiers);
    unsigned downModifiers = applyKeyvalModifiers(keyval, modifiers);

    auto* view = WKViewGetView(m_testController.mainWebView()->platformView());
    unsigned keycode = 0;
    auto* keymap = wpe_display_get_keymap(wpe_view_get_display(view));
    GUniqueOutPtr<WPEKeymapEntry> entries;
    guint entriesCount;
    if (wpe_keymap_get_entries_for_keyval(keymap, keyval, &entries.outPtr(), &entriesCount))
        keycode = entries.get()[0].keycode;

    auto* event = wpe_event_keyboard_new(WPE_EVENT_KEYBOARD_KEY_DOWN, view, WPE_INPUT_SOURCE_KEYBOARD, secToMsTimestamp(time), static_cast<WPEModifiers>(downModifiers), keycode, keyval);
    wpe_view_event(view, event);
    wpe_event_unref(event);
    event = wpe_event_keyboard_new(WPE_EVENT_KEYBOARD_KEY_UP, view, WPE_INPUT_SOURCE_KEYBOARD, secToMsTimestamp(time), static_cast<WPEModifiers>(modifiers), keycode, keyval);
    wpe_view_event(view, event);
    wpe_event_unref(event);
}

void EventSenderProxyClientWPE::rawKeyDown(WKStringRef keyRef, WKEventModifiers wkModifiers, unsigned location)
{
    unsigned modifiers = wkEventModifiersToWPE(wkModifiers);
    auto keyval = wpeKeyvalForKeyRef(keyRef, location, modifiers);
    modifiers = applyKeyvalModifiers(keyval, modifiers);

    auto* view = WKViewGetView(m_testController.mainWebView()->platformView());
    unsigned keycode = 0;
    auto* keymap = wpe_display_get_keymap(wpe_view_get_display(view));
    GUniqueOutPtr<WPEKeymapEntry> entries;
    guint entriesCount;
    if (wpe_keymap_get_entries_for_keyval(keymap, keyval, &entries.outPtr(), &entriesCount))
        keycode = entries.get()[0].keycode;

    auto* event = wpe_event_keyboard_new(WPE_EVENT_KEYBOARD_KEY_DOWN, view, WPE_INPUT_SOURCE_KEYBOARD, secToMsTimestamp(MonotonicTime::now().secondsSinceEpoch().value()), static_cast<WPEModifiers>(modifiers), keycode, keyval);
    wpe_view_event(view, event);
    wpe_event_unref(event);
}

void EventSenderProxyClientWPE::rawKeyUp(WKStringRef keyRef, WKEventModifiers wkModifiers, unsigned location)
{
    unsigned modifiers = wkEventModifiersToWPE(wkModifiers);
    auto keyval = wpeKeyvalForKeyRef(keyRef, location, modifiers);

    auto* view = WKViewGetView(m_testController.mainWebView()->platformView());
    unsigned keycode = 0;
    auto* keymap = wpe_display_get_keymap(wpe_view_get_display(view));
    GUniqueOutPtr<WPEKeymapEntry> entries;
    guint entriesCount;
    if (wpe_keymap_get_entries_for_keyval(keymap, keyval, &entries.outPtr(), &entriesCount))
        keycode = entries.get()[0].keycode;

    auto* event = wpe_event_keyboard_new(WPE_EVENT_KEYBOARD_KEY_UP, view, WPE_INPUT_SOURCE_KEYBOARD, secToMsTimestamp(MonotonicTime::now().secondsSinceEpoch().value()), static_cast<WPEModifiers>(modifiers), keycode, keyval);
    wpe_view_event(view, event);
    wpe_event_unref(event);
}

#if ENABLE(TOUCH_EVENTS)
void EventSenderProxyClientWPE::addTouchPoint(int x, int y, double)
{
    uint32_t id = 0;
    for (const auto& point : m_touchPoints) {
        if (point.id == id)
            id++;
    }
    m_touchPoints.append({ id, TouchPoint::State::Down, x, y });
}

void EventSenderProxyClientWPE::updateTouchPoint(int index, int x, int y, double)
{
    ASSERT(index >= 0 && static_cast<size_t>(index) < m_touchPoints.size());

    auto& point = m_touchPoints[index];
    point.x = x;
    point.y = y;
    point.state = TouchPoint::State::Move;
}

void EventSenderProxyClientWPE::releaseTouchPoint(int index, double)
{
    ASSERT(index >= 0 && static_cast<size_t>(index) < m_touchPoints.size());

    auto& point = m_touchPoints[index];
    point.state = TouchPoint::State::Up;
}

void EventSenderProxyClientWPE::cancelTouchPoint(int index, double)
{
    ASSERT(index >= 0 && static_cast<size_t>(index) < m_touchPoints.size());

    auto& point = m_touchPoints[index];
    point.state = TouchPoint::State::Cancel;
}

void EventSenderProxyClientWPE::clearTouchPoints()
{
    m_touchPoints.clear();
}

struct EventSenderProxyClientWPE::TouchPointContext {
    const TouchPoint::State targetState;
    const std::optional<TouchPoint::State> newState;
    const WPEEventType eventType;
    const double time;
    const WPEModifiers modifiers;
    WPEView* view;
};

std::function<bool(EventSenderProxyClientWPE::TouchPoint&)> EventSenderProxyClientWPE::pointProcessor(const TouchPointContext& context)
{
    return [context](TouchPoint& point) -> bool {
        if (point.state != context.targetState)
            return false;

        if (context.newState)
            point.state = *context.newState;

        auto* event = wpe_event_touch_new(context.eventType, context.view, WPE_INPUT_SOURCE_TOUCHSCREEN, secToMsTimestamp(context.time), context.modifiers, point.id, point.x, point.y);
        wpe_view_event(context.view, event);
        wpe_event_unref(event);
        return true;
    };
}

void EventSenderProxyClientWPE::touchStart(double time)
{
    auto* view = WKViewGetView(m_testController.mainWebView()->platformView());
    auto downPointProcessor = pointProcessor(TouchPointContext { TouchPoint::State::Down, TouchPoint::State::Stationary, WPE_EVENT_TOUCH_DOWN, time, static_cast<WPEModifiers>(m_touchModifiers), view });
    for (auto& point : m_touchPoints)
        downPointProcessor(point);
}

void EventSenderProxyClientWPE::touchMove(double time)
{
    auto* view = WKViewGetView(m_testController.mainWebView()->platformView());
    auto movePointProcessor = pointProcessor(TouchPointContext { TouchPoint::State::Move, TouchPoint::State::Stationary, WPE_EVENT_TOUCH_MOVE, time, static_cast<WPEModifiers>(m_touchModifiers), view });
    for (auto& point : m_touchPoints)
        movePointProcessor(point);
}

void EventSenderProxyClientWPE::touchEnd(double time)
{
    auto* view = WKViewGetView(m_testController.mainWebView()->platformView());
    auto upPointProcessor = pointProcessor(TouchPointContext { TouchPoint::State::Up, std::nullopt, WPE_EVENT_TOUCH_UP, time, static_cast<WPEModifiers>(0), view });
    m_touchPoints.removeAllMatching(upPointProcessor);
}

void EventSenderProxyClientWPE::touchCancel(double time)
{
    auto* view = WKViewGetView(m_testController.mainWebView()->platformView());
    auto cancelPointProcessor = pointProcessor(TouchPointContext { TouchPoint::State::Cancel, std::nullopt, WPE_EVENT_TOUCH_CANCEL, time, static_cast<WPEModifiers>(0), view });
    m_touchPoints.removeAllMatching(cancelPointProcessor);
}

void EventSenderProxyClientWPE::setTouchModifier(WKEventModifiers wkModifiers, bool enable)
{
    unsigned modifiers = wkEventModifiersToWPE(wkModifiers);
    if (enable)
        m_touchModifiers |= modifiers;
    else
        m_touchModifiers &= ~modifiers;
}
#endif // ENABLE(TOUCH_EVENTS)

} // namespace WTR

#endif // ENABLE(WPE_PLATFORM)
