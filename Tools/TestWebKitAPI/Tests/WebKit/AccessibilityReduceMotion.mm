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
#import <WebKit/WKProcessPoolPrivate.h>
#import <WebKit/_WKProcessPoolConfiguration.h>

#import <pal/spi/mac/HIServicesSPI.h>

static bool reduceMotionNotificationReceived = false;
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
    RetainPtr<NSObject> accessibilityObserver = [[NSDistributedNotificationCenter defaultCenter] addObserverForName:(__bridge id)kAXInterfaceReduceMotionStatusDidChangeNotification object:nil queue:[NSOperationQueue currentQueue] usingBlock:^(NSNotification *note) {
        reduceMotionNotificationReceived = true;
    }];

    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    WKRetainPtr<WKContextRef> context = adoptWK(TestWebKitAPI::Util::createContextForInjectedBundleTest("InternalsInjectedBundleTest"));
    configuration.get().processPool = (WKProcessPool *)context.get();
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 300, 300) configuration:configuration.get() addToWindow:YES]);

    CFPreferencesSetAppValue(kAXInterfaceReduceMotionKey, kCFBooleanFalse, CFSTR("com.apple.universalaccess"));

    auto observer = adoptNS([[WKPreferenceObserverForTesting alloc] init]);

    [webView synchronouslyLoadTestPageNamed:@"simple"];

    CFPreferencesSetAppValue(kAXInterfaceReduceMotionKey, kCFBooleanTrue, CFSTR("com.apple.universalaccess"));

    TestWebKitAPI::Util::run(&receivedPreferenceNotification);

    auto reduceMotion = [&] {
        return [webView stringByEvaluatingJavaScript:@"window.internals.userPrefersReducedMotion()"].boolValue;
    };
    
    [webView synchronouslyLoadTestPageNamed:@"simple"];

    TestWebKitAPI::Util::run(&reduceMotionNotificationReceived);

    ASSERT_TRUE(reduceMotion());

    CFPreferencesSetAppValue(kAXInterfaceReduceMotionKey, nullptr, CFSTR("com.apple.universalaccess"));

    [[NSNotificationCenter defaultCenter] removeObserver:accessibilityObserver.get()];
}

#endif
