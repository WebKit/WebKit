/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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

#import "HTTPServer.h"
#import "Test.h"
#import "TestNavigationDelegate.h"
#import "Utilities.h"
#import <WebKit/WKNavigationActionPrivate.h>
#import <WebKit/WKWebViewPrivate.h>
#import <WebKit/WebKit.h>

@interface ShouldOpenAppLinksTestNavigationDelegate: TestNavigationDelegate
@property (nonatomic) RetainPtr<WKNavigationAction> lastNavigationAction;
@end

@implementation ShouldOpenAppLinksTestNavigationDelegate

- (instancetype)init
{
    [super init];

    __unsafe_unretained ShouldOpenAppLinksTestNavigationDelegate *unretainedSelf = self;
    self.decidePolicyForNavigationAction = ^(WKNavigationAction *action, void (^decisionHandler)(WKNavigationActionPolicy)) {
        unretainedSelf.lastNavigationAction = action;
        decisionHandler(WKNavigationActionPolicyAllow);
    };

    self.webContentProcessDidTerminate = ^(WKWebView *webView, _WKProcessTerminationReason) {
        [webView reload];
    };

    return self;
}

@end

static TestWebKitAPI::HTTPServer shouldOpenAppLinksTestServer()
{
    return TestWebKitAPI::HTTPServer({
        { "/1"_s, { "<a href=\"2\" id=\"test_link\">Go to page 2</a>"_s } },
        { "/2"_s, { ""_s } },
    });
}

TEST(ShouldOpenAppLinks, DisallowAppLinksWhenReloadingAfterWebProcessCrash)
{
    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600)]);
    auto delegate = adoptNS([ShouldOpenAppLinksTestNavigationDelegate new]);
    [webView setNavigationDelegate:delegate.get()];

    auto server = shouldOpenAppLinksTestServer();

    NSURLRequest *request = [NSURLRequest requestWithURL:[NSURL URLWithString:[NSString stringWithFormat:@"http://127.0.0.1:%d/1", server.port()]]];
    [webView loadRequest:request];
    [delegate waitForDidFinishNavigation];

    [webView _killWebContentProcess];
    [delegate waitForDidFinishNavigation];

    EXPECT_NOT_NULL([delegate lastNavigationAction]);
    EXPECT_FALSE([[delegate lastNavigationAction] _shouldOpenAppLinks]);
}

TEST(ShouldOpenAppLinks, DisallowAppLinksWhenReloadingAfterWebProcessCrashAfterFollowingLink)
{
    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600)]);
    auto delegate = adoptNS([ShouldOpenAppLinksTestNavigationDelegate new]);
    [webView setNavigationDelegate:delegate.get()];

    auto server = shouldOpenAppLinksTestServer();

    NSURLRequest *request = [NSURLRequest requestWithURL:[NSURL URLWithString:[NSString stringWithFormat:@"http://127.0.0.1:%d/1", server.port()]]];
    [webView loadRequest:request];
    [delegate waitForDidFinishNavigation];

    [webView evaluateJavaScript:@"document.getElementById(\"test_link\").click()" completionHandler:nil];
    [delegate waitForDidFinishNavigation];

    [webView _killWebContentProcess];
    [delegate waitForDidFinishNavigation];

    EXPECT_NOT_NULL([delegate lastNavigationAction]);
    EXPECT_FALSE([[delegate lastNavigationAction] _shouldOpenAppLinks]);
}
