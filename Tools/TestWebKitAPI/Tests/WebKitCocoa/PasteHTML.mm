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

#if PLATFORM(COCOA)

#import "PlatformUtilities.h"
#import "TestWKWebView.h"
#import <WebCore/LegacyNSPasteboardTypes.h>
#import <WebKit/WKPreferencesPrivate.h>
#import <WebKit/WKPreferencesRefPrivate.h>
#import <WebKit/WKWebViewConfigurationPrivate.h>
#import <wtf/RetainPtr.h>
#import <wtf/text/WTFString.h>

#if PLATFORM(IOS_FAMILY)
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
    [[UIPasteboard generalPasteboard] setItems:@[@{ (__bridge NSString *)kUTTypeHTML : html}]];
}
#endif

static RetainPtr<TestWKWebView> createWebViewWithCustomPasteboardDataSetting(bool enabled)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 400, 400)]);
    auto preferences = (__bridge WKPreferencesRef)[[webView configuration] preferences];
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

TEST(PasteHTML, PreservesMSOList)
{
    writeHTMLToPasteboard([NSString stringWithContentsOfFile:[[NSBundle mainBundle] pathForResource:@"mso-list" ofType:@"html" inDirectory:@"TestWebKitAPI.resources"]
        encoding:NSUTF8StringEncoding error:NULL]);

    auto webView = createWebViewWithCustomPasteboardDataSetting(true);
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
    EXPECT_TRUE([webView stringByEvaluatingJavaScript:@"pastedHTML.includes('<p class=\"MsoNormal\" style=\"margin-left:0cm;text-indent:0cm;mso-pagination:none;\\n"
        "mso-list:l1 level1 lfo1;mso-layout-grid-align:none;text-autospace:none\">')"].boolValue);
    EXPECT_TRUE([webView stringByEvaluatingJavaScript:@"pastedHTML.includes('<p class=\"MsoNormal\" style=\"mso-list:l1 level1 lfo1;margin-left:0cm\">')"].boolValue);
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
    EXPECT_TRUE([webView stringByEvaluatingJavaScript:@"htmlInDataTransfer.includes('<p class=\"MsoNormal\" style=\"margin-left:0cm;text-indent:0cm;mso-pagination:none;\\n"
        "mso-list:l1 level1 lfo1;mso-layout-grid-align:none;text-autospace:none\">')"].boolValue);
    EXPECT_TRUE([webView stringByEvaluatingJavaScript:@"htmlInDataTransfer.includes('<p class=\"MsoNormal\" style=\"mso-list:l1 level1 lfo1;margin-left:0cm\">')"].boolValue);
    EXPECT_FALSE([webView stringByEvaluatingJavaScript:@"htmlInDataTransfer.includes('/Users/webkitten/Library/')"].boolValue);

    [webView stringByEvaluatingJavaScript:@"editor.innerHTML = ''; editor.focus();"];
    [webView stringByEvaluatingJavaScript:@"document.execCommand('insertHTML', false, htmlInDataTransfer);"];
    [webView stringByEvaluatingJavaScript:@"getSelection().setPosition(document.querySelector('.MsoListParagraphCxSpLast'));"];
    [webView stringByEvaluatingJavaScript:@"getSelection().modify('move', 'forward', 'lineboundary');"];
    EXPECT_WK_STREQ("rgb(255, 0, 0)", [webView stringByEvaluatingJavaScript:@"document.queryCommandValue('foreColor')"]);
}

