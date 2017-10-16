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

#include "config.h"

#if WK_API_ENABLED && PLATFORM(MAC)

#import "PlatformUtilities.h"
#import "TestWKWebView.h"
#import <WebKit/WKPreferencesPrivate.h>
#import <WebKit/WKPreferencesRefPrivate.h>
#import <WebKit/WKWebViewConfigurationPrivate.h>
#import <wtf/RetainPtr.h>
#import <wtf/text/WTFString.h>

@interface WKWebView ()
- (void)paste:(id)sender;
@end

static RetainPtr<TestWKWebView> createWebViewWithCustomPasteboardDataEnabled()
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 400, 400)]);
    auto preferences = (WKPreferencesRef)[[webView configuration] preferences];
    WKPreferencesSetDataTransferItemsEnabled(preferences, true);
    WKPreferencesSetCustomPasteboardDataEnabled(preferences, true);
    return webView;
}

TEST(PasteWebArchive, ExposesHTMLTypeInDataTransfer)
{
    auto* url = [NSURL URLWithString:@"file:///some-file.html"];
    auto* markup = [@"<!DOCTYPE html><html><body><meta content=\"secret\"><b onmouseover=\"dangerousCode()\">hello</b>"
        "<!-- secret-->, world<script>dangerousCode()</script></html>" dataUsingEncoding:NSUTF8StringEncoding];
    auto mainResource = adoptNS([[WebResource alloc] initWithData:markup URL:url MIMEType:@"text/html" textEncodingName:@"utf-8" frameName:nil]);
    auto archive = adoptNS([[WebArchive alloc] initWithMainResource:mainResource.get() subresources:nil subframeArchives:nil]);

    [[NSPasteboard generalPasteboard] declareTypes:@[WebArchivePboardType] owner:nil];
    [[NSPasteboard generalPasteboard] setData:[archive data] forType:WebArchivePboardType];

    auto webView = createWebViewWithCustomPasteboardDataEnabled();
    [webView synchronouslyLoadTestPageNamed:@"paste-rtfd"];
    [webView paste:nil];

    EXPECT_TRUE([webView stringByEvaluatingJavaScript:@"clipboardData.types.includes('text/html')"]);
    [webView stringByEvaluatingJavaScript:@"html = clipboardData.values[0]"];
    EXPECT_TRUE([webView stringByEvaluatingJavaScript:@"html.includes('hello')"].boolValue);
    EXPECT_TRUE([webView stringByEvaluatingJavaScript:@"html.includes(', world')"].boolValue);
    EXPECT_FALSE([webView stringByEvaluatingJavaScript:@"html.includes('secret')"].boolValue);
    EXPECT_FALSE([webView stringByEvaluatingJavaScript:@"html.includes('dangerousCode')"].boolValue);

    [webView stringByEvaluatingJavaScript:@"editor.innerHTML = html; editor.focus(); getSelection().setPosition(editor, 0)"];
    EXPECT_TRUE([webView stringByEvaluatingJavaScript:@"document.queryCommandState('bold')"].boolValue);
    [webView stringByEvaluatingJavaScript:@"getSelection().modify('move', 'forward', 'lineboundary')"];
    EXPECT_FALSE([webView stringByEvaluatingJavaScript:@"document.queryCommandState('bold')"].boolValue);
    EXPECT_WK_STREQ("hello, world", [webView stringByEvaluatingJavaScript:@"editor.textContent"]);
}

#endif // WK_API_ENABLED && PLATFORM(MAC)



