/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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

#if PLATFORM(IOS_FAMILY)

#import "Helpers/PlatformUtilities.h"
#import "Helpers/cocoa/TestElementFullscreenDelegate.h"
#import "Helpers/cocoa/TestWKWebView.h"
#import <WebKit/WKWebViewConfigurationPrivate.h>
#import <WebKit/WKWebViewPrivate.h>
#import <WebKit/WebKit.h>

TEST(ElementFullscreen, VideoInShadowDOMIsChildOfElementFullscreen)
{
    if (![NSBundle.mainBundle.bundleIdentifier isEqualToString:@"org.webkit.TestWebKitAPI"])
        return;

    RetainPtr configuration = retainPtr([WKWebViewConfiguration _test_configurationWithTestPlugInClassName:@"WebProcessPlugInWithInternals" configureJSCForTesting:YES]);
    [configuration setAllowsInlineMediaPlayback:YES];
    [configuration preferences].elementFullscreenEnabled = YES;
    [configuration setMediaTypesRequiringUserActionForPlayback:WKAudiovisualMediaTypeNone];
    RetainPtr fullscreenDelegate = adoptNS([[TestElementFullscreenDelegate alloc] init]);
    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
    [webView _setFullscreenDelegate:fullscreenDelegate.get()];

    [webView synchronouslyLoadHTMLString:@"<div id='host'></div>"
        "<script>"
        "const host = document.getElementById('host');"
        "const shadowRoot = host.attachShadow({ mode: 'closed' });"
        "window.video = document.createElement('video');"
        "video.src = 'video-with-audio.mp4';"
        "video.muted = true;"
        "video.playsInline = true;"
        "shadowRoot.appendChild(video);"
        "</script>"];

    NSString *queryResult = [webView objectByEvaluatingJavaScript:@"document.getElementById('host').querySelector('video') === null ? 'true' : 'false'"];
    EXPECT_WK_STREQ(@"true", queryResult);

    [webView objectByEvaluatingJavaScript:@"video.play()"];
    [webView waitForNextPresentationUpdate];

    NSString *beforeFullscreen = [webView stringByEvaluatingJavaScript:@"window.internals.isChildOfElementFullscreen(video)"];
    EXPECT_WK_STREQ(@"false", beforeFullscreen);

    [webView evaluateJavaScript:@"document.getElementById('host').requestFullscreen()" completionHandler:nil];
    [fullscreenDelegate waitForDidEnterElementFullscreen];
    [webView waitForNextPresentationUpdate];

    NSString *duringFullscreen = [webView stringByEvaluatingJavaScript:@"window.internals.isChildOfElementFullscreen(video)"];
    EXPECT_WK_STREQ(@"true", duringFullscreen);
}

#endif
