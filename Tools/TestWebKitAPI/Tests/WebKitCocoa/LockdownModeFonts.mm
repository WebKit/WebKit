/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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
#import "TestNavigationDelegate.h"
#import "TestWKWebView.h"
#include <pal/cf/CoreTextSoftLink.h>

namespace TestWebKitAPI {

// rdar://136524076
#if PLATFORM(MAC) && __MAC_OS_X_VERSION_MIN_REQUIRED >= 150000
TEST(LockdownMode, DISABLED_SVGFonts)
#else
TEST(LockdownMode, SVGFonts)
#endif
{
    auto webViewConfiguration = adoptNS([WKWebViewConfiguration new]);
    webViewConfiguration.get().defaultWebpagePreferences.lockdownModeEnabled = YES;
    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:webViewConfiguration.get()]);
    NSURL *url = [NSBundle.test_resourcesBundle URLForResource:@"SVGFont" withExtension:@"html"];
    [webView loadRequest:[NSURLRequest requestWithURL:url]];
    [webView _test_waitForDidFinishNavigation];

    auto target1Result = static_cast<NSNumber *>([webView objectByEvaluatingJavaScript:@"document.getElementById('target1').offsetWidth"]).intValue;
    auto target2Result = static_cast<NSNumber *>([webView objectByEvaluatingJavaScript:@"document.getElementById('target2').offsetWidth"]).intValue;
    auto referenceResult = static_cast<NSNumber *>([webView objectByEvaluatingJavaScript:@"document.getElementById('reference').offsetWidth"]).intValue;
    EXPECT_EQ(target1Result, referenceResult);
    EXPECT_EQ(target2Result, referenceResult);
}

TEST(LockdownMode, NotAllowedFontLoadingAPI)
{
    @autoreleasepool {
        auto webViewConfiguration = adoptNS([WKWebViewConfiguration new]);
        webViewConfiguration.get().defaultWebpagePreferences.lockdownModeEnabled = YES;
        auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:webViewConfiguration.get()]);
        NSURL *url = [NSBundle.test_resourcesBundle URLForResource:@"ImmediateFont" withExtension:@"html"];
        [webView loadRequest:[NSURLRequest requestWithURL:url]];
        [webView _test_waitForDidFinishNavigation];

        NSURL *fontURL = [NSBundle.test_resourcesBundle URLForResource:@"Ahem" withExtension:@"ttf"];
        NSData *fontData = [NSData dataWithContentsOfURL:fontURL];
        NSError *error = nil;
        NSMutableArray<NSNumber *> *array = [NSMutableArray arrayWithCapacity:fontData.length];
        const auto* fontBytes = static_cast<const uint8_t*>(fontData.bytes);
        for (NSUInteger i = 0; i < fontData.length; ++i)
            [array addObject:[NSNumber numberWithUnsignedChar:fontBytes[i]]];
        NSData *json = [NSJSONSerialization dataWithJSONObject:array options:0 error:&error];
        auto encoded = adoptNS([[NSString alloc] initWithData:json encoding:NSUTF8StringEncoding]);

        [webView objectByEvaluatingJavaScript:@""
            "let target = document.getElementById('target');"
            "let reference = document.getElementById('reference');"];
        auto beforeTargetResult = static_cast<NSNumber *>([webView objectByEvaluatingJavaScript:@"target.offsetWidth"]).intValue;
        [webView objectByEvaluatingJavaScript:[NSString stringWithFormat:@""
            "let fontData = new Uint8Array(%@);"
            "let font = new FontFace('WebFont', fontData);"
            "document.fonts.add(font);"
            "target.style.setProperty('font-family', 'WebFont, Helvetica');", encoded.get()]];
        auto targetResult = static_cast<NSNumber *>([webView objectByEvaluatingJavaScript:@"target.offsetWidth"]).intValue;
        auto referenceResult = static_cast<NSNumber *>([webView objectByEvaluatingJavaScript:@"reference.offsetWidth"]).intValue;

        EXPECT_NE(beforeTargetResult, targetResult);
        if (!PAL::canLoad_CoreText_CTFontManagerCreateMemorySafeFontDescriptorFromData())
            EXPECT_EQ(targetResult, referenceResult);
        else
            EXPECT_NE(targetResult, referenceResult);
    }
}

TEST(LockdownMode, AllowedFontLoadingAPI)
{
    @autoreleasepool {
        auto webViewConfiguration = adoptNS([WKWebViewConfiguration new]);
        webViewConfiguration.get().defaultWebpagePreferences.lockdownModeEnabled = YES;
        auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:webViewConfiguration.get()]);
        NSURL *url = [NSBundle.test_resourcesBundle URLForResource:@"ImmediateFont" withExtension:@"html"];
        [webView loadRequest:[NSURLRequest requestWithURL:url]];
        [webView _test_waitForDidFinishNavigation];

        NSURL *fontURL = [NSBundle.test_resourcesBundle URLForResource:@"Ahem-10000A" withExtension:@"ttf"];
        NSData *fontData = [NSData dataWithContentsOfURL:fontURL];
        NSError *error = nil;
        NSMutableArray<NSNumber *> *array = [NSMutableArray arrayWithCapacity:fontData.length];
        const auto* fontBytes = static_cast<const uint8_t*>(fontData.bytes);
        for (NSUInteger i = 0; i < fontData.length; ++i)
            [array addObject:[NSNumber numberWithUnsignedChar:fontBytes[i]]];
        NSData *json = [NSJSONSerialization dataWithJSONObject:array options:0 error:&error];
        auto encoded = adoptNS([[NSString alloc] initWithData:json encoding:NSUTF8StringEncoding]);

        [webView objectByEvaluatingJavaScript:@""
            "let target = document.getElementById('target');"
            "let reference = document.getElementById('reference');"];
        auto beforeTargetResult = static_cast<NSNumber *>([webView objectByEvaluatingJavaScript:@"target.offsetWidth"]).intValue;
        [webView objectByEvaluatingJavaScript:[NSString stringWithFormat:@""
            "let fontData = new Uint8Array(%@);"
            "let font = new FontFace('WebFont', fontData);"
            "document.fonts.add(font);"
            "target.style.setProperty('font-family', 'WebFont, Helvetica');", encoded.get()]];
        auto targetResult = static_cast<NSNumber *>([webView objectByEvaluatingJavaScript:@"target.offsetWidth"]).intValue;
        auto referenceResult = static_cast<NSNumber *>([webView objectByEvaluatingJavaScript:@"reference.offsetWidth"]).intValue;

        EXPECT_NE(beforeTargetResult, targetResult);
        EXPECT_NE(targetResult, referenceResult);
    }
}

