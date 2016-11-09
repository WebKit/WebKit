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
#import <WebCore/NSMenuSPI.h>
#import <WebCore/PlatformEventFactoryMac.h>
#import <wtf/AutodrainedPool.h>
#import <wtf/RetainPtr.h>
#import <wtf/mac/AppKitCompatibilityDeclarations.h>

namespace TestWebKitAPI {

static bool canCallMenuTypeForEvent()
{
    return [NSMenu respondsToSelector:@selector(menuTypeForEvent:)];
}

static void buildAndPerformTest(NSEventType buttonEvent, NSEventModifierFlags modifierFlags, WebCore::MouseButton expectedButton, NSMenuType expectedMenu)
{
    AutodrainedPool pool;
    RetainPtr<WebView> webView = adoptNS([[WebView alloc] init]);
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

TEST(WebKit1, MenuAndButtonForNormalLeftClick)
{
    buildAndPerformTest(NSEventTypeLeftMouseDown, 0, WebCore::LeftButton, NSMenuTypeNone);
}

TEST(WebKit1, MenuAndButtonForNormalRightClick)
{
    buildAndPerformTest(NSEventTypeRightMouseDown, 0, WebCore::RightButton, NSMenuTypeContextMenu);
}

TEST(WebKit1, MenuAndButtonForNormalMiddleClick)
{
    buildAndPerformTest(NSEventTypeOtherMouseDown, 0, WebCore::MiddleButton, NSMenuTypeNone);
}

TEST(WebKit1, MenuAndButtonForControlLeftClick)
{
    buildAndPerformTest(NSEventTypeLeftMouseDown, NSEventModifierFlagControl, WebCore::LeftButton, NSMenuTypeContextMenu);
}

TEST(WebKit1, MenuAndButtonForControlRightClick)
{
    buildAndPerformTest(NSEventTypeRightMouseDown, NSEventModifierFlagControl, WebCore::RightButton, NSMenuTypeContextMenu);
}

TEST(WebKit1, MenuAndButtonForControlMiddleClick)
{
    buildAndPerformTest(NSEventTypeOtherMouseDown, NSEventModifierFlagControl, WebCore::MiddleButton, NSMenuTypeNone);
}
    
TEST(WebKit1, MenuAndButtonForShiftLeftClick)
{
    buildAndPerformTest(NSEventTypeLeftMouseDown, NSEventModifierFlagShift, WebCore::LeftButton, NSMenuTypeNone);
}

TEST(WebKit1, MenuAndButtonForShiftRightClick)
{
    buildAndPerformTest(NSEventTypeRightMouseDown, NSEventModifierFlagShift, WebCore::RightButton, NSMenuTypeContextMenu);
}

TEST(WebKit1, MenuAndButtonForShiftMiddleClick)
{
    buildAndPerformTest(NSEventTypeOtherMouseDown, NSEventModifierFlagShift, WebCore::MiddleButton, NSMenuTypeNone);
}

TEST(WebKit1, MenuAndButtonForCommandLeftClick)
{
    buildAndPerformTest(NSEventTypeLeftMouseDown, NSEventModifierFlagCommand, WebCore::LeftButton, NSMenuTypeNone);
}

TEST(WebKit1, MenuAndButtonForCommandRightClick)
{
    buildAndPerformTest(NSEventTypeRightMouseDown, NSEventModifierFlagCommand, WebCore::RightButton, NSMenuTypeContextMenu);
}

TEST(WebKit1, MenuAndButtonForCommandMiddleClick)
{
    buildAndPerformTest(NSEventTypeOtherMouseDown, NSEventModifierFlagCommand, WebCore::MiddleButton, NSMenuTypeNone);
}

TEST(WebKit1, MenuAndButtonForAltLeftClick)
{
    buildAndPerformTest(NSEventTypeLeftMouseDown, NSEventModifierFlagOption, WebCore::LeftButton, NSMenuTypeNone);
}

TEST(WebKit1, MenuAndButtonForAltRightClick)
{
    buildAndPerformTest(NSEventTypeRightMouseDown, NSEventModifierFlagOption, WebCore::RightButton, NSMenuTypeContextMenu);
}

TEST(WebKit1, MenuAndButtonForAltMiddleClick)
{
    buildAndPerformTest(NSEventTypeOtherMouseDown, NSEventModifierFlagOption, WebCore::MiddleButton, NSMenuTypeNone);
}


} // namespace TestWebKitAPI
