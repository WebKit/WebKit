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

#if PLATFORM(MAC)

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
    auto preferences = (__bridge WKPreferencesRef)[[webView configuration] preferences];
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

TEST(PasteWebArchive, SanitizesHTML)
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

    EXPECT_WK_STREQ("hello, world", [webView stringByEvaluatingJavaScript:@"editor.textContent"]);
    [webView stringByEvaluatingJavaScript:@"editor.focus(); getSelection().setPosition(editor, 0)"];
    EXPECT_TRUE([webView stringByEvaluatingJavaScript:@"document.queryCommandState('bold')"].boolValue);
    [webView stringByEvaluatingJavaScript:@"getSelection().modify('move', 'forward', 'lineboundary')"];
    EXPECT_FALSE([webView stringByEvaluatingJavaScript:@"document.queryCommandState('bold')"].boolValue);
    EXPECT_FALSE([webView stringByEvaluatingJavaScript:@"editor.innerHTML.includes('secret')"].boolValue);
    EXPECT_FALSE([webView stringByEvaluatingJavaScript:@"editor.innerHTML.includes('dangerousCode')"].boolValue);
}

TEST(PasteWebArchive, PreservesMSOList)
{
    auto *url = [NSURL URLWithString:@"file:///some-file.html"];
    auto *markup = [NSData dataWithContentsOfFile:[[NSBundle mainBundle] pathForResource:@"mso-list" ofType:@"html" inDirectory:@"TestWebKitAPI.resources"]];
    auto mainResource = adoptNS([[WebResource alloc] initWithData:markup URL:url MIMEType:@"text/html" textEncodingName:@"utf-8" frameName:nil]);
    auto archive = adoptNS([[WebArchive alloc] initWithMainResource:mainResource.get() subresources:nil subframeArchives:nil]);

    [[NSPasteboard generalPasteboard] declareTypes:@[WebArchivePboardType] owner:nil];
    [[NSPasteboard generalPasteboard] setData:[archive data] forType:WebArchivePboardType];

    auto webView = createWebViewWithCustomPasteboardDataEnabled();
    [webView synchronouslyLoadTestPageNamed:@"paste-rtfd"];
    [webView paste:nil];

    EXPECT_WK_STREQ("[\"text/html\"]", [webView stringByEvaluatingJavaScript:@"JSON.stringify(clipboardData.types)"]);
    [webView stringByEvaluatingJavaScript:@"window.htmlInDataTransfer = clipboardData.values[0]"];
    [webView stringByEvaluatingJavaScript:@"window.pastedHTML = editor.innerHTML"];

    EXPECT_FALSE([webView stringByEvaluatingJavaScript:@"pastedHTML.startsWith('<html xmlns:o=\"urn:schemas-microsoft-com:office:office\"')"].boolValue);
    EXPECT_TRUE([webView stringByEvaluatingJavaScript:@"pastedHTML.includes('<style class=\"WebKit-mso-list-quirks-style\">\\n<!--\\n')"].boolValue);
    EXPECT_TRUE([webView stringByEvaluatingJavaScript:@"pastedHTML.includes('/* Style Definitions */')"].boolValue);
    EXPECT_TRUE([webView stringByEvaluatingJavaScript:@"pastedHTML.includes('/* List Definitions */')"].boolValue);
    EXPECT_TRUE([webView stringByEvaluatingJavaScript:@"pastedHTML.includes('@list l0:level1')"].boolValue);
    EXPECT_TRUE([webView stringByEvaluatingJavaScript:@"pastedHTML.includes('}\\n\\n-->\\n</style>')"].boolValue);
    EXPECT_TRUE([webView stringByEvaluatingJavaScript:@"pastedHTML.includes('[if !supportLists]')"].boolValue);
    EXPECT_TRUE([webView stringByEvaluatingJavaScript:@"pastedHTML.includes('[endif]')"].boolValue);
    EXPECT_TRUE([webView stringByEvaluatingJavaScript:@"pastedHTML.includes(' style=\"text-indent:-.25in;mso-list:l0 level1 lfo1\">')"].boolValue);
    EXPECT_FALSE([webView stringByEvaluatingJavaScript:@"pastedHTML.includes('/Users/webkitten/Library/')"].boolValue);

    [webView stringByEvaluatingJavaScript:@"getSelection().setPosition(document.querySelector('.MsoListParagraphCxSpLast'));"];
    [webView stringByEvaluatingJavaScript:@"getSelection().modify('move', 'forward', 'lineboundary');"];
    EXPECT_WK_STREQ("rgb(255, 0, 0)", [webView stringByEvaluatingJavaScript:@"document.queryCommandValue('foreColor')"]);

    EXPECT_TRUE([webView stringByEvaluatingJavaScript:@"htmlInDataTransfer.startsWith('<html xmlns:o=\"urn:schemas-microsoft-com:office:office\"')"].boolValue);
    EXPECT_TRUE([webView stringByEvaluatingJavaScript:@"htmlInDataTransfer.includes('<head><style class=\"WebKit-mso-list-quirks-style\">\\n<!--\\n')"].boolValue);
    EXPECT_TRUE([webView stringByEvaluatingJavaScript:@"htmlInDataTransfer.includes('/* Style Definitions */')"].boolValue);
    EXPECT_TRUE([webView stringByEvaluatingJavaScript:@"htmlInDataTransfer.includes('/* List Definitions */')"].boolValue);
    EXPECT_TRUE([webView stringByEvaluatingJavaScript:@"htmlInDataTransfer.includes('@list l0:level1')"].boolValue);
    EXPECT_TRUE([webView stringByEvaluatingJavaScript:@"htmlInDataTransfer.includes('}\\n\\n-->\\n</style></head>')"].boolValue);
    EXPECT_TRUE([webView stringByEvaluatingJavaScript:@"htmlInDataTransfer.includes('[if !supportLists]')"].boolValue);
    EXPECT_TRUE([webView stringByEvaluatingJavaScript:@"htmlInDataTransfer.includes('[endif]')"].boolValue);
    EXPECT_TRUE([webView stringByEvaluatingJavaScript:@"htmlInDataTransfer.includes(' style=\"text-indent:-.25in;mso-list:l0 level1 lfo1\">')"].boolValue);
    EXPECT_FALSE([webView stringByEvaluatingJavaScript:@"htmlInDataTransfer.includes('/Users/webkitten/Library/')"].boolValue);

    [webView stringByEvaluatingJavaScript:@"editor.innerHTML = ''; editor.focus();"];
    [webView stringByEvaluatingJavaScript:@"document.execCommand('insertHTML', false, htmlInDataTransfer);"];
    [webView stringByEvaluatingJavaScript:@"getSelection().setPosition(document.querySelector('.MsoListParagraphCxSpLast'));"];
    [webView stringByEvaluatingJavaScript:@"getSelection().modify('move', 'forward', 'lineboundary');"];
    EXPECT_WK_STREQ("rgb(255, 0, 0)", [webView stringByEvaluatingJavaScript:@"document.queryCommandValue('foreColor')"]);
}

