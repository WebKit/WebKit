/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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

#if WK_API_ENABLED && PLATFORM(MAC)

#import "OffscreenWindow.h"
#import "PlatformUtilities.h"
#import "TestWKWebView.h"
#import <Carbon/Carbon.h>
#import <WebKit/WKWebViewPrivate.h>
#import <wtf/RetainPtr.h>

enum class TabDirection : uint8_t {
    Forward,
    Backward,
};

@interface NSApplication ()
- (void)_setCurrentEvent:(NSEvent *)event;
@end

@interface FocusableView : NSView
@end

@implementation FocusableView

- (BOOL)canBecomeKeyView
{
    return YES;
}

@end

TEST(WebKit, TabOutOfWebView)
{
    RetainPtr<FocusableView> beforeView = adoptNS([[FocusableView alloc] initWithFrame:NSMakeRect(0, 200, 100, 100)]);
    RetainPtr<WKWebViewConfiguration> configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    RetainPtr<TestWKWebView> webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 100, 100, 100) configuration:configuration.get() addToWindow:NO]);
    RetainPtr<FocusableView> afterView = adoptNS([[FocusableView alloc] initWithFrame:NSMakeRect(0, 0, 100, 100)]);
    RetainPtr<OffscreenWindow> window = adoptNS([[OffscreenWindow alloc] initWithSize:CGSizeMake(800, 600)]);

    [[window contentView] addSubview:beforeView.get()];
    [[window contentView] addSubview:webView.get()];
    [[window contentView] addSubview:afterView.get()];

    [beforeView setNextKeyView:webView.get()];
    [webView setNextKeyView:afterView.get()];
    [afterView setNextKeyView:beforeView.get()];

    auto simulateTabKeyPress = ^(TabDirection direction) {
        NSString *characters;
        unsigned short keyCode = kVK_Tab;
        NSEventModifierFlags flags = 0;

        switch (direction) {
        case TabDirection::Forward:
            characters = @"\t";
            break;
        case TabDirection::Backward:
            characters = @"\x0019";
            flags = NSEventModifierFlagShift;
            break;
        }

        NSEvent *event = [NSEvent keyEventWithType:NSEventTypeKeyDown location:NSMakePoint(5, 5) modifierFlags:flags timestamp:GetCurrentEventTime() windowNumber:[window windowNumber] context:[NSGraphicsContext currentContext] characters:characters charactersIgnoringModifiers:characters isARepeat:NO keyCode:keyCode];

        [NSApp _setCurrentEvent:event];
        [[window firstResponder] keyDown:event];
        [NSApp _setCurrentEvent:nil];

        event = [NSEvent keyEventWithType:NSEventTypeKeyUp location:NSMakePoint(5, 5) modifierFlags:flags timestamp:GetCurrentEventTime() windowNumber:[window windowNumber] context:[NSGraphicsContext currentContext] characters:characters charactersIgnoringModifiers:characters isARepeat:NO keyCode:keyCode];

        [NSApp _setCurrentEvent:event];
        [[window firstResponder] keyUp:event];
        [NSApp _setCurrentEvent:nil];

        // Bounce through JavaScript so that asynchronous
        // web content focus changes have definitely taken effect.
        [webView stringByEvaluatingJavaScript:@""];
    };

    [webView synchronouslyLoadHTMLString:@"<input id='one'><input id='two'>"];

    // Focus the first view.
    [window makeFirstResponder:beforeView.get()];
    EXPECT_EQ([window firstResponder], beforeView.get());

    // Tab into the web view, which will focus the first input.
    simulateTabKeyPress(TabDirection::Forward);
    EXPECT_EQ([window firstResponder], webView.get());
    EXPECT_WK_STREQ("one", [webView stringByEvaluatingJavaScript:@"document.activeElement.id"]);

    // Tab from the first input to the second input.
    simulateTabKeyPress(TabDirection::Forward);
    EXPECT_EQ([window firstResponder], webView.get());
    EXPECT_WK_STREQ("two", [webView stringByEvaluatingJavaScript:@"document.activeElement.id"]);

    // Tab out of the web view.
    simulateTabKeyPress(TabDirection::Forward);
    EXPECT_EQ([window firstResponder], afterView.get());

    // Reverse-tab back into the web view.
    simulateTabKeyPress(TabDirection::Backward);
    EXPECT_EQ([window firstResponder], webView.get());
    EXPECT_WK_STREQ("two", [webView stringByEvaluatingJavaScript:@"document.activeElement.id"]);

    // Reverse-tab from the second input to the first input.
    simulateTabKeyPress(TabDirection::Backward);
    EXPECT_WK_STREQ("one", [webView stringByEvaluatingJavaScript:@"document.activeElement.id"]);

    // Reverse-tab back out of the web view to the first view.
    simulateTabKeyPress(TabDirection::Backward);
    EXPECT_EQ([window firstResponder], beforeView.get());
}

#endif
