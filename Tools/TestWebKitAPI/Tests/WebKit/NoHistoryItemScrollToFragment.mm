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

#include "config.h"

#if WK_API_ENABLED

#import "PlatformUtilities.h"
#import "Test.h"
#import "TestWKWebView.h"
#import <WebKit/WKUIDelegatePrivate.h>
#import <WebKit/WKWebViewConfiguration.h>
#import <WebKit/WKWebViewPrivate.h>
#import <WebKit/_WKProcessPoolConfiguration.h>

static bool testDone;

#if PLATFORM(MAC)
@interface DidScrollToFragmentDelegate : NSObject <WKUIDelegatePrivate>
@end

@implementation DidScrollToFragmentDelegate

- (void)_webViewDidScroll:(WKWebView *)webView
{
    testDone = true;
}

@end

#endif
#if PLATFORM(IOS_FAMILY)

@interface DidScrollToFragmentScrollViewDelegate : NSObject <UIScrollViewDelegate>

@end

@implementation DidScrollToFragmentScrollViewDelegate

- (void)scrollViewDidScroll:(UIScrollView *)scrollView
{
    // Ignore scrolls to 0,0 and from inset changes.
    if (scrollView.contentOffset.y > 300)
        testDone = true;
}

@end
#endif

namespace TestWebKitAPI {

TEST(WebKit, NoHistoryItemScrollToFragment)
{
    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    auto processPoolConfig = adoptNS([[_WKProcessPoolConfiguration alloc] init]);
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500) configuration:configuration.get() processPoolConfiguration:processPoolConfig.get()]);

#if PLATFORM(MAC)
    auto delegate = adoptNS([[DidScrollToFragmentDelegate alloc] init]);
    [webView setUIDelegate:delegate.get()];
#endif
#if PLATFORM(IOS_FAMILY)
    auto delegateForScrollView = adoptNS([[DidScrollToFragmentScrollViewDelegate alloc] init]);
    [webView scrollView].delegate = delegateForScrollView.get();
#endif

    NSURL* resourceURL = [[NSBundle mainBundle] URLForResource:@"scroll-to-anchor" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"];
    NSString *testFileContents = [NSString stringWithContentsOfURL:resourceURL encoding:NSUTF8StringEncoding error:nil];
    [webView synchronouslyLoadHTMLString:testFileContents];

    TestWebKitAPI::Util::run(&testDone);
}

} // namespace TestWebKitAPI

#endif // WK_API_ENABLED
