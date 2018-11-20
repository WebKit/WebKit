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
#include "WebEventFactory.h"

#include <WebCore/PlatformKeyboardEvent.h>
#include <WebCore/Scrollbar.h>
#include <cmath>
#include <wpe/wpe.h>

namespace WebKit {

static inline bool isWPEKeyCodeFromKeyPad(unsigned keyCode)
{
    return keyCode >= WPE_KEY_KP_Space && keyCode <= WPE_KEY_KP_9;
}

static WebEvent::Modifiers modifiersForEventModifiers(unsigned eventModifiers)
{
    unsigned modifiers = 0;

    if (eventModifiers & wpe_input_keyboard_modifier_control)
        modifiers |= WebEvent::ControlKey;
    if (eventModifiers & wpe_input_keyboard_modifier_shift)
        modifiers |= WebEvent::ShiftKey;
    if (eventModifiers & wpe_input_keyboard_modifier_alt)
        modifiers |= WebEvent::AltKey;
    if (eventModifiers & wpe_input_keyboard_modifier_meta)
        modifiers |= WebEvent::MetaKey;

    return static_cast<WebEvent::Modifiers>(modifiers);
}

WallTime wallTimeForEventTime(uint64_t timestamp)
{
    // This works if and only if the WPE backend uses CLOCK_MONOTONIC for its
    // event timestamps, and so long as g_get_monotonic_time() continues to do
    // so as well, and so long as WTF::MonotonicTime continues to use
    // g_get_monotonic_time(). It also assumes the event timestamp is in
    // milliseconds.
    return timestamp ? MonotonicTime::fromRawSeconds(timestamp / 1000.).approximateWallTime() : WallTime::now();
}

WebKeyboardEvent WebEventFactory::createWebKeyboardEvent(struct wpe_input_keyboard_event* event)
{
    String singleCharacterString = WebCore::PlatformKeyboardEvent::singleCharacterString(event->key_code);

    return WebKeyboardEvent(event->pressed ? WebEvent::KeyDown : WebEvent::KeyUp,
        WebCore::PlatformKeyboardEvent::singleCharacterString(event->key_code),
        WebCore::PlatformKeyboardEvent::keyValueForWPEKeyCode(event->key_code),
        WebCore::PlatformKeyboardEvent::keyCodeForHardwareKeyCode(event->hardware_key_code),
        WebCore::PlatformKeyboardEvent::keyIdentifierForWPEKeyCode(event->key_code),
        WebCore::PlatformKeyboardEvent::windowsKeyCodeForWPEKeyCode(event->key_code),
        event->key_code,
        isWPEKeyCodeFromKeyPad(event->key_code),
        modifiersForEventModifiers(event->modifiers),
        wallTimeForEventTime(event->time));
}

static inline short pressedMouseButtons(uint32_t modifiers)
{
    // MouseEvent.buttons
    // https://www.w3.org/TR/uievents/#ref-for-dom-mouseevent-buttons-1

    // 0 MUST indicate no button is currently active.
    short buttons = 0;

    // 1 MUST indicate the primary button of the device (in general, the left button or the only button on
    // single-button devices, used to activate a user interface control or select text).
    if (modifiers & wpe_input_pointer_modifier_button1)
        buttons |= 1;

    // 2 MUST indicate the secondary button (in general, the right button, often used to display a context menu),
    // if present.
    if (modifiers & wpe_input_pointer_modifier_button2)
        buttons |= 2;

    // 4 MUST indicate the auxiliary button (in general, the middle button, often combined with a mouse wheel).
    if (modifiers & wpe_input_pointer_modifier_button3)
        buttons |= 4;

    return buttons;
}

WebMouseEvent WebEventFactory::createWebMouseEvent(struct wpe_input_pointer_event* event, float deviceScaleFactor)
{
    WebEvent::Type type = WebEvent::NoType;
    switch (event->type) {
    case wpe_input_pointer_event_type_motion:
        type = WebEvent::MouseMove;
        break;
    case wpe_input_pointer_event_type_button:
        type = event->state ? WebEvent::MouseDown : WebEvent::MouseUp;
        break;
    case wpe_input_pointer_event_type_null:
        ASSERT_NOT_REACHED();
    }

    WebMouseEvent::Button button = WebMouseEvent::NoButton;
    switch (event->type) {
    case wpe_input_pointer_event_type_motion:
    case wpe_input_pointer_event_type_button:
        if (event->button == 1)
            button = WebMouseEvent::LeftButton;
        else if (event->button == 2)
            button = WebMouseEvent::RightButton;
        else if (event->button == 3)
            button = WebMouseEvent::MiddleButton;
        break;
    case wpe_input_pointer_event_type_null:
        ASSERT_NOT_REACHED();
    }

    unsigned clickCount = (type == WebEvent::MouseDown) ? 1 : 0;

    // FIXME: Proper button support. deltaX/Y/Z. Click count.
    WebCore::IntPoint position(event->x, event->y);
    position.scale(1 / deviceScaleFactor);
    return WebMouseEvent(type, button, pressedMouseButtons(event->modifiers), position, position,
        0, 0, 0, clickCount, modifiersForEventModifiers(event->modifiers), wallTimeForEventTime(event->time));
}

WebWheelEvent WebEventFactory::createWebWheelEvent(struct wpe_input_axis_event* event, float deviceScaleFactor)
{
    // FIXME: We shouldn't hard-code this.
    enum Axis {
        Vertical,
        Horizontal,
        Smooth
    };

    WebCore::FloatSize wheelTicks;
    WebCore::FloatSize delta;
    switch (event->axis) {
    case Vertical:
        wheelTicks = WebCore::FloatSize(0, std::copysign(1, event->value));
        delta = wheelTicks;
        delta.scale(WebCore::Scrollbar::pixelsPerLineStep());
        break;
    case Horizontal:
        wheelTicks = WebCore::FloatSize(std::copysign(1, event->value), 0);
        delta = wheelTicks;
        delta.scale(WebCore::Scrollbar::pixelsPerLineStep());
        break;
    case Smooth:
        wheelTicks = WebCore::FloatSize(0, event->value / deviceScaleFactor);
        delta = wheelTicks;
        break;
    default:
        ASSERT_NOT_REACHED();
    };

    WebCore::IntPoint position(event->x, event->y);
    position.scale(1 / deviceScaleFactor);
    return WebWheelEvent(WebEvent::Wheel, position, position,
        delta, wheelTicks, WebWheelEvent::ScrollByPixelWheelEvent, static_cast<WebEvent::Modifiers>(0), wallTimeForEventTime(event->time));
}

#if ENABLE(TOUCH_EVENTS)
static WebKit::WebPlatformTouchPoint::TouchPointState stateForTouchPoint(int mainEventId, const struct wpe_input_touch_event_raw* point)
{
    if (point->id != mainEventId)
        return WebKit::WebPlatformTouchPoint::TouchStationary;

    switch (point->type) {
    case wpe_input_touch_event_type_down:
        return WebKit::WebPlatformTouchPoint::TouchPressed;
    case wpe_input_touch_event_type_motion:
        return WebKit::WebPlatformTouchPoint::TouchMoved;
    case wpe_input_touch_event_type_up:
        return WebKit::WebPlatformTouchPoint::TouchReleased;
    case wpe_input_touch_event_type_null:
        ASSERT_NOT_REACHED();
        break;
    };

    return WebKit::WebPlatformTouchPoint::TouchStationary;
}

WebTouchEvent WebEventFactory::createWebTouchEvent(struct wpe_input_touch_event* event, float deviceScaleFactor)
{
    WebEvent::Type type = WebEvent::NoType;
    switch (event->type) {
    case wpe_input_touch_event_type_down:
        type = WebEvent::TouchStart;
        break;
    case wpe_input_touch_event_type_motion:
        type = WebEvent::TouchMove;
        break;
    case wpe_input_touch_event_type_up:
        type = WebEvent::TouchEnd;
        break;
    case wpe_input_touch_event_type_null:
        ASSERT_NOT_REACHED();
    }

    Vector<WebKit::WebPlatformTouchPoint> touchPoints;
    touchPoints.reserveCapacity(event->touchpoints_length);

    for (unsigned i = 0; i < event->touchpoints_length; ++i) {
        auto& point = event->touchpoints[i];
        if (point.type == wpe_input_touch_event_type_null)
            continue;

        WebCore::IntPoint pointCoordinates(point.x, point.y);
        pointCoordinates.scale(1 / deviceScaleFactor);
        touchPoints.uncheckedAppend(
            WebKit::WebPlatformTouchPoint(point.id, stateForTouchPoint(event->id, &point),
                pointCoordinates, pointCoordinates));
    }

    return WebTouchEvent(type, WTFMove(touchPoints), WebEvent::Modifiers(0), wallTimeForEventTime(event->time));
}
#endif // ENABLE(TOUCH_EVENTS)

} // namespace WebKit
