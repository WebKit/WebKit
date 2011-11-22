/*
 * Copyright (C) 2011 Samsung Electronics
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

#include "EflKeyboardUtilities.h"
#include <WebCore/Scrollbar.h>

using namespace WebCore;

namespace WebKit {

enum {
    VerticalScrollDirection = 0,
    HorizontalScrollDirection = 1
};

enum {
    LeftButton = 1,
    MiddleButton = 2,
    RightButton = 3
};

static inline WebEvent::Modifiers modifiersForEvent(const Evas_Modifier* modifiers)
{
    unsigned result = 0;

    if (evas_key_modifier_is_set(modifiers, "Shift"))
        result |= WebEvent::ShiftKey;
    if (evas_key_modifier_is_set(modifiers, "Control"))
        result |= WebEvent::ControlKey;
    if (evas_key_modifier_is_set(modifiers, "Alt"))
        result |= WebEvent::AltKey;
    if (evas_key_modifier_is_set(modifiers, "Meta"))
        result |= WebEvent::MetaKey;

    return static_cast<WebEvent::Modifiers>(result);
}

static inline WebMouseEvent::Button buttonForEvent(int button)
{
    if (button == LeftButton)
        return WebMouseEvent::LeftButton;
    if (button == MiddleButton)
        return WebMouseEvent::MiddleButton;
    if (button == RightButton)
        return WebMouseEvent::RightButton;

    return WebMouseEvent::NoButton;
}

static inline int clickCountForEvent(const Evas_Button_Flags flags)
{
    if (flags & EVAS_BUTTON_TRIPLE_CLICK)
        return 3;
    if (flags & EVAS_BUTTON_DOUBLE_CLICK)
        return 2;

    return 1;
}

WebMouseEvent WebEventFactory::createWebMouseEvent(const Evas_Event_Mouse_Down* event, const Evas_Point* position)
{
    return WebMouseEvent(WebEvent::MouseDown,
                         buttonForEvent(event->button),
                         IntPoint(event->canvas.x - position->x, event->canvas.y - position->y),
                         IntPoint(event->canvas.x, event->canvas.y),
                         0 /* deltaX */,
                         0 /* deltaY */,
                         0 /* deltaZ */,
                         clickCountForEvent(event->flags),
                         modifiersForEvent(event->modifiers),
                         event->timestamp);
}

WebMouseEvent WebEventFactory::createWebMouseEvent(const Evas_Event_Mouse_Up* event, const Evas_Point* position)
{
    return WebMouseEvent(WebEvent::MouseUp,
                         buttonForEvent(event->button),
                         IntPoint(event->canvas.x - position->x, event->canvas.y - position->y),
                         IntPoint(event->canvas.x, event->canvas.y),
                         0 /* deltaX */,
                         0 /* deltaY */,
                         0 /* deltaZ */,
                         clickCountForEvent(event->flags),
                         modifiersForEvent(event->modifiers),
                         event->timestamp);
}

WebMouseEvent WebEventFactory::createWebMouseEvent(const Evas_Event_Mouse_Move* event, const Evas_Point* position)
{
    return WebMouseEvent(WebEvent::MouseMove,
                         buttonForEvent(event->buttons),
                         IntPoint(event->cur.canvas.x - position->x, event->cur.canvas.y - position->y),
                         IntPoint(event->cur.canvas.x, event->cur.canvas.y),
                         0 /* deltaX */,
                         0 /* deltaY */,
                         0 /* deltaZ */,
                         0 /* clickCount */,
                         modifiersForEvent(event->modifiers),
                         event->timestamp);
}

WebWheelEvent WebEventFactory::createWebWheelEvent(const Evas_Event_Mouse_Wheel* event, const Evas_Point* position)
{
    float deltaX = 0;
    float deltaY = 0;
    float wheelTicksX = 0;
    float wheelTicksY = 0;

    // A negative z value means (in EFL) that we are scrolling down, so we need
    // to invert the value.
    if (event->direction == VerticalScrollDirection) {
        deltaX = 0;
        deltaY = - event->z;
    } else if (event->direction == HorizontalScrollDirection) {
        deltaX = - event->z;
        deltaY = 0;
    }

    wheelTicksX = deltaX;
    wheelTicksY = deltaY;
    deltaX *= static_cast<float>(Scrollbar::pixelsPerLineStep());
    deltaY *= static_cast<float>(Scrollbar::pixelsPerLineStep());

    return WebWheelEvent(WebEvent::Wheel,
                         IntPoint(event->canvas.x - position->x, event->canvas.y - position->y),
                         IntPoint(event->canvas.x, event->canvas.y),
                         FloatSize(deltaX, deltaY),
                         FloatSize(wheelTicksX, wheelTicksY),
                         WebWheelEvent::ScrollByPixelWheelEvent,
                         modifiersForEvent(event->modifiers),
                         event->timestamp);
}

WebKeyboardEvent WebEventFactory::createWebKeyboardEvent(const Evas_Event_Key_Down* event)
{
    String keyName = String(event->key);
    return WebKeyboardEvent(WebEvent::KeyDown,
                            String::fromUTF8(event->string),
                            String::fromUTF8(event->string),
                            keyIdentifierForEvasKeyName(keyName),
                            windowsKeyCodeForEvasKeyName(keyName),
                            0 /* FIXME: nativeVirtualKeyCode */,
                            0 /* macCharCode */,
                            false /* FIXME: isAutoRepeat */,
                            false /* FIXME: isKeypad */,
                            false /* isSystemKey */,
                            modifiersForEvent(event->modifiers),
                            event->timestamp);
}

WebKeyboardEvent WebEventFactory::createWebKeyboardEvent(const Evas_Event_Key_Up* event)
{
    String keyName = String(event->key);
    return WebKeyboardEvent(WebEvent::KeyUp,
                            String::fromUTF8(event->string),
                            String::fromUTF8(event->string),
                            keyIdentifierForEvasKeyName(keyName),
                            windowsKeyCodeForEvasKeyName(keyName),
                            0 /* FIXME: nativeVirtualKeyCode */,
                            0 /* macCharCode */,
                            false /* FIXME: isAutoRepeat */,
                            false /* FIXME: isKeypad */,
                            false /* isSystemKey */,
                            modifiersForEvent(event->modifiers),
                            event->timestamp);
}

} // namespace WebKit
