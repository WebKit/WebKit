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
#include "WKPagePrivateWPE.h"

#include "NativeWebKeyboardEvent.h"
#include "NativeWebMouseEvent.h"
#include "WKAPICast.h"
#include "WebPageProxy.h"

#if USE(LIBWPE)
#include <wpe/wpe.h>
#endif

#if ENABLE(WPE_PLATFORM)
#include <wpe/GRefPtrWPE.h>
#include <wpe/wpe-platform.h>

static WPEModifiers wkEventModifiersToWPE(WKEventModifiers wkModifiers)
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
    return static_cast<WPEModifiers>(modifiers);
}
#endif

void WKPageHandleKeyboardEvent(WKPageRef pageRef, WKKeyboardEvent event)
{
    using WebKit::NativeWebKeyboardEvent;

#if ENABLE(WPE_PLATFORM)
    if (auto* view = WebKit::toImpl(pageRef)->wpeView()) {
        GRefPtr<WPEEvent> wpeEvent = adoptGRef(wpe_event_keyboard_new(event.type == kWKEventKeyDown ? WPE_EVENT_KEYBOARD_KEY_DOWN : WPE_EVENT_KEYBOARD_KEY_UP,
            view, WPE_INPUT_SOURCE_KEYBOARD, 0, wkEventModifiersToWPE(event.modifiers), event.hardwareKeyCode, event.keyCode));
        WebKit::toImpl(pageRef)->handleKeyboardEvent(NativeWebKeyboardEvent(wpeEvent.get(), unsafeMakeSpan(event.text, event.length), false));
        return;
    }
#endif

#if USE(LIBWPE)
    wpe_input_keyboard_event wpeEvent;
    wpeEvent.time = 0;
    wpeEvent.key_code = event.keyCode;
    wpeEvent.hardware_key_code = event.hardwareKeyCode;
    wpeEvent.modifiers = event.modifiers;
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
    std::optional<Vector<WebCore::CompositionUnderline>> preeditUnderlines;
    std::optional<WebKit::EditingRange> preeditSelectionRange;
    WebKit::toImpl(pageRef)->handleKeyboardEvent(NativeWebKeyboardEvent(&wpeEvent, unsafeMakeSpan(event.text, event.length), false, handledByInputMethod, WTF::move(preeditUnderlines), WTF::move(preeditSelectionRange)));
#endif
}

void WKPageHandleMouseEvent(WKPageRef pageRef, WKMouseEvent event)
{
    using WebKit::NativeWebMouseEvent;

#if ENABLE(WPE_PLATFORM)
    if (auto* view = WebKit::toImpl(pageRef)->wpeView()) {
        GRefPtr<WPEEvent> wpeEvent;

        auto wkEventButtonToWPE = [](WKEventMouseButton button) {
            switch (button) {
            case kWKEventMouseButtonLeftButton:
                return WPE_BUTTON_PRIMARY;
            case kWKEventMouseButtonMiddleButton:
                return WPE_BUTTON_MIDDLE;
            case kWKEventMouseButtonRightButton:
                return WPE_BUTTON_SECONDARY;
            case kWKEventMouseButtonNoButton:
                return 0;
            default:
                break;
            }
            return button;
        };

        switch (event.type) {
        case kWKEventMouseDown:
        case kWKEventMouseUp:
            wpeEvent = adoptGRef(wpe_event_pointer_button_new(event.type == kWKEventMouseDown ? WPE_EVENT_POINTER_DOWN : WPE_EVENT_POINTER_UP, view, WPE_INPUT_SOURCE_MOUSE,
                0, wkEventModifiersToWPE(event.modifiers), wkEventButtonToWPE(event.button), event.position.x, event.position.y, event.type == kWKEventMouseDown ? 1 : 0));
            break;
        case kWKEventMouseMove:
            wpeEvent = adoptGRef(wpe_event_pointer_move_new(WPE_EVENT_POINTER_MOVE, view, WPE_INPUT_SOURCE_MOUSE, 0, wkEventModifiersToWPE(event.modifiers),
                event.position.x, event.position.y, 0, 0));
            break;
        }

        WebKit::toImpl(pageRef)->handleMouseEvent(NativeWebMouseEvent(wpeEvent.get()));
        return;
    }
#endif

#if USE(LIBWPE)
    wpe_input_pointer_event wpeEvent;

    switch (event.type) {
    case kWKEventMouseDown:
        wpeEvent.type = wpe_input_pointer_event_type_button;
        wpeEvent.state = 1;
        break;
    case kWKEventMouseUp:
        wpeEvent.type = wpe_input_pointer_event_type_button;
        wpeEvent.state = 0;
        break;
    case kWKEventMouseMove:
        wpeEvent.type = wpe_input_pointer_event_type_motion;
        wpeEvent.state = 0;
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
    wpeEvent.modifiers = event.modifiers;

    const float deviceScaleFactor = 1;

    WebKit::toImpl(pageRef)->handleMouseEvent(NativeWebMouseEvent(&wpeEvent, deviceScaleFactor));
#endif
}
