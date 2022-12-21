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
#import "PlatformWebView.h"
#import "Test.h"
#import <JavaScriptCore/JavaScriptCore.h>
#import <WebKit/WKPagePrivate.h>
#import <WebKit/WKPreferencesPrivate.h>

// This test loads file-with-video.html. Then it calls a JavaScript method to create a source buffer and play the video,
// waits for WKMediaNetworkingActivity notification to be fired.

namespace TestWebKitAPI {

static NSString * const WebKitMediaStreamingActivity = @"WebKitMediaStreamingActivity";

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

    __block bool isMediaStreamingChanged = false;
    __block bool isMediaStreaming = false;

    auto localeChangeObserver = [[NSNotificationCenter defaultCenter] addObserverForName:WebKitMediaStreamingActivity object: nil queue: nil usingBlock:^(NSNotification *note) {
        isMediaStreamingChanged = true;
        if ([[note object] isKindOfClass:[NSNumber class]])
            isMediaStreaming = [[note object] boolValue];
    }];

    [webView objectByEvaluatingJavaScript:@"playVideo()"];
    EXPECT_TRUE(isMediaStreamingChanged);
    EXPECT_TRUE(isMediaStreaming);

    isMediaStreamingChanged = false;
    [webView objectByEvaluatingJavaScript:@"unloadVideo()"];
    EXPECT_TRUE(isMediaStreamingChanged);
    EXPECT_FALSE(isMediaStreaming);

    [[NSNotificationCenter defaultCenter] removeObserver:localeChangeObserver];
}

} // namespace TestWebKitAPI
