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

#import "config.h"

// FIXME: These tests touch the user's *actual* global and AVFoundation defaults domains.
// We should find a way to get the same test coverage using volatile domains or swizzling or something.

#if WK_HAVE_C_SPI

#import "PlatformUtilities.h"
#import "TestWKWebView.h"
#import <WebKit/PreferenceObserver.h>

#import <wtf/ObjCRuntimeExtras.h>

static bool receivedPreferenceNotification = false;

@interface WKTestPreferenceObserver : WKPreferenceObserver
- (void)preferenceDidChange:(NSString *)domain key:(NSString *)key encodedValue:(NSString *)encodedValue;
@end

@implementation WKTestPreferenceObserver
- (void)preferenceDidChange:(NSString *)domain key:(NSString *)key encodedValue:(NSString *)encodedValue
{
    receivedPreferenceNotification = true;
    [super preferenceDidChange:domain key:key encodedValue:encodedValue];
}
@end

// FIXME: Remove this pragma once webkit.org/b/221848 is resolved.
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-const-variable"
static const CFStringRef globalDomain = kCFPreferencesAnyApplication;
#pragma clang diagnostic pop
static const CFStringRef testDomain = CFSTR("com.apple.avfoundation");

#define TEST_KEY() ((CFStringRef)[NSString stringWithFormat:@"TestWebKitAPI_TestKey_%s", ::testing::UnitTest::GetInstance()->current_test_info()->name()])
#define CLEAR_DEFAULTS() { \
    CFPreferencesSetValue(TEST_KEY(), nil, kCFPreferencesAnyApplication, kCFPreferencesCurrentUser, kCFPreferencesCurrentHost); \
    CFPreferencesSetAppValue(TEST_KEY(), nil, testDomain); \
}

static constexpr unsigned preferenceQueryMaxCount = 10;
static constexpr float preferenceQuerySleepTime = 1;

static void waitForPreferenceSynchronization()
{
    __block bool didSynchronize = false;
    dispatch_async(dispatch_get_main_queue(), ^{
        didSynchronize = true;
    });
    TestWebKitAPI::Util::run(&didSynchronize);
}

TEST(WebKit, PreferenceObserver)
{
    CLEAR_DEFAULTS();

    receivedPreferenceNotification = false;

    CFPreferencesSetAppValue(TEST_KEY(), CFSTR("1"), testDomain);

    auto observer = adoptNS([[WKTestPreferenceObserver alloc] init]);

    CFPreferencesSetAppValue(TEST_KEY(), CFSTR("2"), testDomain);

    TestWebKitAPI::Util::run(&receivedPreferenceNotification);

    CLEAR_DEFAULTS();
}

TEST(WebKit, PreferenceObserverArray)
{
    CLEAR_DEFAULTS();

    receivedPreferenceNotification = false;

    NSArray *array = @[@1, @2, @3];

    auto userDefaults = adoptNS([[NSUserDefaults alloc] initWithSuiteName:(NSString *)testDomain]);
    [userDefaults setObject:array forKey:(NSString *)TEST_KEY()];

    auto observer = adoptNS([[WKTestPreferenceObserver alloc] init]);

    NSArray *changedArray = @[@3, @2, @1];
    [userDefaults setObject:changedArray forKey:(NSString *)TEST_KEY()];

    TestWebKitAPI::Util::run(&receivedPreferenceNotification);

    CLEAR_DEFAULTS();
}

TEST(WebKit, PreferenceChanges)
{
    CLEAR_DEFAULTS();

    CFPreferencesSetAppValue(TEST_KEY(), CFSTR("0"), testDomain);

    auto observer = adoptNS([[WKTestPreferenceObserver alloc] init]);
    
    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    WKRetainPtr<WKContextRef> context = adoptWK(TestWebKitAPI::Util::createContextForInjectedBundleTest("InternalsInjectedBundleTest"));
    [configuration setProcessPool:(WKProcessPool *)context.get()];
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 300, 300) configuration:configuration.get() addToWindow:YES]);
    [webView synchronouslyLoadTestPageNamed:@"simple"];

    receivedPreferenceNotification = false;

    CFPreferencesSetAppValue(TEST_KEY(), CFSTR("1"), testDomain);

    EXPECT_EQ(1, CFPreferencesGetAppIntegerValue(TEST_KEY(), testDomain, nullptr));

    TestWebKitAPI::Util::run(&receivedPreferenceNotification);

    auto preferenceValue = [&] {
        waitForPreferenceSynchronization();
        NSString *js = [NSString stringWithFormat:@"window.internals.readPreferenceInteger(\"%@\",\"%@\")", (NSString *)testDomain, (NSString *)TEST_KEY()];
        return [webView stringByEvaluatingJavaScript:js].intValue;
    };

    EXPECT_EQ(preferenceValue(), 1);

    receivedPreferenceNotification = false;

    CFPreferencesSetAppValue(TEST_KEY(), CFSTR("2"), testDomain);

    TestWebKitAPI::Util::run(&receivedPreferenceNotification);

    EXPECT_EQ(preferenceValue(), 2);

    CLEAR_DEFAULTS();
}

