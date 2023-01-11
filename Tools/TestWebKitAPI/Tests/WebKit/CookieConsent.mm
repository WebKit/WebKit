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

#import "TestWKWebView.h"
#import <WebKit/WKPreferencesPrivate.h>
#import <WebKit/WebKitPrivate.h>
#import <WebKit/_WKFeature.h>

@interface CookieConsentDelegate : NSObject<WKUIDelegatePrivate>
@property (nonatomic) BOOL decision;
@end

@implementation CookieConsentDelegate

- (void)_webView:(WKWebView *)webView requestCookieConsentWithMoreInfoHandler:(void (^)(void))moreInfoHandler decisionHandler:(void (^)(BOOL))decisionHandler
{
    decisionHandler(self.decision);
}

@end

namespace TestWebKitAPI {

static RetainPtr<TestWKWebView> createWebViewForTestingCookieConsent()
{
    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    for (_WKFeature *feature in WKPreferences._features) {
        if ([feature.key isEqualToString:@"CookieConsentAPIEnabled"]) {
            [[configuration preferences] _setEnabled:YES forFeature:feature];
            break;
        }
    }
    return adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 400, 400) configuration:configuration.get()]);
}

static void checkForString(TestWKWebView *webView, NSString *expectedString)
{
    auto contentsAsString = RetainPtr { webView.contentsAsString };
    BOOL containsString = [contentsAsString containsString:expectedString];
    EXPECT_TRUE(containsString);
    if (!containsString)
        NSLog(@"Expected: \"%@\" to contain: \"%@\"", contentsAsString.get(), expectedString);
}

TEST(CookieConsent, BasicDecisionHandler)
{
    auto webView = createWebViewForTestingCookieConsent();
    auto delegate = adoptNS([[CookieConsentDelegate alloc] init]);
    [delegate setDecision:YES];
    [webView setUIDelegate:delegate.get()];
    [webView synchronouslyLoadTestPageNamed:@"cookie-consent-basic"];
    [webView waitForNextPresentationUpdate];
    checkForString(webView.get(), @"returned: true");

    [delegate setDecision:NO];
    [webView _killWebContentProcessAndResetState];
    [webView synchronouslyLoadTestPageNamed:@"cookie-consent-basic"];
    [webView waitForNextPresentationUpdate];
    checkForString(webView.get(), @"returned: false");
}

TEST(CookieConsent, NotEnabled)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 400, 400)]);
    [webView synchronouslyLoadTestPageNamed:@"cookie-consent-basic"];
    [webView waitForNextPresentationUpdate];
    checkForString(webView.get(), @"not enabled");
}

TEST(CookieConsent, ThrowsExceptionWithoutDelegate)
{
    auto webView = createWebViewForTestingCookieConsent();
    [webView synchronouslyLoadTestPageNamed:@"cookie-consent-basic"];
    [webView waitForNextPresentationUpdate];
    checkForString(webView.get(), @"threw exception: NotSupportedError: The operation is not supported");
}

} // namespace TestWebKitAPI
