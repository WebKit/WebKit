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

#if PLATFORM(MAC)

#import "PlatformUtilities.h"
#import <WebKit/WKUIDelegatePrivate.h>
#import <WebKit/WKWebView.h>
#import <wtf/RetainPtr.h>

#if WK_API_ENABLED

static bool isDone;
static bool isUserInitiated;

@interface PrintFrameController : NSObject <WKNavigationDelegate, WKUIDelegate>
@end

@implementation PrintFrameController

- (void)webView:(WKWebView *)webView didFinishNavigation:(WKNavigation *)navigation
{
    isDone = true;
}

- (void)_webView:(WKWebView *)webView printFrame:(_WKFrameHandle *)frame userInitiated:(BOOL)userInitiated
{
    isUserInitiated = userInitiated;
    isDone = true;
}

@end

TEST(WebKit2, PrintFrame)
{
    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600)]);

    auto window = adoptNS([[NSWindow alloc] initWithContentRect:[webView frame] styleMask:NSBorderlessWindowMask backing:NSBackingStoreBuffered defer:YES]);
    [[window contentView] addSubview:webView.get()];

    auto controller = adoptNS([[PrintFrameController alloc] init]);
    [webView setNavigationDelegate:controller.get()];
    [webView setUIDelegate:controller.get()];

    [webView loadHTMLString:@"<body onclick=\"print()\">" baseURL:nil];
    TestWebKitAPI::Util::run(&isDone);
    isDone = false;

    NSPoint clickPoint = NSMakePoint(100, 100);

    [[webView hitTest:clickPoint] mouseDown:[NSEvent mouseEventWithType:NSLeftMouseDown location:clickPoint modifierFlags:0 timestamp:0 windowNumber:[window windowNumber] context:nil eventNumber:0 clickCount:1 pressure:1]];
    [[webView hitTest:clickPoint] mouseUp:[NSEvent mouseEventWithType:NSLeftMouseUp location:clickPoint modifierFlags:0 timestamp:0 windowNumber:[window windowNumber] context:nil eventNumber:0 clickCount:1 pressure:1]];
    TestWebKitAPI::Util::run(&isDone);
    isDone = false;

    ASSERT_TRUE(isUserInitiated);

    [webView evaluateJavaScript:@"print()" completionHandler:nil];

    TestWebKitAPI::Util::run(&isDone);
    isDone = false;

    ASSERT_FALSE(isUserInitiated);
}

#endif

#endif
