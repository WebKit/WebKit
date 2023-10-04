/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
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

#import "PlatformUtilities.h"
#import <Carbon/Carbon.h> // For GetCurrentEventTime
#import <WebCore/PlatformEventFactoryMac.h>
#import <pal/spi/mac/NSMenuSPI.h>
#import <wtf/RetainPtr.h>

namespace TestWebKitAPI {

static bool canCallMenuTypeForEvent()
{
    return [NSMenu respondsToSelector:@selector(menuTypeForEvent:)];
}

static void buildAndPerformTest(NSEventType buttonEvent, NSEventModifierFlags modifierFlags, WebCore::MouseButton expectedButton, NSMenuType expectedMenu)
{
    @autoreleasepool {
        auto webView = adoptNS([[WebView alloc] init]);
        NSEvent *event = [NSEvent mouseEventWithType:buttonEvent
                                            location:NSMakePoint(100, 100)
                                       modifierFlags:modifierFlags
                                           timestamp:GetCurrentEventTime()
                                        windowNumber:[[webView window] windowNumber]
                                             context:[NSGraphicsContext currentContext]
                                         eventNumber:0
                                          clickCount:0
                                            pressure:0];

        auto pme = WebCore::PlatformEventFactory::createPlatformMouseEvent(event, nil, webView.get());

        EXPECT_EQ(expectedButton, pme.button());
        EXPECT_TRUE(!modifierFlags || pme.modifierFlags() & modifierFlags);
        EXPECT_EQ(expectedMenu, pme.menuTypeForEvent());
        if (canCallMenuTypeForEvent())
            EXPECT_EQ(expectedMenu, [NSMenu menuTypeForEvent:event]);
    }
}

TEST(WebKitLegacy, MenuAndButtonForNormalLeftClick)
{
    buildAndPerformTest(NSEventTypeLeftMouseDown, 0, WebCore::MouseButton::Left, NSMenuTypeNone);
}

TEST(WebKitLegacy, MenuAndButtonForNormalRightClick)
{
    buildAndPerformTest(NSEventTypeRightMouseDown, 0, WebCore::MouseButton::Right, NSMenuTypeContextMenu);
}

TEST(WebKitLegacy, MenuAndButtonForNormalMiddleClick)
{
    buildAndPerformTest(NSEventTypeOtherMouseDown, 0, WebCore::MouseButton::Middle, NSMenuTypeNone);
}

TEST(WebKitLegacy, MenuAndButtonForControlLeftClick)
{
    buildAndPerformTest(NSEventTypeLeftMouseDown, NSEventModifierFlagControl, WebCore::MouseButton::Left, NSMenuTypeContextMenu);
}

TEST(WebKitLegacy, MenuAndButtonForControlRightClick)
{
    buildAndPerformTest(NSEventTypeRightMouseDown, NSEventModifierFlagControl, WebCore::MouseButton::Right, NSMenuTypeContextMenu);
}

TEST(WebKitLegacy, MenuAndButtonForControlMiddleClick)
{
    buildAndPerformTest(NSEventTypeOtherMouseDown, NSEventModifierFlagControl, WebCore::MouseButton::Middle, NSMenuTypeNone);
}
    
TEST(WebKitLegacy, MenuAndButtonForShiftLeftClick)
{
    buildAndPerformTest(NSEventTypeLeftMouseDown, NSEventModifierFlagShift, WebCore::MouseButton::Left, NSMenuTypeNone);
}

TEST(WebKitLegacy, MenuAndButtonForShiftRightClick)
{
    buildAndPerformTest(NSEventTypeRightMouseDown, NSEventModifierFlagShift, WebCore::MouseButton::Right, NSMenuTypeContextMenu);
}

TEST(WebKitLegacy, MenuAndButtonForShiftMiddleClick)
{
    buildAndPerformTest(NSEventTypeOtherMouseDown, NSEventModifierFlagShift, WebCore::MouseButton::Middle, NSMenuTypeNone);
}

TEST(WebKitLegacy, MenuAndButtonForCommandLeftClick)
{
    buildAndPerformTest(NSEventTypeLeftMouseDown, NSEventModifierFlagCommand, WebCore::MouseButton::Left, NSMenuTypeNone);
}

TEST(WebKitLegacy, MenuAndButtonForCommandRightClick)
{
    buildAndPerformTest(NSEventTypeRightMouseDown, NSEventModifierFlagCommand, WebCore::MouseButton::Right, NSMenuTypeContextMenu);
}

TEST(WebKitLegacy, MenuAndButtonForCommandMiddleClick)
{
    buildAndPerformTest(NSEventTypeOtherMouseDown, NSEventModifierFlagCommand, WebCore::MouseButton::Middle, NSMenuTypeNone);
}

TEST(WebKitLegacy, MenuAndButtonForAltLeftClick)
{
    buildAndPerformTest(NSEventTypeLeftMouseDown, NSEventModifierFlagOption, WebCore::MouseButton::Left, NSMenuTypeNone);
}

TEST(WebKitLegacy, MenuAndButtonForAltRightClick)
{
    buildAndPerformTest(NSEventTypeRightMouseDown, NSEventModifierFlagOption, WebCore::MouseButton::Right, NSMenuTypeContextMenu);
}

TEST(WebKitLegacy, MenuAndButtonForAltMiddleClick)
{
    buildAndPerformTest(NSEventTypeOtherMouseDown, NSEventModifierFlagOption, WebCore::MouseButton::Middle, NSMenuTypeNone);
}


} // namespace TestWebKitAPI
