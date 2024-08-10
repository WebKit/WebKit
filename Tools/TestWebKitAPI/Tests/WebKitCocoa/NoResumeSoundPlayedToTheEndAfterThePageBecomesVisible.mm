/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
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
#import "Test.h"
#import "TestWKWebView.h"
#import <WebKit/WKPreferencesPrivate.h>
#import <WebKit/WKUIDelegatePrivate.h>
#import <WebKit/WKWebViewConfigurationPrivate.h>
#import <WebKit/_WKFullscreenDelegate.h>
#import <wtf/RetainPtr.h>
#import <wtf/Seconds.h>

// FIXME: when rdar://132766445 is resolved
#if PLATFORM(IOS)
TEST(WebKit, DISABLED_NoResumeSoundPlayedToTheEndAfterThePageBecomesVisible)
#else
TEST(WebKit, NoResumeSoundPlayedToTheEndAfterThePageBecomesVisible)
#endif
{
    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 500, 500) configuration:configuration.get() addToWindow:YES]);

    bool isHidden = false;
    [webView performAfterReceivingMessage:@"hidden" action:[&] { isHidden = true; }];
    [webView objectByEvaluatingJavaScript:@"document.addEventListener('visibilitychange', event => { if (document.hidden) window.webkit.messageHandlers.testHandler.postMessage('hidden') })"];


    bool isEnded = false;
    [webView performAfterReceivingMessage:@"audioEnded" action:[&] { isEnded = true; }];

    [webView synchronouslyLoadTestPageNamed:@"playable-audio"];

#if PLATFORM(MAC)
    [webView.get().window setIsVisible:NO];
#else
    webView.get().window.hidden = YES;
#endif

    TestWebKitAPI::Util::run(&isHidden);

    TestWebKitAPI::Util::runFor(1_s);

    [webView evaluateJavaScript:@"window.playAudio()" completionHandler:nil];

    TestWebKitAPI::Util::run(&isEnded);

    bool isVisible = false;
    [webView performAfterReceivingMessage:@"visible" action:[&] { isVisible = true; }];
    [webView objectByEvaluatingJavaScript:@"document.addEventListener('visibilitychange', event => { if (!document.hidden) window.webkit.messageHandlers.testHandler.postMessage('visible') })"];


#if PLATFORM(MAC)
    [webView.get().window setIsVisible:YES];
#else
    webView.get().window.hidden = NO;
#endif

    TestWebKitAPI::Util::run(&isVisible);

    ASSERT_STREQ([[webView stringByEvaluatingJavaScript:@"window.audioEl.paused"] UTF8String], "1");
}
