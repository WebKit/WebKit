/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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
#import "Helpers/Test.h"
#import "Helpers/cocoa/TestWKWebView.h"
#import <WebKit/WKPreferencesPrivate.h>
#import <WebKit/WKWebViewPrivate.h>
#import <WebKit/WKWebViewPrivateForTesting.h>
#import <notify.h>
#import <wtf/RetainPtr.h>
#import <wtf/darwin/DispatchExtras.h>

namespace TestWebKitAPI {

static constexpr auto WebKitMediaStreamingActivityNotificationName = "com.apple.WebKit.mediaStreamingActivity";

// rdar://180954825 — when a WebProcess holding a media streaming token is suspended, the
// UIProcess must drop the token so the NetworkProcess gets notified that media streaming
// has ended. Otherwise the cellular modem can stay pinned in an media-streaming state.
TEST(WebKit, ManagedMSEMediaStreamingActivityClearedOnProcessSuspend)
{
    RetainPtr configuration = adoptNS([WKWebViewConfiguration new]);
    [[configuration preferences] _setManagedMediaSourceEnabled:YES];
    [[configuration preferences] _setAllowFileAccessFromFileURLs:YES];
    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 400, 400) configuration:configuration.get()]);
    [webView synchronouslyLoadTestPageNamed:@"file-with-managedmse"];

    if (![[webView objectByEvaluatingJavaScript:@"window.ManagedMediaSource !== undefined"] boolValue])
        return;
    if (![[webView objectByEvaluatingJavaScript:@"isMP4Supported() || isWebMVP9Supported() || isWebMOpusSupported()"] boolValue])
        return;

    int token = NOTIFY_TOKEN_INVALID;
    if (notify_register_check(WebKitMediaStreamingActivityNotificationName, &token) != NOTIFY_STATUS_OK)
        return;

    __block bool isMediaStreamingChanged = false;
    __block bool isMediaStreaming = false;
    int status = notify_register_dispatch(WebKitMediaStreamingActivityNotificationName, &token, mainDispatchQueueSingleton(), ^(int notifyToken) {
        uint64_t state = 0;
        notify_get_state(notifyToken, &state);
        isMediaStreamingChanged = true;
        isMediaStreaming = !!state;
    });
    if (status != NOTIFY_STATUS_OK)
        return;

    [webView objectByEvaluatingJavaScript:@"loadVideo()"];
    Util::run(&isMediaStreamingChanged);
    EXPECT_TRUE(isMediaStreaming);

    isMediaStreamingChanged = false;
    [webView _setThrottleStateForTesting:0];
    Util::run(&isMediaStreamingChanged);
    EXPECT_FALSE(isMediaStreaming);

    notify_cancel(token);
}

TEST(WebKit, ManagedMSEMediaStreamingActivityClearedOnProcessCrash)
{
    RetainPtr configuration = adoptNS([WKWebViewConfiguration new]);
    [[configuration preferences] _setManagedMediaSourceEnabled:YES];
    [[configuration preferences] _setAllowFileAccessFromFileURLs:YES];
    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 400, 400) configuration:configuration.get()]);
    [webView synchronouslyLoadTestPageNamed:@"file-with-managedmse"];

    if (![[webView objectByEvaluatingJavaScript:@"window.ManagedMediaSource !== undefined"] boolValue])
        return;
    if (![[webView objectByEvaluatingJavaScript:@"isMP4Supported() || isWebMVP9Supported() || isWebMOpusSupported()"] boolValue])
        return;

    int token = NOTIFY_TOKEN_INVALID;
    if (notify_register_check(WebKitMediaStreamingActivityNotificationName, &token) != NOTIFY_STATUS_OK)
        return;

    __block bool isMediaStreamingChanged = false;
    __block bool isMediaStreaming = false;
    int status = notify_register_dispatch(WebKitMediaStreamingActivityNotificationName, &token, mainDispatchQueueSingleton(), ^(int notifyToken) {
        uint64_t state = 0;
        notify_get_state(notifyToken, &state);
        isMediaStreamingChanged = true;
        isMediaStreaming = !!state;
    });
    if (status != NOTIFY_STATUS_OK)
        return;

    [webView objectByEvaluatingJavaScript:@"loadVideo()"];
    Util::run(&isMediaStreamingChanged);
    EXPECT_TRUE(isMediaStreaming);

    isMediaStreamingChanged = false;
    [webView _killWebContentProcess];
    Util::run(&isMediaStreamingChanged);
    EXPECT_FALSE(isMediaStreaming);

    notify_cancel(token);
}

} // namespace TestWebKitAPI
