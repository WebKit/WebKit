/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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

#if PLATFORM(MAC)

#import "Helpers/PlatformUtilities.h"
#import "Helpers/Test.h"
#import "Helpers/cocoa/TestWKWebView.h"
#import <WebKit/WKPreferencesPrivate.h>
#import <WebKit/WKProcessPoolPrivate.h>
#import <WebKit/WKWebViewConfigurationPrivate.h>
#import <WebKit/_WKFeature.h>
#import <notify.h>
#import <wtf/Scope.h>

namespace TestWebKitAPI {

TEST(TextScaleMeta, FollowsContentSizeCategory)
{
    NSString *contentSizeCategoryKey = @"UIPreferredContentSizeCategoryName";
    RetainPtr defaults = [NSUserDefaults standardUserDefaults];
    auto removeContentSizeCategory = makeScopeExit([&] {
        [defaults removeObjectForKey:contentSizeCategoryKey];
    });
    [defaults setObject:@"UICTContentSizeCategoryXXXL" forKey:contentSizeCategoryKey];

    RetainPtr configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [configuration setProcessPool:adoptNS([[WKProcessPool alloc] init]).get()];
    [configuration preferences]._contentSizeCategoryTextScaleEnabled = YES;
    for (_WKFeature *feature in [WKPreferences _features]) {
        if ([feature.key isEqualToString:@"CSSTextScaleMetaEnabled"])
            [[configuration preferences] _setEnabled:YES forFeature:feature];
    }

    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
    [webView synchronouslyLoadHTMLString:@"<meta name='text-scale' content='scale'>"];
    auto rootFontSize = [&] {
        return [webView stringByEvaluatingJavaScript:@"getComputedStyle(document.documentElement).fontSize"];
    };
    EXPECT_WK_STREQ("19px", rootFontSize());

    [defaults setObject:@"UICTContentSizeCategoryL" forKey:contentSizeCategoryKey];
    notify_post("ApplePreferredContentSizeCategoryChangedNotification");
    Util::waitForConditionWithLogging([&] {
        return [rootFontSize() isEqualToString:@"16px"];
    }, 5, @"Expected the root font size to go back to 16px after the content size category changed.");
}

} // namespace TestWebKitAPI

#endif // PLATFORM(MAC)
