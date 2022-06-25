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

#if PLATFORM(IOS_FAMILY)

#import "JavaScriptTest.h"
#import "PlatformUtilities.h"
#import "PlatformWebView.h"
#import "Test.h"
#import "TestNavigationDelegate.h"
#import "TestWKWebView.h"
#import <WebKit/WKWebViewPrivate.h>
#import <wtf/RetainPtr.h>

namespace TestWebKitAPI {

TEST(WebKit, ReloadWithDifferingInitialScale)
{
    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 375, 375) configuration:configuration.get()]);

    // It is important that we load from a file, not a HTML string, otherwise we don't
    // get a back-forward list item, and thus don't try to restore state.
    [webView synchronouslyLoadTestPageNamed:@"long-email-viewport"];

    [webView waitForNextPresentationUpdate];

    // "fixed-width-viewport" requests a width=375 viewport and has no overflowing
    // content, so we should end up with an initial scale of 1.
    EXPECT_EQ([webView scrollView].zoomScale, 1);

    // Unparenting the view will cause the current state
    // (including isInitialScale=YES and pageScaleFactor=1)
    // to be saved to the back-forward item.
    [webView removeFromSuperview];
    [webView waitForNextPresentationUpdate];

    [webView addToTestWindow];

    // Install the user script, so that the next time we load the page,
    // the document lays out very wide, causing the initial scale to be small.
    auto userScript = adoptNS([[WKUserScript alloc] initWithSource:@"document.body.style.width = '1500px'; setTimeout(function () { window.webkit.messageHandlers.testHandler.postMessage('ranUserScript'); }, 0)" injectionTime:WKUserScriptInjectionTimeAtDocumentEnd forMainFrameOnly:YES]);
    [[webView configuration].userContentController addUserScript:userScript.get()];

    // Reload, causing both the user script and the page state restoration code to run.
    [webView reload];

    [webView waitForMessage:@"ranUserScript"];
    [webView waitForNextPresentationUpdate];

    // Ensure that we did *not* restore pageScaleFactor to 1.
    EXPECT_EQ([webView scrollView].zoomScale, 0.25);
}

} // namespace TestWebKitAPI

#endif
