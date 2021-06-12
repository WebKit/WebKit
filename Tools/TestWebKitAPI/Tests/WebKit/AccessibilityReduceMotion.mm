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

#if ENABLE(CFPREFS_DIRECT_MODE)

#import "PlatformUtilities.h"
#import "TestWKWebView.h"
#import "WKWebViewConfigurationExtras.h"
#import <WebKit/PreferenceObserver.h>
#import <WebKit/WKProcessPoolPrivate.h>
#import <WebKit/_WKProcessPoolConfiguration.h>

#if PLATFORM(MAC)
#import <pal/spi/mac/HIServicesSPI.h>
#endif

#if PLATFORM(IOS_FAMILY)
#include <pal/spi/cocoa/AccessibilitySupportSPI.h>
#include <wtf/SoftLinking.h>

SOFT_LINK_LIBRARY(libAccessibility)
SOFT_LINK_CONSTANT(libAccessibility, kAXSReduceMotionPreference, CFStringRef);
SOFT_LINK_CONSTANT(libAccessibility, kAXSReduceMotionChangedNotification, CFStringRef);

#define NOTIFICATION_CENTER CFNotificationCenterGetDarwinNotifyCenter()
#define REDUCED_MOTION_PREFERENCE getkAXSReduceMotionPreference()
#define REDUCED_MOTION_CHANGED_NOTIFICATION getkAXSReduceMotionChangedNotification()
#define ACCESSIBILITY_DOMAIN CFSTR("com.apple.Accessibility")
#else
#define NOTIFICATION_CENTER CFNotificationCenterGetDistributedCenter()
#define REDUCED_MOTION_PREFERENCE kAXInterfaceReduceMotionKey
#define REDUCED_MOTION_CHANGED_NOTIFICATION kAXInterfaceReduceMotionStatusDidChangeNotification
#define ACCESSIBILITY_DOMAIN CFSTR("com.apple.universalaccess")
#endif

static bool receivedPreferenceNotification = false;

@interface WKPreferenceObserverForTesting : WKPreferenceObserver
- (void)preferenceDidChange:(NSString *)domain key:(NSString *)key encodedValue:(NSString *)encodedValue;
@end

@implementation WKPreferenceObserverForTesting
- (void)preferenceDidChange:(NSString *)domain key:(NSString *)key encodedValue:(NSString *)encodedValue
{
    receivedPreferenceNotification = true;
    [super preferenceDidChange:domain key:key encodedValue:encodedValue];
}
@end

TEST(WebKit, AccessibilityReduceMotion)
{
    WKWebViewConfiguration *configuration = [WKWebViewConfiguration _test_configurationWithTestPlugInClassName:@"WebProcessPlugInWithInternals" configureJSCForTesting:YES];
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 300, 300) configuration:configuration addToWindow:YES]);

    CFPreferencesSetAppValue(REDUCED_MOTION_PREFERENCE, kCFBooleanFalse, ACCESSIBILITY_DOMAIN);

    auto observer = adoptNS([[WKPreferenceObserverForTesting alloc] init]);

    [webView synchronouslyLoadTestPageNamed:@"simple"];

    auto reduceMotion = [&] {
        return [webView stringByEvaluatingJavaScript:@"window.internals.userPrefersReducedMotion()"].boolValue;
    };

    ASSERT_FALSE(reduceMotion());

    CFPreferencesSetAppValue(REDUCED_MOTION_PREFERENCE, kCFBooleanTrue, ACCESSIBILITY_DOMAIN);

    TestWebKitAPI::Util::run(&receivedPreferenceNotification);

    [webView synchronouslyLoadTestPageNamed:@"simple"];

    ASSERT_TRUE(reduceMotion());

    CFPreferencesSetAppValue(REDUCED_MOTION_PREFERENCE, nullptr, ACCESSIBILITY_DOMAIN);
}

#endif
