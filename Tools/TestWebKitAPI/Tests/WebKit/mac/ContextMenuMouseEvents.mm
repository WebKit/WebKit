/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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
#import "TestCocoa.h"

#if PLATFORM(MAC)

#import "OffscreenWindow.h"
#import "TestNavigationDelegate.h"
#import "TestUIDelegate.h"
#import "TestWKWebView.h"
#import <Carbon/Carbon.h>
#import <wtf/BlockPtr.h>

namespace TestWebKitAPI {
    
static void runTest(NSEventModifierFlags flags, NSEventType mouseDownType, NSEventType mouseUpType)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 800, 600)]);
    [webView synchronouslyLoadTestPageNamed:@"context-menu-control-click"];

    auto navigationDelegate = adoptNS([[TestNavigationDelegate alloc] init]);
    [webView setNavigationDelegate:navigationDelegate.get()];

    __block bool navigationAttempted = false;
    [navigationDelegate setDecidePolicyForNavigationAction:^(WKNavigationAction *navigationAction, void (^decisionHandler)(WKNavigationActionPolicy)) {
        decisionHandler(WKNavigationActionPolicyCancel);
        navigationAttempted = true;
    }];

    auto uiDelegate = adoptNS([[TestUIDelegate alloc] init]);
    [webView setUIDelegate:uiDelegate.get()];

    __block bool done = false;
    __block void(^completionHandlerCopy)(NSMenu *);
    [uiDelegate setGetContextMenuFromProposedMenu:^(NSMenu *, _WKContextMenuElementInfo *, id <NSSecureCoding>, void (^completionHandler)(NSMenu *)) {
        dispatch_async(dispatch_get_main_queue(), ^{
            completionHandlerCopy = [completionHandler copy];
            done = true;
        });
    }];

    auto clickPoint = NSMakePoint(10, 590);
    [webView mouseDownAtPoint:clickPoint simulatePressure:NO withFlags:flags eventType:mouseDownType];
    Util::run(&done);

    [webView mouseUpAtPoint:clickPoint withFlags:flags eventType:mouseUpType];
    completionHandlerCopy(nil);

    EXPECT_FALSE([[webView objectByEvaluatingJavaScript:@"window.clickSeen"] boolValue]);
    EXPECT_FALSE([[webView objectByEvaluatingJavaScript:@"window.mouseupSeen"] boolValue]);
    EXPECT_TRUE([[webView objectByEvaluatingJavaScript:@"window.contextmenuSeen"] boolValue]);
    EXPECT_TRUE([[webView objectByEvaluatingJavaScript:@"window.mousedownSeen"] boolValue]);

    EXPECT_FALSE(navigationAttempted);
}

TEST(ContextMenuMouseEvents, ControlClick)
{
    runTest(NSEventModifierFlagControl, NSEventTypeLeftMouseDown, NSEventTypeLeftMouseUp);
}

TEST(ContextMenuMouseEvents, RightClick)
{
    runTest(0, NSEventTypeRightMouseDown, NSEventTypeRightMouseUp);
}

} // namespace TestWebKitAPI

#endif // PLATFORM(MAC)
