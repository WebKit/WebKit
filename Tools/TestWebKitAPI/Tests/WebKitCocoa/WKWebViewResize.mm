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
#import "TestWKWebView.h"
#import <WebKit/WKWebViewPrivateForTesting.h>
#import <wtf/RetainPtr.h>

#if HAVE(MAC_CATALYST_LIVE_RESIZE)

TEST(WKWebViewResize, DoesNotAssertInDeallocAfterChangingFrame)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600)]);
    EXPECT_FALSE([webView _hasResizeAssertion]);

    [webView setFrame:NSMakeRect(0, 0, 400, 300)];
    EXPECT_TRUE([webView _hasResizeAssertion]);

    bool didThrow = false;
    @try {
        webView = nil;
    } @catch (NSException *exception) {
        didThrow = true;
    }
    EXPECT_FALSE(didThrow);
}

TEST(WKWebViewResize, DoesNotAssertInDeallocAfterChangingBounds)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600)]);
    EXPECT_FALSE([webView _hasResizeAssertion]);

    [webView setBounds:NSMakeRect(0, 0, 400, 300)];
    EXPECT_TRUE([webView _hasResizeAssertion]);

    bool didThrow = false;
    @try {
        webView = nil;
    } @catch (NSException *exception) {
        didThrow = true;
    }
    EXPECT_FALSE(didThrow);
}

TEST(WKWebViewResize, RemovesAssertionsAfterMovingToWindow)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600)]);
    EXPECT_FALSE([webView _hasResizeAssertion]);

    [webView setFrame:NSMakeRect(0, 0, 400, 300)];
    EXPECT_TRUE([webView _hasResizeAssertion]);

    auto window = adoptNS([[UIWindow alloc] initWithFrame:NSMakeRect(0, 0, 320, 568)]);

    [window addSubview:webView.get()];
    EXPECT_FALSE([webView _hasResizeAssertion]);

    [webView setFrame:NSMakeRect(0, 0, 200, 150)];
    EXPECT_TRUE([webView _hasResizeAssertion]);

    [webView removeFromSuperview];
    EXPECT_FALSE([webView _hasResizeAssertion]);
}

#endif // HAVE(MAC_CATALYST_LIVE_RESIZE)
