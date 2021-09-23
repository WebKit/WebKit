/*
 * Copyright (C) 2019 Microsoft Corporation.
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
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#import "config.h"
#import "NativeWebMouseEvent.h"
#import "WebPageInspectorInputAgent.h"
#import "WebPageProxy.h"
#import <WebCore/IntPoint.h>
#import <WebCore/IntSize.h>
#import "NativeWebKeyboardEvent.h"

namespace WebKit {

using namespace WebCore;

void WebPageInspectorInputAgent::platformDispatchMouseEvent(const String& type, int x, int y, std::optional<int>&& optionalModifiers, const String& button, std::optional<int>&& optionalClickCount, unsigned short buttons) {
    IntPoint locationInWindow(x, y);

    NSEventModifierFlags modifiers = 0;
    if (optionalModifiers) {
        int inputModifiers = *optionalModifiers;
        if (inputModifiers & 1)
            modifiers |= NSEventModifierFlagShift;
        if (inputModifiers & 2)
            modifiers |= NSEventModifierFlagControl;
        if (inputModifiers & 4)
            modifiers |= NSEventModifierFlagOption;
        if (inputModifiers & 8)
            modifiers |= NSEventModifierFlagCommand;
    }
    int clickCount = optionalClickCount ? *optionalClickCount : 0;

    NSTimeInterval timestamp = [NSDate timeIntervalSinceReferenceDate];
    NSWindow *window = m_page.platformWindow();
    NSInteger windowNumber = window.windowNumber;

    NSEventType downEventType = (NSEventType)0;
    NSEventType dragEventType = (NSEventType)0;
    NSEventType upEventType = (NSEventType)0;

    if (!button || button == "none") {
        downEventType = NSEventTypeMouseMoved;
        dragEventType = NSEventTypeMouseMoved;
        upEventType = NSEventTypeMouseMoved;
    } else if (button == "left") {
        downEventType = NSEventTypeLeftMouseDown;
        dragEventType = NSEventTypeLeftMouseDragged;
        upEventType = NSEventTypeLeftMouseUp;
    } else if (button == "middle") {
        downEventType = NSEventTypeOtherMouseDown;
        dragEventType = NSEventTypeLeftMouseDragged;
        upEventType = NSEventTypeOtherMouseUp;
    } else if (button == "right") {
        downEventType = NSEventTypeRightMouseDown;
        upEventType = NSEventTypeRightMouseUp;
    }

    NSInteger eventNumber = 0;

    NSEvent* event = nil;
    if (type == "move") {
        event = [NSEvent mouseEventWithType:dragEventType location:locationInWindow modifierFlags:modifiers timestamp:timestamp windowNumber:windowNumber context:nil eventNumber:eventNumber clickCount:clickCount pressure:0.0f];
    } else if (type == "down") {
        event = [NSEvent mouseEventWithType:downEventType location:locationInWindow modifierFlags:modifiers timestamp:timestamp windowNumber:windowNumber context:nil eventNumber:eventNumber clickCount:clickCount pressure:WebCore::ForceAtClick];
    } else if (type == "up") {
        event = [NSEvent mouseEventWithType:upEventType location:locationInWindow modifierFlags:modifiers timestamp:timestamp windowNumber:windowNumber context:nil eventNumber:eventNumber clickCount:clickCount pressure:0.0f];
    }

    if (event) {
        NativeWebMouseEvent nativeEvent(event, nil, [window contentView]);
        nativeEvent.playwrightSetButtons(buttons);
        m_page.handleMouseEvent(nativeEvent);
    }
}

void WebPageInspectorInputAgent::platformDispatchKeyEvent(WebKeyboardEvent::Type type, const String& text, const String& unmodifiedText, const String& key, const String& code, const String& keyIdentifier, int windowsVirtualKeyCode, int nativeVirtualKeyCode, bool isAutoRepeat, bool isKeypad, bool isSystemKey, OptionSet<WebEvent::Modifier> modifiers, Vector<String>& commands, WallTime timestamp)
{
    Vector<WebCore::KeypressCommand> macCommands;
    for (const String& command : commands) {
      m_page.registerKeypressCommandName(command);
      macCommands.append(WebCore::KeypressCommand(command.utf8().data()));
    }
    if (text.length() > 0 && macCommands.size() == 0)
      macCommands.append(WebCore::KeypressCommand("insertText:", text));
    NativeWebKeyboardEvent event(
        type,
        text,
        unmodifiedText,
        key,
        code,
        keyIdentifier,
        windowsVirtualKeyCode,
        nativeVirtualKeyCode,
        isAutoRepeat,
        isKeypad,
        isSystemKey,
        modifiers,
        timestamp,
        WTFMove(macCommands));
    m_page.handleKeyboardEvent(event);
}

} // namespace WebKit
