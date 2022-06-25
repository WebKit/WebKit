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

#include "config.h"

#if ENABLE(INPUT_TYPE_DATE) && PLATFORM(MAC)

#import "TestWKWebView.h"
#import <WebKit/WKPreferencesPrivate.h>
#import <WebKit/_WKInternalDebugFeature.h>
#import <wtf/RetainPtr.h>

static RetainPtr<TestWKWebView> createWebViewForTest()
{
    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    for (_WKInternalDebugFeature *feature in [WKPreferences _internalDebugFeatures]) {
        if ([feature.key isEqualToString:@"InputTypeDateEnabled"]) {
            [[configuration preferences] _setEnabled:YES forInternalDebugFeature:feature];
            break;
        }
    }

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 400, 400) configuration:configuration.get()]);
    return webView;
}

TEST(DateInputTests, IgnoresUserEditsToLocale)
{
    NSString *dateInputHTMLString = @"<input type='date' value='2020-09-03'>";
    NSString *retrieveOffsetWidthJavaScript = @"document.getElementsByTagName(\"input\")[0].offsetWidth";

    NSString *domain = @"NSGlobalDomain";
    NSString *preferencesKey = @"AppleICUDateFormatStrings";
    NSString *customDateFormat = @"M///d///yy";

    // Set custom date format.
    system([NSString stringWithFormat:@"defaults write %@ %@ -dict-add %@ %@", domain, preferencesKey, @"1", customDateFormat].UTF8String);

    auto webView = createWebViewForTest();
    [webView synchronouslyLoadHTMLString:dateInputHTMLString];

    int widthForCustomFormat = [[webView stringByEvaluatingJavaScript:retrieveOffsetWidthJavaScript] intValue];

    // Remove custom date format.
    system([NSString stringWithFormat:@"defaults delete %@ %@", domain, preferencesKey].UTF8String);

    // Create a new web view, since locale is cached on the old one.
    webView = createWebViewForTest();
    [webView synchronouslyLoadHTMLString:dateInputHTMLString];

    int widthForDefaultFormat = [[webView stringByEvaluatingJavaScript:retrieveOffsetWidthJavaScript] intValue];

    EXPECT_EQ(widthForCustomFormat, widthForDefaultFormat);
}

#endif // ENABLE(INPUT_TYPE_DATE) && PLATFORM(MAC)
