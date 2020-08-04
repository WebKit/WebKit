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
#import <WebKit/WKProcessPoolPrivate.h>
#import <WebKit/_WKProcessPoolConfiguration.h>

#import <pal/spi/cocoa/NSAccessibilitySPI.h>
#import <wtf/SoftLinking.h>

#if PLATFORM(MAC)
#import <pal/spi/mac/NSApplicationSPI.h>
#endif

SOFT_LINK_LIBRARY(libAccessibility)
SOFT_LINK_CONSTANT(libAccessibility, kAXSApplicationAccessibilityEnabledNotification, CFStringRef);

TEST(WebKit, EnableAccessibilityCrash)
{
    {
        auto poolConfiguration = adoptNS([[_WKProcessPoolConfiguration alloc] init]);
        auto pool = adoptNS([[WKProcessPool alloc] _initWithConfiguration:poolConfiguration.get()]);
        auto viewConfiguration = adoptNS([[WKWebViewConfiguration alloc] init]);
        [viewConfiguration setProcessPool:pool.get()];
    }

    CFNotificationCenterPostNotification(CFNotificationCenterGetDarwinNotifyCenter(),  getkAXSApplicationAccessibilityEnabledNotification(), NULL, NULL, false);
}

#if WK_HAVE_C_SPI

TEST(WebKit, AccessibilityHasPreferencesServiceAccess)
{
    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    WKRetainPtr<WKContextRef> context = adoptWK(TestWebKitAPI::Util::createContextForInjectedBundleTest("InternalsInjectedBundleTest"));
    configuration.get().processPool = (WKProcessPool *)context.get();
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 300, 300) configuration:configuration.get() addToWindow:YES]);

    [webView synchronouslyLoadTestPageNamed:@"simple"];

    [NSApp accessibilitySetEnhancedUserInterfaceAttribute:@(YES)];

    auto sandboxAccess = [&] {
        return [webView stringByEvaluatingJavaScript:@"window.internals.hasSandboxMachLookupAccessToGlobalName('com.apple.WebKit.WebContent', 'com.apple.cfprefsd.daemon')"].boolValue;
    };

    [webView synchronouslyLoadTestPageNamed:@"simple"];

    ASSERT_TRUE(sandboxAccess());

    [NSApp accessibilitySetEnhancedUserInterfaceAttribute:@(NO)];
}

TEST(WebKit, AccessibilityHasNoPreferencesServiceAccessWhenPostingNotification)
{
    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    WKRetainPtr<WKContextRef> context = adoptWK(TestWebKitAPI::Util::createContextForInjectedBundleTest("InternalsInjectedBundleTest"));
    configuration.get().processPool = (WKProcessPool *)context.get();
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 300, 300) configuration:configuration.get() addToWindow:YES]);

    [webView synchronouslyLoadTestPageNamed:@"simple"];

    [[NSNotificationCenter defaultCenter] postNotificationName:NSApplicationDidChangeAccessibilityEnhancedUserInterfaceNotification object:nil userInfo:nil];

    auto sandboxAccess = [&] {
        return [webView stringByEvaluatingJavaScript:@"window.internals.hasSandboxMachLookupAccessToGlobalName('com.apple.WebKit.WebContent', 'com.apple.cfprefsd.daemon')"].boolValue;
    };

    [webView synchronouslyLoadTestPageNamed:@"simple"];

    ASSERT_TRUE(!sandboxAccess());
}

#if PLATFORM(IOS_FAMILY)
TEST(WebKit, AccessibilityHasFrontboardServiceAccess)
{
    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    WKRetainPtr<WKContextRef> context = adoptWK(TestWebKitAPI::Util::createContextForInjectedBundleTest("InternalsInjectedBundleTest"));
    configuration.get().processPool = (WKProcessPool *)context.get();
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 300, 300) configuration:configuration.get() addToWindow:YES]);

    [webView synchronouslyLoadTestPageNamed:@"simple"];

    [[NSNotificationCenter defaultCenter] postNotificationName:NSApplicationDidChangeAccessibilityEnhancedUserInterfaceNotification object:nil userInfo:nil];

    auto sandboxAccess = [&] {
        return [webView stringByEvaluatingJavaScript:@"window.internals.hasSandboxMachLookupAccessToGlobalName('com.apple.WebKit.WebContent', 'com.apple.frontboard.systemappservices')"].boolValue;
    };

    ASSERT_TRUE(sandboxAccess());
}
#endif // PLATFORM(IOS_FAMILY)

#endif
