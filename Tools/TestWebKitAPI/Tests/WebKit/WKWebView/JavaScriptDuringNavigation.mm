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

#import "Helpers/PlatformUtilities.h"
#import "Helpers/Test.h"
#import <WebKit/WKWebView.h>
#import <wtf/RetainPtr.h>

static bool jsNavigationComplete;
static size_t alerts;
static bool receivedBothAlerts;
static RetainPtr<NSURL> jsNavigationFirstURL;
static RetainPtr<NSURL> jsNavigationSecondURL;

@interface JSNavigationDelegate : NSObject <WKNavigationDelegate, WKUIDelegate>
@end

@implementation JSNavigationDelegate

- (void)webView:(WKWebView *)webView didFinishNavigation:(WKNavigation *)navigation
{
    jsNavigationComplete = true;
}

- (void)webView:(WKWebView *)webView decidePolicyForNavigationAction:(WKNavigationAction *)navigationAction decisionHandler:(void (^)(WKNavigationActionPolicy))decisionHandler
{
    if ([navigationAction.request.URL.absoluteString isEqualToString:[jsNavigationSecondURL absoluteString]]) {
        [webView evaluateJavaScript:@"alert(document.location);" completionHandler:^(id, NSError *) {
            decisionHandler(WKNavigationActionPolicyAllow);
        }];
    } else
        decisionHandler(WKNavigationActionPolicyAllow);
}

- (void)webView:(WKWebView *)webView decidePolicyForNavigationResponse:(WKNavigationResponse *)navigationResponse decisionHandler:(void (^)(WKNavigationResponsePolicy))decisionHandler
{
    if ([navigationResponse.response.URL.absoluteString isEqualToString:[jsNavigationSecondURL absoluteString]]) {
        [webView evaluateJavaScript:@"alert(document.location);" completionHandler:^(id, NSError *) {
            decisionHandler(WKNavigationResponsePolicyAllow);
        }];
    } else
        decisionHandler(WKNavigationResponsePolicyAllow);
}

- (void)webView:(WKWebView *)webView runJavaScriptAlertPanelWithMessage:(NSString *)message initiatedByFrame:(WKFrameInfo *)frame completionHandler:(void (^)(void))completionHandler
{
    EXPECT_STREQ(message.UTF8String, [[jsNavigationFirstURL absoluteString] UTF8String]);
    if (++alerts == 2)
        receivedBothAlerts = true;
    completionHandler();
}

@end

TEST(WebKit, JavaScriptDuringNavigation)
{
    jsNavigationFirstURL = [NSBundle.test_resourcesBundle URLForResource:@"simple" withExtension:@"html"];
    jsNavigationSecondURL = [NSBundle.test_resourcesBundle URLForResource:@"simple2" withExtension:@"html"];
    
    RetainPtr webView = adoptNS([[WKWebView alloc] init]);
    RetainPtr delegate = adoptNS([[JSNavigationDelegate alloc] init]);
    [webView setNavigationDelegate:delegate.get()];
    [webView setUIDelegate:delegate.get()];

    [webView loadRequest:[NSURLRequest requestWithURL:jsNavigationFirstURL.get()]];
    TestWebKitAPI::Util::run(&jsNavigationComplete);

    [webView loadRequest:[NSURLRequest requestWithURL:jsNavigationSecondURL.get()]];
    TestWebKitAPI::Util::run(&receivedBothAlerts);
}
