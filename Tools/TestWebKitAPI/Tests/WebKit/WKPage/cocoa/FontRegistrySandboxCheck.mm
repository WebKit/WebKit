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

#import "Helpers/PlatformUtilities.h"
#import "Helpers/cocoa/TestWKWebView.h"
#import <WebKit/WKPreferencesPrivate.h>
#import <WebKit/WKProcessPoolPrivate.h>
#import <WebKit/WKWebViewPrivate.h>
#import <pal/spi/cf/CoreTextSPI.h>

TEST(WebKit, FontdSandboxCheck)
{
    RetainPtr configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    configuration.get().preferences._shouldAllowUserInstalledFonts = NO;
    auto context = adoptWK(TestWebKitAPI::Util::createContextForInjectedBundleTest("InternalsInjectedBundleTest"));
    configuration.get().processPool = (WKProcessPool *)context.get();
    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 300, 300) configuration:configuration.get() addToWindow:YES]);

    auto sandboxAccess = [&] {
        return [webView stringByEvaluatingJavaScript:@"window.internals.hasSandboxMachLookupAccessToXPCServiceName('com.apple.WebKit.WebContent', 'com.apple.fonts')"].boolValue;
    };

#if HAVE(STATIC_FONT_REGISTRY)
    ASSERT_FALSE(sandboxAccess());
#endif

    [webView _switchFromStaticFontRegistryToUserFontRegistry];

#if ENABLE(REMOVE_XPC_AND_MACH_SANDBOX_EXTENSIONS_IN_WEBCONTENT)
    ASSERT_FALSE(sandboxAccess());
#else
    ASSERT_TRUE(sandboxAccess());
#endif
}

TEST(WebKit, UserInstalledFontsWork)
{
    NSURL *fontURL = [NSBundle.test_resourcesBundle URLForResource:@"Ahem" withExtension:@"ttf"];
    CFErrorRef error = nil;
    auto registrationSucceeded = CTFontManagerRegisterFontsForURL(static_cast<CFURLRef>(fontURL), kCTFontManagerScopeUser, &error);

    auto context = adoptWK(TestWebKitAPI::Util::createContextForInjectedBundleTest("InternalsInjectedBundleTest"));
    WKContextSetUsesSingleWebProcess(context.get(), true);

    RetainPtr configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    configuration.get().processPool = (WKProcessPool *)context.get();
    [configuration.get().processPool _warmInitialProcess];

    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 600, 500) configuration:configuration.get() addToWindow:YES]);
    [webView synchronouslyLoadTestPageNamed:@"UserInstalledAhem"];
    auto result = [webView stringByEvaluatingJavaScript:@"document.getElementById('target').offsetWidth"].intValue;
    ASSERT_EQ(result, 12 * 48);

    if (registrationSucceeded) {
        error = nil;
        CTFontManagerUnregisterFontsForURL(static_cast<CFURLRef>(fontURL), kCTFontManagerScopeUser, &error);
        ASSERT_FALSE(error);
    }
}

#if PLATFORM(MAC)
static void checkChineseGenericSystemFont(NSString *generic, NSArray<NSString *> *families)
{
    // Check in the UI process: the WebContent registry is precisely what this
    // test exercises, and cannot establish whether a system asset is installed.
    RetainPtr<NSString> installedFamily;
    for (NSString *family in families) {
        RetainPtr attributes = @{ (__bridge NSString *)kCTFontFamilyNameAttribute: family, (__bridge NSString *)kCTFontUserInstalledAttribute: @NO };
        RetainPtr descriptor = adoptCF(CTFontDescriptorCreateWithAttributes((__bridge CFDictionaryRef)attributes.get()));
        RetainPtr mandatoryAttributes = [NSSet setWithArray:[attributes allKeys]];
        RetainPtr matches = adoptCF(CTFontDescriptorCreateMatchingFontDescriptors(descriptor.get(), (__bridge CFSetRef)mandatoryAttributes.get()));
        if (matches && CFArrayGetCount(matches.get())) {
            installedFamily = family;
            break;
        }
    }
    if (!installedFamily) {
        SUCCEED() << "No matching system font asset is installed";
        return;
    }

    RetainPtr configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    configuration.get().preferences._shouldAllowUserInstalledFonts = NO;
    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
    NSString *html = @"<!DOCTYPE html><html lang='zh-Hans'><canvas width='1000' height='100'></canvas>";
    NSString *drawingScript = @"const canvas = document.querySelector('canvas');"
        "const context = canvas.getContext('2d');"
        "function pixels(family) {"
        "context.clearRect(0, 0, canvas.width, canvas.height);"
        "context.font = '48px ' + family;"
        "context.fillText('人人生而自由,在尊严和权利上一律平等。', 0, 60);"
        "return Array.from(context.getImageData(0, 0, canvas.width, canvas.height).data).join(',');"
        "}";
    [webView synchronouslyLoadHTMLString:html];
    [webView stringByEvaluatingJavaScript:drawingScript];
    RetainPtr actual = [webView stringByEvaluatingJavaScript:[NSString stringWithFormat:@"pixels('%@')", generic]];
    EXPECT_FALSE([actual isEqualToString:[webView stringByEvaluatingJavaScript:@"pixels('serif')"]]);

    // Explicit family names retain the normal font access policy. Obtain the
    // reference pixels in a separate view that allows installed fonts.
    RetainPtr referenceConfiguration = adoptNS([[WKWebViewConfiguration alloc] init]);
    referenceConfiguration.get().preferences._shouldAllowUserInstalledFonts = YES;
    RetainPtr referenceView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:referenceConfiguration.get()]);
    [referenceView synchronouslyLoadHTMLString:html];
    [referenceView stringByEvaluatingJavaScript:drawingScript];
    RetainPtr reference = [referenceView stringByEvaluatingJavaScript:[NSString stringWithFormat:@"pixels('\"%@\"')", installedFamily.get()]];
    EXPECT_TRUE([actual isEqualToString:reference.get()]);
}

TEST(WebKit, KaiSystemFontWithoutUserInstalledFonts)
{
    checkChineseGenericSystemFont(@"generic(kai)", @[@"Kaiti SC", @"Kaiti TC", @"STKaiti", @"BiauKaiTC", @"BiauKaiHK"]);
}

TEST(WebKit, FangsongSystemFontWithoutUserInstalledFonts)
{
    checkChineseGenericSystemFont(@"generic(fangsong)", @[@"STFangsong"]);
}
#endif

#endif // WK_HAVE_C_SPI