TEST(PasteWebArchive, PreservesMSOListInCompatibilityMode)
{
    auto *url = [NSURL URLWithString:@"file:///some-file.html"];
    auto *markup = [NSData dataWithContentsOfFile:[[NSBundle mainBundle] pathForResource:@"mso-list-compat-mode" ofType:@"html" inDirectory:@"TestWebKitAPI.resources"]];
    auto mainResource = adoptNS([[WebResource alloc] initWithData:markup URL:url MIMEType:@"text/html" textEncodingName:@"utf-8" frameName:nil]);
    auto archive = adoptNS([[WebArchive alloc] initWithMainResource:mainResource.get() subresources:nil subframeArchives:nil]);

    [[NSPasteboard generalPasteboard] declareTypes:@[WebArchivePboardType] owner:nil];
    [[NSPasteboard generalPasteboard] setData:[archive data] forType:WebArchivePboardType];

    auto webView = createWebViewWithCustomPasteboardDataEnabled();
    [webView synchronouslyLoadTestPageNamed:@"paste-rtfd"];
    [webView paste:nil];

    EXPECT_WK_STREQ("[\"text/html\"]", [webView stringByEvaluatingJavaScript:@"JSON.stringify(clipboardData.types)"]);
    [webView stringByEvaluatingJavaScript:@"window.htmlInDataTransfer = clipboardData.values[0]"];
    [webView stringByEvaluatingJavaScript:@"window.pastedHTML = editor.innerHTML"];

    EXPECT_FALSE([webView stringByEvaluatingJavaScript:@"pastedHTML.startsWith('<html xmlns:o=\"urn:schemas-microsoft-com:office:office\"')"].boolValue);
    EXPECT_TRUE([webView stringByEvaluatingJavaScript:@"pastedHTML.includes('<style class=\"WebKit-mso-list-quirks-style\">\\n<!--\\n')"].boolValue);
    EXPECT_TRUE([webView stringByEvaluatingJavaScript:@"pastedHTML.includes('/* Style Definitions */')"].boolValue);
    EXPECT_TRUE([webView stringByEvaluatingJavaScript:@"pastedHTML.includes('/* List Definitions */')"].boolValue);
    EXPECT_TRUE([webView stringByEvaluatingJavaScript:@"pastedHTML.includes('@list l0:level1')"].boolValue);
    EXPECT_TRUE([webView stringByEvaluatingJavaScript:@"pastedHTML.includes('}\\n\\n-->\\n</style>')"].boolValue);
    EXPECT_TRUE([webView stringByEvaluatingJavaScript:@"pastedHTML.includes('[if !supportLists]')"].boolValue);
    EXPECT_TRUE([webView stringByEvaluatingJavaScript:@"pastedHTML.includes('[endif]')"].boolValue);
    EXPECT_TRUE([webView stringByEvaluatingJavaScript:@"pastedHTML.includes('<p style=\"margin-left:.5in;text-indent:-.25in;mso-list:l0 level1 lfo1\">')"].boolValue);
    EXPECT_FALSE([webView stringByEvaluatingJavaScript:@"pastedHTML.includes('/Users/webkitten/Library/')"].boolValue);

    EXPECT_TRUE([webView stringByEvaluatingJavaScript:@"htmlInDataTransfer.startsWith('<html xmlns:o=\"urn:schemas-microsoft-com:office:office\"')"].boolValue);
    EXPECT_TRUE([webView stringByEvaluatingJavaScript:@"htmlInDataTransfer.includes('<head><style class=\"WebKit-mso-list-quirks-style\">\\n<!--\\n')"].boolValue);
    EXPECT_TRUE([webView stringByEvaluatingJavaScript:@"htmlInDataTransfer.includes('/* Style Definitions */')"].boolValue);
    EXPECT_TRUE([webView stringByEvaluatingJavaScript:@"htmlInDataTransfer.includes('/* List Definitions */')"].boolValue);
    EXPECT_TRUE([webView stringByEvaluatingJavaScript:@"htmlInDataTransfer.includes('@list l0:level1')"].boolValue);
    EXPECT_TRUE([webView stringByEvaluatingJavaScript:@"htmlInDataTransfer.includes('}\\n\\n-->\\n</style></head>')"].boolValue);
    EXPECT_TRUE([webView stringByEvaluatingJavaScript:@"htmlInDataTransfer.includes('[if !supportLists]')"].boolValue);
    EXPECT_TRUE([webView stringByEvaluatingJavaScript:@"htmlInDataTransfer.includes('[endif]')"].boolValue);
    EXPECT_TRUE([webView stringByEvaluatingJavaScript:@"pastedHTML.includes('<p style=\"margin-left:.5in;text-indent:-.25in;mso-list:l0 level1 lfo1\">')"].boolValue);
    EXPECT_FALSE([webView stringByEvaluatingJavaScript:@"htmlInDataTransfer.includes('/Users/webkitten/Library/')"].boolValue);
}