TEST(LockdownMode, NotSupportedFontLoadingAPI)
{
    @autoreleasepool {
        auto webViewConfiguration = adoptNS([WKWebViewConfiguration new]);
        webViewConfiguration.get().defaultWebpagePreferences.lockdownModeEnabled = YES;
        auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:webViewConfiguration.get()]);
        NSURL *url = [NSBundle.test_resourcesBundle URLForResource:@"ImmediateFont" withExtension:@"html"];
        [webView loadRequest:[NSURLRequest requestWithURL:url]];
        [webView _test_waitForDidFinishNavigation];

        NSURL *fontURL = [NSBundle.test_resourcesBundle URLForResource:@"Ahem-CFF" withExtension:@"otf"];
        NSData *fontData = [NSData dataWithContentsOfURL:fontURL];
        NSError *error = nil;
        NSMutableArray<NSNumber *> *array = [NSMutableArray arrayWithCapacity:fontData.length];
        const auto* fontBytes = static_cast<const uint8_t*>(fontData.bytes);
        for (NSUInteger i = 0; i < fontData.length; ++i)
            [array addObject:[NSNumber numberWithUnsignedChar:fontBytes[i]]];
        NSData *json = [NSJSONSerialization dataWithJSONObject:array options:0 error:&error];
        auto encoded = adoptNS([[NSString alloc] initWithData:json encoding:NSUTF8StringEncoding]);

        [webView objectByEvaluatingJavaScript:@""
            "let target = document.getElementById('target');"
            "let reference = document.getElementById('reference');"];
        auto beforeTargetResult = static_cast<NSNumber *>([webView objectByEvaluatingJavaScript:@"target.offsetWidth"]).intValue;
        [webView objectByEvaluatingJavaScript:[NSString stringWithFormat:@""
            "let fontData = new Uint8Array(%@);"
            "let font = new FontFace('WebFont', fontData);"
            "document.fonts.add(font);"
            "target.style.setProperty('font-family', 'WebFont, Helvetica');", encoded.get()]];
        auto targetResult = static_cast<NSNumber *>([webView objectByEvaluatingJavaScript:@"target.offsetWidth"]).intValue;
        auto referenceResult = static_cast<NSNumber *>([webView objectByEvaluatingJavaScript:@"reference.offsetWidth"]).intValue;

        EXPECT_NE(beforeTargetResult, targetResult);
        EXPECT_EQ(targetResult, referenceResult);
    }
}

TEST(LockdownMode, AllowedFont)
{
    @autoreleasepool {
        auto webViewConfiguration = adoptNS([WKWebViewConfiguration new]);
        webViewConfiguration.get().defaultWebpagePreferences.lockdownModeEnabled = YES;
        auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:webViewConfiguration.get()]);
        NSURL *url = [NSBundle.test_resourcesBundle URLForResource:@"LockdownModeFonts" withExtension:@"html"];
        [webView loadRequest:[NSURLRequest requestWithURL:url]];
        [webView _test_waitForDidFinishNavigation];

        [webView objectByEvaluatingJavaScript:@""
            "let target = document.getElementById('target');"
            "let reference = document.getElementById('reference');"];
        auto targetResult = static_cast<NSNumber *>([webView objectByEvaluatingJavaScript:@"target.offsetWidth"]).intValue;
        auto referenceResult = static_cast<NSNumber *>([webView objectByEvaluatingJavaScript:@"reference.offsetWidth"]).intValue;

        EXPECT_NE(targetResult, referenceResult);
    }
}

TEST(LockdownMode, NotAllowedFont)
{
    @autoreleasepool {
        auto webViewConfiguration = adoptNS([WKWebViewConfiguration new]);
        webViewConfiguration.get().defaultWebpagePreferences.lockdownModeEnabled = YES;
        auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:webViewConfiguration.get()]);
        NSURL *url = [NSBundle.test_resourcesBundle URLForResource:@"LockdownModeFonts" withExtension:@"html"];
        [webView loadRequest:[NSURLRequest requestWithURL:url]];
        [webView _test_waitForDidFinishNavigation];

        [webView objectByEvaluatingJavaScript:@""
            "let target = document.getElementById('target-not-allowed');"
            "let reference = document.getElementById('reference');"];
        auto targetResult = static_cast<NSNumber *>([webView objectByEvaluatingJavaScript:@"target.offsetWidth"]).intValue;
        auto referenceResult = static_cast<NSNumber *>([webView objectByEvaluatingJavaScript:@"reference.offsetWidth"]).intValue;

#if PLATFORM(WATCHOS)
        EXPECT_NE(targetResult, referenceResult);
#else
    if (!PAL::canLoad_CoreText_CTFontManagerCreateMemorySafeFontDescriptorFromData())
        EXPECT_EQ(targetResult, referenceResult);
    else
        EXPECT_NE(targetResult, referenceResult);
#endif
    }
}
}
