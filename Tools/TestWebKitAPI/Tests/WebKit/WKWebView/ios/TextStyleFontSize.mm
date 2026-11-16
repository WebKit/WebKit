/*
 * Copyright (C) 2021-2022 Apple Inc. All rights reserved.
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

#import "HTTPServer.h"
#import "Helpers/cocoa/TestNavigationDelegate.h"
#import "Helpers/cocoa/TestWKWebView.h"
#import "InstanceMethodSwizzler.h"
#import "PlatformUtilities.h"
#import "UIKitSPIForTesting.h"
#import <WebCore/FontCacheCoreText.h>
#import <WebKit/WKPreferencesPrivate.h>
#import <WebKit/WKPreferencesRef.h>
#import <WebKit/WKWebViewPrivate.h>

#if PLATFORM(IOS_FAMILY)

#include <WebCore/RenderThemeIOS.h>

static auto contentSizeCategory = kCTFontContentSizeCategoryXXXL;

@interface TextStyleFontSizeWebView : TestWKWebView

@end

@implementation TextStyleFontSizeWebView

- (NSString *)_contentSizeCategory
{
    return static_cast<NSString *>(contentSizeCategory);
}

@end

TEST(TextStyleFontSize, Startup)
{
    RetainPtr descriptor = adoptCF(CTFontDescriptorCreateWithTextStyle(kCTUIFontTextStyleBody, contentSizeCategory, nullptr));
    RetainPtr sizeNumber = adoptCF(CTFontDescriptorCopyAttribute(descriptor.get(), kCTFontSizeAttribute));
    auto expected = static_cast<NSNumber *>(sizeNumber.get()).integerValue;

    static NSString *testMarkup = @"<html><head></head><body><div id='target' style='-webkit-text-size-adjust: none; font: -apple-system-body;'>Hello</div></body></html>";

    RetainPtr webView = adoptNS([[TextStyleFontSizeWebView alloc] initWithFrame:CGRectMake(0, 0, 960, 360)]);
    [webView synchronouslyLoadHTMLString:testMarkup];
    auto actual = [webView stringByEvaluatingJavaScript:@"parseInt(window.getComputedStyle(document.getElementById('target')).getPropertyValue('font-size'))"].integerValue;
    
    ASSERT_EQ(actual, expected);
}

TEST(TextStyleFontSize, AfterCrash)
{
    auto *originalContentSizeCategory = static_cast<NSString *>(WebCore::contentSizeCategory());
    auto *preferredContentSizeCategory = [[UIApplication sharedApplication] preferredContentSizeCategory];
    ASSERT_TRUE((!originalContentSizeCategory && !preferredContentSizeCategory) || [originalContentSizeCategory isEqualToString:preferredContentSizeCategory]);

    WebCore::setContentSizeCategory(contentSizeCategory);

    RetainPtr descriptor = adoptCF(CTFontDescriptorCreateWithTextStyle(kCTUIFontTextStyleBody, contentSizeCategory, nullptr));
    RetainPtr sizeNumber = adoptCF(CTFontDescriptorCopyAttribute(descriptor.get(), kCTFontSizeAttribute));
    auto expected = static_cast<NSNumber *>(sizeNumber.get()).integerValue;

    static NSString *testMarkup = @"<html><head></head><body><div id='target' style='-webkit-text-size-adjust: none; font: -apple-system-body;'>Hello</div></body></html>";

    RetainPtr webView = adoptNS([[TextStyleFontSizeWebView alloc] initWithFrame:CGRectMake(0, 0, 960, 360)]);
    [webView synchronouslyLoadHTMLString:testMarkup];
    [webView _killWebContentProcessAndResetState];
    [webView synchronouslyLoadHTMLString:testMarkup];
    auto actual = [webView stringByEvaluatingJavaScript:@"parseInt(window.getComputedStyle(document.getElementById('target')).getPropertyValue('font-size'))"].integerValue;
    
    ASSERT_EQ(actual, expected);

    WebCore::setContentSizeCategory(String());
}

TEST(TextStyleFontSize, ChangeDuringProvisionalNavigation)
{
    using namespace TestWebKitAPI;

    static NSString *testMarkup = @"<html><head></head><body><div id='target' style='-webkit-text-size-adjust: none; font: -apple-system-body;'>Hello</div></body></html>";
    HTTPServer server({
        { "/first"_s, { "first"_s } },
        { "/second"_s, { testMarkup } },
    }, HTTPServer::Protocol::HttpsProxy);

    auto originalContentSizeCategory = contentSizeCategory;
    contentSizeCategory = kCTFontContentSizeCategoryL;

    RetainPtr descriptor = adoptCF(CTFontDescriptorCreateWithTextStyle(kCTUIFontTextStyleBody, kCTFontContentSizeCategoryXXXL, nullptr));
    RetainPtr sizeNumber = adoptCF(CTFontDescriptorCopyAttribute(descriptor.get(), kCTFontSizeAttribute));
    auto expected = static_cast<NSNumber *>(sizeNumber.get()).integerValue;

    RetainPtr navigationDelegate = adoptNS([TestNavigationDelegate new]);
    [navigationDelegate allowAnyTLSCertificate];
    RetainPtr webView = adoptNS([[TextStyleFontSizeWebView alloc] initWithFrame:CGRectMake(0, 0, 960, 360) configuration:server.httpsProxyConfiguration()]);
    [webView setNavigationDelegate:navigationDelegate.get()];

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://domain1.com/first"]]];
    [navigationDelegate waitForDidFinishNavigation];
    auto firstProcessIdentifier = [webView _webProcessIdentifier];

    // Change the content size category after the provisional page for the cross-site navigation has been created but before it commits.
    navigationDelegate.get().decidePolicyForNavigationResponse = ^(WKNavigationResponse *, void (^decisionHandler)(WKNavigationResponsePolicy)) {
        contentSizeCategory = kCTFontContentSizeCategoryXXXL;
        [[NSNotificationCenter defaultCenter] postNotificationName:UIContentSizeCategoryDidChangeNotification object:nil];
        decisionHandler(WKNavigationResponsePolicyAllow);
    };

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://domain2.com/second"]]];
    [navigationDelegate waitForDidFinishNavigation];
    EXPECT_NE([webView _webProcessIdentifier], firstProcessIdentifier);

    auto actual = [webView stringByEvaluatingJavaScript:@"parseInt(window.getComputedStyle(document.getElementById('target')).getPropertyValue('font-size'))"].integerValue;
    EXPECT_EQ(actual, expected);

    contentSizeCategory = originalContentSizeCategory;
}

#endif // PLATFORM(IOS_FAMILY)
