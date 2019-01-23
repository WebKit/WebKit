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
#import "PlatformWebView.h"
#import <WebKit/WKProcessPoolPrivate.h>
#import <WebKit/WKWebViewPrivate.h>
#import <WebKit/_WKProcessPoolConfiguration.h>
#import <wtf/RetainPtr.h>

static bool didFinishLoad { false };
static bool didBecomeUnresponsive { false };

@interface ResponsivenessTimerDelegate : NSObject <WKNavigationDelegate>
@end
    
@implementation ResponsivenessTimerDelegate

- (void)webView:(WKWebView *)webView didFinishNavigation:(WKNavigation *)navigation
{
    didFinishLoad = true;
}

- (void)_webViewWebProcessDidBecomeUnresponsive:(WKWebView *)webView
{
    didBecomeUnresponsive = true;
}

@end

namespace TestWebKitAPI {

TEST(WebKit, ResponsivenessTimerShouldNotFireAfterTearDown)
{
    auto processPoolConfiguration = adoptNS([_WKProcessPoolConfiguration new]);
    auto processPool = adoptNS([[WKProcessPool alloc] _initWithConfiguration:processPoolConfiguration.get()]);
    auto delegate = adoptNS([ResponsivenessTimerDelegate new]);

    auto configuration = adoptNS([WKWebViewConfiguration new]);
    [configuration setProcessPool:processPool.get()];
    auto webView1 = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
    [webView1 setNavigationDelegate:delegate.get()];
    NSURLRequest *request = [NSURLRequest requestWithURL:[[NSBundle mainBundle] URLForResource:@"simple" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"]];
    [webView1 loadRequest:request];
    Util::run(&didFinishLoad);

    EXPECT_FALSE(didBecomeUnresponsive);

    auto webView2 = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
    [webView2 setNavigationDelegate:delegate.get()];

    didFinishLoad = false;
    [webView2 loadRequest:request];
    Util::run(&didFinishLoad);

    EXPECT_FALSE(didBecomeUnresponsive);

    // Call stopLoading() and close() on the first page in quick succession.
    [webView1 stopLoading];
    [webView1 _close];

    // We need to wait here because it takes 3 seconds for a process to be recognized as unresponsive.
    Util::sleep(4);

    // We should not report the second page sharing the same process as unresponsive.
    EXPECT_FALSE(didBecomeUnresponsive);
}

} // namespace TestWebKitAPI

#endif
