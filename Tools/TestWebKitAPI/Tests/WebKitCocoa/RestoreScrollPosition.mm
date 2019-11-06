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

#import "PlatformUtilities.h"
#import "Test.h"
#import "TestWKWebView.h"
#import <WebKit/WKProcessPoolPrivate.h>
#import <WebKit/WKWebViewPrivate.h>
#import <WebKit/_WKProcessPoolConfiguration.h>

namespace TestWebKitAPI {

#if PLATFORM(IOS_FAMILY)

TEST(WebKit, RestoreScrollPositionDuringResize)
{
    auto processPoolConfiguration = adoptNS([[_WKProcessPoolConfiguration alloc] init]);
    processPoolConfiguration.get().pageCacheEnabled = NO;
    auto processPool = adoptNS([[WKProcessPool alloc] _initWithConfiguration:processPoolConfiguration.get()]);

    auto webViewConfiguration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [webViewConfiguration setProcessPool:processPool.get()];
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500) configuration:webViewConfiguration.get()]);
    [webView synchronouslyLoadTestPageNamed:@"simple-tall"];
    [webView stringByEvaluatingJavaScript:@"scrollTo(0, 1000)"];
    [webView waitForNextPresentationUpdate];

    CGPoint contentOffsetAfterScrolling = [webView scrollView].contentOffset;
    EXPECT_EQ(0, contentOffsetAfterScrolling.x);
    EXPECT_EQ(1000, contentOffsetAfterScrolling.y);

    [webView synchronouslyLoadTestPageNamed:@"simple"];
    [webView waitForNextPresentationUpdate];

    contentOffsetAfterScrolling = [webView scrollView].contentOffset;
    EXPECT_EQ(0, contentOffsetAfterScrolling.x);
    EXPECT_EQ(0, contentOffsetAfterScrolling.y);

    [webView _beginAnimatedResizeWithUpdates:^{
        [webView setFrame:CGRectMake(0, 0, [webView frame].size.width + 100, 500)];
    }];
    [webView synchronouslyGoBack];

    TestWebKitAPI::Util::sleep(0.5);
    [webView _endAnimatedResize];

    // Should restore the scroll position.
    int timeout = 0;
    do {
        if (timeout)
            TestWebKitAPI::Util::sleep(0.1);
    } while ([webView scrollView].contentOffset.y != 1000 && ++timeout <= 30);

    contentOffsetAfterScrolling = [webView scrollView].contentOffset;
    EXPECT_EQ(0, contentOffsetAfterScrolling.x);
    EXPECT_EQ(1000, contentOffsetAfterScrolling.y);
}

#endif

}