TEST(PasteHTML, PreservesMSOListInCompatibilityMode)
{
    writeHTMLToPasteboard([NSString stringWithContentsOfFile:[[NSBundle mainBundle] pathForResource:@"mso-list-compat-mode" ofType:@"html" inDirectory:@"TestWebKitAPI.resources"]
        encoding:NSUTF8StringEncoding error:NULL]);

    auto webView = createWebViewWithCustomPasteboardDataSetting(true);
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

TEST(PasteHTML, PreservesMSOListOnH4)
{
    writeHTMLToPasteboard([NSString stringWithContentsOfFile:[[NSBundle mainBundle] pathForResource:@"mso-list-on-h4" ofType:@"html" inDirectory:@"TestWebKitAPI.resources"]
        encoding:NSUTF8StringEncoding error:NULL]);

    auto webView = createWebViewWithCustomPasteboardDataSetting(true);
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
    EXPECT_TRUE([webView stringByEvaluatingJavaScript:@"pastedHTML.includes('<h4 style=\"margin-left:67.5pt;text-indent:-.25in;mso-list:l0 level1 lfo4\">')"].boolValue);
    EXPECT_FALSE([webView stringByEvaluatingJavaScript:@"pastedHTML.includes('/Users/webkitten/Library/')"].boolValue);

    EXPECT_TRUE([webView stringByEvaluatingJavaScript:@"htmlInDataTransfer.startsWith('<html xmlns:o=\"urn:schemas-microsoft-com:office:office\"')"].boolValue);
    EXPECT_TRUE([webView stringByEvaluatingJavaScript:@"htmlInDataTransfer.includes('<head><style class=\"WebKit-mso-list-quirks-style\">\\n<!--\\n')"].boolValue);
    EXPECT_TRUE([webView stringByEvaluatingJavaScript:@"htmlInDataTransfer.includes('/* Style Definitions */')"].boolValue);
    EXPECT_TRUE([webView stringByEvaluatingJavaScript:@"htmlInDataTransfer.includes('/* List Definitions */')"].boolValue);
    EXPECT_TRUE([webView stringByEvaluatingJavaScript:@"htmlInDataTransfer.includes('@list l0:level1')"].boolValue);
    EXPECT_TRUE([webView stringByEvaluatingJavaScript:@"htmlInDataTransfer.includes('}\\n\\n-->\\n</style></head>')"].boolValue);
    EXPECT_TRUE([webView stringByEvaluatingJavaScript:@"htmlInDataTransfer.includes('[if !supportLists]')"].boolValue);
    EXPECT_TRUE([webView stringByEvaluatingJavaScript:@"htmlInDataTransfer.includes('[endif]')"].boolValue);
    EXPECT_TRUE([webView stringByEvaluatingJavaScript:@"htmlInDataTransfer.includes('<h4 style=\"margin-left:67.5pt;text-indent:-.25in;mso-list:l0 level1 lfo4\">')"].boolValue);
    EXPECT_FALSE([webView stringByEvaluatingJavaScript:@"htmlInDataTransfer.includes('/Users/webkitten/Library/')"].boolValue);
}

TEST(PasteHTML, StripsMSOListWhenMissingMSOHTMLElement)
{
    auto *markup = [NSString stringWithContentsOfFile:[[NSBundle mainBundle] pathForResource:@"mso-list" ofType:@"html" inDirectory:@"TestWebKitAPI.resources"] encoding:NSUTF8StringEncoding error:NULL];

    writeHTMLToPasteboard([markup substringFromIndex:[markup rangeOfString:@">"].location + 1]);

    auto webView = createWebViewWithCustomPasteboardDataSetting(true);
    [webView synchronouslyLoadTestPageNamed:@"paste-rtfd"];
    [webView paste:nil];

    EXPECT_WK_STREQ("[\"text/html\"]", [webView stringByEvaluatingJavaScript:@"JSON.stringify(clipboardData.types)"]);
    [webView stringByEvaluatingJavaScript:@"window.htmlInDataTransfer = clipboardData.values[0]"];
    [webView stringByEvaluatingJavaScript:@"window.pastedHTML = editor.innerHTML"];

    EXPECT_FALSE([webView stringByEvaluatingJavaScript:@"pastedHTML.startsWith('<html xmlns:o=\"urn:schemas-microsoft-com:office:office\"')"].boolValue);
    EXPECT_FALSE([webView stringByEvaluatingJavaScript:@"pastedHTML.includes('<style class=\"WebKit-mso-list-quirks-style\">\\n<!--\\n')"].boolValue);
    EXPECT_FALSE([webView stringByEvaluatingJavaScript:@"pastedHTML.includes('/* Style Definitions */')"].boolValue);
    EXPECT_FALSE([webView stringByEvaluatingJavaScript:@"pastedHTML.includes('/* List Definitions */')"].boolValue);
    EXPECT_FALSE([webView stringByEvaluatingJavaScript:@"pastedHTML.includes('@list l0:level1')"].boolValue);
    EXPECT_FALSE([webView stringByEvaluatingJavaScript:@"pastedHTML.includes('}\\n\\n-->\\n</style>')"].boolValue);
    EXPECT_FALSE([webView stringByEvaluatingJavaScript:@"pastedHTML.includes('[if !supportLists]')"].boolValue);
    EXPECT_FALSE([webView stringByEvaluatingJavaScript:@"pastedHTML.includes('[endif]')"].boolValue);
    EXPECT_FALSE([webView stringByEvaluatingJavaScript:@"pastedHTML.includes(' style=\"text-indent:-.25in;mso-list:l0 level1 lfo1\">')"].boolValue);
    EXPECT_FALSE([webView stringByEvaluatingJavaScript:@"pastedHTML.includes('/Users/webkitten/Library/')"].boolValue);

    [webView stringByEvaluatingJavaScript:@"getSelection().setPosition(document.querySelector('.MsoListParagraphCxSpLast'));"];
    [webView stringByEvaluatingJavaScript:@"getSelection().modify('move', 'forward', 'lineboundary');"];
    EXPECT_WK_STREQ("rgb(255, 0, 0)", [webView stringByEvaluatingJavaScript:@"document.queryCommandValue('foreColor')"]);

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

TEST(PasteHTML, StripsSystemFontNames)
{
    writeHTMLToPasteboard([NSString stringWithContentsOfFile:[[NSBundle mainBundle] pathForResource:@"cocoa-writer-markup-with-system-fonts" ofType:@"html" inDirectory:@"TestWebKitAPI.resources"] encoding:NSUTF8StringEncoding error:NULL]);

    auto webView = createWebViewWithCustomPasteboardDataSetting(true);
    [webView synchronouslyLoadTestPageNamed:@"paste-rtfd"];
    [webView paste:nil];

    EXPECT_WK_STREQ("[\"text/html\"]", [webView stringByEvaluatingJavaScript:@"JSON.stringify(clipboardData.types)"]);
    [webView stringByEvaluatingJavaScript:@"window.htmlInDataTransfer = clipboardData.values[0]"];
    [webView stringByEvaluatingJavaScript:@"window.pastedHTML = editor.innerHTML"];

    EXPECT_TRUE([webView stringByEvaluatingJavaScript:@"pastedHTML.includes('Hello Cocoa')"].boolValue);
    EXPECT_TRUE([webView stringByEvaluatingJavaScript:@"pastedHTML.includes('font-weight: bold')"].boolValue);
    EXPECT_TRUE([webView stringByEvaluatingJavaScript:@"!pastedHTML.includes('.AppleSystemUIFont')"].boolValue);
    EXPECT_TRUE([webView stringByEvaluatingJavaScript:@"!pastedHTML.includes('.SFUI')"].boolValue);
    EXPECT_TRUE([webView stringByEvaluatingJavaScript:@"!pastedHTML.includes('.SF')"].boolValue);

    EXPECT_TRUE([webView stringByEvaluatingJavaScript:@"htmlInDataTransfer.includes('Hello Cocoa')"].boolValue);
    EXPECT_TRUE([webView stringByEvaluatingJavaScript:@"htmlInDataTransfer.includes('font-weight: bold')"].boolValue);
    EXPECT_TRUE([webView stringByEvaluatingJavaScript:@"!htmlInDataTransfer.includes('.AppleSystemUIFont')"].boolValue);
    EXPECT_TRUE([webView stringByEvaluatingJavaScript:@"!htmlInDataTransfer.includes('.SFUI')"].boolValue);
    EXPECT_TRUE([webView stringByEvaluatingJavaScript:@"!htmlInDataTransfer.includes('.SF')"].boolValue);

    EXPECT_WK_STREQ([webView stringByEvaluatingJavaScript:@"getComputedStyle(document.querySelector('.s2')).fontFamily"],
        [webView stringByEvaluatingJavaScript:@"getComputedStyle(document.body).fontFamily"]);
    EXPECT_WK_STREQ([webView stringByEvaluatingJavaScript:@"getComputedStyle(document.querySelector('.s4')).fontFamily"],
        [webView stringByEvaluatingJavaScript:@"getComputedStyle(document.body).fontFamily"]);
}

#endif // PLATFORM(COCOA)
