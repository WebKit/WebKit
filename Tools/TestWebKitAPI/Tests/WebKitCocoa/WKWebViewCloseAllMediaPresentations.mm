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

#import "config.h"

#import "PlatformUtilities.h"
#import "TestWKWebView.h"
#import <WebCore/PictureInPictureSupport.h>
#import <WebKit/WKPreferencesPrivate.h>
#import <WebKit/WKWebViewPrivate.h>
#import <wtf/RetainPtr.h>

TEST(WKWebViewCloseAllMediaPresentations, PictureInPicture)
{
    if (!WebCore::supportsPictureInPicture())
        return;

    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    configuration.get().preferences._allowsPictureInPictureMediaPlayback = YES;
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get() addToWindow:YES]);

    [webView synchronouslyLoadHTMLString:@"<video src=video-with-audio.mp4 webkit-playsinline playsinline></video>"];

    [webView objectByEvaluatingJavaScriptWithUserGesture:@"document.querySelector('video').webkitSetPresentationMode('picture-in-picture')"];

    do {
        id result = [webView objectByEvaluatingJavaScript:@"document.querySelector('video').webkitDisplayingFullscreen"];
        if ([result boolValue])
            break;

        TestWebKitAPI::Util::sleep(0.5);
    } while (true);

    [webView objectByEvaluatingJavaScript:@"document.querySelector('video').addEventListener('webkitpresentationmodechanged', event => { window.webkit.messageHandlers.testHandler.postMessage('presentationmodechanged'); });"];

    __block bool presentationModeChanged = false;
    [webView performAfterReceivingMessage:@"presentationmodechanged" action:^{ presentationModeChanged = true; }];

    [webView _closeAllMediaPresentations];

    TestWebKitAPI::Util::run(&presentationModeChanged);

    EXPECT_STREQ([webView stringByEvaluatingJavaScript:@"document.querySelector('video').webkitPresentationMode"].UTF8String, "inline");
}

#if ENABLE(FULLSCREEN_API)

TEST(WKWebViewCloseAllMediaPresentations, VideoFullscreen)
{
    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    configuration.get().preferences._fullScreenEnabled = YES;
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get() addToWindow:YES]);

    [webView synchronouslyLoadHTMLString:@"<video src=video-with-audio.mp4 webkit-playsinline playsinline></video>"];
    [webView objectByEvaluatingJavaScript:@"document.querySelector('video').addEventListener('webkitpresentationmodechanged', event => { window.webkit.messageHandlers.testHandler.postMessage('presentationmodechanged'); });"];

    __block bool presentationModeChanged = false;
    [webView performAfterReceivingMessage:@"presentationmodechanged" action:^{ presentationModeChanged = true; }];

    [webView objectByEvaluatingJavaScriptWithUserGesture:@"document.querySelector('video').webkitEnterFullscreen()"];

    TestWebKitAPI::Util::run(&presentationModeChanged);

    presentationModeChanged = false;
    [webView performAfterReceivingMessage:@"presentationmodechanged" action:^{ presentationModeChanged = true; }];

    [webView _closeAllMediaPresentations];

    TestWebKitAPI::Util::run(&presentationModeChanged);

    EXPECT_STREQ([webView stringByEvaluatingJavaScript:@"document.querySelector('video').webkitPresentationMode"].UTF8String, "inline");
}

TEST(WKWebViewCloseAllMediaPresentations, ElementFullscreen)
{
    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    configuration.get().preferences._fullScreenEnabled = YES;
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get() addToWindow:YES]);

    [webView synchronouslyLoadHTMLString:@"<div id=\"target\" style=\"width:100px;height:100px;background-color:red\"></div>"];
    [webView objectByEvaluatingJavaScript:@"document.querySelector('#target').addEventListener('webkitfullscreenchange', event => { window.webkit.messageHandlers.testHandler.postMessage('fullscreenchange'); });"];

    __block bool fullscreenModeChanged = false;
    [webView performAfterReceivingMessage:@"fullscreenchange" action:^{ fullscreenModeChanged = true; }];

    [webView objectByEvaluatingJavaScriptWithUserGesture:@"document.querySelector('#target').webkitRequestFullscreen()"];

    TestWebKitAPI::Util::run(&fullscreenModeChanged);

    fullscreenModeChanged = false;
    [webView performAfterReceivingMessage:@"fullscreenchange" action:^{ fullscreenModeChanged = true; }];

    [webView _closeAllMediaPresentations];

    TestWebKitAPI::Util::run(&fullscreenModeChanged);

    EXPECT_STREQ([webView stringByEvaluatingJavaScript:@"document.webkitFullscreenElement"].UTF8String, "<null>");
}

#endif
