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

#import "config.h"

#if PLATFORM(IOS) && USE(UICONTEXTMENU)

#import "TestContextMenuDriver.h"
#import "TestWKWebView.h"
#import "TestWKWebViewController.h"
#import "Utilities.h"
#import <WebKit/WKWebViewConfigurationPrivate.h>
#import <WebKit/WebKit.h>

static bool willPresentCalled;
static bool contextMenuRequested;
static RetainPtr<NSURL> simpleURL;

@interface TestContextMenuUIDelegate : NSObject <WKUIDelegate>
@end

@implementation TestContextMenuUIDelegate

- (void)webView:(WKWebView *)webView contextMenuConfigurationForElement:(WKContextMenuElementInfo *)elementInfo completionHandler:(void(^)(UIContextMenuConfiguration * _Nullable))completionHandler
{
    EXPECT_TRUE([elementInfo.linkURL.absoluteString isEqualToString:[simpleURL absoluteString]]);
    contextMenuRequested = true;
    completionHandler([UIContextMenuConfiguration configurationWithIdentifier:nil previewProvider:nil actionProvider:nil]);
}

- (void)webView:(WKWebView *)webView contextMenuWillPresentForElement:(WKContextMenuElementInfo *)elementInfo
{
    willPresentCalled = true;
}

- (void)webView:(WKWebView *)webView contextMenuForElement:(WKContextMenuElementInfo *)elementInfo willCommitWithAnimator:(id<UIContextMenuInteractionCommitAnimating>)animator
{
}

- (void)webView:(WKWebView *)webView contextMenuDidEndForElement:(WKContextMenuElementInfo *)elementInfo
{
}

@end

TEST(WebKit, ContextMenuBasic)
{
    auto window = adoptNS([[UIWindow alloc] initWithFrame:NSMakeRect(0, 0, 800, 600)]);
    auto driver = adoptNS([TestContextMenuDriver new]);
    auto uiDelegate = adoptNS([TestContextMenuUIDelegate new]);
    auto configuration = adoptNS([WKWebViewConfiguration new]);
    [configuration _setClickInteractionDriverForTesting:(id<_UIClickInteractionDriving>)driver.get()];
    auto webViewController = adoptNS([[TestWKWebViewController alloc] initWithFrame:CGRectMake(0, 0, 200, 200) configuration:configuration.get()]);
    TestWKWebView *webView = [webViewController webView];
    [window addSubview:webView];
    [webView setUIDelegate:uiDelegate.get()];

    simpleURL = [[NSBundle mainBundle] URLForResource:@"simple" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"];
    [webView synchronouslyLoadHTMLString:[NSString stringWithFormat:@"<a href='%@'>This is a link</a>", simpleURL.get()]];
    [driver begin:^(BOOL result) {
        if (result) {
            [driver clickDown];
            [driver clickUp];
            [driver end];
        }
    }];
    TestWebKitAPI::Util::run(&willPresentCalled);
}

#endif // PLATFORM(IOS) && USE(UICONTEXTMENU)
