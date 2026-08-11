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

#import "Helpers/PlatformUtilities.h"
#import "Helpers/cocoa/TestWKWebView.h"
#import <WebKit/WKProcessPoolPrivate.h>
#import <WebKit/WKWebViewPrivateForTesting.h>
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
        RetainPtr poolConfiguration = adoptNS([[_WKProcessPoolConfiguration alloc] init]);
        RetainPtr pool = adoptNS([[WKProcessPool alloc] _initWithConfiguration:poolConfiguration.get()]);
        RetainPtr viewConfiguration = adoptNS([[WKWebViewConfiguration alloc] init]);
        [viewConfiguration setProcessPool:pool.get()];
    }

    CFNotificationCenterPostNotification(CFNotificationCenterGetDarwinNotifyCenter(),  getkAXSApplicationAccessibilityEnabledNotificationSingleton(), NULL, NULL, false);
}

#if WK_HAVE_C_SPI

TEST(WebKit, AccessibilityHasPreferencesServiceAccess)
{
    RetainPtr configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    WKRetainPtr<WKContextRef> context = adoptWK(TestWebKitAPI::Util::createContextForInjectedBundleTest("InternalsInjectedBundleTest"));
    configuration.get().processPool = (WKProcessPool *)context.get();
    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 300, 300) configuration:configuration.get() addToWindow:YES]);

    [webView synchronouslyLoadTestPageNamed:@"simple"];

    [NSApp accessibilitySetEnhancedUserInterfaceAttribute:@(YES)];

    auto sandboxAccess = [&] {
        return [webView stringByEvaluatingJavaScript:@"window.internals.hasSandboxMachLookupAccessToGlobalName('com.apple.WebKit.WebContent', 'com.apple.cfprefsd.daemon')"].boolValue;
    };

    [webView synchronouslyLoadTestPageNamed:@"simple"];

#if ENABLE(CFPREFS_DIRECT_MODE)
    ASSERT_FALSE(sandboxAccess());
#else
    ASSERT_TRUE(sandboxAccess());
#endif

    [NSApp accessibilitySetEnhancedUserInterfaceAttribute:@(NO)];
}

#if ENABLE(CFPREFS_DIRECT_MODE)
TEST(WebKit, AccessibilityHasNoPreferencesServiceAccessWhenPostingNotification)
{
    RetainPtr configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    WKRetainPtr<WKContextRef> context = adoptWK(TestWebKitAPI::Util::createContextForInjectedBundleTest("InternalsInjectedBundleTest"));
    configuration.get().processPool = (WKProcessPool *)context.get();
    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 300, 300) configuration:configuration.get() addToWindow:YES]);

    [webView synchronouslyLoadTestPageNamed:@"simple"];

    [[NSNotificationCenter defaultCenter] postNotificationName:NSApplicationDidChangeAccessibilityEnhancedUserInterfaceNotification object:nil userInfo:nil];

    auto sandboxAccess = [&] {
        return [webView stringByEvaluatingJavaScript:@"window.internals.hasSandboxMachLookupAccessToGlobalName('com.apple.WebKit.WebContent', 'com.apple.cfprefsd.daemon')"].boolValue;
    };

    [webView synchronouslyLoadTestPageNamed:@"simple"];

    ASSERT_TRUE(!sandboxAccess());
}
#endif

#if PLATFORM(IOS_FAMILY)
TEST(WebKit, AccessibilityHasFrontboardServiceAccess)
{
    RetainPtr configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    WKRetainPtr<WKContextRef> context = adoptWK(TestWebKitAPI::Util::createContextForInjectedBundleTest("InternalsInjectedBundleTest"));
    configuration.get().processPool = (WKProcessPool *)context.get();
    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 300, 300) configuration:configuration.get() addToWindow:YES]);

    [webView synchronouslyLoadTestPageNamed:@"simple"];

    [[NSNotificationCenter defaultCenter] postNotificationName:NSApplicationDidChangeAccessibilityEnhancedUserInterfaceNotification object:nil userInfo:nil];

    auto sandboxAccess = [&] {
        return [webView stringByEvaluatingJavaScript:@"window.internals.hasSandboxMachLookupAccessToGlobalName('com.apple.WebKit.WebContent', 'com.apple.frontboard.systemappservices')"].boolValue;
    };

    ASSERT_TRUE(sandboxAccess());
}
#endif // PLATFORM(IOS_FAMILY)

#endif

#if PLATFORM(MAC) && USE(RUNNINGBOARD)

TEST(WebKit, AccessibilityChildrenPreventsProcessSuspensionOnFrontmostTab)
{
    RetainPtr configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 300, 300) configuration:configuration.get() addToWindow:YES]);

    [webView synchronouslyLoadTestPageNamed:@"simple"];

    // Page should acquire the remote AX element activity when -accessibilityChildren runs to keep itself running as long as it's attached to a window.
    // The remote AX element token arrives asynchronously from the WebProcess, so poll until it is available.
    RetainPtr<NSArray> children;
    EXPECT_TRUE(TestWebKitAPI::Util::waitFor([&] {
        children = [webView accessibilityChildren];
        return [children count] > 0;
    }));
    EXPECT_TRUE([webView _hasAccessibilityActivityForTesting]);

    // Page should drop the AX activity after being removed from the window. Test in a Util::waitFor
    // since it takes a run loop turn for the window removal to propagate.
    [webView removeFromTestWindow];
    EXPECT_TRUE(TestWebKitAPI::Util::waitFor([&] {
        return ![webView _hasAccessibilityActivityForTesting];
    }));

    // Page should re-acquire the AX activity after being added back to the window.
    [webView addToTestWindow];
    EXPECT_TRUE([webView _hasAccessibilityActivityForTesting]);
}

#endif

