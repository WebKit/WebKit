/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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
#import <WebKit/WKWebViewPrivate.h>
#import <wtf/RetainPtr.h>

static bool finishedLoad = false;

@interface FirstResponderNavigationDelegate : NSObject <WKNavigationDelegate>
@end

@implementation FirstResponderNavigationDelegate
- (void)webView:(WKWebView *)webView didFinishNavigation:(WKNavigation *)navigation
{
    finishedLoad = true;
}
@end

namespace TestWebKitAPI {

TEST(WebKit, FirstResponderSuppression)
{
    RetainPtr<NSWindow> window = adoptNS([[NSWindow alloc] initWithContentRect:NSMakeRect(0, 0, 800, 600) styleMask:NSBorderlessWindowMask backing:NSBackingStoreBuffered defer:NO]);
    [window makeKeyAndOrderFront:nil];

    RetainPtr<FirstResponderNavigationDelegate> delegate = adoptNS([[FirstResponderNavigationDelegate alloc] init]);

    RetainPtr<WKWebView> webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600)]);
    [webView _setShouldSuppressFirstResponderChanges:YES];
    [webView setNavigationDelegate:delegate.get()];
    [[window contentView] addSubview:webView.get()];

    // Start off with the content view as first responder.
    [window makeFirstResponder:[window contentView]];
    EXPECT_EQ([window firstResponder], [window contentView]);

    // Ensure having an autofocused input field does not steal focus.
    NSString *testHTML = @"<!doctype html><html><body><input type=\"text\" autofocus /></body></html>";
    [webView loadHTMLString:testHTML baseURL:nil];
    TestWebKitAPI::Util::run(&finishedLoad);
    EXPECT_EQ([window firstResponder], [window contentView]);
    finishedLoad = false;

    [webView _setShouldSuppressFirstResponderChanges:NO];

    // Ensure having an autofocused input field does steal focus.
    [webView loadHTMLString:testHTML baseURL:nil];
    TestWebKitAPI::Util::run(&finishedLoad);
    EXPECT_NE([window firstResponder], [window contentView]);
}

} // namespace TestWebKitAPI

#endif // PLATFORM(MAC)
