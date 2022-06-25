/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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
#import "TestNavigationDelegate.h"
#import "TestProtocol.h"
#import "TestUIDelegate.h"
#import "TestWKWebView.h"
#import "WKWebViewConfigurationExtras.h"
#import <QuartzCore/QuartzCore.h>
#import <wtf/RetainPtr.h>

namespace TestWebKitAPI {

TEST(FocusWebView, DoNotFocusWebViewWhenUnparented)
{
    auto *configuration = [WKWebViewConfiguration _test_configurationWithTestPlugInClassName:@"WebProcessPlugInWithInternals" configureJSCForTesting:YES];
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 400, 400) configuration:configuration]);
    [webView synchronouslyLoadTestPageNamed:@"open-in-new-tab"];

    auto uiDelegate = adoptNS([TestUIDelegate new]);
    [webView setUIDelegate:uiDelegate.get()];

    __block bool calledFocusWebView = false;
    [uiDelegate setFocusWebView:^(WKWebView *viewToFocus) {
        EXPECT_EQ(viewToFocus, webView.get());
        calledFocusWebView = true;
    }];

    [webView objectByEvaluatingJavaScript:@"openNewTab()"];
    EXPECT_TRUE(calledFocusWebView);

    [webView removeFromSuperview];
    [CATransaction flush];
    __block bool doneUnparenting = false;
    dispatch_async(dispatch_get_main_queue(), ^{
        doneUnparenting = true;
    });
    Util::run(&doneUnparenting);

    calledFocusWebView = false;
    [webView objectByEvaluatingJavaScript:@"openNewTab()"];
    EXPECT_FALSE(calledFocusWebView);
}

} // namespace TestWebKitAPI

#endif // PLATFORM(MAC)
