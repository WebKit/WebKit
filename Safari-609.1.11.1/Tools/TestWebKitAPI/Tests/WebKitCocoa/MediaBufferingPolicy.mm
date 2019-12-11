/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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

#include "config.h"

#if WK_HAVE_C_SPI

#import "PlatformUtilities.h"
#import "TestWKWebView.h"

#import <WebKit/WKWebViewConfigurationPrivate.h>
#import <WebKit/WKWebViewPrivate.h>

static void waitUntilBufferingPolicyIsEqualTo(WKWebView* webView, const char* expected)
{
    NSString* observed;
    int tries = 0;
    do {
        observed = [webView stringByEvaluatingJavaScript:@"window.internals.elementBufferingPolicy(document.querySelector('video'))"];
        if ([observed isEqualToString:@(expected)])
            break;

        TestWebKitAPI::Util::sleep(0.1);
    } while (++tries <= 100);

    EXPECT_WK_STREQ(expected, observed);
}

TEST(WebKit, MediaBufferingPolicy)
{
    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    auto context = adoptWK(TestWebKitAPI::Util::createContextForInjectedBundleTest("InternalsInjectedBundleTest"));
    configuration.get().processPool = (WKProcessPool *)context.get();
    configuration.get()._mediaDataLoadsAutomatically = YES;
    configuration.get().mediaTypesRequiringUserActionForPlayback = WKAudiovisualMediaTypeNone;
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 300, 300) configuration:configuration.get() addToWindow:YES]);

    __block bool isPlaying = false;
    [webView performAfterReceivingMessage:@"playing" action:^() { isPlaying = true; }];

    [webView synchronouslyLoadTestPageNamed:@"video-with-audio"];
    TestWebKitAPI::Util::run(&isPlaying);

    waitUntilBufferingPolicyIsEqualTo(webView.get(), "Default");

    [webView stringByEvaluatingJavaScript:@"document.querySelector('video').pause()"];

    // Suspending the process also forces a memory warning, which should purge whatever possible ASAP.
    [webView _processWillSuspendImminentlyForTesting];
    waitUntilBufferingPolicyIsEqualTo(webView.get(), "PurgeResources");

    // And should switch back to default when buffering is allowed.
    [webView _processDidResumeForTesting];
    waitUntilBufferingPolicyIsEqualTo(webView.get(), "Default");

    // All resources should be marked as purgeable when the document is hidden.
#if PLATFORM(MAC)
    [webView.get().window setIsVisible:NO];
#else
    webView.get().window.hidden = YES;
#endif
    waitUntilBufferingPolicyIsEqualTo(webView.get(), "MakeResourcesPurgeable");

#if PLATFORM(MAC)
    [webView.get().window setIsVisible:YES];
#else
    webView.get().window.hidden = NO;
#endif

    // Policy should go back to default when playback starts.
    isPlaying = false;
    [webView stringByEvaluatingJavaScript:@"go()"];
    TestWebKitAPI::Util::run(&isPlaying);

    waitUntilBufferingPolicyIsEqualTo(webView.get(), "Default");
}

#endif // WK_HAVE_C_SPI
