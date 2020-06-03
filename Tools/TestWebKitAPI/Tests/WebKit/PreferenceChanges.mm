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

#if WK_HAVE_C_SPI

#import "PlatformUtilities.h"
#import "TestWKWebView.h"
#import <WebKit/PreferenceObserver.h>

#import <wtf/ObjCRuntimeExtras.h>

static bool done = false;

@interface WKTestPreferenceObserver : WKPreferenceObserver
- (void)preferenceDidChange:(NSString *)domain key:(NSString *)key encodedValue:(NSString *)encodedValue;
@end

@implementation WKTestPreferenceObserver
- (void)preferenceDidChange:(NSString *)domain key:(NSString *)key encodedValue:(NSString *)encodedValue
{
    done = true;
    [super preferenceDidChange:domain key:key encodedValue:encodedValue];
}
@end

static const CFStringRef testKey = CFSTR("testkey");
static const CFStringRef testDomain = CFSTR("kCFPreferencesAnyApplication");

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
    done = false;

    CFPreferencesSetAppValue(testKey, CFSTR("1"), testDomain);

    auto observer = adoptNS([[WKTestPreferenceObserver alloc] init]);

    CFPreferencesSetAppValue(testKey, CFSTR("2"), testDomain);

    TestWebKitAPI::Util::run(&done);
}

TEST(WebKit, PreferenceObserverArray)
{
    done = false;

    NSArray *array = @[@1, @2, @3];

    auto userDefaults = adoptNS([[NSUserDefaults alloc] initWithSuiteName:(NSString *)testDomain]);
    [userDefaults.get() setObject:array forKey:(NSString *)testKey];

    auto observer = adoptNS([[WKTestPreferenceObserver alloc] init]);

    NSArray *changedArray = @[@3, @2, @1];
    [userDefaults.get() setObject:changedArray forKey:(NSString *)testKey];

    TestWebKitAPI::Util::run(&done);
}

TEST(WebKit, PreferenceChanges)
{
    auto observer = adoptNS([[WKTestPreferenceObserver alloc] init]);

    CFPreferencesSetAppValue(testKey, CFSTR("1"), testDomain);

    EXPECT_EQ(1, CFPreferencesGetAppIntegerValue(testKey, testDomain, nullptr));

    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    WKRetainPtr<WKContextRef> context = adoptWK(TestWebKitAPI::Util::createContextForInjectedBundleTest("InternalsInjectedBundleTest"));
    configuration.get().processPool = (WKProcessPool *)context.get();
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 300, 300) configuration:configuration.get() addToWindow:YES]);

    auto preferenceValue = [&] {
        waitForPreferenceSynchronization();
        NSString *js = [NSString stringWithFormat:@"window.internals.readPreferenceInteger(\"%@\",\"%@\")", (NSString *)testDomain, (NSString *)testKey];
        return [webView stringByEvaluatingJavaScript:js].intValue;
    };

    EXPECT_EQ(preferenceValue(), 1);

    CFPreferencesSetAppValue(testKey, CFSTR("2"), testDomain);

    EXPECT_EQ(preferenceValue(), 2);
}

TEST(WebKit, PreferenceChangesArray)
{
    auto observer = adoptNS([[WKTestPreferenceObserver alloc] init]);

    NSArray *array = @[@1, @2, @3];

    auto userDefaults = adoptNS([[NSUserDefaults alloc] initWithSuiteName:(NSString *)testDomain]);
    [userDefaults.get() setObject:array forKey:(NSString *)testKey];

    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    WKRetainPtr<WKContextRef> context = adoptWK(TestWebKitAPI::Util::createContextForInjectedBundleTest("InternalsInjectedBundleTest"));
    configuration.get().processPool = (WKProcessPool *)context.get();
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 300, 300) configuration:configuration.get() addToWindow:YES]);

    auto preferenceValue = [&] {
        waitForPreferenceSynchronization();
        NSString *js = [NSString stringWithFormat:@"window.internals.encodedPreferenceValue(\"%@\",\"%@\")", (NSString *)testDomain, (NSString *)testKey];
        return [webView stringByEvaluatingJavaScript:js];
    };

    preferenceValue();

    NSArray *changedArray = @[@3, @2, @1];
    [userDefaults.get() setObject:changedArray forKey:(NSString *)testKey];

    auto encodedString = preferenceValue();
    auto encodedData = adoptNS([[NSData alloc] initWithBase64EncodedString:encodedString options:0]);
    ASSERT_TRUE(encodedData);
    NSError *err = nil;
    auto object = retainPtr([NSKeyedUnarchiver unarchivedObjectOfClass:[NSObject class] fromData:encodedData.get() error:&err]);
    ASSERT_TRUE(!err);
    ASSERT_TRUE(object);
    ASSERT_TRUE([object isEqual:changedArray]);
}

TEST(WebKit, PreferenceChangesDictionary)
{
    auto observer = adoptNS([[WKTestPreferenceObserver alloc] init]);

    NSDictionary *dict = @{
        @"a" : @1,
        @"b" : @2,
    };

    auto userDefaults = adoptNS([[NSUserDefaults alloc] initWithSuiteName:(NSString *)testDomain]);
    [userDefaults.get() setObject:dict forKey:(NSString *)testKey];

    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    WKRetainPtr<WKContextRef> context = adoptWK(TestWebKitAPI::Util::createContextForInjectedBundleTest("InternalsInjectedBundleTest"));
    configuration.get().processPool = (WKProcessPool *)context.get();
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 300, 300) configuration:configuration.get() addToWindow:YES]);

    auto preferenceValue = [&] {
        waitForPreferenceSynchronization();
        NSString *js = [NSString stringWithFormat:@"window.internals.encodedPreferenceValue(\"%@\",\"%@\")", (NSString *)testDomain, (NSString *)testKey];
        return [webView stringByEvaluatingJavaScript:js];
    };

    preferenceValue();

    NSDictionary *changedDict = @{
        @"a" : @1,
        @"b" : @2,
        @"c" : @3,
    };
    [userDefaults.get() setObject:changedDict forKey:(NSString *)testKey];

    auto encodedString = preferenceValue();
    auto encodedData = adoptNS([[NSData alloc] initWithBase64EncodedString:encodedString options:0]);
    ASSERT_TRUE(encodedData);
    NSError *err = nil;
    auto object = retainPtr([NSKeyedUnarchiver unarchivedObjectOfClass:[NSObject class] fromData:encodedData.get() error:&err]);
    ASSERT_TRUE(!err);
    ASSERT_TRUE(object);
    ASSERT_TRUE([object isEqual:changedDict]);
}

