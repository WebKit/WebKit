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
#import <wtf/cocoa/VectorCocoa.h>
#import <wtf/text/StringBuilder.h>

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

    ASSERT_TRUE([preferredLanguage() isEqual:@"en-gb"]);
}

#endif // WK_HAVE_C_SPI

// On older macOSes, CFPREFS_DIRECT_MODE is disabled and the WebProcess does not see the updated AppleLanguages
// after the AppleLanguagePreferencesChangedNotification notification.
// FIXME: This test is currently disabled on Apple Silicon because it times out there (https://bugs.webkit.org/show_bug.cgi?id=222619).
#if PLATFORM(MAC) && __MAC_OS_X_VERSION_MIN_REQUIRED >= 110000 && CPU(X86_64)
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
        m_savedAppleLanguages.replace("\n", "");
        m_savedAppleLanguages.replace(" ", "");
    }

    ~AppleLanguagesTest()
    {
        // Restore previous system language.
        system([NSString stringWithFormat:@"defaults write NSGlobalDomain AppleLanguages '%@'", (NSString *)m_savedAppleLanguages].UTF8String);
    }

private:
    String m_savedAppleLanguages;
};

TEST_F(AppleLanguagesTest, UpdateAppleLanguages)
{
    // Tests uses "en-GB" language initially.
    system([NSString stringWithFormat:@"defaults write NSGlobalDomain AppleLanguages '(\"en-GB\")'"].UTF8String);

    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 300, 300) configuration:configuration.get() addToWindow:YES]);
    [webView synchronouslyLoadTestPageNamed:@"simple"];

    // We only listen for preference changes when the application is active.
    [[NSNotificationCenter defaultCenter] postNotificationName:NSApplicationDidBecomeActiveNotification object:NSApp userInfo:nil];

    auto preferredLanguage = [&] {
        return [webView stringByEvaluatingJavaScript:@"navigator.language"];
    };
    EXPECT_WK_STREQ(@"en-gb", preferredLanguage());

    __block bool done = false;
    [webView evaluateJavaScript:@"onlanguagechange = () => { webkit.messageHandlers.testHandler.postMessage(navigator.language); }; true;" completionHandler:^(id value, NSError *error) {
        EXPECT_TRUE(!error);
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);

    done = false;
    __block bool didChangeLanguage = false;
    [webView performAfterReceivingAnyMessage:^(NSString *newLanguage) {
        EXPECT_WK_STREQ(@"en-us", newLanguage);
        didChangeLanguage = true;
        done = true;
    }];

    // Switch system language from "en-GB" to "en-US". Make sure that we fire a languagechange event at the Window and that navigator.language
    // now reports "en-gb".
    system([NSString stringWithFormat:@"defaults write NSGlobalDomain AppleLanguages '(\"en-US\")'"].UTF8String);
    CFNotificationCenterPostNotification(CFNotificationCenterGetDistributedCenter(), CFSTR("AppleLanguagePreferencesChangedNotification"), nullptr, nullptr, true);

    // Implement our own timeout because we would fail to reset the language if we let the test actually time out.
    dispatch_after(dispatch_time(DISPATCH_TIME_NOW, 10 * NSEC_PER_SEC), dispatch_get_main_queue(), ^{
        done = true;
    });

    TestWebKitAPI::Util::run(&done);
    EXPECT_TRUE(didChangeLanguage);
}
#endif
