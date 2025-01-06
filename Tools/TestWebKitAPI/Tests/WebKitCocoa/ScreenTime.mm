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

#if ENABLE(SCREEN_TIME)

#import "TestWKWebView.h"
#import <ScreenTime/STWebpageController.h>
#import <WebKit/WKPreferencesPrivate.h>
#import <WebKit/WKWebViewPrivate.h>
#import <WebKit/_WKFeature.h>
#import <wtf/RetainPtr.h>

static void *blockedStateObserverChangeKVOContext = &blockedStateObserverChangeKVOContext;
static bool stateDidChange = false;

static RetainPtr<TestWKWebView> webViewForScreenTimeTests()
{
    RetainPtr configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    auto preferences = [configuration preferences];
    for (_WKFeature *feature in [WKPreferences _features]) {
        if ([feature.key isEqualToString:@"ScreenTimeEnabled"])
            [preferences _setEnabled:YES forFeature:feature];
    }
    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 400, 300) configuration:configuration.get()]);
    return webView;
}

@interface STWebpageController ()
@property (setter=setURLIsBlocked:) BOOL URLIsBlocked;
@end

@interface WKWebView (Internal)
- (STWebpageController *)_screenTimeWebpageController;
@end

@interface BlockedStateObserver : NSObject
- (instancetype)initWithWebView:(TestWKWebView *)webView;
@end

@implementation BlockedStateObserver {
    RetainPtr<TestWKWebView> _webView;
}

- (instancetype)initWithWebView:(TestWKWebView *)webView
{
    if (!(self = [super init]))
        return nil;

    _webView = webView;
    [_webView addObserver:self forKeyPath:@"_isBlockedByScreenTime" options:(NSKeyValueObservingOptionOld | NSKeyValueObservingOptionNew) context:&blockedStateObserverChangeKVOContext];
    return self;
}

- (void)observeValueForKeyPath:(NSString *)keyPath ofObject:(id)object change:(NSDictionary *)change context:(void *)context
{
    if (context == &blockedStateObserverChangeKVOContext) {
        stateDidChange = true;
        return;
    }

    [super observeValueForKeyPath:keyPath ofObject:object change:change context:context];
}
@end

TEST(ScreenTime, IsBlockedByScreenTimeTrue)
{
    RetainPtr webView = webViewForScreenTimeTests();
    [webView synchronouslyLoadHTMLString:@""];

    RetainPtr controller = [webView _screenTimeWebpageController];
    [controller setURLIsBlocked:YES];

    EXPECT_TRUE([webView _isBlockedByScreenTime]);
}

TEST(ScreenTime, IsBlockedByScreenTimeFalse)
{
    RetainPtr webView = webViewForScreenTimeTests();

    RetainPtr controller = [webView _screenTimeWebpageController];
    [controller setURLIsBlocked:NO];

    [webView synchronouslyLoadHTMLString:@""];

    EXPECT_FALSE([webView _isBlockedByScreenTime]);
}

TEST(ScreenTime, IsBlockedByScreenTimeMultiple)
{
    RetainPtr webView = webViewForScreenTimeTests();

    RetainPtr controller = [webView _screenTimeWebpageController];
    [controller setURLIsBlocked:YES];
    [controller setURLIsBlocked:NO];

    [webView synchronouslyLoadHTMLString:@""];

    EXPECT_FALSE([webView _isBlockedByScreenTime]);
}

TEST(ScreenTime, IsBlockedByScreenTimeKVO)
{
    RetainPtr webView = webViewForScreenTimeTests();
    auto observer = adoptNS([[BlockedStateObserver alloc] initWithWebView:webView.get()]);

    [webView synchronouslyLoadHTMLString:@""];

    RetainPtr controller = [webView _screenTimeWebpageController];
    [controller setURLIsBlocked:YES];

    TestWebKitAPI::Util::run(&stateDidChange);

    EXPECT_TRUE([webView _isBlockedByScreenTime]);

    stateDidChange = false;

    [controller setURLIsBlocked:NO];

    TestWebKitAPI::Util::run(&stateDidChange);

    EXPECT_FALSE([webView _isBlockedByScreenTime]);

    stateDidChange = false;

    [controller setURLIsBlocked:YES];

    TestWebKitAPI::Util::run(&stateDidChange);

    EXPECT_TRUE([webView _isBlockedByScreenTime]);
}

#endif
