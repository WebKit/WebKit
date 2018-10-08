/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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

#if PLATFORM(IOS)

#import "PlatformUtilities.h"
#import <UIKit/UIKit.h>
#import <wtf/RetainPtr.h>

static bool loadComplete;
static bool loadFailed;
static bool testComplete;
static RetainPtr<NSString> numberOfSetTimeoutCallbacks;

@interface SetTimeoutFunctionWebViewDelegate : NSObject <UIWebViewDelegate>
@end

@implementation SetTimeoutFunctionWebViewDelegate

IGNORE_WARNINGS_BEGIN("deprecated-implementations")
- (void)webViewDidFinishLoad:(UIWebView *)webView
IGNORE_WARNINGS_END
{
    loadComplete = true;
}

IGNORE_WARNINGS_BEGIN("deprecated-implementations")
- (void)webView:(UIWebView *)webView didFailLoadWithError:(NSError *)error
IGNORE_WARNINGS_END
{
    loadComplete = true;
    loadFailed = true;
}

IGNORE_WARNINGS_BEGIN("deprecated-implementations")
- (BOOL)webView:(UIWebView *)webView shouldStartLoadWithRequest:(NSURLRequest *)request navigationType:(UIWebViewNavigationType)navigationType
IGNORE_WARNINGS_END
{
    NSString *prefix = @"fired-";
    NSString *queryString = request.URL.query;
    if ([queryString hasPrefix:prefix]) {
        numberOfSetTimeoutCallbacks = [queryString substringFromIndex:prefix.length];
        testComplete = true;
        return NO;
    }
    return YES;
}

@end

namespace TestWebKitAPI {

TEST(WebKitLegacy, SetTimeoutFunction)
{
    RetainPtr<UIWindow> uiWindow = adoptNS([[UIWindow alloc] initWithFrame:NSMakeRect(0, 0, 800, 600)]);
    RetainPtr<UIWebView> uiWebView = adoptNS([[UIWebView alloc] initWithFrame:[uiWindow frame]]);
    [uiWindow addSubview:uiWebView.get()];

    RetainPtr<SetTimeoutFunctionWebViewDelegate> uiDelegate = adoptNS([[SetTimeoutFunctionWebViewDelegate alloc] init]);
    uiWebView.get().delegate = uiDelegate.get();

    RetainPtr<NSURL> url = [[NSBundle mainBundle] URLForResource:@"set-timeout-function" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"];
    [uiWebView loadRequest:[NSURLRequest requestWithURL:url.get()]];
    Util::run(&loadComplete);
    EXPECT_TRUE(loadComplete);
    EXPECT_FALSE(loadFailed);

    Util::run(&testComplete);
    EXPECT_WK_STREQ("3", numberOfSetTimeoutCallbacks.get());
}

}

#endif // PLATFORM(IOS)
