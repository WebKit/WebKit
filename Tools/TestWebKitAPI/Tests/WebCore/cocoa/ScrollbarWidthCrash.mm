/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
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
#import "TestInputDelegate.h"
#import "TestNavigationDelegate.h"
#import "TestWKWebView.h"
#import "UISideCompositingScope.h"

#import <WebKit/WKWebViewPrivate.h>

static bool loaded = false;
static bool webProcessCrashed = false;

@interface CrashNavigationDelegate : NSObject <WKNavigationDelegate>
@end

@implementation CrashNavigationDelegate

- (void)_webView:(WKWebView *)webView webContentProcessDidTerminateWithReason:(_WKProcessTerminationReason)reason
{
    webProcessCrashed = true;
}

- (void)webView:(WKWebView *)webView didFinishNavigation:(WKNavigation *)navigation
{
    loaded = true;
}

@end

namespace TestWebKitAPI {

static const CGFloat viewHeight = 500;

static NSString *htmlPage = @"<meta name='viewport' content='width=device-width, initial-scale=1'><html style='width: 100%; height: 5000px;'>";

TEST(ScrollbarWidthCrash, DynamicallyChangeScrollbarWidth)
{
    UISideCompositingScope scope { UISideCompositingState::Disabled };

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, viewHeight)]);

    RetainPtr delegate = adoptNS([[CrashNavigationDelegate alloc] init]);

    [webView synchronouslyLoadHTMLString:htmlPage];
    [webView setNavigationDelegate:delegate.get()];

    [webView stringByEvaluatingJavaScript:@"document.documentElement.style.scrollbarWidth = 'none'"];
    Util::waitFor([] {
        return webProcessCrashed || loaded;
    });
    EXPECT_FALSE(webProcessCrashed);
}


} // namespace TestWebKitAPI
