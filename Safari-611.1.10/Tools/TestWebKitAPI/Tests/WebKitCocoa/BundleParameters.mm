/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
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
#import "WKWebViewConfigurationExtras.h"
#import <WebKit/WKProcessPoolPrivate.h>
#import <wtf/RetainPtr.h>

static bool isDone;

TEST(WebKit, BundleParameters)
{
    @autoreleasepool {
        NSString * const testPlugInClassName = @"BundleParametersPlugIn";
        auto configuration = retainPtr([WKWebViewConfiguration _test_configurationWithTestPlugInClassName:testPlugInClassName]);
        auto webView = adoptNS([[WKWebView alloc] initWithFrame:CGRectZero configuration:configuration.get()]);

        [webView evaluateJavaScript:TestWebKitAPI::Util::TestPlugInClassNameParameter completionHandler:^(id result, NSError *error) {
            EXPECT_TRUE([result isKindOfClass:[NSString class]]);
            EXPECT_WK_STREQ(result, testPlugInClassName);
            isDone = true;
        }];

        TestWebKitAPI::Util::run(&isDone);
        isDone = false;

        NSString * const testParameter1 = @"TestParameter1";
        [webView evaluateJavaScript:testParameter1 completionHandler:^(id result, NSError *error) {
            EXPECT_NULL(result);
            isDone = true;
        }];

        TestWebKitAPI::Util::run(&isDone);
        isDone = false;

        NSString * const testString = @"PASS";
        [[configuration processPool] _setObject:testString forBundleParameter:testParameter1];
        [webView evaluateJavaScript:testParameter1 completionHandler:^(id result, NSError *error) {
            EXPECT_TRUE([result isKindOfClass:[NSString class]]);
            EXPECT_WK_STREQ(result, testString);
            isDone = true;
        }];

        TestWebKitAPI::Util::run(&isDone);
        isDone = false;

        NSDictionary * const testDictionary = @{ @"result" : @"PASS" };
        [[configuration processPool] _setObject:testDictionary forBundleParameter:testParameter1];
        [webView evaluateJavaScript:testParameter1 completionHandler:^(id result, NSError *error) {
            EXPECT_TRUE([result isKindOfClass:[NSDictionary class]]);
            EXPECT_TRUE([result isEqualToDictionary:testDictionary]);
            isDone = true;
        }];

        TestWebKitAPI::Util::run(&isDone);
        isDone = false;

        [[configuration processPool] _setObject:nil forBundleParameter:testParameter1];
        [webView evaluateJavaScript:testParameter1 completionHandler:^(id result, NSError *error) {
            EXPECT_NULL(result);
            isDone = true;
        }];

        TestWebKitAPI::Util::run(&isDone);
        isDone = false;

        NSString * const testParameter2 = @"TestParameter2";
        NSDictionary * const testBundleParametersDictionary = @{ testParameter1 : testString, testParameter2 : testString };

        [[configuration processPool] _setObjectsForBundleParametersWithDictionary:testBundleParametersDictionary];
        [webView evaluateJavaScript:testParameter1 completionHandler:^(id result, NSError *error) {
            EXPECT_TRUE([result isKindOfClass:[NSString class]]);
            EXPECT_WK_STREQ(result, testString);
            isDone = true;
        }];

        TestWebKitAPI::Util::run(&isDone);
        isDone = false;

        [webView evaluateJavaScript:testParameter2 completionHandler:^(id result, NSError *error) {
            EXPECT_TRUE([result isKindOfClass:[NSString class]]);
            EXPECT_WK_STREQ(result, testString);
            isDone = true;
        }];

        TestWebKitAPI::Util::run(&isDone);
    }
}
