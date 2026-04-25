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

#import "Helpers/PlatformUtilities.h"
#import "Helpers/cocoa/TestElementFullscreenDelegate.h"
#import "Helpers/cocoa/TestWKWebView.h"
#import <WebKit/WKWebViewPrivate.h>
#import <WebKit/WebKit.h>

TEST(ElementFullscreen, IntersectionObserver)
{
    // This test must run in TestWebKitAPI.app
    if (![NSBundle.mainBundle.bundleIdentifier isEqualToString:@"org.webkit.TestWebKitAPI"])
        return;

    RetainPtr configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [configuration preferences].elementFullscreenEnabled = YES;
    RetainPtr fullscreenDelegate = adoptNS([[TestElementFullscreenDelegate alloc] init]);
    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
    [webView _setFullscreenDelegate:fullscreenDelegate.get()];

    [webView synchronouslyLoadHTMLString:
        @"<div style=\"height: 300%\"><div id=target style=\"height: 50%\"; }</div>"
        "<script>"
        "let observer = new IntersectionObserver(entries => { entries.forEach(entry => {"
        "    let log = target.appendChild(document.createElement('div'));"
        "    log.innerText = `intersecting(${entry.isIntersecting});"
        "};"
        "target.scrollIntoView();"
        "</script>"];

    [webView evaluateJavaScript:@"target.requestFullscreen()" completionHandler:nil];
    [fullscreenDelegate waitForDidEnterElementFullscreen];

    auto contents = [webView stringByEvaluatingJavaScript:@"target.innerText"];
    EXPECT_STREQ(contents.UTF8String, "intersecting(true)");
}
