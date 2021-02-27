/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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
#import "TestNavigationDelegate.h"
#import "TestWKWebView.h"
#import "WKWebViewConfigurationExtras.h"
#import <WebKit/WKWebViewConfigurationPrivate.h>
#import <WebKit/WKWebViewPrivateForTesting.h>
#import <WebKit/_WKAppHighlight.h>
#import <WebKit/_WKAppHighlightDelegate.h>
#import <wtf/RetainPtr.h>
#import <wtf/Vector.h>

#if ENABLE(APP_HIGHLIGHTS)

@interface AppHighlightDelegate : NSObject <_WKAppHighlightDelegate>
@property (nonatomic, copy) void (^storeAppHighlightCallback)(WKWebView *, _WKAppHighlight *, BOOL);
@end

@implementation AppHighlightDelegate
- (void)_webView:(WKWebView *)webView storeAppHighlight:(_WKAppHighlight *)highlight inNewGroup:(BOOL)inNewGroup
{
    if (_storeAppHighlightCallback)
        _storeAppHighlightCallback(webView, highlight, inNewGroup);
}
@end

namespace TestWebKitAPI {

TEST(AppHighlights, AppHighlightCreateAndRestore)
{
    WKWebViewConfiguration *configuration = [WKWebViewConfiguration _test_configurationWithTestPlugInClassName:@"WebProcessPlugInWithInternals" configureJSCForTesting:YES];
    auto delegate = adoptNS([[AppHighlightDelegate alloc] init]);
    auto webViewCreate = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500) configuration:configuration]);
    auto webViewRestore = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500) configuration:configuration]);
    [webViewCreate _setAppHighlightDelegate:delegate.get()];
    [webViewCreate synchronouslyLoadHTMLString:@"Test"];
    [webViewCreate stringByEvaluatingJavaScript:@"document.execCommand('SelectAll')"];
    __block bool finished = NO;
    [delegate setStoreAppHighlightCallback:^(WKWebView *delegateWebView, _WKAppHighlight *highlight, BOOL inNewGroup) {
        EXPECT_EQ(delegateWebView, webViewCreate.get());
        EXPECT_NOT_NULL(highlight);
        EXPECT_WK_STREQ(highlight.text, @"Test");
        EXPECT_NOT_NULL(highlight.highlight);
        
        [webViewRestore synchronouslyLoadHTMLString:@"Test"];
        [webViewRestore _restoreAppHighlights:@[ highlight.highlight ]];
        
        TestWebKitAPI::Util::waitForConditionWithLogging([&] () -> bool {
            return [webViewRestore stringByEvaluatingJavaScript:@"internals.numberOfAppHighlights()"].intValue == 1;
        }, 2, @"Expected Highlights to be populated.");
        
        finished = YES;
    }];
    [webViewCreate _addAppHighlight];
    TestWebKitAPI::Util::run(&finished);
}

}

#endif
