/*
 * Copyright (C) 2013, 2011 Apple Inc. All rights reserved.
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

#import "config.h"
#import "WebIOSEventFactory.h"

#if PLATFORM(IOS_FAMILY)

#import <WebCore/KeyEventCodesIOS.h>
#import <WebCore/PlatformEventFactoryIOS.h>

static WebKit::WebEvent::Modifiers modifiersForEvent(::WebEvent *event)
{
    unsigned modifiers = 0;
    WebEventFlags eventModifierFlags = event.modifierFlags;

    if (eventModifierFlags & WebEventFlagMaskShift)
        modifiers |= WebKit::WebEvent::ShiftKey;
    if (eventModifierFlags & WebEventFlagMaskControl)
        modifiers |= WebKit::WebEvent::ControlKey;
    if (eventModifierFlags & WebEventFlagMaskAlternate)
        modifiers |= WebKit::WebEvent::AltKey;
    if (eventModifierFlags & WebEventFlagMaskCommand)
        modifiers |= WebKit::WebEvent::MetaKey;
    if (eventModifierFlags & WebEventFlagMaskAlphaShift)
        modifiers |= WebKit::WebEvent::CapsLockKey;

    return static_cast<WebKit::WebEvent::Modifiers>(modifiers);
}

WebKit::WebKeyboardEvent WebIOSEventFactory::createWebKeyboardEvent(::WebEvent *event)
{
    WebKit::WebEvent::Type type = (event.type == WebEventKeyUp) ? WebKit::WebEvent::KeyUp : WebKit::WebEvent::KeyDown;
    String text = event.characters;
    String unmodifiedText = event.charactersIgnoringModifiers;
    String key = WebCore::keyForKeyEvent(event);
    String code = WebCore::codeForKeyEvent(event);
    String keyIdentifier = WebCore::keyIdentifierForKeyEvent(event);
    int windowsVirtualKeyCode = event.keyCode;
    // FIXME: This is not correct. WebEvent.keyCode represents the Windows native virtual key code.
    int nativeVirtualKeyCode = event.keyCode;
    int macCharCode = 0;
    bool autoRepeat = event.isKeyRepeating;
    bool isKeypad = false;
    bool isSystemKey = false;
    WebKit::WebEvent::Modifiers modifiers = modifiersForEvent(event);
    double timestamp = event.timestamp;

    if (windowsVirtualKeyCode == '\r') {
        text = "\r"_s;
        unmodifiedText = text;
    }

    // The adjustments below are only needed in backward compatibility mode, but we cannot tell what mode we are in from here.

    // Turn 0x7F into 8, because backspace needs to always be 8.
    if (text == "\x7F")
        text = "\x8"_s;
    if (unmodifiedText == "\x7F")
        unmodifiedText = "\x8"_s;
    // Always use 9 for tab.
    if (windowsVirtualKeyCode == 9) {
        text = "\x9"_s;
        unmodifiedText = text;
    }

    return WebKit::WebKeyboardEvent(type, text, unmodifiedText, key, code, keyIdentifier, windowsVirtualKeyCode, nativeVirtualKeyCode, macCharCode, autoRepeat, isKeypad, isSystemKey, modifiers, WallTime::fromRawSeconds(timestamp));
}

WebKit::WebMouseEvent WebIOSEventFactory::createWebMouseEvent(::WebEvent *event)
{
    // This currently only supports synthetic mouse moved events with no button pressed.
    ASSERT_ARG(event, event.type == WebEventMouseMoved);

    auto type = WebKit::WebEvent::MouseMove;
    auto button = WebKit::WebMouseEvent::NoButton;
    unsigned short buttons = 0;
    auto position = WebCore::IntPoint(event.locationInWindow);
    float deltaX = 0;
    float deltaY = 0;
    float deltaZ = 0;
    int clickCount = 0;
    auto modifiers = static_cast<WebKit::WebEvent::Modifiers>(0);
    double timestamp = event.timestamp;

    return WebKit::WebMouseEvent(type, button, buttons, position, position, deltaX, deltaY, deltaZ, clickCount, modifiers, WallTime::fromRawSeconds(timestamp));
}

#endif // PLATFORM(IOS_FAMILY)
