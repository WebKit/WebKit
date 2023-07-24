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
#include <wtf/HashMap.h>
#include <wtf/NeverDestroyed.h>

namespace WebKit {

static inline bool isWPEKeyCodeFromKeyPad(unsigned keyCode)
{
    return keyCode >= WPE_KEY_KP_Space && keyCode <= WPE_KEY_KP_9;
}

static OptionSet<WebEventModifier> modifiersForEventModifiers(unsigned eventModifiers)
{
    OptionSet<WebEventModifier> modifiers;
    if (eventModifiers & wpe_input_keyboard_modifier_control)
        modifiers.add(WebEventModifier::ControlKey);
    if (eventModifiers & wpe_input_keyboard_modifier_shift)
        modifiers.add(WebEventModifier::ShiftKey);
    if (eventModifiers & wpe_input_keyboard_modifier_alt)
        modifiers.add(WebEventModifier::AltKey);
    if (eventModifiers & wpe_input_keyboard_modifier_meta)
        modifiers.add(WebEventModifier::MetaKey);

    return modifiers;
}

static OptionSet<WebEventModifier> modifiersForKeyboardEvent(struct wpe_input_keyboard_event* event)
{
    OptionSet<WebEventModifier> modifiers = modifiersForEventModifiers(event->modifiers);

    if (!event->pressed)
        return modifiers;

    // Handling of modifier masks in WPE is modelled after X. This code makes WPE to behave similar
    // to other platforms and other browsers under X (see http://crbug.com/127142#c8).

    switch (event->key_code) {
    case WPE_KEY_Control_L:
    case WPE_KEY_Control_R:
        modifiers.add(WebEventModifier::ControlKey);
        break;
    case WPE_KEY_Shift_L:
    case WPE_KEY_Shift_R:
        modifiers.add(WebEventModifier::ShiftKey);
        break;
    case WPE_KEY_Alt_L:
    case WPE_KEY_Alt_R:
        modifiers.add(WebEventModifier::AltKey);
        break;
    case WPE_KEY_Meta_L:
    case WPE_KEY_Meta_R:
        modifiers.add(WebEventModifier::MetaKey);
        break;
    case WPE_KEY_Caps_Lock:
        modifiers.add(WebEventModifier::CapsLockKey);
        break;
    }

    return modifiers;
}

WallTime wallTimeForEventTime(uint64_t msTimeStamp)
{
    // WPE event time field is an uint32_t, too small for full ms timestamps since
    // the epoch, and are expected to be just timestamps with monotonic behavior
    // to be compared among themselves, not against WallTime-like measurements.
    // Thus the need to define a reference origin based on the first event received.

    static uint64_t firstEventTimeStamp;
    static WallTime firstEventWallTime;
    static std::once_flag once;

    // Fallback for zero timestamps.
    if (!msTimeStamp)
        return WallTime::now();

    std::call_once(once, [msTimeStamp]() {
        firstEventTimeStamp = msTimeStamp;
        firstEventWallTime = WallTime::now();
    });

    uint64_t delta = msTimeStamp - firstEventTimeStamp;

    return firstEventWallTime + Seconds(delta / 1000.);
}

WebKeyboardEvent WebEventFactory::createWebKeyboardEvent(struct wpe_input_keyboard_event* event, const String& text, bool isAutoRepeat, bool handledByInputMethod, std::optional<Vector<WebCore::CompositionUnderline>>&& preeditUnderlines, std::optional<EditingRange>&& preeditSelectionRange)
{
    return WebKeyboardEvent({ event->pressed ? WebEventType::KeyDown : WebEventType::KeyUp, modifiersForKeyboardEvent(event), wallTimeForEventTime(event->time) },
        text.isNull() ? WebCore::PlatformKeyboardEvent::singleCharacterString(event->key_code) : text,
        WebCore::PlatformKeyboardEvent::keyValueForWPEKeyCode(event->key_code),
        WebCore::PlatformKeyboardEvent::keyCodeForHardwareKeyCode(event->hardware_key_code),
        WebCore::PlatformKeyboardEvent::keyIdentifierForWPEKeyCode(event->key_code),
        WebCore::PlatformKeyboardEvent::windowsKeyCodeForWPEKeyCode(event->key_code),
        event->key_code,
        handledByInputMethod,
        WTFMove(preeditUnderlines),
        WTFMove(preeditSelectionRange),
        isAutoRepeat,
        isWPEKeyCodeFromKeyPad(event->key_code)
        );
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

static inline unsigned clickCount(struct wpe_input_pointer_event* event)
{
    static const uint32_t doubleClickTime = 500; // Milliseconds.
    static const int pixelThreshold = 5;
    static struct wpe_input_pointer_event gLastClickEvent = { };
    static unsigned gLastClickCount = 1;

    bool cancelPreviousClick = (event->time - gLastClickEvent.time) > doubleClickTime
        || std::abs(event->x - gLastClickEvent.x) > pixelThreshold
        || std::abs(event->y - gLastClickEvent.y) > pixelThreshold;

    if (event->type == wpe_input_pointer_event_type_button && event->state) {
        if (!cancelPreviousClick && (event->button == gLastClickEvent.button))
            ++gLastClickCount;
        else
            gLastClickCount = 1;
        gLastClickEvent = *event;
    } else if (event->type == wpe_input_pointer_event_type_motion) {
        if (cancelPreviousClick) {
            gLastClickCount = 0;
            gLastClickEvent = { };
        }
    }

    return gLastClickCount;
}

WebMouseEvent WebEventFactory::createWebMouseEvent(struct wpe_input_pointer_event* event, float deviceScaleFactor, WebMouseEventSyntheticClickType syntheticClickType)
{
    auto type = WebEventType::NoType;
    switch (event->type) {
    case wpe_input_pointer_event_type_motion:
        type = WebEventType::MouseMove;
        break;
    case wpe_input_pointer_event_type_button:
        type = event->state ? WebEventType::MouseDown : WebEventType::MouseUp;
        break;
    case wpe_input_pointer_event_type_null:
        ASSERT_NOT_REACHED();
    }

    WebMouseEventButton button = WebMouseEventButton::NoButton;
    switch (event->type) {
    case wpe_input_pointer_event_type_motion:
    case wpe_input_pointer_event_type_button:
        if (event->button == 1)
            button = WebMouseEventButton::LeftButton;
        else if (event->button == 2)
            button = WebMouseEventButton::RightButton;
        else if (event->button == 3)
            button = WebMouseEventButton::MiddleButton;
        break;
    case wpe_input_pointer_event_type_null:
        ASSERT_NOT_REACHED();
    }

    // FIXME: Proper button support. deltaX/Y/Z.
    WebCore::IntPoint position(event->x, event->y);
    position.scale(1 / deviceScaleFactor);
    return WebMouseEvent({ type, modifiersForEventModifiers(event->modifiers), wallTimeForEventTime(event->time) }, button, pressedMouseButtons(event->modifiers), position, position,
        0, 0, 0, clickCount(event), 0, syntheticClickType);
}

WebWheelEvent WebEventFactory::createWebWheelEvent(struct wpe_input_axis_event* event, float deviceScaleFactor, WebWheelEvent::Phase phase, WebWheelEvent::Phase momentumPhase)
{
    WebCore::IntPoint position(event->x, event->y);
    position.scale(1 / deviceScaleFactor);

    WebCore::FloatSize wheelTicks;
    WebCore::FloatSize delta;
    bool hasPreciseScrollingDeltas = false;

#if WPE_CHECK_VERSION(1, 5, 0)
    if (event->type & wpe_input_axis_event_type_mask_2d) {
        auto* event2D = reinterpret_cast<struct wpe_input_axis_2d_event*>(event);
        switch (event->type & (wpe_input_axis_event_type_mask_2d - 1)) {
        case wpe_input_axis_event_type_motion:
            wheelTicks = WebCore::FloatSize(std::copysign(1, event2D->x_axis), std::copysign(1, event2D->y_axis));
            delta = wheelTicks;
            delta.scale(WebCore::Scrollbar::pixelsPerLineStep());
            break;
        case wpe_input_axis_event_type_motion_smooth:
            wheelTicks = WebCore::FloatSize(event2D->x_axis / deviceScaleFactor, event2D->y_axis / deviceScaleFactor);
            delta = wheelTicks;
            hasPreciseScrollingDeltas = true;
            break;
        default:
            return WebWheelEvent();
        }

        return WebWheelEvent({ WebEventType::Wheel, OptionSet<WebEventModifier> { }, wallTimeForEventTime(event->time) }, position, position,
            delta, wheelTicks, WebWheelEvent::ScrollByPixelWheelEvent, phase, momentumPhase,
            hasPreciseScrollingDeltas);
    }
#endif

    // FIXME: We shouldn't hard-code this.
    enum Axis {
        Vertical,
        Horizontal,
        Smooth
    };

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
        hasPreciseScrollingDeltas = true;
        break;
    default:
        return WebWheelEvent();
    };

    return WebWheelEvent({ WebEventType::Wheel, OptionSet<WebEventModifier> { }, wallTimeForEventTime(event->time) }, position, position,
        delta, wheelTicks, WebWheelEvent::ScrollByPixelWheelEvent, phase, momentumPhase,
        hasPreciseScrollingDeltas);
}

#if ENABLE(TOUCH_EVENTS)
static WebKit::WebPlatformTouchPoint::State stateForTouchPoint(int mainEventId, const struct wpe_input_touch_event_raw* point)
{
    if (point->id != mainEventId)
        return WebKit::WebPlatformTouchPoint::State::Stationary;

    switch (point->type) {
    case wpe_input_touch_event_type_down:
        return WebKit::WebPlatformTouchPoint::State::Pressed;
    case wpe_input_touch_event_type_motion:
        return WebKit::WebPlatformTouchPoint::State::Moved;
    case wpe_input_touch_event_type_up:
        return WebKit::WebPlatformTouchPoint::State::Released;
    case wpe_input_touch_event_type_null:
        ASSERT_NOT_REACHED();
        break;
    };

    return WebKit::WebPlatformTouchPoint::State::Stationary;
}

WebTouchEvent WebEventFactory::createWebTouchEvent(struct wpe_input_touch_event* event, float deviceScaleFactor)
{
    auto type = WebEventType::NoType;
    static NeverDestroyed<HashMap<int32_t, int32_t>> activeTrackingTouchPoints;
    static int32_t uniqueTouchPointId = WebCore::mousePointerID + 1;
    int32_t pointId;

    switch (event->type) {
    case wpe_input_touch_event_type_down:
        type = WebEventType::TouchStart;
        activeTrackingTouchPoints.get().add(event->id, uniqueTouchPointId);
        pointId = uniqueTouchPointId;
        uniqueTouchPointId++;
        break;
    case wpe_input_touch_event_type_motion:
        type = WebEventType::TouchMove;
        pointId = activeTrackingTouchPoints.get().get(event->id);
        break;
    case wpe_input_touch_event_type_up:
        type = WebEventType::TouchEnd;
        pointId = activeTrackingTouchPoints.get().take(event->id);
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
            WebKit::WebPlatformTouchPoint(pointId, stateForTouchPoint(event->id, &point),
                pointCoordinates, pointCoordinates));
    }

    return WebTouchEvent({ type, OptionSet<WebEventModifier> { }, wallTimeForEventTime(event->time) }, WTFMove(touchPoints));
}
#endif // ENABLE(TOUCH_EVENTS)

} // namespace WebKit
