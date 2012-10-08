/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
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

#include "config.h"

#import <Cocoa/Cocoa.h>
#include <gtest/gtest.h>

#include "KeyboardEvent.h"
#include "WebInputEvent.h"
#include "WebInputEventFactory.h"

using WebKit::WebInputEventFactory;
using WebKit::WebKeyboardEvent;

namespace {

NSEvent* BuildFakeKeyEvent(NSUInteger keyCode, unichar character, NSUInteger modifierFlags)
{
    NSString* string = [NSString stringWithCharacters:&character length:1];
    return [NSEvent keyEventWithType:NSKeyDown
                            location:NSZeroPoint
                       modifierFlags:modifierFlags
                           timestamp:0.0
                        windowNumber:0
                             context:nil
                          characters:string
         charactersIgnoringModifiers:string
                           isARepeat:NO
                             keyCode:keyCode];
}

} // namespace

// Test that arrow keys don't have numpad modifier set.
TEST(WebInputEventFactoryTestMac, ArrowKeyNumPad)
{
    // Left
    NSEvent* macEvent = BuildFakeKeyEvent(0x7B, NSLeftArrowFunctionKey, NSNumericPadKeyMask);
    WebKeyboardEvent webEvent = WebInputEventFactory::keyboardEvent(macEvent);
    EXPECT_EQ(0, webEvent.modifiers);

    // Right
    macEvent = BuildFakeKeyEvent(0x7C, NSRightArrowFunctionKey, NSNumericPadKeyMask);
    webEvent = WebInputEventFactory::keyboardEvent(macEvent);
    EXPECT_EQ(0, webEvent.modifiers);

    // Down
    macEvent = BuildFakeKeyEvent(0x7D, NSDownArrowFunctionKey, NSNumericPadKeyMask);
    webEvent = WebInputEventFactory::keyboardEvent(macEvent);
    EXPECT_EQ(0, webEvent.modifiers);

    // Up
    macEvent = BuildFakeKeyEvent(0x7E, NSUpArrowFunctionKey, NSNumericPadKeyMask);
    webEvent = WebInputEventFactory::keyboardEvent(macEvent);
    EXPECT_EQ(0, webEvent.modifiers);
}
