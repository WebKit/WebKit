/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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

#import "Helpers/Utilities.h"
#import "Helpers/cocoa/TestNavigationDelegate.h"
#import <WebKit/WKWebViewConfigurationPrivate.h>
#import <WebKit/WKWebViewPrivate.h>
#import <wtf/BlockPtr.h>
#import <wtf/RetainPtr.h>

TEST(WebKit, DoAfterNextPresentationUpdateOnRelatedOffscreenView)
{
    RetainPtr webView1 = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 100, 100)]);
    [webView1 loadHTMLString:@"<body>hello world!</body>" baseURL:nil];
    [webView1 _test_waitForDidFinishNavigation];

    RetainPtr configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [configuration _setRelatedWebView:webView1.get()];
    RetainPtr webView2 = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 100, 100) configuration:configuration.get()]);
    [webView2 loadHTMLString:@"<body>related</body>" baseURL:nil];
    [webView2 _test_waitForDidFinishNavigation];

    __block bool firstUpdateFired = false;
    __block bool secondUpdateFired = false;
    [webView1 _doAfterNextPresentationUpdate:^{
        firstUpdateFired = true;
        [webView2 _doAfterNextPresentationUpdate:^{
            secondUpdateFired = true;
        }];
    }];

    EXPECT_TRUE(TestWebKitAPI::Util::runFor(&secondUpdateFired, 5_s));
    EXPECT_TRUE(firstUpdateFired);
    EXPECT_TRUE(secondUpdateFired);
}
