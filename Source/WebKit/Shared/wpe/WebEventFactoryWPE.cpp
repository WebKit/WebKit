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
#include "WebEventFactory.h"

#if ENABLE(WPE_PLATFORM)
#include "WebEventConversion.h"
#include <WebCore/FloatPoint.h>
#include <WebCore/Scrollbar.h>
#include <wpe/wpe-platform.h>
#include <wtf/MonotonicTime.h>

namespace WebKit {

using namespace WebCore;

static MonotonicTime monotonicTimeForEvent(WPEEvent* event)
{
    auto time = wpe_event_get_time(event);
    if (!time)
        return MonotonicTime::now();
    return monotonicTimeForEventTimeInMilliseconds(time);
}

static OptionSet<WebEventModifier> modifiersFromWPEModifiers(WPEModifiers wpeModifiers)
{
    OptionSet<WebEventModifier> modifiers;
    if (wpeModifiers & WPE_MODIFIER_KEYBOARD_CONTROL)
        modifiers.add(WebEventModifier::ControlKey);
    if (wpeModifiers & WPE_MODIFIER_KEYBOARD_SHIFT)
        modifiers.add(WebEventModifier::ShiftKey);
    if (wpeModifiers & WPE_MODIFIER_KEYBOARD_ALT)
        modifiers.add(WebEventModifier::AltKey);
    if (wpeModifiers & WPE_MODIFIER_KEYBOARD_META)
        modifiers.add(WebEventModifier::MetaKey);
    if (wpeModifiers & WPE_MODIFIER_KEYBOARD_CAPS_LOCK)
        modifiers.add(WebEventModifier::CapsLockKey);
    return modifiers;
}

static WebMouseEventButton buttonForWPEButton(guint button)
{
    switch (button) {
    case 1:
        return WebMouseEventButton::Left;
    case 2:
        return WebMouseEventButton::Middle;
    case 3:
        return WebMouseEventButton::Right;
    }
    return WebMouseEventButton::None;
}

static WebMouseEventButton buttonForWPEModifiers(WPEModifiers modifiers)
{
    if (modifiers & WPE_MODIFIER_POINTER_BUTTON1)
        return WebMouseEventButton::Left;
    if (modifiers & WPE_MODIFIER_POINTER_BUTTON2)
        return WebMouseEventButton::Middle;
    if (modifiers & WPE_MODIFIER_POINTER_BUTTON3)
        return WebMouseEventButton::Right;
    return WebMouseEventButton::None;
}

static FloatPoint movementDeltaFromEvent(WPEEvent* event)
{
    double x, y;
    wpe_event_pointer_move_get_delta(event, &x, &y);
    return FloatPoint(x, y);
}

static short pressedMouseButtons(WPEModifiers modifiers)
{
    // MouseEvent.buttons
    // https://www.w3.org/TR/uievents/#ref-for-dom-mouseevent-buttons-1

    // 0 MUST indicate no button is currently active.
    short buttons = 0;

    // 1 MUST indicate the primary button of the device (in general, the left button or the only button on
    // single-button devices, used to activate a user interface control or select text).
    if (modifiers & WPE_MODIFIER_POINTER_BUTTON1)
        buttons |= 1;

    // 4 MUST indicate the auxiliary button (in general, the middle button, often combined with a mouse wheel).
    if (modifiers & WPE_MODIFIER_POINTER_BUTTON2)
        buttons |= 4;

    // 2 MUST indicate the secondary button (in general, the right button, often used to display a context menu),
    // if present.
    if (modifiers & WPE_MODIFIER_POINTER_BUTTON3)
        buttons |= 2;

    return buttons;
}

static IntPoint positionFromEvent(WPEEvent* event)
{
    double x, y;
    if (wpe_event_get_position(event, &x, &y))
        return { clampToInteger(x), clampToInteger(y) };
    return { };
}

WebMouseEvent WebEventFactory::createWebMouseEvent(WPEEvent* event)
{
    auto modifiers = wpe_event_get_modifiers(event);
    FloatPoint movementDelta;
    std::optional<WebEventType> type;
    auto button = WebMouseEventButton::None;
    auto position = positionFromEvent(event);
    unsigned clickCount = 0;

    switch (wpe_event_get_event_type(event)) {
    case WPE_EVENT_POINTER_DOWN:
        type = WebEventType::MouseDown;
        button = buttonForWPEButton(wpe_event_pointer_button_get_button(event));
        clickCount = wpe_event_pointer_button_get_press_count(event);
        break;
    case WPE_EVENT_POINTER_UP:
        type = WebEventType::MouseUp;
        button = buttonForWPEButton(wpe_event_pointer_button_get_button(event));
        break;
    case WPE_EVENT_POINTER_MOVE:
    case WPE_EVENT_POINTER_ENTER:
    case WPE_EVENT_POINTER_LEAVE:
        type = WebEventType::MouseMove;
        button = buttonForWPEModifiers(modifiers);
        movementDelta = movementDeltaFromEvent(event);
        break;
    default:
        RELEASE_ASSERT_NOT_REACHED();
    }

    return WebMouseEvent({ type.value(), modifiersFromWPEModifiers(modifiers), monotonicTimeForEvent(event) },
        button,
        pressedMouseButtons(modifiers),
        position,
        position,
        movementDelta.x(),
        movementDelta.y(),
        0 /* deltaZ */,
        clickCount);
}

WebWheelEvent WebEventFactory::createWebWheelEvent(WPEEvent* event)
{
    auto phase = wpe_event_scroll_is_stop(event) ? WebWheelEvent::Phase::PhaseEnded : WebWheelEvent::Phase::PhaseChanged;
    return createWebWheelEvent(event, phase);
}

WebWheelEvent WebEventFactory::createWebWheelEvent(WPEEvent* event, WebWheelEvent::Phase phase)
{
    double deltaX, deltaY;
    wpe_event_scroll_get_deltas(event, &deltaX, &deltaY);
    bool hasPreciseScrollingDeltas = wpe_event_scroll_has_precise_deltas(event);
    auto position = positionFromEvent(event);

    auto wheelTicks = FloatSize(deltaX, deltaY);
    FloatSize delta;
    if (hasPreciseScrollingDeltas) {
        double wpeScrollDeltaMultiplier;

        switch (wpe_event_get_input_source(event)) {
        case WPE_INPUT_SOURCE_MOUSE:
        case WPE_INPUT_SOURCE_PEN:
        case WPE_INPUT_SOURCE_KEYBOARD:
        case WPE_INPUT_SOURCE_TOUCHPAD:
        case WPE_INPUT_SOURCE_TRACKPOINT:
            wpeScrollDeltaMultiplier = 2.5;
            break;
        case WPE_INPUT_SOURCE_TOUCHSCREEN:
        case WPE_INPUT_SOURCE_TABLET_PAD:
            wpeScrollDeltaMultiplier = 1.0;
            break;
        default:
            RELEASE_ASSERT_NOT_REACHED();
        }

        delta = wheelTicks.scaled(wpeScrollDeltaMultiplier);
    } else {
        auto* view = wpe_event_get_view(event);
        float stepX = wheelTicks.width() ? static_cast<float>(Scrollbar::pixelsPerLineStep(wpe_view_get_width(view))) : 0;
        float stepY = wheelTicks.height() ? static_cast<float>(Scrollbar::pixelsPerLineStep(wpe_view_get_height(view))) : 0;
        delta = wheelTicks.scaled(stepX, stepY);
    }

    return WebWheelEvent({ WebEventType::Wheel, modifiersFromWPEModifiers(wpe_event_get_modifiers(event)), monotonicTimeForEvent(event) },
        position, position, delta, wheelTicks, WebWheelEvent::ScrollByPixelWheelEvent, phase, WebWheelEvent::Phase::PhaseNone, hasPreciseScrollingDeltas);
}

WebKeyboardEvent WebEventFactory::createWebKeyboardEvent(WPEEvent* event, const String& text, bool isAutoRepeat)
{
    auto type = wpe_event_get_event_type(event) == WPE_EVENT_KEYBOARD_KEY_DOWN ? WebEventType::KeyDown : WebEventType::KeyUp;
    auto keyval = wpe_event_keyboard_get_keyval(event);
    auto keycode = wpe_event_keyboard_get_keycode(event);
    return WebKeyboardEvent({ type, modifiersFromWPEModifiers(wpe_event_get_modifiers(event)), monotonicTimeForEvent(event) },
        text.isNull() ? WebKeyboardEvent::singleCharacterStringForWPEKeyval(keyval) : text,
        WebKeyboardEvent::keyValueStringForWPEKeyval(keyval),
        WebKeyboardEvent::keyCodeStringForWPEKeycode(keycode),
        WebKeyboardEvent::keyIdentifierForWPEKeyval(keyval),
        WebKeyboardEvent::windowsKeyCodeForWPEKeyval(keyval),
        keyval, false, std::nullopt, std::nullopt, isAutoRepeat,
        keyval >= WPE_KEY_KP_Space && keyval <= WPE_KEY_KP_9);
}

#if ENABLE(TOUCH_EVENTS)
WebTouchEvent WebEventFactory::createWebTouchEvent(WPEEvent* event, Vector<WebPlatformTouchPoint>&& touchPoints)
{
    std::optional<WebEventType> type;
    switch (wpe_event_get_event_type(event)) {
    case WPE_EVENT_TOUCH_DOWN:
        type = WebEventType::TouchStart;
        break;
    case WPE_EVENT_TOUCH_UP:
        type = WebEventType::TouchEnd;
        break;
    case WPE_EVENT_TOUCH_MOVE:
        type = WebEventType::TouchMove;
        break;
    case WPE_EVENT_TOUCH_CANCEL:
        type = WebEventType::TouchCancel;
        break;
    default:
        RELEASE_ASSERT_NOT_REACHED();
    }

    return WebTouchEvent({ type.value(), modifiersFromWPEModifiers(wpe_event_get_modifiers(event)), monotonicTimeForEvent(event) }, WTFMove(touchPoints), { }, { });
}
#endif // ENABLE(TOUCH_EVENTS)

} // namespace WebKit

#endif // ENABLE(WPE_PLATFORM)