TEST(WebKit, GlobalPreferenceChangesUsingDefaultsWrite)
{
    CLEAR_DEFAULTS();

    system([NSString stringWithFormat:@"defaults write %@ %@ 0", (__bridge id)globalDomain, (__bridge id)TEST_KEY()].UTF8String);
    
    auto observer = adoptNS([[WKTestPreferenceObserver alloc] init]);
    
    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    WKRetainPtr<WKContextRef> context = adoptWK(TestWebKitAPI::Util::createContextForInjectedBundleTest("InternalsInjectedBundleTest"));
    [configuration setProcessPool:(WKProcessPool *)context.get()];
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 300, 300) configuration:configuration.get() addToWindow:YES]);
    [webView synchronouslyLoadTestPageNamed:@"simple"];

    receivedPreferenceNotification = false;

    system([NSString stringWithFormat:@"defaults write %@ %@ 1", (__bridge id)globalDomain, (__bridge id)TEST_KEY()].UTF8String);

    EXPECT_EQ(1, CFPreferencesGetAppIntegerValue(TEST_KEY(), globalDomain, nullptr));

    TestWebKitAPI::Util::run(&receivedPreferenceNotification);

    auto preferenceValue = [&] {
        waitForPreferenceSynchronization();
        NSString *js = [NSString stringWithFormat:@"window.internals.readPreferenceInteger(\"%@\",\"%@\")", (NSString *)globalDomain, (NSString *)TEST_KEY()];
        return [webView stringByEvaluatingJavaScript:js].intValue;
    };

    preferenceValue();

    receivedPreferenceNotification = false;

    system([NSString stringWithFormat:@"defaults write %@ %@ 2", (__bridge id)globalDomain, (__bridge id)TEST_KEY()].UTF8String);

    TestWebKitAPI::Util::run(&receivedPreferenceNotification);

    for (unsigned i = 0; i < preferenceQueryMaxCount && preferenceValue() != 2; i++) {
        TestWebKitAPI::Util::spinRunLoop();
        TestWebKitAPI::Util::sleep(preferenceQuerySleepTime);
    }

    EXPECT_EQ(preferenceValue(), 2);

    CLEAR_DEFAULTS();
}

TEST(WebKit, PreferenceChangesArray)
{
    CLEAR_DEFAULTS();

    auto observer = adoptNS([[WKTestPreferenceObserver alloc] init]);

    NSArray *array = @[@1, @2, @3];

    auto userDefaults = adoptNS([[NSUserDefaults alloc] initWithSuiteName:(NSString *)testDomain]);
    [userDefaults setObject:array forKey:(NSString *)TEST_KEY()];

    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    WKRetainPtr<WKContextRef> context = adoptWK(TestWebKitAPI::Util::createContextForInjectedBundleTest("InternalsInjectedBundleTest"));
    [configuration setProcessPool:(WKProcessPool *)context.get()];
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 300, 300) configuration:configuration.get() addToWindow:YES]);

    auto preferenceValue = [&] {
        waitForPreferenceSynchronization();
        NSString *js = [NSString stringWithFormat:@"window.internals.encodedPreferenceValue(\"%@\",\"%@\")", (NSString *)testDomain, (NSString *)TEST_KEY()];
        return [webView stringByEvaluatingJavaScript:js];
    };

    preferenceValue();

    NSArray *changedArray = @[@3, @2, @1];
    [userDefaults setObject:changedArray forKey:(NSString *)TEST_KEY()];

    RetainPtr<NSObject> object;
    for (unsigned i = 0; i < preferenceQueryMaxCount && ![object isEqual:changedArray]; i++) {
        auto encodedString = preferenceValue();
        auto encodedData = adoptNS([[NSData alloc] initWithBase64EncodedString:encodedString options:0]);
        ASSERT_TRUE(encodedData);
        NSError *err = nil;
        object = retainPtr([NSKeyedUnarchiver unarchivedObjectOfClass:[NSObject class] fromData:encodedData.get() error:&err]);
        TestWebKitAPI::Util::spinRunLoop();
        TestWebKitAPI::Util::sleep(preferenceQuerySleepTime);
    }

    ASSERT_TRUE([object isEqual:changedArray]);

    CLEAR_DEFAULTS();
}

