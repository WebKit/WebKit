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

#if PLATFORM(MAC)

#import "Helpers/PlatformUtilities.h"
#import "Helpers/cocoa/TestWKWebView.h"
#import <WebKit/WKPreferencesPrivate.h>
#import <WebKit/WKWebViewConfigurationPrivate.h>
#import <WebKit/WKWebViewPrivate.h>
#import <WebKit/WKWebViewPrivateForTesting.h>
#import <wtf/RetainPtr.h>

namespace TestWebKitAPI {

// A MediaSource-backed element fires no "progress" events, so the playback session model must learn
// about a new seekable range some other way. Without that, the fullscreen scrubber stays disabled
// because the UI process still holds the ranges captured when the element was first attached.
TEST(VideoControlsManager, MediaSourceSeekableRangesFollowDurationChange)
{
    RetainPtr configuration = adoptNS([WKWebViewConfiguration new]);
    [configuration setMediaTypesRequiringUserActionForPlayback:WKAudiovisualMediaTypeNone];
    [[configuration preferences] _setMediaSourceEnabled:YES];
    [[configuration preferences] _setAllowFileAccessFromFileURLs:YES];

    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
    [webView synchronouslyLoadTestPageNamed:@"mse-seekable-ranges"];

    if (![[webView objectByEvaluatingJavaScript:@"window.MediaSource !== undefined"] boolValue])
        return;
    if (![[webView objectByEvaluatingJavaScript:@"isMP4Supported() || isWebMVP9Supported() || isWebMOpusSupported()"] boolValue])
        return;

    EXPECT_TRUE([[webView objectByCallingAsyncFunction:@"return await playVideo()" withArguments:nil] boolValue]);

    Util::waitForConditionWithLogging([&] {
        return !![webView _hasActiveVideoForControlsManager];
    }, 10, @"Timed out while waiting for the video controls manager");
    EXPECT_TRUE([webView _hasActiveVideoForControlsManager]);

    // The appended segment gives the media source a duration, so the controls manager starts out
    // with a non-empty seekable range.
    Util::waitForConditionWithLogging([&] {
        return [webView _maximumSeekableTime] > 0;
    }, 10, @"Timed out while waiting for the initial seekable range");
    EXPECT_GT([webView _maximumSeekableTime], 0.0);

    [webView objectByEvaluatingJavaScript:@"setDuration(60)"];
    Util::waitForConditionWithLogging([&] {
        return [webView _maximumSeekableTime] == 60.0;
    }, 10, @"Timed out while waiting for the seekable range to follow the new duration");
    EXPECT_EQ([webView _maximumSeekableTime], 60.0);
}

} // namespace TestWebKitAPI

#endif // PLATFORM(MAC)
