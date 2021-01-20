/*
 * Copyright (C) 2020 Sony Interactive Entertainment Inc.
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
#include "WKPagePrivatePlayStation.h"

#include "NativeWebKeyboardEvent.h"
#include "NativeWebMouseEvent.h"
#include "NativeWebWheelEvent.h"
#include "WebEventFactory.h"
#include "WebPageProxy.h"
#include <wpe/wpe.h>

void WKPageSetSize(WKPageRef pageRef, WKSize size)
{
    notImplemented();
}

void WKPageHandleKeyboardEvent(WKPageRef pageRef, WKKeyboardEvent event)
{
    using WebKit::NativeWebKeyboardEvent;

    wpe_input_keyboard_event wpeEvent;
    wpeEvent.time = 0;
    wpeEvent.key_code = event.virtualKeyCode;
    wpeEvent.hardware_key_code = event.virtualKeyCode;
    wpeEvent.modifiers = 0; // TODO: Handle modifiers.
    switch (event.type) {
    case kWKEventKeyDown:
        wpeEvent.pressed = true;
        break;
    case kWKEventKeyUp:
        wpeEvent.pressed = false;
        break;
    default:
        ASSERT_NOT_REACHED();
        return;
    }

    NativeWebKeyboardEvent::HandledByInputMethod handledByInputMethod = NativeWebKeyboardEvent::HandledByInputMethod::No;
    Optional<Vector<WebCore::CompositionUnderline>> preeditUnderlines;
    Optional<WebKit::EditingRange> preeditSelectionRange;
    WebKit::toImpl(pageRef)->handleKeyboardEvent(NativeWebKeyboardEvent(&wpeEvent, "", handledByInputMethod, WTFMove(preeditUnderlines), WTFMove(preeditSelectionRange)));
}

void WKPageHandleMouseEvent(WKPageRef pageRef, WKMouseEvent event)
{
    using WebKit::NativeWebMouseEvent;

    wpe_input_pointer_event wpeEvent;

    switch (event.type) {
    case kWKEventMouseDown:
    case kWKEventMouseUp:
        wpeEvent.type = wpe_input_pointer_event_type_button;
        break;
    case kWKEventMouseMove:
        wpeEvent.type = wpe_input_pointer_event_type_motion;
        break;
    default:
        ASSERT_NOT_REACHED();
        return;
    }

    switch (event.button) {
    case kWKEventMouseButtonLeftButton:
        wpeEvent.button = 1;
        break;
    case kWKEventMouseButtonMiddleButton:
        wpeEvent.button = 3;
        break;
    case kWKEventMouseButtonRightButton:
        wpeEvent.button = 2;
        break;
    case kWKEventMouseButtonNoButton:
        wpeEvent.button = 0;
        break;
    default:
        ASSERT_NOT_REACHED();
        return;
    }
    wpeEvent.time = 0;
    wpeEvent.x = event.position.x;
    wpeEvent.y = event.position.y;
    wpeEvent.state = 0;
    wpeEvent.modifiers = 0; // TODO: Handle modifiers.

    const float deviceScaleFactor = 1;

    WebKit::toImpl(pageRef)->handleMouseEvent(NativeWebMouseEvent(&wpeEvent, deviceScaleFactor));
}

void WKPageHandleWheelEvent(WKPageRef pageRef, WKWheelEvent event)
{
    using WebKit::WebWheelEvent;
    using WebKit::NativeWebWheelEvent;

    wpe_input_axis_2d_event wpeEvent;
    wpeEvent.base.type = (wpe_input_axis_event_type)(wpe_input_axis_event_type_motion_smooth | wpe_input_axis_event_type_mask_2d);
    wpeEvent.base.time = 0;
    wpeEvent.base.x = event.position.x;
    wpeEvent.base.y = event.position.y;
    wpeEvent.base.axis = 0;
    wpeEvent.base.value = 0;
    wpeEvent.base.modifiers = 0; // TODO: Handle modifiers.
    wpeEvent.x_axis = event.delta.width;
    wpeEvent.x_axis = event.delta.height;

    const float deviceScaleFactor = 1;

    WebKit::toImpl(pageRef)->handleWheelEvent(NativeWebWheelEvent(&wpeEvent.base, deviceScaleFactor, WebWheelEvent::Phase::PhaseNone, WebWheelEvent::Phase::PhaseNone));
}