TEST(WebKit, PreferenceChangesDictionary)
{
    CLEAR_DEFAULTS();

    auto observer = adoptNS([[WKTestPreferenceObserver alloc] init]);

    NSDictionary *dict = @{
        @"a" : @1,
        @"b" : @2,
    };

    auto userDefaults = adoptNS([[NSUserDefaults alloc] initWithSuiteName:(NSString *)testDomain]);
    [userDefaults setObject:dict forKey:(NSString *)TEST_KEY()];

    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    WKRetainPtr<WKContextRef> context = adoptWK(TestWebKitAPI::Util::createContextForInjectedBundleTest("InternalsInjectedBundleTest"));
    [configuration setProcessPool:(WKProcessPool *)context.get()];
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 300, 300) configuration:configuration.get() addToWindow:YES]);

    auto preferenceValue = [&] {
        waitForPreferenceSynchronization();
        NSString *js = [NSString stringWithFormat:@"window.internals.encodedPreferenceValue(\"%@\",\"%@\")", (NSString *)testDomain, (NSString *)TEST_KEY()];
        return [webView stringByEvaluatingJavaScript:js];
    };

    preferenceValue();

    NSDictionary *changedDict = @{
        @"a" : @1,
        @"b" : @2,
        @"c" : @3,
    };
    [userDefaults setObject:changedDict forKey:(NSString *)TEST_KEY()];

    RetainPtr<NSObject> object;
    for (unsigned i = 0; i < preferenceQueryMaxCount && ![object isEqual:changedDict]; i++) {
        auto encodedString = preferenceValue();
        auto encodedData = adoptNS([[NSData alloc] initWithBase64EncodedString:encodedString options:0]);
        ASSERT_TRUE(encodedData);
        NSError *err = nil;
        object = retainPtr([NSKeyedUnarchiver unarchivedObjectOfClass:[NSObject class] fromData:encodedData.get() error:&err]);
        TestWebKitAPI::Util::spinRunLoop();
        TestWebKitAPI::Util::sleep(preferenceQuerySleepTime);
    }
    
    ASSERT_TRUE([object isEqual:changedDict]);

    CLEAR_DEFAULTS();
}

TEST(WebKit, PreferenceChangesData)
{
    CLEAR_DEFAULTS();

    auto observer = adoptNS([[WKTestPreferenceObserver alloc] init]);

    NSData *data = [NSData dataWithBytes:"abc" length:3];

    auto userDefaults = adoptNS([[NSUserDefaults alloc] initWithSuiteName:(NSString *)testDomain]);
    [userDefaults setObject:data forKey:(NSString *)TEST_KEY()];

    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    WKRetainPtr<WKContextRef> context = adoptWK(TestWebKitAPI::Util::createContextForInjectedBundleTest("InternalsInjectedBundleTest"));
    [configuration setProcessPool:(WKProcessPool *)context.get()];
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 300, 300) configuration:configuration.get() addToWindow:YES]);

    auto preferenceValue = [&] {
        waitForPreferenceSynchronization();
        NSString *js = [NSString stringWithFormat:@"window.internals.encodedPreferenceValue(\"%@\",\"%@\")", (NSString *)testDomain, (NSString *)TEST_KEY()];
        return [webView stringByEvaluatingJavaScript:js];
    };

    preferenceValue();

    NSData *changedData = [NSData dataWithBytes:"abcd" length:4];
    [userDefaults setObject:changedData forKey:(NSString *)TEST_KEY()];

    RetainPtr<NSObject> object;
    for (unsigned i = 0; i < preferenceQueryMaxCount && ![object isEqual:changedData]; i++) {
        auto encodedString = preferenceValue();
        auto encodedData = adoptNS([[NSData alloc] initWithBase64EncodedString:encodedString options:0]);
        ASSERT_TRUE(encodedData);
        NSError *err = nil;
        object = retainPtr([NSKeyedUnarchiver unarchivedObjectOfClass:[NSObject class] fromData:encodedData.get() error:&err]);
        TestWebKitAPI::Util::spinRunLoop();
        TestWebKitAPI::Util::sleep(preferenceQuerySleepTime);
    }
    
    ASSERT_TRUE([object isEqual:changedData]);

    CLEAR_DEFAULTS();
}

