/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
 * Portions Copyright (c) 2010 Motorola Mobility, Inc.  All rights reserved.
 * Copyright (C) 2011 Igalia S.L.
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

#include <WebCore/GtkUtilities.h>
#include <WebCore/GtkVersioning.h>
#include <WebCore/PlatformKeyboardEvent.h>
#include <WebCore/Scrollbar.h>
#include <WebCore/WindowsKeyboardCodes.h>
#include <gdk/gdk.h>
#include <gdk/gdkkeysyms.h>
#include <wtf/ASCIICType.h>
#include <wtf/MathExtras.h>

namespace WebKit {

using namespace WebCore;

#if GTK_CHECK_VERSION(4, 7, 0)
// Keep in sync with https://gitlab.gnome.org/GNOME/gtk/-/blob/493660a296af3b8a140714988ddece4199818a04/gtk/gtkscrolledwindow.c#L204
static const double gtkScrollDeltaMultiplier = 2.5;
#endif

static inline bool isGdkKeyCodeFromKeyPad(unsigned keyval)
{
    return keyval >= GDK_KEY_KP_Space && keyval <= GDK_KEY_KP_9;
}

static inline OptionSet<WebEventModifier> modifiersForEvent(const GdkEvent* event)
{
    OptionSet<WebEventModifier> modifiers;
    GdkModifierType state;

    // Check for a valid state in GdkEvent.
    if (!gdk_event_get_state(event, &state))
        return modifiers;

    if (state & GDK_CONTROL_MASK)
        modifiers.add(WebEventModifier::ControlKey);
    if (state & GDK_SHIFT_MASK)
        modifiers.add(WebEventModifier::ShiftKey);
    if (state & GDK_MOD1_MASK)
        modifiers.add(WebEventModifier::AltKey);
    if (state & GDK_META_MASK)
        modifiers.add(WebEventModifier::MetaKey);
    if (PlatformKeyboardEvent::modifiersContainCapsLock(state))
        modifiers.add(WebEventModifier::CapsLockKey);

    GdkEventType type = gdk_event_get_event_type(const_cast<GdkEvent*>(event));
    if (type != GDK_KEY_PRESS)
        return modifiers;

    // Modifier masks are set different in X than other platforms. This code makes WebKitGTK
    // to behave similar to other platforms and other browsers under X (see http://crbug.com/127142#c8).

    guint keyval;
    gdk_event_get_keyval(event, &keyval);
    switch (keyval) {
    case GDK_KEY_Control_L:
    case GDK_KEY_Control_R:
        modifiers.add(WebEventModifier::ControlKey);
        break;
    case GDK_KEY_Shift_L:
    case GDK_KEY_Shift_R:
        modifiers.add(WebEventModifier::ShiftKey);
        break;
    case GDK_KEY_Alt_L:
    case GDK_KEY_Alt_R:
        modifiers.add(WebEventModifier::AltKey);
        break;
    case GDK_KEY_Meta_L:
    case GDK_KEY_Meta_R:
        modifiers.add(WebEventModifier::MetaKey);
        break;
    case GDK_KEY_Caps_Lock:
        modifiers.add(WebEventModifier::CapsLockKey);
        break;
    }

    return modifiers;
}

static inline WebMouseEventButton buttonForEvent(const GdkEvent* event)
{
    WebMouseEventButton button = WebMouseEventButton::NoButton;
    GdkEventType type = gdk_event_get_event_type(const_cast<GdkEvent*>(event));
    switch (type) {
    case GDK_ENTER_NOTIFY:
    case GDK_LEAVE_NOTIFY:
    case GDK_MOTION_NOTIFY: {
        GdkModifierType state;
        gdk_event_get_state(event, &state);
        if (state & GDK_BUTTON1_MASK)
            button = WebMouseEventButton::LeftButton;
        else if (state & GDK_BUTTON2_MASK)
            button = WebMouseEventButton::MiddleButton;
        else if (state & GDK_BUTTON3_MASK)
            button = WebMouseEventButton::RightButton;
        break;
    }
    case GDK_BUTTON_PRESS:
#if !USE(GTK4)
    case GDK_2BUTTON_PRESS:
    case GDK_3BUTTON_PRESS:
#endif
    case GDK_BUTTON_RELEASE: {
        guint eventButton;
        gdk_event_get_button(event, &eventButton);

        if (eventButton == 1)
            button = WebMouseEventButton::LeftButton;
        else if (eventButton == 2)
            button = WebMouseEventButton::MiddleButton;
        else if (eventButton == 3)
            button = WebMouseEventButton::RightButton;
        break;
    }
    default:
        ASSERT_NOT_REACHED();
    }

    return button;
}

static inline short pressedMouseButtons(GdkModifierType state)
{
    // MouseEvent.buttons
    // https://www.w3.org/TR/uievents/#ref-for-dom-mouseevent-buttons-1

    // 0 MUST indicate no button is currently active.
    short buttons = 0;

    // 1 MUST indicate the primary button of the device (in general, the left button or the only button on
    // single-button devices, used to activate a user interface control or select text).
    if (state & GDK_BUTTON1_MASK)
        buttons |= 1;

    // 4 MUST indicate the auxiliary button (in general, the middle button, often combined with a mouse wheel).
    if (state & GDK_BUTTON2_MASK)
        buttons |= 4;

    // 2 MUST indicate the secondary button (in general, the right button, often used to display a context menu),
    // if present.
    if (state & GDK_BUTTON3_MASK)
        buttons |= 2;

    return buttons;
}

WebMouseEvent WebEventFactory::createWebMouseEvent(const GdkEvent* event, int currentClickCount, std::optional<FloatSize> delta)
{
    double x, y;
    gdk_event_get_coords(event, &x, &y);
    double xRoot, yRoot;
    gdk_event_get_root_coords(event, &xRoot, &yRoot);

    return createWebMouseEvent(event, { clampToInteger(x), clampToInteger(y) }, { clampToInteger(xRoot), clampToInteger(yRoot) }, currentClickCount, delta);
}

WebMouseEvent WebEventFactory::createWebMouseEvent(const GdkEvent* event, const IntPoint& position, const IntPoint& globalPosition, int currentClickCount, std::optional<FloatSize> delta)
{
#if USE(GTK4)
    // This can happen when a NativeWebMouseEvent representing a crossing event is copied.
    if (!event)
        return createWebMouseEvent(position);
#endif

    GdkModifierType state = static_cast<GdkModifierType>(0);
    gdk_event_get_state(event, &state);

    WebEvent::Type type = static_cast<WebEvent::Type>(0);
    FloatSize movementDelta;

    switch (gdk_event_get_event_type(const_cast<GdkEvent*>(event))) {
    case GDK_MOTION_NOTIFY:
    case GDK_ENTER_NOTIFY:
    case GDK_LEAVE_NOTIFY:
        type = WebEvent::MouseMove;
        if (delta)
            movementDelta = delta.value();
        break;
#if !USE(GTK4)
    case GDK_2BUTTON_PRESS:
    case GDK_3BUTTON_PRESS:
#endif
    case GDK_BUTTON_PRESS: {
        type = WebEvent::MouseDown;
        guint eventButton;
        gdk_event_get_button(event, &eventButton);
        auto modifier = stateModifierForGdkButton(eventButton);
        state = static_cast<GdkModifierType>(state | modifier);
        break;
    }
    case GDK_BUTTON_RELEASE: {
        type = WebEvent::MouseUp;
        guint eventButton;
        gdk_event_get_button(event, &eventButton);
        auto modifier = stateModifierForGdkButton(eventButton);
        state = static_cast<GdkModifierType>(state & ~modifier);
        break;
    }
    default :
        ASSERT_NOT_REACHED();
    }

    return WebMouseEvent({ type, modifiersForEvent(event), wallTimeForEvent(event) },
        buttonForEvent(event),
        pressedMouseButtons(state),
        position,
        globalPosition,
        movementDelta.width(),
        movementDelta.height(),
        0 /* deltaZ */,
        currentClickCount
        );
}

WebMouseEvent WebEventFactory::createWebMouseEvent(const IntPoint& position)
{
    // Mouse events without GdkEvent are crossing events, handled as a mouse move.
    return WebMouseEvent({ WebEvent::MouseMove, { }, WallTime::now() }, WebMouseEventButton::NoButton, 0, position, position, 0, 0, 0, 0);
}

WebWheelEvent WebEventFactory::createWebWheelEvent(const GdkEvent* event)
{
    WebWheelEvent::Phase phase = !event || gdk_event_is_scroll_stop_event(event) ?
        WebWheelEvent::Phase::PhaseEnded :
        WebWheelEvent::Phase::PhaseChanged;
    return createWebWheelEvent(event, phase, WebWheelEvent::Phase::PhaseNone);
}

WebWheelEvent WebEventFactory::createWebWheelEvent(const GdkEvent* event, WebWheelEvent::Phase phase, WebWheelEvent::Phase momentumPhase)
{
    double x, y;
    gdk_event_get_coords(event, &x, &y);
    double xRoot, yRoot;
    gdk_event_get_root_coords(event, &xRoot, &yRoot);

    return createWebWheelEvent(event, { clampToInteger(x), clampToInteger(y) }, { clampToInteger(xRoot), clampToInteger(yRoot) }, phase, momentumPhase);
}

WebWheelEvent WebEventFactory::createWebWheelEvent(const GdkEvent* event, const IntPoint& position, const IntPoint& globalPosition, WebWheelEvent::Phase phase, WebWheelEvent::Phase momentumPhase)
{
    std::optional<FloatSize> wheelTicks;
    GdkScrollDirection direction;
    if (!gdk_event_get_scroll_direction(event, &direction)) {
        direction = GDK_SCROLL_SMOOTH;
        double deltaX, deltaY;
        if (gdk_event_get_scroll_deltas(event, &deltaX, &deltaY))
            wheelTicks = FloatSize(-deltaX, -deltaY);
    }

    if (!wheelTicks) {
        switch (direction) {
        case GDK_SCROLL_UP:
            wheelTicks = FloatSize(0, 1);
            break;
        case GDK_SCROLL_DOWN:
            wheelTicks = FloatSize(0, -1);
            break;
        case GDK_SCROLL_LEFT:
            wheelTicks = FloatSize(1, 0);
            break;
        case GDK_SCROLL_RIGHT:
            wheelTicks = FloatSize(-1, 0);
            break;
        case GDK_SCROLL_SMOOTH:
            wheelTicks = FloatSize(0, 0);
            break;
        }
    }

    return createWebWheelEvent(event, position, globalPosition, wheelTicks.value(), phase, momentumPhase);
}

WebWheelEvent WebEventFactory::createWebWheelEvent(const GdkEvent* event, const IntPoint& position, const IntPoint& globalPosition, const FloatSize& wheelTicks)
{
    WebWheelEvent::Phase phase = gdk_event_get_event_type(const_cast<GdkEvent*>(event)) != GDK_SCROLL
        || gdk_event_is_scroll_stop_event(event) ?
        WebWheelEvent::Phase::PhaseEnded :
        WebWheelEvent::Phase::PhaseChanged;
    return createWebWheelEvent(event, position, globalPosition, wheelTicks, phase, WebWheelEvent::Phase::PhaseNone);
}

WebWheelEvent WebEventFactory::createWebWheelEvent(const GdkEvent* event, const IntPoint& position, const IntPoint& globalPosition, const FloatSize& wheelTicks, WebWheelEvent::Phase phase, WebWheelEvent::Phase momentumPhase)
{
    // FIXME: [GTK] Add a setting to change the pixels per line used for scrolling
    // https://bugs.webkit.org/show_bug.cgi?id=54826
    float step = static_cast<float>(Scrollbar::pixelsPerLineStep());
    FloatSize delta;
    bool hasPreciseScrollingDeltas = false;

#if GTK_CHECK_VERSION(4, 7, 0)
    hasPreciseScrollingDeltas = gdk_event_get_event_type(const_cast<GdkEvent*>(event)) != GDK_SCROLL
        || gdk_scroll_event_get_unit(const_cast<GdkEvent*>(event)) != GDK_SCROLL_UNIT_WHEEL;

    if (hasPreciseScrollingDeltas)
        delta = wheelTicks.scaled(gtkScrollDeltaMultiplier);
    else
        delta = wheelTicks.scaled(step);
#else
    delta = wheelTicks.scaled(step);

    GdkScrollDirection direction;
    if (!gdk_event_get_scroll_direction(event, &direction)) {
        double deltaX, deltaY;
        if (gdk_event_get_scroll_deltas(event, &deltaX, &deltaY)) {
            if (auto* device = gdk_event_get_source_device(event))
                hasPreciseScrollingDeltas = gdk_device_get_source(device) != GDK_SOURCE_MOUSE;
        }
    }
#endif

    return WebWheelEvent({ WebEvent::Wheel, modifiersForEvent(event), wallTimeForEvent(event) },
        position,
        globalPosition,
        delta,
        wheelTicks,
        WebWheelEvent::ScrollByPixelWheelEvent,
        phase,
        momentumPhase,
        hasPreciseScrollingDeltas
        );
}

WebKeyboardEvent WebEventFactory::createWebKeyboardEvent(const GdkEvent* event, const String& text, bool handledByInputMethod, std::optional<Vector<CompositionUnderline>>&& preeditUnderlines, std::optional<EditingRange>&& preeditSelectionRange, Vector<String>&& commands)
{
    guint keyval;
    gdk_event_get_keyval(event, &keyval);
    guint16 keycode;
    gdk_event_get_keycode(event, &keycode);
    GdkEventType type = gdk_event_get_event_type(const_cast<GdkEvent*>(event));

    return WebKeyboardEvent(
        { type == GDK_KEY_RELEASE ? WebEvent::KeyUp : WebEvent::KeyDown, modifiersForEvent(event), wallTimeForEvent(event) },
        text.isNull() ? PlatformKeyboardEvent::singleCharacterString(keyval) : text,
        PlatformKeyboardEvent::keyValueForGdkKeyCode(keyval),
        PlatformKeyboardEvent::keyCodeForHardwareKeyCode(keycode),
        PlatformKeyboardEvent::keyIdentifierForGdkKeyCode(keyval),
        PlatformKeyboardEvent::windowsKeyCodeForGdkKeyCode(keyval),
        static_cast<int>(keyval),
        handledByInputMethod,
        WTFMove(preeditUnderlines),
        WTFMove(preeditSelectionRange),
        WTFMove(commands),
        isGdkKeyCodeFromKeyPad(keyval)
        );
}

#if ENABLE(TOUCH_EVENTS)
WebTouchEvent WebEventFactory::createWebTouchEvent(const GdkEvent* event, Vector<WebPlatformTouchPoint>&& touchPoints)
{
    WebEvent::Type type = WebEvent::NoType;
    GdkEventType eventType = gdk_event_get_event_type(const_cast<GdkEvent*>(event));
    switch (eventType) {
    case GDK_TOUCH_BEGIN:
        type = WebEvent::TouchStart;
        break;
    case GDK_TOUCH_UPDATE:
        type = WebEvent::TouchMove;
        break;
    case GDK_TOUCH_END:
        type = WebEvent::TouchEnd;
        break;
    case GDK_TOUCH_CANCEL:
        type = WebEvent::TouchCancel;
        break;
    default:
        ASSERT_NOT_REACHED();
    }

    return WebTouchEvent({ type, modifiersForEvent(event), wallTimeForEvent(event) }, WTFMove(touchPoints));
}
#endif

} // namespace WebKit
