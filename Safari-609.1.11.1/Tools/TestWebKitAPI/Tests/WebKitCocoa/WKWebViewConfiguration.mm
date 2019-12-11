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

#import "config.h"

#import "PlatformUtilities.h"
#import <WebKit/WKWebView.h>
#import <WebKit/WKWebViewConfigurationPrivate.h>
#import <WebKit/WKWebsiteDataStore.h>
#import <wtf/Function.h>
#import <wtf/RetainPtr.h>

TEST(WebKit, InvalidConfiguration)
{
    auto shouldThrowExceptionWhenUsed = [](Function<void(WKWebViewConfiguration *)>&& modifier) {
        @try {
            auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
            modifier(configuration.get());
            auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
            EXPECT_TRUE(false);
        } @catch (NSException *exception) {
            EXPECT_WK_STREQ(NSInvalidArgumentException, exception.name);
        }
    };

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wnonnull"
    shouldThrowExceptionWhenUsed([](WKWebViewConfiguration *configuration) {
        [configuration setProcessPool:nil];
    });
    shouldThrowExceptionWhenUsed([](WKWebViewConfiguration *configuration) {
        [configuration setPreferences:nil];
    });
    shouldThrowExceptionWhenUsed([](WKWebViewConfiguration *configuration) {
        [configuration setUserContentController:nil];
    });
    shouldThrowExceptionWhenUsed([](WKWebViewConfiguration *configuration) {
        [configuration setWebsiteDataStore:nil];
    });
    shouldThrowExceptionWhenUsed([](WKWebViewConfiguration *configuration) {
        [configuration _setVisitedLinkStore:nil];
    });
#pragma clang diagnostic pop

    // Related WebViews cannot use different data stores.
    auto configurationForEphemeralView = adoptNS([[WKWebViewConfiguration alloc] init]);
    configurationForEphemeralView.get().websiteDataStore = [WKWebsiteDataStore nonPersistentDataStore];
    auto ephemeralWebView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configurationForEphemeralView.get()]);
    shouldThrowExceptionWhenUsed([&](WKWebViewConfiguration *configuration) {
        [configuration _setRelatedWebView:ephemeralWebView.get()];
    });
}

TEST(WebKit, ConfigurationGroupIdentifierIsCopied)
{
    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [configuration _setGroupIdentifier:@"TestGroupIdentifier"];

    auto configuationCopy = adoptNS([configuration copy]);
    EXPECT_STREQ([configuration _groupIdentifier].UTF8String, [configuationCopy _groupIdentifier].UTF8String);
}

TEST(WebKit, DefaultConfigurationEME)
{
    auto configuration = adoptNS([WKWebViewConfiguration new]);
    EXPECT_TRUE([configuration _legacyEncryptedMediaAPIEnabled]);
    auto webView = adoptNS([[WKWebView alloc] initWithFrame:CGRectMake(0, 0, 100, 100) configuration:configuration.get()]);
    [webView loadHTMLString:@"<html>hi</html>" baseURL:nil];
    __block bool done = false;
    [webView evaluateJavaScript:@"window.WebKitMediaKeys ? 'ENABLED' : 'DISABLED'" completionHandler:^(id result, NSError *){
        EXPECT_TRUE([result isEqualToString:@"ENABLED"]);
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
}
