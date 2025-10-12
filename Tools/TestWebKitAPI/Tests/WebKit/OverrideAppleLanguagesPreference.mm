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

#import "PlatformUtilities.h"
#import "TestWKWebView.h"
#import <WebKit/PreferenceObserver.h>
#import <wtf/ObjCRuntimeExtras.h>
#import <wtf/cocoa/VectorCocoa.h>
#import <wtf/darwin/DispatchExtras.h>
#import <wtf/text/StringBuilder.h>

#if WK_HAVE_C_SPI

TEST(WebKit, OverrideAppleLanguagesPreference)
{
    NSDictionary *dict = @{
        @"AppleLanguages": @[ @"en-GB" ],
    };

    [[NSUserDefaults standardUserDefaults] setVolatileDomain:dict forName:NSArgumentDomain];

    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    WKRetainPtr<WKContextRef> context = adoptWK(TestWebKitAPI::Util::createContextForInjectedBundleTest("InternalsInjectedBundleTest"));
    configuration.get().processPool = (WKProcessPool *)context.get();
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 300, 300) configuration:configuration.get() addToWindow:YES]);

    auto preferredLanguage = [&] {
        return [webView stringByEvaluatingJavaScript:@"window.internals.userPreferredLanguages()[0]"];
    };

    ASSERT_TRUE([preferredLanguage() isEqual:@"en-GB"]);
}

#endif // WK_HAVE_C_SPI

TEST(WebKit, OverrideAppleLanguagesPreferenceAffectsNavigatorLanguage)
{
    NSDictionary *dict = @{
        @"AppleLanguages": @[ @"en-GB" ],
    };
    [[NSUserDefaults standardUserDefaults] setVolatileDomain:dict forName:NSArgumentDomain];

    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 300, 300) configuration:configuration.get() addToWindow:YES]);

    EXPECT_WK_STREQ("en-GB", [webView stringByEvaluatingJavaScript:@"window.navigator.language"]);
}

// On older macOSes, CFPREFS_DIRECT_MODE is disabled and the WebProcess does not see the updated AppleLanguages
// after the AppleLanguagePreferencesChangedNotification notification.
#if PLATFORM(MAC) && ENABLE(CFPREFS_DIRECT_MODE)
class AppleLanguagesTest : public testing::Test {
public:
    AppleLanguagesTest()
    {
        // Save current system language to restore it later.
        auto task = adoptNS([[NSTask alloc] init]);
        [task setLaunchPath:@"/usr/bin/defaults"];
        [task setArguments:@[@"read", @"NSGlobalDomain", @"AppleLanguages"]];
        auto pipe = adoptNS([[NSPipe alloc] init]);
        [task setStandardOutput:pipe.get()];
        auto fileHandle = [pipe fileHandleForReading];
        [task launch];
        NSData *data = [fileHandle readDataToEndOfFile];
        m_savedAppleLanguages = [[NSString alloc] initWithData:data encoding:NSUTF8StringEncoding];
        m_savedAppleLanguages = makeStringByReplacingAll(m_savedAppleLanguages, '\n', ""_s);
        m_savedAppleLanguages = makeStringByReplacingAll(m_savedAppleLanguages, ' ', ""_s);
    }

    ~AppleLanguagesTest()
    {
        // Restore previous system language.
        system(adoptNS([[NSString alloc] initWithFormat:@"defaults write NSGlobalDomain AppleLanguages '%@'", m_savedAppleLanguages.createNSString().get()]).get().UTF8String);
    }

private:
    String m_savedAppleLanguages;
};

static IMP sharedInstanceMethodOriginal = nil;
static IMP preferenceDidChangeMethodOriginal = nil;
static bool preferenceObserverSharedInstanceCalled = false;
static bool preferenceObserverPreferenceDidChangeCalled = false;

static WKPreferenceObserver *sharedInstanceMethodOverride(id self, SEL selector)
{
    WKPreferenceObserver *observer = wtfCallIMP<WKPreferenceObserver *>(sharedInstanceMethodOriginal, self, selector);
    preferenceObserverSharedInstanceCalled = true;
    return observer;
}

static WKPreferenceObserver *preferenceDidChangeMethodOverride(id self, SEL selector, NSString *domain, NSString *key, NSString *encodedValue)
{
    NSLog(@"preferenceDidChangeMethodOverride: domain=%@, key=%@, encodedValue=%@", domain, key, encodedValue);
    if ([key isEqualToString:@"AppleLanguages"])
        preferenceObserverPreferenceDidChangeCalled = true;
    WKPreferenceObserver *observer = wtfCallIMP<WKPreferenceObserver *>(preferenceDidChangeMethodOriginal, self, selector, domain, key, encodedValue);
    return observer;
}

