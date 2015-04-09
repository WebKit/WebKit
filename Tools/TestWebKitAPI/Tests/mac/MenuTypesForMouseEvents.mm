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
    
    auto pme = WebCore::PlatformEventFactory::createPlatformMouseEvent(event, webView.get());
    
    EXPECT_EQ(expectedButton, pme.button());
    EXPECT_TRUE(!modifierFlags || pme.modifierFlags() & modifierFlags);
    EXPECT_EQ(expectedMenu, pme.menuTypeForEvent());
    if (canCallMenuTypeForEvent())
        EXPECT_EQ(expectedMenu, [NSMenu menuTypeForEvent:event]);
}

TEST(WebKit1, MenuAndButtonForNormalLeftClick)
{
    buildAndPerformTest(NSLeftMouseDown, 0, WebCore::LeftButton, NSMenuTypeNone);
}

TEST(WebKit1, MenuAndButtonForNormalRightClick)
{
    buildAndPerformTest(NSRightMouseDown, 0, WebCore::RightButton, NSMenuTypeContextMenu);
}

TEST(WebKit1, MenuAndButtonForNormalMiddleClick)
{
    buildAndPerformTest(NSOtherMouseDown, 0, WebCore::MiddleButton, NSMenuTypeNone);
}

TEST(WebKit1, MenuAndButtonForControlLeftClick)
{
    buildAndPerformTest(NSLeftMouseDown, NSControlKeyMask, WebCore::LeftButton, NSMenuTypeContextMenu);
}

TEST(WebKit1, MenuAndButtonForControlRightClick)
{
    buildAndPerformTest(NSRightMouseDown, NSControlKeyMask, WebCore::RightButton, NSMenuTypeContextMenu);
}

TEST(WebKit1, MenuAndButtonForControlMiddleClick)
{
    buildAndPerformTest(NSOtherMouseDown, NSControlKeyMask, WebCore::MiddleButton, NSMenuTypeNone);
}
    
TEST(WebKit1, MenuAndButtonForShiftLeftClick)
{
    buildAndPerformTest(NSLeftMouseDown, NSShiftKeyMask, WebCore::LeftButton, NSMenuTypeNone);
}

TEST(WebKit1, MenuAndButtonForShiftRightClick)
{
    buildAndPerformTest(NSRightMouseDown, NSShiftKeyMask, WebCore::RightButton, NSMenuTypeContextMenu);
}

TEST(WebKit1, MenuAndButtonForShiftMiddleClick)
{
    buildAndPerformTest(NSOtherMouseDown, NSShiftKeyMask, WebCore::MiddleButton, NSMenuTypeNone);
}

TEST(WebKit1, MenuAndButtonForCommandLeftClick)
{
    buildAndPerformTest(NSLeftMouseDown, NSCommandKeyMask, WebCore::LeftButton, NSMenuTypeNone);
}

TEST(WebKit1, MenuAndButtonForCommandRightClick)
{
    buildAndPerformTest(NSRightMouseDown, NSCommandKeyMask, WebCore::RightButton, NSMenuTypeContextMenu);
}

TEST(WebKit1, MenuAndButtonForCommandMiddleClick)
{
    buildAndPerformTest(NSOtherMouseDown, NSCommandKeyMask, WebCore::MiddleButton, NSMenuTypeNone);
}

TEST(WebKit1, MenuAndButtonForAltLeftClick)
{
    buildAndPerformTest(NSLeftMouseDown, NSAlternateKeyMask, WebCore::LeftButton, NSMenuTypeNone);
}

TEST(WebKit1, MenuAndButtonForAltRightClick)
{
    buildAndPerformTest(NSRightMouseDown, NSAlternateKeyMask, WebCore::RightButton, NSMenuTypeContextMenu);
}

TEST(WebKit1, MenuAndButtonForAltMiddleClick)
{
    buildAndPerformTest(NSOtherMouseDown, NSAlternateKeyMask, WebCore::MiddleButton, NSMenuTypeNone);
}


} // namespace TestWebKitAPI
