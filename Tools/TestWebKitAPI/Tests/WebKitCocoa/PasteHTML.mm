/*
 * Copyright (C) 2017-2018 Apple Inc. All rights reserved.
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

#if WK_API_ENABLED && PLATFORM(COCOA)

#import "PlatformUtilities.h"
#import "TestWKWebView.h"
#import <WebCore/LegacyNSPasteboardTypes.h>
#import <WebKit/WKPreferencesPrivate.h>
#import <WebKit/WKPreferencesRefPrivate.h>
#import <WebKit/WKWebViewConfigurationPrivate.h>
#import <wtf/RetainPtr.h>
#import <wtf/text/WTFString.h>

#if PLATFORM(IOS)
#include <MobileCoreServices/MobileCoreServices.h>
#endif

@interface WKWebView ()
- (void)paste:(id)sender;
@end

#if PLATFORM(MAC)
void writeHTMLToPasteboard(NSString *html)
{
    [[NSPasteboard generalPasteboard] declareTypes:@[WebCore::legacyHTMLPasteboardType()] owner:nil];
    [[NSPasteboard generalPasteboard] setString:html forType:WebCore::legacyHTMLPasteboardType()];
}
#else
void writeHTMLToPasteboard(NSString *html)
{
    [[UIPasteboard generalPasteboard] setItems:@[@{ (NSString *)kUTTypeHTML : html}]];
}
#endif

static RetainPtr<TestWKWebView> createWebViewWithCustomPasteboardDataSetting(bool enabled)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 400, 400)]);
    auto preferences = (WKPreferencesRef)[[webView configuration] preferences];
    WKPreferencesSetDataTransferItemsEnabled(preferences, true);
    WKPreferencesSetCustomPasteboardDataEnabled(preferences, enabled);
    return webView;
}

TEST(PasteHTML, ExposesHTMLTypeInDataTransfer)
{
    auto webView = createWebViewWithCustomPasteboardDataSetting(true);
    [webView synchronouslyLoadTestPageNamed:@"paste-rtfd"];

    writeHTMLToPasteboard(@"<!DOCTYPE html><html><body><p><u>hello</u>, world</p></body></html>");
    [webView paste:nil];

    EXPECT_TRUE([webView stringByEvaluatingJavaScript:@"clipboardData.types.includes('text/html')"]);
    [webView stringByEvaluatingJavaScript:@"editor.innerHTML = clipboardData.values[0]; editor.focus()"];
    EXPECT_TRUE([webView stringByEvaluatingJavaScript:@"document.queryCommandState('underline')"].boolValue);
    [webView stringByEvaluatingJavaScript:@"getSelection().modify('move', 'forward', 'lineboundary')"];
    EXPECT_FALSE([webView stringByEvaluatingJavaScript:@"document.queryCommandState('underline')"].boolValue);
    EXPECT_WK_STREQ("hello, world", [webView stringByEvaluatingJavaScript:@"editor.textContent"]);
}

TEST(PasteHTML, SanitizesHTML)
{
    auto webView = createWebViewWithCustomPasteboardDataSetting(true);
    [webView synchronouslyLoadTestPageNamed:@"paste-rtfd"];

    writeHTMLToPasteboard(@"<!DOCTYPE html><meta content=\"secret\"><b onmouseover=\"dangerousCode()\">hello</b>"
        "<!-- secret-->, world<script>dangerousCode()</script>';");
    [webView paste:nil];

    EXPECT_TRUE([webView stringByEvaluatingJavaScript:@"clipboardData.types.includes('text/html')"].boolValue);
    EXPECT_TRUE([webView stringByEvaluatingJavaScript:@"clipboardData.values[0].includes('hello')"].boolValue);
    EXPECT_TRUE([webView stringByEvaluatingJavaScript:@"clipboardData.values[0].includes('world')"].boolValue);
    EXPECT_FALSE([webView stringByEvaluatingJavaScript:@"clipboardData.values[0].includes('secret')"].boolValue);
    EXPECT_FALSE([webView stringByEvaluatingJavaScript:@"clipboardData.values[0].includes('dangerousCode')"].boolValue);
}

TEST(PasteHTML, DoesNotSanitizeHTMLWhenCustomPasteboardDataIsDisabled)
{
    auto webView = createWebViewWithCustomPasteboardDataSetting(false);
    [webView synchronouslyLoadTestPageNamed:@"paste-rtfd"];

    writeHTMLToPasteboard(@"<!DOCTYPE html><meta content=\"secret\"><b onmouseover=\"dangerousCode()\">hello</b>"
        "<!-- secret-->, world<script>dangerousCode()</script>';");
    [webView paste:nil];

    EXPECT_TRUE([webView stringByEvaluatingJavaScript:@"clipboardData.types.includes('text/html')"].boolValue);
    EXPECT_TRUE([webView stringByEvaluatingJavaScript:@"clipboardData.values[0].includes('hello')"].boolValue);
    EXPECT_TRUE([webView stringByEvaluatingJavaScript:@"clipboardData.values[0].includes('world')"].boolValue);
    EXPECT_TRUE([webView stringByEvaluatingJavaScript:@"clipboardData.values[0].includes('secret')"].boolValue);
    EXPECT_TRUE([webView stringByEvaluatingJavaScript:@"clipboardData.values[0].includes('dangerousCode')"].boolValue);
}

TEST(PasteHTML, StripsFileURLs)
{
    auto webView = createWebViewWithCustomPasteboardDataSetting(true);
    [webView synchronouslyLoadTestPageNamed:@"paste-rtfd"];

    writeHTMLToPasteboard(@"<!DOCTYPE html><html><body><a alt='hello' href='file:///private/var/folders/secret/files/'>world</a>");
    [webView paste:nil];

    EXPECT_TRUE([webView stringByEvaluatingJavaScript:@"clipboardData.types.includes('text/html')"].boolValue);
    EXPECT_TRUE([webView stringByEvaluatingJavaScript:@"clipboardData.values[0].includes('hello')"].boolValue);
    EXPECT_TRUE([webView stringByEvaluatingJavaScript:@"clipboardData.values[0].includes('world')"].boolValue);
    EXPECT_FALSE([webView stringByEvaluatingJavaScript:@"clipboardData.values[0].includes('secret')"].boolValue);
}

TEST(PasteHTML, DoesNotStripFileURLsWhenCustomPasteboardDataIsDisabled)
{
    auto webView = createWebViewWithCustomPasteboardDataSetting(false);
    [webView synchronouslyLoadTestPageNamed:@"paste-rtfd"];

    writeHTMLToPasteboard(@"<!DOCTYPE html><html><body><a alt='hello' href='file:///private/var/folders/secret/files/'>world</a>");
    [webView paste:nil];

    EXPECT_TRUE([webView stringByEvaluatingJavaScript:@"clipboardData.types.includes('text/html')"].boolValue);
    EXPECT_TRUE([webView stringByEvaluatingJavaScript:@"clipboardData.values[0].includes('hello')"].boolValue);
    EXPECT_TRUE([webView stringByEvaluatingJavaScript:@"clipboardData.values[0].includes('world')"].boolValue);
    EXPECT_TRUE([webView stringByEvaluatingJavaScript:@"clipboardData.values[0].includes('secret')"].boolValue);
}

TEST(PasteHTML, KeepsHTTPURLs)
{
    auto webView = createWebViewWithCustomPasteboardDataSetting(true);
    [webView synchronouslyLoadTestPageNamed:@"paste-rtfd"];

    writeHTMLToPasteboard(@"<!DOCTYPE html><html><body><a title='hello' href='https://svn.webkit.org/repository/webkit/trunk/LayoutTests/editing/resources/abe.png'>world</a>");
    [webView paste:nil];

    EXPECT_TRUE([webView stringByEvaluatingJavaScript:@"clipboardData.types.includes('text/html')"].boolValue);
    EXPECT_TRUE([webView stringByEvaluatingJavaScript:@"clipboardData.values[0].includes('hello')"].boolValue);
    EXPECT_TRUE([webView stringByEvaluatingJavaScript:@"clipboardData.values[0].includes('world')"].boolValue);
    EXPECT_TRUE([webView stringByEvaluatingJavaScript:@"clipboardData.values[0].includes('abe.png')"].boolValue);
}

#endif // WK_API_ENABLED && PLATFORM(COCOA)
