/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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

#import "PlatformUtilities.h"
#import <WebKit/WebKitLegacy.h>
#import <WebKit/WKWebView.h>
#import <wtf/RetainPtr.h>

static bool didFinishTest;
const static NSURL *targetUrl = [[NSURL alloc] initWithString:@"http://www.example.com/"];
const static unsigned expectedModifierFlags = 0;
const static int expectedButtonNumber = -1;

const static int expectedWKButtonNumber = 0; // unlike DOM spec, 0 is the value for no button in Cocoa.

@interface NavigationActionDelegate : NSObject <WKNavigationDelegate>
@end

@implementation NavigationActionDelegate

- (void)webView:(WKWebView *)webView decidePolicyForNavigationAction:(WKNavigationAction *)navigationAction decisionHandler:(void (^)(WKNavigationActionPolicy))decisionHandler
{
    if ([navigationAction.request.URL isEqual:targetUrl]) {
        EXPECT_EQ(navigationAction.modifierFlags, expectedModifierFlags);
        EXPECT_EQ(navigationAction.buttonNumber, expectedWKButtonNumber);
        didFinishTest = true;
    }

    decisionHandler(WKNavigationActionPolicyAllow);
}

@end

@interface WebPolicyActionDelegate : NSObject <WebPolicyDelegate>
@end

@implementation WebPolicyActionDelegate

- (void)webView:(WebView *)webView decidePolicyForNavigationAction:(NSDictionary *)actionInformation request:(NSURLRequest *)request frame:(WebFrame *)frame decisionListener:(id<WebPolicyDecisionListener>)listener
{
    if ([request.URL isEqual:targetUrl]) {
        EXPECT_EQ([actionInformation[WebActionModifierFlagsKey] unsignedIntValue], expectedModifierFlags);
        EXPECT_EQ([actionInformation[WebActionButtonKey] intValue], expectedButtonNumber);
        didFinishTest = true;
    }

    [listener use];
}

@end

namespace TestWebKitAPI {

TEST(WebKit, IsNavigationActionTrusted)
{
    @autoreleasepool {
        RetainPtr<WKWebView> webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600)]);

        RetainPtr<NavigationActionDelegate> delegate = adoptNS([[NavigationActionDelegate alloc] init]);
        [webView setNavigationDelegate:delegate.get()];

        NSURLRequest *request = [NSURLRequest requestWithURL:[[NSBundle mainBundle] URLForResource:@"IsNavigationActionTrusted" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"]];
        [webView loadRequest:request];

        didFinishTest = false;
        Util::run(&didFinishTest);
    }
}

TEST(WebKitLegacy, IsNavigationActionTrusted)
{
    @autoreleasepool {
        RetainPtr<WebView> webView = adoptNS([[WebView alloc] init]);

        RetainPtr<WebPolicyActionDelegate> delegate = adoptNS([[WebPolicyActionDelegate alloc] init]);
        [webView setPolicyDelegate:delegate.get()];
        [[webView mainFrame] loadRequest:[NSURLRequest requestWithURL:[[NSBundle mainBundle] URLForResource:@"IsNavigationActionTrusted" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"]]];

        didFinishTest = false;
        Util::run(&didFinishTest);
    }
}

} // namespace TestWebKitAPI
