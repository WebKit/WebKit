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

#if WK_HAVE_C_SPI

#import "PlatformUtilities.h"
#import "TestWKWebView.h"
#import <WebKit/PreferenceObserver.h>

static bool done = false;

@interface WKTestPreferenceObserver : WKPreferenceObserver
- (void)preferenceDidChange:(NSString *)domain key:(NSString *)key encodedValue:(NSString *)encodedValue;
@end

@implementation WKTestPreferenceObserver
- (void)preferenceDidChange:(NSString *)domain key:(NSString *)key encodedValue:(NSString *)encodedValue
{
    done = true;
}
@end

TEST(WebKit, PreferenceObserver)
{
    done = false;

    CFPreferencesSetAppValue(CFSTR("testkey"), CFSTR("1"), CFSTR("com.apple.coremedia"));

    auto observer = adoptNS([[WKTestPreferenceObserver alloc] init]);

    CFPreferencesSetAppValue(CFSTR("testkey"), CFSTR("2"), CFSTR("com.apple.coremedia"));

    TestWebKitAPI::Util::run(&done);
}

TEST(WebKit, PreferenceObserverArray)
{
    done = false;

    NSArray *array = @[@1, @2, @3];

    auto userDefaults = adoptNS([[NSUserDefaults alloc] initWithSuiteName:@"com.apple.coremedia"]);
    [userDefaults.get() setObject:array forKey:@"testkey"];

    auto observer = adoptNS([[WKTestPreferenceObserver alloc] init]);

    NSArray *changedArray = @[@3, @2, @1];
    [userDefaults.get() setObject:changedArray forKey:@"testkey"];

    TestWebKitAPI::Util::run(&done);
}

TEST(WebKit, PreferenceChanges)
{
    CFPreferencesSetAppValue(CFSTR("testkey"), CFSTR("1"), CFSTR("com.apple.coremedia"));

    EXPECT_EQ(1, CFPreferencesGetAppIntegerValue(CFSTR("testkey"), CFSTR("com.apple.coremedia"), nullptr));

    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    WKRetainPtr<WKContextRef> context = adoptWK(TestWebKitAPI::Util::createContextForInjectedBundleTest("InternalsInjectedBundleTest"));
    configuration.get().processPool = (WKProcessPool *)context.get();
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 300, 300) configuration:configuration.get() addToWindow:YES]);

    auto preferenceValue = [&] {
        return [webView stringByEvaluatingJavaScript:@"window.internals.readPreferenceInteger(\"com.apple.coremedia\", \"testkey\")"].intValue;
    };

    EXPECT_EQ(preferenceValue(), 1);

    CFPreferencesSetAppValue(CFSTR("testkey"), CFSTR("2"), CFSTR("com.apple.coremedia"));

    EXPECT_EQ(preferenceValue(), 2);
}

TEST(WebKit, PreferenceChangesArray)
{
    NSArray *array = @[@1, @2, @3];

    auto userDefaults = adoptNS([[NSUserDefaults alloc] initWithSuiteName:@"com.apple.coremedia"]);
    [userDefaults.get() setObject:array forKey:@"testkey"];

    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    WKRetainPtr<WKContextRef> context = adoptWK(TestWebKitAPI::Util::createContextForInjectedBundleTest("InternalsInjectedBundleTest"));
    configuration.get().processPool = (WKProcessPool *)context.get();
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 300, 300) configuration:configuration.get() addToWindow:YES]);

    auto preferenceValue = [&] {
        return [webView stringByEvaluatingJavaScript:@"window.internals.encodedPreferenceValue(\"com.apple.coremedia\", \"testkey\")"];
    };

    preferenceValue();

    NSArray *changedArray = @[@3, @2, @1];
    [userDefaults.get() setObject:changedArray forKey:@"testkey"];

    auto encodedString = preferenceValue();
    auto encodedData = adoptNS([[NSData alloc] initWithBase64EncodedString:encodedString options:0]);
    ASSERT_TRUE(encodedData);
    NSError *err = nil;
    auto object = retainPtr([NSKeyedUnarchiver unarchivedObjectOfClass:[NSObject class] fromData:encodedData.get() error:&err]);
    ASSERT_TRUE(!err);
    ASSERT_TRUE(object);
    ASSERT_TRUE([object isEqual:changedArray]);
}

#endif // WK_HAVE_C_SPI
