/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
 * Copyright (C) 2011 Nokia Corporation and/or its subsidiary(-ies).
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
#import "EventSenderProxy.h"

#import "StringFunctions.h"
#import "PlatformWebView.h"
#import "TestController.h"
#import <WebKit2/WKPagePrivate.h>
#import <WebKit2/WKString.h>

namespace WTR {

static int buildModifierFlags(WKEventModifiers modifiers)
{
    int flags = 0;
    if (modifiers & kWKEventModifiersControlKey)
        flags |= NSControlKeyMask;
    if (modifiers & kWKEventModifiersShiftKey)
        flags |= NSShiftKeyMask;
    if (modifiers & kWKEventModifiersAltKey)
        flags |= NSAlternateKeyMask;
    if (modifiers & kWKEventModifiersMetaKey)
        flags |= NSCommandKeyMask;
    return flags;
}

void EventSenderProxy::keyDown(WKStringRef keyRef, WKEventModifiers modifiersRef, unsigned int keyLocation, double timestamp)
{
    NSString* character = [NSString stringWithCString:toSTD(keyRef).c_str() 
                                   encoding:[NSString defaultCStringEncoding]];

    NSString *eventCharacter = character;
    unsigned short keyCode = 0;
    if ([character isEqualToString:@"leftArrow"]) {
        const unichar ch = NSLeftArrowFunctionKey;
        eventCharacter = [NSString stringWithCharacters:&ch length:1];
        keyCode = 0x7B;
    } else if ([character isEqualToString:@"rightArrow"]) {
        const unichar ch = NSRightArrowFunctionKey;
        eventCharacter = [NSString stringWithCharacters:&ch length:1];
        keyCode = 0x7C;
    } else if ([character isEqualToString:@"upArrow"]) {
        const unichar ch = NSUpArrowFunctionKey;
        eventCharacter = [NSString stringWithCharacters:&ch length:1];
        keyCode = 0x7E;
    } else if ([character isEqualToString:@"downArrow"]) {
        const unichar ch = NSDownArrowFunctionKey;
        eventCharacter = [NSString stringWithCharacters:&ch length:1];
        keyCode = 0x7D;
    } else if ([character isEqualToString:@"pageUp"]) {
        const unichar ch = NSPageUpFunctionKey;
        eventCharacter = [NSString stringWithCharacters:&ch length:1];
        keyCode = 0x74;
    } else if ([character isEqualToString:@"pageDown"]) {
        const unichar ch = NSPageDownFunctionKey;
        eventCharacter = [NSString stringWithCharacters:&ch length:1];
        keyCode = 0x79;
    } else if ([character isEqualToString:@"home"]) {
        const unichar ch = NSHomeFunctionKey;
        eventCharacter = [NSString stringWithCharacters:&ch length:1];
        keyCode = 0x73;
    } else if ([character isEqualToString:@"end"]) {
        const unichar ch = NSEndFunctionKey;
        eventCharacter = [NSString stringWithCharacters:&ch length:1];
        keyCode = 0x77;
    } else if ([character isEqualToString:@"insert"]) {
        const unichar ch = NSInsertFunctionKey;
        eventCharacter = [NSString stringWithCharacters:&ch length:1];
        keyCode = 0x72;
    } else if ([character isEqualToString:@"delete"]) {
        const unichar ch = NSDeleteFunctionKey;
        eventCharacter = [NSString stringWithCharacters:&ch length:1];
        keyCode = 0x75;
    } else if ([character isEqualToString:@"printScreen"]) {
        const unichar ch = NSPrintScreenFunctionKey;
        eventCharacter = [NSString stringWithCharacters:&ch length:1];
        keyCode = 0x0; // There is no known virtual key code for PrintScreen.
    }

    // Compare the input string with the function-key names defined by the DOM spec (i.e. "F1",...,"F24").
    // If the input string is a function-key name, set its key code.
    for (unsigned i = 1; i <= 24; i++) {
        if ([character isEqualToString:[NSString stringWithFormat:@"F%u", i]]) {
            const unichar ch = NSF1FunctionKey + (i - 1);
            eventCharacter = [NSString stringWithCharacters:&ch length:1];
            switch (i) {
                case 1: keyCode = 0x7A; break;
                case 2: keyCode = 0x78; break;
                case 3: keyCode = 0x63; break;
                case 4: keyCode = 0x76; break;
                case 5: keyCode = 0x60; break;
                case 6: keyCode = 0x61; break;
                case 7: keyCode = 0x62; break;
                case 8: keyCode = 0x64; break;
                case 9: keyCode = 0x65; break;
                case 10: keyCode = 0x6D; break;
                case 11: keyCode = 0x67; break;
                case 12: keyCode = 0x6F; break;
                case 13: keyCode = 0x69; break;
                case 14: keyCode = 0x6B; break;
                case 15: keyCode = 0x71; break;
                case 16: keyCode = 0x6A; break;
                case 17: keyCode = 0x40; break;
                case 18: keyCode = 0x4F; break;
                case 19: keyCode = 0x50; break;
                case 20: keyCode = 0x5A; break;
            }
        }
    }

    // FIXME: No keyCode is set for most keys.
    if ([character isEqualToString:@"\t"])
        keyCode = 0x30;
    else if ([character isEqualToString:@" "])
        keyCode = 0x31;
    else if ([character isEqualToString:@"\r"])
        keyCode = 0x24;
    else if ([character isEqualToString:@"\n"])
        keyCode = 0x4C;
    else if ([character isEqualToString:@"\x8"])
        keyCode = 0x33;
    else if ([character isEqualToString:@"7"])
        keyCode = 0x1A;
    else if ([character isEqualToString:@"5"])
        keyCode = 0x17;
    else if ([character isEqualToString:@"9"])
        keyCode = 0x19;
    else if ([character isEqualToString:@"0"])
        keyCode = 0x1D;
    else if ([character isEqualToString:@"a"])
        keyCode = 0x00;
    else if ([character isEqualToString:@"b"])
        keyCode = 0x0B;
    else if ([character isEqualToString:@"d"])
        keyCode = 0x02;
    else if ([character isEqualToString:@"e"])
        keyCode = 0x0E;

    NSString *charactersIgnoringModifiers = eventCharacter;

    int modifierFlags = 0;

    if ([character length] == 1 && [character characterAtIndex:0] >= 'A' && [character characterAtIndex:0] <= 'Z') {
        modifierFlags |= NSShiftKeyMask;
        charactersIgnoringModifiers = [character lowercaseString];
    }

    modifierFlags |= buildModifierFlags(modifiersRef);

    if (keyLocation == 0x03 /*DOM_KEY_LOCATION_NUMPAD*/)
        modifierFlags |= NSNumericPadKeyMask;

    // FIXME: [[[mainFrame frameView] documentView] layout];

    WKPageSetShouldSendKeyboardEventSynchronously([m_testController->mainWebView()->platformView() pageRef], true);

    NSEvent *event = [NSEvent keyEventWithType:NSKeyDown
                        location:NSMakePoint(5, 5)
                        modifierFlags:modifierFlags
                        timestamp:timestamp//[self currentEventTime]
                        windowNumber:[m_testController->mainWebView()->platformWindow() windowNumber]
                        context:[NSGraphicsContext currentContext]
                        characters:eventCharacter
                        charactersIgnoringModifiers:charactersIgnoringModifiers
                        isARepeat:NO
                        keyCode:keyCode];

    [[m_testController->mainWebView()->platformWindow() firstResponder] keyDown:event];

    event = [NSEvent keyEventWithType:NSKeyUp
                        location:NSMakePoint(5, 5)
                        modifierFlags:modifierFlags
                        timestamp:timestamp//[self currentEventTime]
                        windowNumber:[m_testController->mainWebView()->platformWindow() windowNumber]
                        context:[NSGraphicsContext currentContext]
                        characters:eventCharacter
                        charactersIgnoringModifiers:charactersIgnoringModifiers
                        isARepeat:NO
                        keyCode:keyCode];

    [[m_testController->mainWebView()->platformWindow() firstResponder] keyUp:event];

    WKPageSetShouldSendKeyboardEventSynchronously([m_testController->mainWebView()->platformView() pageRef], false);
}

} // namespace WTR