// FIXME: This test times out on Apple Silicon (webkit.org/b/222619) and is a flaky failure on Intel (webkit.org/b/228309)
TEST_F(AppleLanguagesTest, DISABLED_UpdateAppleLanguages)
{
    preferenceObserverSharedInstanceCalled = false;
    Method sharedInstanceMethod = class_getClassMethod(objc_getClass("WKPreferenceObserver"), @selector(sharedInstance));
    ASSERT(sharedInstanceMethod);
    sharedInstanceMethodOriginal = method_setImplementation(sharedInstanceMethod, (IMP)sharedInstanceMethodOverride);
    ASSERT(sharedInstanceMethodOriginal);
    Method preferenceDidChangeMethod = class_getInstanceMethod(objc_getClass("WKPreferenceObserver"), @selector(preferenceDidChange:key:encodedValue:));
    ASSERT(preferenceDidChangeMethod);
    preferenceDidChangeMethodOriginal = method_setImplementation(preferenceDidChangeMethod, (IMP)preferenceDidChangeMethodOverride);
    ASSERT(preferenceDidChangeMethodOriginal);

    // Tests uses "en-GB" language initially.
    system([NSString stringWithFormat:@"defaults write NSGlobalDomain AppleLanguages '(\"en-GB\")'"].UTF8String);

    auto getLanguageFromNSUserDefaults = [] {
        auto languages = makeVector<String>([[NSUserDefaults standardUserDefaults] valueForKey:@"AppleLanguages"]);
        return languages.isEmpty() ? emptyString() : languages[0];
    };
    unsigned timeout = 0;
    while (getLanguageFromNSUserDefaults() != "en-GB"_s && ++timeout < 100)
        TestWebKitAPI::Util::runFor(0.1_s);
    EXPECT_WK_STREQ(@"en-GB", getLanguageFromNSUserDefaults());

    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 300, 300) configuration:configuration.get() addToWindow:YES]);
    [webView synchronouslyLoadTestPageNamed:@"simple"];

    // We only listen for preference changes when the application is active.
    [[NSNotificationCenter defaultCenter] postNotificationName:NSApplicationDidBecomeActiveNotification object:NSApp userInfo:nil];
    timeout = 0;
    while (!preferenceObserverSharedInstanceCalled && ++timeout < 100)
        TestWebKitAPI::Util::runFor(0.1_s);
    EXPECT_TRUE(preferenceObserverSharedInstanceCalled);
    if (!preferenceObserverSharedInstanceCalled)
        return;

    auto preferredLanguage = [&] {
        return [webView stringByEvaluatingJavaScript:@"navigator.language"];
    };
    EXPECT_WK_STREQ(@"en-GB", preferredLanguage());

    __block bool done = false;
    __block bool didChangeLanguage = false;
    [webView performAfterReceivingAnyMessage:^(NSString *newLanguage) {
        EXPECT_WK_STREQ(@"en-US", newLanguage);
        didChangeLanguage = true;
        done = true;
    }];

    [webView evaluateJavaScript:@"onlanguagechange = () => { webkit.messageHandlers.testHandler.postMessage(navigator.language); }; true;" completionHandler:^(id value, NSError *error) {
        EXPECT_TRUE(!error);
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
    done = false;

    // Switch system language from "en-GB" to "en-US". Make sure that we fire a languagechange event at the Window and that navigator.language
    // now reports "en-GB".
    system([NSString stringWithFormat:@"defaults write NSGlobalDomain AppleLanguages '(\"en-US\")'"].UTF8String);

    // Implement our own timeout because we would fail to reset the language if we let the test actually time out.
    dispatch_after(dispatch_time(DISPATCH_TIME_NOW, 10 * NSEC_PER_SEC), mainDispatchQueueSingleton(), ^{
        done = true;
    });

    TestWebKitAPI::Util::run(&done);
    EXPECT_TRUE(didChangeLanguage);
    EXPECT_TRUE(preferenceObserverPreferenceDidChangeCalled);
    EXPECT_WK_STREQ(@"en-US", preferredLanguage());
    EXPECT_WK_STREQ(@"en-US", getLanguageFromNSUserDefaults());

    method_setImplementation(sharedInstanceMethod, (IMP)sharedInstanceMethodOriginal);
}
#endif