TEST(WebKit, PreferenceChangesData)
{
    auto observer = adoptNS([[WKTestPreferenceObserver alloc] init]);

    NSData *data = [NSData dataWithBytes:"abc" length:3];

    auto userDefaults = adoptNS([[NSUserDefaults alloc] initWithSuiteName:(NSString *)testDomain]);
    [userDefaults.get() setObject:data forKey:(NSString *)testKey];

    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    WKRetainPtr<WKContextRef> context = adoptWK(TestWebKitAPI::Util::createContextForInjectedBundleTest("InternalsInjectedBundleTest"));
    configuration.get().processPool = (WKProcessPool *)context.get();
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 300, 300) configuration:configuration.get() addToWindow:YES]);

    auto preferenceValue = [&] {
        waitForPreferenceSynchronization();
        NSString *js = [NSString stringWithFormat:@"window.internals.encodedPreferenceValue(\"%@\",\"%@\")", (NSString *)testDomain, (NSString *)testKey];
        return [webView stringByEvaluatingJavaScript:js];
    };

    preferenceValue();

    NSData *changedData = [NSData dataWithBytes:"abcd" length:4];
    [userDefaults.get() setObject:changedData forKey:(NSString *)testKey];

    auto encodedString = preferenceValue();
    auto encodedData = adoptNS([[NSData alloc] initWithBase64EncodedString:encodedString options:0]);
    ASSERT_TRUE(encodedData);
    NSError *err = nil;
    auto object = retainPtr([NSKeyedUnarchiver unarchivedObjectOfClass:[NSObject class] fromData:encodedData.get() error:&err]);
    ASSERT_TRUE(!err);
    ASSERT_TRUE(object);
    ASSERT_TRUE([object isEqual:changedData]);
}

TEST(WebKit, PreferenceChangesDate)
{
    auto observer = adoptNS([[WKTestPreferenceObserver alloc] init]);

    NSDate *date = [NSDate dateWithTimeIntervalSinceNow:0];

    auto userDefaults = adoptNS([[NSUserDefaults alloc] initWithSuiteName:(NSString *)testDomain]);
    [userDefaults.get() setObject:date forKey:(NSString *)testKey];

    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    WKRetainPtr<WKContextRef> context = adoptWK(TestWebKitAPI::Util::createContextForInjectedBundleTest("InternalsInjectedBundleTest"));
    configuration.get().processPool = (WKProcessPool *)context.get();
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 300, 300) configuration:configuration.get() addToWindow:YES]);

    auto preferenceValue = [&] {
        waitForPreferenceSynchronization();
        NSString *js = [NSString stringWithFormat:@"window.internals.encodedPreferenceValue(\"%@\",\"%@\")", (NSString *)testDomain, (NSString *)testKey];
        return [webView stringByEvaluatingJavaScript:js];
    };

    preferenceValue();

    NSDate *changedDate = [NSDate dateWithTimeIntervalSinceNow:10];
    [userDefaults.get() setObject:changedDate forKey:(NSString *)testKey];

    auto encodedString = preferenceValue();
    auto encodedData = adoptNS([[NSData alloc] initWithBase64EncodedString:encodedString options:0]);
    ASSERT_TRUE(encodedData);
    NSError *err = nil;
    auto object = retainPtr([NSKeyedUnarchiver unarchivedObjectOfClass:[NSObject class] fromData:encodedData.get() error:&err]);
    ASSERT_TRUE(!err);
    ASSERT_TRUE(object);
    ASSERT_TRUE([object isEqual:changedDate]);
}

static IMP sharedInstanceMethodOriginal = nil;

static WKPreferenceObserver *sharedInstanceMethodOverride(id self, SEL selector)
{
    done = true;
    return wtfCallIMP<WKPreferenceObserver *>(sharedInstanceMethodOriginal, self, selector);
}

TEST(WebKit, PreferenceObserverStartedOnActivation)
{
    done = false;
    Method sharedInstanceMethod = class_getClassMethod(objc_getClass("WKPreferenceObserver"), @selector(sharedInstance));
    ASSERT(sharedInstanceMethod);
    sharedInstanceMethodOriginal = method_setImplementation(sharedInstanceMethod, (IMP)sharedInstanceMethodOverride);
    ASSERT(sharedInstanceMethodOriginal);

    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    WKRetainPtr<WKContextRef> context = adoptWK(TestWebKitAPI::Util::createContextForInjectedBundleTest("InternalsInjectedBundleTest"));
    configuration.get().processPool = (WKProcessPool *)context.get();
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 300, 300) configuration:configuration.get() addToWindow:YES]);

    [webView synchronouslyLoadTestPageNamed:@"simple"];

    [[NSNotificationCenter defaultCenter] postNotificationName:NSApplicationDidBecomeActiveNotification object:NSApp userInfo:nil];

    TestWebKitAPI::Util::run(&done);
}

#endif // WK_HAVE_C_SPI