TEST(WebKit, PreferenceChangesDate)
{
    CLEAR_DEFAULTS();

    auto observer = adoptNS([[WKTestPreferenceObserver alloc] init]);

    NSDate *date = [NSDate dateWithTimeIntervalSinceNow:0];

    auto userDefaults = adoptNS([[NSUserDefaults alloc] initWithSuiteName:(NSString *)testDomain]);
    [userDefaults setObject:date forKey:(NSString *)TEST_KEY()];

    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    WKRetainPtr<WKContextRef> context = adoptWK(TestWebKitAPI::Util::createContextForInjectedBundleTest("InternalsInjectedBundleTest"));
    [configuration setProcessPool:(WKProcessPool *)context.get()];
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 300, 300) configuration:configuration.get() addToWindow:YES]);

    auto preferenceValue = [&] {
        waitForPreferenceSynchronization();
        NSString *js = [NSString stringWithFormat:@"window.internals.encodedPreferenceValue(\"%@\",\"%@\")", (NSString *)testDomain, (NSString *)TEST_KEY()];
        return [webView stringByEvaluatingJavaScript:js];
    };

    preferenceValue();

    NSDate *changedDate = [NSDate dateWithTimeIntervalSinceNow:10];
    [userDefaults setObject:changedDate forKey:(NSString *)TEST_KEY()];

    RetainPtr<NSObject> object;
    for (unsigned i = 0; i < preferenceQueryMaxCount && ![object isEqual:changedDate]; i++) {
        auto encodedString = preferenceValue();
        auto encodedData = adoptNS([[NSData alloc] initWithBase64EncodedString:encodedString options:0]);
        ASSERT_TRUE(encodedData);
        NSError *err = nil;
        object = retainPtr([NSKeyedUnarchiver unarchivedObjectOfClass:[NSObject class] fromData:encodedData.get() error:&err]);
        TestWebKitAPI::Util::spinRunLoop();
        TestWebKitAPI::Util::sleep(preferenceQuerySleepTime);
    }
    
    ASSERT_TRUE([object isEqual:changedDate]);

    CLEAR_DEFAULTS();
}

TEST(WebKit, PreferenceChangesNil)
{
    CLEAR_DEFAULTS();

    auto observer = adoptNS([[WKTestPreferenceObserver alloc] init]);

    auto userDefaults = adoptNS([[NSUserDefaults alloc] initWithSuiteName:(NSString *)testDomain]);
    [userDefaults setObject:@1 forKey:(NSString *)TEST_KEY()];

    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    WKRetainPtr<WKContextRef> context = adoptWK(TestWebKitAPI::Util::createContextForInjectedBundleTest("InternalsInjectedBundleTest"));
    [configuration setProcessPool:(WKProcessPool *)context.get()];
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 300, 300) configuration:configuration.get() addToWindow:YES]);
    [webView synchronouslyLoadTestPageNamed:@"simple"];

    auto preferenceValue = [&] {
        waitForPreferenceSynchronization();
        NSString *js = [NSString stringWithFormat:@"window.internals.readPreferenceInteger(\"%@\",\"%@\")", (NSString *)testDomain, (NSString *)TEST_KEY()];
        return [webView stringByEvaluatingJavaScript:js].intValue;
    };

    EXPECT_EQ(1, preferenceValue());

    [userDefaults setObject:nil forKey:(NSString *)TEST_KEY()];

    for (unsigned i = 0; i < preferenceQueryMaxCount && preferenceValue(); i++) {
        TestWebKitAPI::Util::spinRunLoop();
        TestWebKitAPI::Util::sleep(preferenceQuerySleepTime);
    }
    
    EXPECT_EQ(0, preferenceValue());

    CLEAR_DEFAULTS();
}

#if ENABLE(CFPREFS_DIRECT_MODE)
static IMP sharedInstanceMethodOriginal = nil;
static bool sharedInstanceCalled = false;

static WKPreferenceObserver *sharedInstanceMethodOverride(id self, SEL selector)
{
    WKPreferenceObserver *observer = wtfCallIMP<WKPreferenceObserver *>(sharedInstanceMethodOriginal, self, selector);
    sharedInstanceCalled = true;
    return observer;
}

TEST(WebKit, PreferenceObserverStartedOnActivation)
{
    sharedInstanceCalled = false;
    Method sharedInstanceMethod = class_getClassMethod(objc_getClass("WKPreferenceObserver"), @selector(sharedInstance));
    ASSERT(sharedInstanceMethod);
    sharedInstanceMethodOriginal = method_setImplementation(sharedInstanceMethod, (IMP)sharedInstanceMethodOverride);
    ASSERT(sharedInstanceMethodOriginal);

    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    WKRetainPtr<WKContextRef> context = adoptWK(TestWebKitAPI::Util::createContextForInjectedBundleTest("InternalsInjectedBundleTest"));
    [configuration setProcessPool:(WKProcessPool *)context.get()];
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 300, 300) configuration:configuration.get() addToWindow:YES]);

    [webView synchronouslyLoadTestPageNamed:@"simple"];

    [[NSNotificationCenter defaultCenter] postNotificationName:NSApplicationDidBecomeActiveNotification object:NSApp userInfo:nil];

    TestWebKitAPI::Util::run(&sharedInstanceCalled);
}
#endif

#endif // WK_HAVE_C_SPI
