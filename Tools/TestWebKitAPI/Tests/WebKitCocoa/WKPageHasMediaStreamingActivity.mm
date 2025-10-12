/*
 * Copyright (C) 2023 Apple Inc. All rights reserved.
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
#import "PlatformWebView.h"
#import "Test.h"
#import "TestWKWebView.h"
#import <JavaScriptCore/JavaScriptCore.h>
#import <WebKit/WKPagePrivate.h>
#import <WebKit/WKPreferencesPrivate.h>
#import <notify.h>
#import <wtf/MonotonicTime.h>
#import <wtf/darwin/DispatchExtras.h>

// This test loads file-with-video.html. Then it calls a JavaScript method to create a source buffer and play the video,
// waits for WKMediaNetworkingActivity notification to be fired.

namespace TestWebKitAPI {

static const char *WebKitMediaStreamingActivity = "com.apple.WebKit.mediaStreamingActivity";

TEST(WebKit, MSEHasMediaStreamingActivity)
{
    auto configuration = adoptNS([WKWebViewConfiguration new]);
    [[configuration preferences] _setMediaSourceEnabled:YES];
    [[configuration preferences] _setAllowFileAccessFromFileURLs:YES];
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 400, 400) configuration:configuration.get()]);
    [webView synchronouslyLoadTestPageNamed:@"file-with-mse"];

    // Bail out of the test early if the platform does not support MSE.
    if (![[webView objectByEvaluatingJavaScript:@"window.MediaSource !== undefined"] boolValue])
        return;
    // Test is only valid if either of those format is supported.
    auto typeSupported = [[webView objectByEvaluatingJavaScript:@"isMP4Supported() || isWebMVP9Supported() || isWebMOpusSupported()"] boolValue];
    if (!typeSupported)
        return;
    int token = NOTIFY_TOKEN_INVALID;
    if (notify_register_check(WebKitMediaStreamingActivity, &token) != NOTIFY_STATUS_OK)
        return;

    __block bool isMediaStreamingChanged = false;
    __block bool isMediaStreaming = false;

    int status = notify_register_dispatch(WebKitMediaStreamingActivity, &token, mainDispatchQueueSingleton(), ^(int token) {
        uint64_t state = 0;
        notify_get_state(token, &state);
        isMediaStreamingChanged = true;
        isMediaStreaming= !!state;
    });

    if (status != NOTIFY_STATUS_OK)
        return;

    [webView objectByEvaluatingJavaScript:@"playVideo()"];
    Util::run(&isMediaStreamingChanged);
    EXPECT_TRUE(isMediaStreaming);

    isMediaStreamingChanged = false;
    [webView objectByEvaluatingJavaScript:@"unloadVideo()"];
    Util::run(&isMediaStreamingChanged);
    EXPECT_FALSE(isMediaStreaming);
    notify_cancel(token);
}

#if ENABLE(MEDIA_SOURCE)
TEST(WebKit, ManagedMSEHasMediaStreamingActivity)
{
    auto configuration = adoptNS([WKWebViewConfiguration new]);
    [[configuration preferences] _setManagedMediaSourceEnabled:YES];
    [[configuration preferences] _setAllowFileAccessFromFileURLs:YES];
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 400, 400) configuration:configuration.get()]);
    [webView synchronouslyLoadTestPageNamed:@"file-with-managedmse"];

    // Bail out of the test early if the platform does not support Managed MSE.
    if (![[webView objectByEvaluatingJavaScript:@"window.ManagedMediaSource !== undefined"] boolValue])
        return;
    // Test is only valid if either of those format is supported.
    auto typeSupported = [[webView objectByEvaluatingJavaScript:@"isMP4Supported() || isWebMVP9Supported() || isWebMOpusSupported()"] boolValue];
    if (!typeSupported)
        return;
    int token = NOTIFY_TOKEN_INVALID;
    if (notify_register_check(WebKitMediaStreamingActivity, &token) != NOTIFY_STATUS_OK)
        return;

    __block bool isMediaStreamingChanged = false;
    __block bool isMediaStreaming = false;

    int status = notify_register_dispatch(WebKitMediaStreamingActivity, &token, mainDispatchQueueSingleton(), ^(int token) {
        uint64_t state = 0;
        notify_get_state(token, &state);
        isMediaStreamingChanged = true;
        isMediaStreaming= !!state;
    });

    if (status != NOTIFY_STATUS_OK)
        return;

    [webView objectByEvaluatingJavaScript:@"loadVideo()"];
    Util::run(&isMediaStreamingChanged);
    EXPECT_TRUE(isMediaStreaming);

    isMediaStreamingChanged = false;
    [webView objectByEvaluatingJavaScript:@"startStreaming()"];
    Util::run(&isMediaStreamingChanged);
    EXPECT_FALSE(isMediaStreaming);

    notify_cancel(token);
}

TEST(WebKit, ManagedMSEHasMediaStreamingActivityWithPolicy)
{
    auto configuration = adoptNS([WKWebViewConfiguration new]);
    [[configuration preferences] _setManagedMediaSourceEnabled:YES];
    [[configuration preferences] _setAllowFileAccessFromFileURLs:YES];
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 400, 400) configuration:configuration.get()]);
    [webView synchronouslyLoadTestPageNamed:@"file-with-managedmse"];

    // Bail out of the test early if the platform does not support Managed MSE.
    if (![[webView objectByEvaluatingJavaScript:@"window.ManagedMediaSource !== undefined"] boolValue])
        return;
    // Test is only valid if either of those format is supported.
    auto typeSupported = [[webView objectByEvaluatingJavaScript:@"isMP4Supported() || isWebMVP9Supported() || isWebMOpusSupported()"] boolValue];
    if (!typeSupported)
        return;
    int token = NOTIFY_TOKEN_INVALID;
    if (notify_register_check(WebKitMediaStreamingActivity, &token) != NOTIFY_STATUS_OK)
        return;

    __block bool isMediaStreamingChanged = false;
    __block bool isMediaStreaming = false;

    int status = notify_register_dispatch(WebKitMediaStreamingActivity, &token, mainDispatchQueueSingleton(), ^(int token) {
        uint64_t state = 0;
        notify_get_state(token, &state);
        isMediaStreamingChanged = true;
        isMediaStreaming= !!state;
    });

    if (status != NOTIFY_STATUS_OK)
        return;

    // Ensure that if it takes too long, notification is fired anyway.
    constexpr double highThreshold = 1;
    [[configuration preferences] _setManagedMediaSourceHighThreshold:highThreshold];

    auto startTime = MonotonicTime::now();
    [webView objectByEvaluatingJavaScript:@"loadVideo()"];
    Util::run(&isMediaStreamingChanged);
    EXPECT_TRUE(isMediaStreaming);

    // notification should be fired after 1s.
    isMediaStreamingChanged = false;
    Util::run(&isMediaStreamingChanged);
    EXPECT_FALSE(isMediaStreaming);

    Seconds duration = MonotonicTime::now() - startTime;
    Seconds expectedDuration { highThreshold };
    EXPECT_TRUE(duration >= expectedDuration);

    notify_cancel(token);
}
#endif

} // namespace TestWebKitAPI