static NSData *msoListMarkupWithoutProperHTMLElement()
{
    auto *markup = [NSData dataWithContentsOfFile:[[NSBundle mainBundle] pathForResource:@"mso-list" ofType:@"html" inDirectory:@"TestWebKitAPI.resources"]];
    auto *markupBytes = (uint8_t *)markup.bytes;
    unsigned length = markup.length;
    for (unsigned i = 0; i < length; i++) {
        if (markupBytes[i] == '>')
            return [markup subdataWithRange:NSMakeRange(i + 1, length - i - 1)];
    }
    return nil;
}

TEST(PasteWebArchive, StripsMSOListWhenMissingMSOHTMLElement)
{
    auto *url = [NSURL URLWithString:@"file:///some-file.html"];
    auto *markup = msoListMarkupWithoutProperHTMLElement();

    auto mainResource = adoptNS([[WebResource alloc] initWithData:markup URL:url MIMEType:@"text/html" textEncodingName:@"utf-8" frameName:nil]);
    auto archive = adoptNS([[WebArchive alloc] initWithMainResource:mainResource.get() subresources:nil subframeArchives:nil]);

    [[NSPasteboard generalPasteboard] declareTypes:@[WebArchivePboardType] owner:nil];
    [[NSPasteboard generalPasteboard] setData:[archive data] forType:WebArchivePboardType];

    auto webView = createWebViewWithCustomPasteboardDataEnabled();
    [webView synchronouslyLoadTestPageNamed:@"paste-rtfd"];
    [webView paste:nil];

    EXPECT_WK_STREQ("[\"text/html\"]", [webView stringByEvaluatingJavaScript:@"JSON.stringify(clipboardData.types)"]);
    [webView stringByEvaluatingJavaScript:@"window.htmlInDataTransfer = clipboardData.values[0]"];
    EXPECT_FALSE([webView stringByEvaluatingJavaScript:@"htmlInDataTransfer.startsWith('<html xmlns:o=\"urn:schemas-microsoft-com:office:office\"')"].boolValue);
    EXPECT_FALSE([webView stringByEvaluatingJavaScript:@"htmlInDataTransfer.includes('<head><style class=\"WebKit-mso-list-quirks-style\">\\n<!--\\n')"].boolValue);
    EXPECT_FALSE([webView stringByEvaluatingJavaScript:@"htmlInDataTransfer.includes('/* Style Definitions */')"].boolValue);
    EXPECT_FALSE([webView stringByEvaluatingJavaScript:@"htmlInDataTransfer.includes('/* List Definitions */')"].boolValue);
    EXPECT_FALSE([webView stringByEvaluatingJavaScript:@"htmlInDataTransfer.includes('@list l0:level1')"].boolValue);
    EXPECT_FALSE([webView stringByEvaluatingJavaScript:@"htmlInDataTransfer.includes('}\\n\\n-->\\n</style></head>')"].boolValue);
    EXPECT_FALSE([webView stringByEvaluatingJavaScript:@"htmlInDataTransfer.includes('[if !supportLists]')"].boolValue);
    EXPECT_FALSE([webView stringByEvaluatingJavaScript:@"htmlInDataTransfer.includes('[endif]')"].boolValue);
    EXPECT_FALSE([webView stringByEvaluatingJavaScript:@"htmlInDataTransfer.includes(' style=\"text-indent:-.25in;mso-list:l0 level1 lfo1\">')"].boolValue);
    EXPECT_FALSE([webView stringByEvaluatingJavaScript:@"htmlInDataTransfer.includes('/Users/webkitten/Library/')"].boolValue);

    [webView stringByEvaluatingJavaScript:@"editor.innerHTML = ''; editor.focus();"];
    [webView stringByEvaluatingJavaScript:@"document.execCommand('insertHTML', false, htmlInDataTransfer);"];
    [webView stringByEvaluatingJavaScript:@"getSelection().setPosition(document.querySelector('.MsoListParagraphCxSpLast'));"];
    [webView stringByEvaluatingJavaScript:@"getSelection().modify('move', 'forward', 'lineboundary');"];
    EXPECT_WK_STREQ("rgb(255, 0, 0)", [webView stringByEvaluatingJavaScript:@"document.queryCommandValue('foreColor')"]);
}

#endif // PLATFORM(MAC)



