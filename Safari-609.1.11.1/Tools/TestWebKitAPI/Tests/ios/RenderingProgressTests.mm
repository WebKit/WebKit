/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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

#include "config.h"

#include "PlatformUtilities.h"
#include "TestNavigationDelegate.h"
#include "TestWKWebView.h"
#include <WebKit/WKNavigationDelegatePrivate.h>
#include <wtf/BlockPtr.h>

@interface MissingResourceSchemeHandler : NSObject<WKURLSchemeHandler>
@end

@implementation MissingResourceSchemeHandler

- (void)webView:(WKWebView *)webView stopURLSchemeTask:(id<WKURLSchemeTask>)urlSchemeTask
{
}

- (void)webView:(WKWebView *)webView startURLSchemeTask:(id<WKURLSchemeTask>)urlSchemeTask
{
}

@end

#if PLATFORM(IOS_FAMILY)

TEST(RenderingProgressTests, DidRenderSignificantAmountOfText)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 156, 195)]);
    [webView _setObservedRenderingProgressEvents:_WKRenderingProgressEventDidRenderSignificantAmountOfText];

    bool observedSignificantRenderedText = false;
    auto navigationDelegate = adoptNS([[TestNavigationDelegate alloc] init]);
    [navigationDelegate setRenderingProgressDidChange:[&] (WKWebView *, _WKRenderingProgressEvents events) {
        if (events & _WKRenderingProgressEventDidRenderSignificantAmountOfText)
            observedSignificantRenderedText = true;
    }];

    [webView setNavigationDelegate:navigationDelegate.get()];
    [webView loadTestPageNamed:@"significant-text-milestone"];
    TestWebKitAPI::Util::run(&observedSignificantRenderedText);

    observedSignificantRenderedText = false;
    [webView loadTestPageNamed:@"significant-text-milestone-article"];
    TestWebKitAPI::Util::run(&observedSignificantRenderedText);
}

#endif // PLATFORM(IOS_FAMILY)

#if PLATFORM(WATCHOS)

TEST(RenderingProgressTests, FirstPaintWithSignificantArea)
{
    auto schemeHandler = adoptNS([[MissingResourceSchemeHandler alloc] init]);
    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [configuration setURLSchemeHandler:schemeHandler.get() forURLScheme:@"missing"];
    // A full-height, but very narrow web view (for instance, a navigation sidebar embedded in an app).
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 160, 568) configuration:configuration.get()]);
    [webView _setObservedRenderingProgressEvents:_WKRenderingProgressEventFirstPaintWithSignificantArea];

    bool observedSignificantPaint = false;
    auto navigationDelegate = adoptNS([[TestNavigationDelegate alloc] init]);
    [navigationDelegate setRenderingProgressDidChange:[&] (WKWebView *, _WKRenderingProgressEvents events) {
        if (events & _WKRenderingProgressEventFirstPaintWithSignificantArea)
            observedSignificantPaint = true;
    }];

    [webView setNavigationDelegate:navigationDelegate.get()];
    [webView loadTestPageNamed:@"paint-significant-area-milestone"];
    TestWebKitAPI::Util::run(&observedSignificantPaint);
}

#endif // PLATFORM(WATCHOS)
