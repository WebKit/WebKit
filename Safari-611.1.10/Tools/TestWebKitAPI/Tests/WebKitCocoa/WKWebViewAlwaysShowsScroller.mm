/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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

#import "PlatformUtilities.h"
#import "TestWKWebView.h"
#import <WebKit/WKWebViewPrivate.h>
#import <wtf/RetainPtr.h>

TEST(WKWebView, AlwaysShowsScroller)
{
    RetainPtr<TestWKWebView> webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 300, 300)]);
    [webView synchronouslyLoadHTMLString:@"<style>::-webkit-scrollbar { width: 20px; height: 20px; }</style>"];

    ASSERT_EQ(300, [webView stringByEvaluatingJavaScript:@"document.body.scrollWidth"].integerValue);
    ASSERT_EQ(300, [webView stringByEvaluatingJavaScript:@"document.body.scrollHeight"].integerValue);

    [webView _setAlwaysShowsHorizontalScroller:YES];
    ASSERT_EQ(300, [webView stringByEvaluatingJavaScript:@"document.body.scrollWidth"].integerValue);
    ASSERT_EQ(280, [webView stringByEvaluatingJavaScript:@"document.body.scrollHeight"].integerValue);

    [webView _setAlwaysShowsVerticalScroller:YES];
    // When using custom scrollers, WebKit fails to dirty layout when we change this, so force it.
    [webView evaluateJavaScript:@"document.body.appendChild(document.createElement(\"div\"))" completionHandler:nil];
    ASSERT_EQ(280, [webView stringByEvaluatingJavaScript:@"document.body.scrollWidth"].integerValue);
    ASSERT_EQ(280, [webView stringByEvaluatingJavaScript:@"document.body.scrollHeight"].integerValue);

    [webView _setAlwaysShowsHorizontalScroller:NO];
    ASSERT_EQ(280, [webView stringByEvaluatingJavaScript:@"document.body.scrollWidth"].integerValue);
    ASSERT_EQ(300, [webView stringByEvaluatingJavaScript:@"document.body.scrollHeight"].integerValue);
}

#endif
