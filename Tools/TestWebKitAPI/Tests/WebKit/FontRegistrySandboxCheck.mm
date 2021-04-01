/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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
#import <WebKit/WKPreferencesPrivate.h>
#import <WebKit/WKProcessPoolPrivate.h>
#import <WebKit/WKWebViewPrivate.h>

TEST(WebKit, FontdSandboxCheck)
{
    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    configuration.get().preferences._shouldAllowUserInstalledFonts = NO;
    auto context = adoptWK(TestWebKitAPI::Util::createContextForInjectedBundleTest("InternalsInjectedBundleTest"));
    configuration.get().processPool = (WKProcessPool *)context.get();
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 300, 300) configuration:configuration.get() addToWindow:YES]);

    auto sandboxAccess = [&] {
        return [webView stringByEvaluatingJavaScript:@"window.internals.hasSandboxMachLookupAccessToXPCServiceName('com.apple.WebKit.WebContent', 'com.apple.fonts')"].boolValue;
    };

#if HAVE(STATIC_FONT_REGISTRY)
    ASSERT_FALSE(sandboxAccess());
#endif

    [webView _switchFromStaticFontRegistryToUserFontRegistry];

    ASSERT_TRUE(sandboxAccess());
}

TEST(WebKit, UserInstalledFontsWork)
{
    NSURL *fontURL = [[NSBundle mainBundle] URLForResource:@"Ahem" withExtension:@"ttf" subdirectory:@"TestWebKitAPI.resources"];
    CFErrorRef error = nil;
    auto registrationSucceeded = CTFontManagerRegisterFontsForURL(static_cast<CFURLRef>(fontURL), kCTFontManagerScopeUser, &error);

    auto context = adoptWK(TestWebKitAPI::Util::createContextForInjectedBundleTest("InternalsInjectedBundleTest"));
    WKContextSetUsesSingleWebProcess(context.get(), true);

    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    configuration.get().processPool = (WKProcessPool *)context.get();
    [configuration.get().processPool _warmInitialProcess];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 600, 500) configuration:configuration.get() addToWindow:YES]);
    [webView synchronouslyLoadTestPageNamed:@"UserInstalledAhem"];
    auto result = [webView stringByEvaluatingJavaScript:@"document.getElementById('target').offsetWidth"].intValue;
    ASSERT_EQ(result, 12 * 48);

    if (registrationSucceeded) {
        error = nil;
        CTFontManagerUnregisterFontsForURL(static_cast<CFURLRef>(fontURL), kCTFontManagerScopeUser, &error);
        ASSERT_FALSE(error);
    }
}

#endif // WK_HAVE_C_SPI
