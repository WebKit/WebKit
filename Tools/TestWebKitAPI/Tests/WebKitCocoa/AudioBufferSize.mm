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
#import "Test.h"
#import "TestWKWebView.h"
#import "UserMediaCaptureUIDelegate.h"
#import <WebKit/WKPreferencesPrivate.h>
#import <WebKit/WKWebViewConfiguration.h>
#import <WebKit/WKWebViewConfigurationPrivate.h>
#import <WebKit/WKWebViewPrivate.h>

#import <wtf/RetainPtr.h>
#import <wtf/Seconds.h>

namespace TestWebKitAPI {

double waitForBufferSizeChange(WKWebView* webView, double oldSize)
{
    int tries = 0;
    do {
        auto size = [webView stringByEvaluatingJavaScript:@"window.internals.currentAudioBufferSize()"].doubleValue;
        if (size != oldSize)
            return size;

        TestWebKitAPI::Util::runFor(0.1_s);
    } while (++tries <= 100);

    ASSERT_NOT_REACHED();
    return 0;
}

TEST(WebKit, AudioBufferSize)
{
    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    auto context = adoptWK(TestWebKitAPI::Util::createContextForInjectedBundleTest("InternalsInjectedBundleTest"));

#if PLATFORM(IOS) || PLATFORM(VISION)
    configuration.get().allowsInlineMediaPlayback = YES;
    configuration.get()._inlineMediaPlaybackRequiresPlaysInlineAttribute = NO;
#endif
    configuration.get()._mediaDataLoadsAutomatically = YES;
    configuration.get().mediaTypesRequiringUserActionForPlayback = WKAudiovisualMediaTypeNone;
    configuration.get().processPool = (WKProcessPool *)context.get();
    configuration.get()._mediaCaptureEnabled = YES;
    auto preferences = [configuration preferences];
    preferences._mediaCaptureRequiresSecureConnection = NO;
    preferences._mockCaptureDevicesEnabled = YES;
    preferences._getUserMediaRequiresFocus = NO;

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 300, 300) configuration:configuration.get() addToWindow:YES]);
    auto delegate = adoptNS([[UserMediaCaptureUIDelegate alloc] init]);
    webView.get().UIDelegate = delegate.get();

    [webView synchronouslyLoadTestPageNamed:@"audio-buffer-size"];

    __block bool gotMessage = false;
    [webView performAfterReceivingMessage:@"playing" action:^{ gotMessage = true; }];
    [webView evaluateJavaScript:@"playVideo()" completionHandler:nil];
    TestWebKitAPI::Util::run(&gotMessage);
    auto bufferSizePlayingAudio = [webView stringByEvaluatingJavaScript:@"window.internals.currentAudioBufferSize()"].doubleValue;

    [webView evaluateJavaScript:@"startCapture()" completionHandler:nil];
    [delegate waitUntilPrompted];
    auto bufferSizeCapturingAudio = waitForBufferSizeChange(webView.get(), bufferSizePlayingAudio);
    ASSERT_LT(bufferSizeCapturingAudio, bufferSizePlayingAudio);

    [webView evaluateJavaScript:@"createWebAudioNode()" completionHandler:nil];
    auto bufferSizeWithWebAudio = waitForBufferSizeChange(webView.get(), bufferSizeCapturingAudio);
    ASSERT_LT(bufferSizeWithWebAudio, bufferSizeCapturingAudio);

    [webView evaluateJavaScript:@"disconnectWebAudioNode()" completionHandler:nil];
    auto bufferSizeWithoutWebAudio = waitForBufferSizeChange(webView.get(), bufferSizeWithWebAudio);
    ASSERT_GT(bufferSizeWithoutWebAudio, bufferSizeWithWebAudio);
    ASSERT_EQ(bufferSizeWithoutWebAudio, bufferSizeCapturingAudio);

    [webView evaluateJavaScript:@"stopCapture()" completionHandler:nil];
    auto bufferSizeWithoutCapture = waitForBufferSizeChange(webView.get(), bufferSizeWithoutWebAudio);
    ASSERT_GT(bufferSizeWithoutCapture, bufferSizeWithoutWebAudio);
    ASSERT_EQ(bufferSizeWithoutCapture, bufferSizePlayingAudio);

    [webView removeFromSuperview];
}

} // namespace TestWebKitAPI

#endif // WK_HAVE_C_SPI
