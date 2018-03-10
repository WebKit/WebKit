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

#if PLATFORM(MAC) && WK_API_ENABLED

#import "PlatformUtilities.h"
#import "TestWKWebView.h"
#import <WebKit/WKPreferencesRef.h>
#import <WebKit/WKPreferencesRefPrivate.h>
#import <wtf/RetainPtr.h>
#import <wtf/text/WTFString.h>

static NSString *imagePath()
{
    return [[NSBundle mainBundle] pathForResource:@"icon" ofType:@"png" inDirectory:@"TestWebKitAPI.resources"];
}

void writeTypesAndDataToPasteboard(id type, ...)
{
    NSMutableArray<NSString *> *types = [NSMutableArray array];
    NSMutableArray *data = [NSMutableArray array];
    va_list argumentList;
    if (type) {
        ASSERT([type isKindOfClass:[NSString class]]);
        [types addObject:type];
        va_start(argumentList, type);
        NSUInteger index = 1;
        id object = va_arg(argumentList, id);
        while (object) {
            if (index % 2)
                [data addObject:object];
            else {
                ASSERT([object isKindOfClass:[NSString class]]);
                [types addObject:object];
            }
            ++index;
            object = va_arg(argumentList, id);
        }
        va_end(argumentList);
    }

    if (types.count != data.count) {
        NSLog(@"Expected number of types: %tu to match number of data items: %tu", types.count, data.count);
        ASSERT_NOT_REACHED();
        return;
    }

    [[NSPasteboard generalPasteboard] declareTypes:types owner:nil];
    [types enumerateObjectsUsingBlock:[&] (NSString *type, NSUInteger index, BOOL *) {
        id dataToWrite = data[index];
        if ([dataToWrite isKindOfClass:[NSData class]])
            [[NSPasteboard generalPasteboard] setData:dataToWrite forType:type];
        else if ([dataToWrite isKindOfClass:[NSString class]])
            [[NSPasteboard generalPasteboard] setString:dataToWrite forType:type];
        else
            [[NSPasteboard generalPasteboard] setPropertyList:dataToWrite forType:type];
    }];
}

static RetainPtr<TestWKWebView> setUpWebView()
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 400, 400)]);
    WKPreferencesSetCustomPasteboardDataEnabled((WKPreferencesRef)[webView configuration].preferences, true);
    [webView synchronouslyLoadTestPageNamed:@"DataTransfer"];
    return webView;
}

static NSString *markupString()
{
    // The script tag and mouseover listener attribute in the markup string below should be omitted during
    // sanitization while pasting.
    return @"<script>foo()</script><strong onmouseover='javascript:void(0)'>HELLO WORLD</strong>";
}

namespace TestWebKitAPI {

TEST(PasteMixedContent, ImageFileAndPlainText)
{
    auto webView = setUpWebView();
    writeTypesAndDataToPasteboard(NSFilenamesPboardType, @[ imagePath() ], NSPasteboardTypeString, imagePath(), nil);
    [webView paste:nil];

    EXPECT_WK_STREQ("Files", [webView stringByEvaluatingJavaScript:@"types.textContent"]);
    EXPECT_WK_STREQ("(FILE, image/png)", [webView stringByEvaluatingJavaScript:@"items.textContent"]);
    EXPECT_WK_STREQ("('icon.png', image/png)", [webView stringByEvaluatingJavaScript:@"files.textContent"]);
    EXPECT_WK_STREQ("", [webView stringByEvaluatingJavaScript:@"urlData.textContent"]);
    EXPECT_WK_STREQ("", [webView stringByEvaluatingJavaScript:@"textData.textContent"]);
    EXPECT_WK_STREQ("", [webView stringByEvaluatingJavaScript:@"htmlData.textContent"]);
    EXPECT_WK_STREQ("", [webView stringByEvaluatingJavaScript:@"rawHTMLData.textContent"]);
}

TEST(PasteMixedContent, ImageFileAndWebArchive)
{
    auto webView = setUpWebView();
    NSURL *mainResourceURL = [NSURL fileURLWithPath:@"/some/nonexistent/file.html"];
    auto mainResource = adoptNS([[WebResource alloc] initWithData:[markupString() dataUsingEncoding:NSUTF8StringEncoding] URL:mainResourceURL MIMEType:@"text/html" textEncodingName:@"utf-8" frameName:nil]);
    auto archive = adoptNS([[WebArchive alloc] initWithMainResource:mainResource.get() subresources:nil subframeArchives:nil]);
    writeTypesAndDataToPasteboard(NSFilenamesPboardType, @[ imagePath() ], WebArchivePboardType, [archive data], nil);
    [webView paste:nil];

    EXPECT_WK_STREQ("Files, text/html", [webView stringByEvaluatingJavaScript:@"types.textContent"]);
    EXPECT_WK_STREQ("(STRING, text/html), (FILE, image/png)", [webView stringByEvaluatingJavaScript:@"items.textContent"]);
    EXPECT_WK_STREQ("('icon.png', image/png)", [webView stringByEvaluatingJavaScript:@"files.textContent"]);
    EXPECT_WK_STREQ("", [webView stringByEvaluatingJavaScript:@"urlData.textContent"]);
    EXPECT_WK_STREQ("", [webView stringByEvaluatingJavaScript:@"textData.textContent"]);
    EXPECT_WK_STREQ("HELLO WORLD", [webView stringByEvaluatingJavaScript:@"htmlData.textContent"]);
    EXPECT_FALSE([[webView stringByEvaluatingJavaScript:@"rawHTMLData.textContent"] containsString:@"script"]);
}

TEST(PasteMixedContent, ImageFileAndHTML)
{
    auto webView = setUpWebView();
    writeTypesAndDataToPasteboard(NSFilenamesPboardType, @[ imagePath() ], NSPasteboardTypeHTML, markupString(), nil);
    [webView paste:nil];

    EXPECT_WK_STREQ("Files, text/html", [webView stringByEvaluatingJavaScript:@"types.textContent"]);
    EXPECT_WK_STREQ("(STRING, text/html), (FILE, image/png)", [webView stringByEvaluatingJavaScript:@"items.textContent"]);
    EXPECT_WK_STREQ("('icon.png', image/png)", [webView stringByEvaluatingJavaScript:@"files.textContent"]);
    EXPECT_WK_STREQ("", [webView stringByEvaluatingJavaScript:@"urlData.textContent"]);
    EXPECT_WK_STREQ("", [webView stringByEvaluatingJavaScript:@"textData.textContent"]);
    EXPECT_WK_STREQ("HELLO WORLD", [webView stringByEvaluatingJavaScript:@"htmlData.textContent"]);
    EXPECT_FALSE([[webView stringByEvaluatingJavaScript:@"rawHTMLData.textContent"] containsString:@"script"]);
}

TEST(PasteMixedContent, ImageFileAndRTF)
{
    auto webView = setUpWebView();
    auto text = adoptNS([[NSMutableAttributedString alloc] init]);
    [text appendAttributedString:[[NSAttributedString alloc] initWithString:@"link to "]];
    [text appendAttributedString:[[NSAttributedString alloc] initWithString:@"apple" attributes:@{ NSLinkAttributeName: [NSURL URLWithString:@"https://www.apple.com/"] }]];
    NSData *rtfData = [text RTFFromRange:NSMakeRange(0, [text length]) documentAttributes:@{ }];
    writeTypesAndDataToPasteboard(NSFilenamesPboardType, @[ imagePath() ], NSPasteboardTypeRTF, rtfData, nil);
    [webView paste:nil];

    EXPECT_WK_STREQ("Files, text/html", [webView stringByEvaluatingJavaScript:@"types.textContent"]);
    EXPECT_WK_STREQ("(STRING, text/html), (FILE, image/png)", [webView stringByEvaluatingJavaScript:@"items.textContent"]);
    EXPECT_WK_STREQ("('icon.png', image/png)", [webView stringByEvaluatingJavaScript:@"files.textContent"]);
    EXPECT_WK_STREQ("", [webView stringByEvaluatingJavaScript:@"urlData.textContent"]);
    EXPECT_WK_STREQ("", [webView stringByEvaluatingJavaScript:@"textData.textContent"]);
    EXPECT_WK_STREQ("link to apple", [webView stringByEvaluatingJavaScript:@"htmlData.textContent"]);
    EXPECT_WK_STREQ("https://www.apple.com/", [webView stringByEvaluatingJavaScript:@"htmlData.querySelector('a').href"]);
}

TEST(PasteMixedContent, ImageFileAndURL)
{
    auto webView = setUpWebView();
    writeTypesAndDataToPasteboard(NSURLPboardType, @"https://www.webkit.org/", NSFilenamesPboardType, @[ imagePath() ], nil);
    [webView paste:nil];

    EXPECT_WK_STREQ("Files, text/uri-list", [webView stringByEvaluatingJavaScript:@"types.textContent"]);
    EXPECT_WK_STREQ("(STRING, text/uri-list), (FILE, image/png)", [webView stringByEvaluatingJavaScript:@"items.textContent"]);
    EXPECT_WK_STREQ("('icon.png', image/png)", [webView stringByEvaluatingJavaScript:@"files.textContent"]);
    EXPECT_WK_STREQ("https://www.webkit.org/", [webView stringByEvaluatingJavaScript:@"urlData.textContent"]);
    EXPECT_WK_STREQ("", [webView stringByEvaluatingJavaScript:@"textData.textContent"]);
    EXPECT_WK_STREQ("", [webView stringByEvaluatingJavaScript:@"htmlData.textContent"]);
}

TEST(PasteMixedContent, ImageFileWithHTMLAndURL)
{
    auto webView = setUpWebView();
    writeTypesAndDataToPasteboard(NSURLPboardType, @"https://www.webkit.org/", NSPasteboardTypeHTML, markupString(), NSFilenamesPboardType, @[ imagePath() ], nil);
    [webView paste:nil];

    EXPECT_WK_STREQ("Files, text/uri-list, text/html", [webView stringByEvaluatingJavaScript:@"types.textContent"]);
    EXPECT_WK_STREQ("(STRING, text/uri-list), (STRING, text/html), (FILE, image/png)", [webView stringByEvaluatingJavaScript:@"items.textContent"]);
    EXPECT_WK_STREQ("('icon.png', image/png)", [webView stringByEvaluatingJavaScript:@"files.textContent"]);
    EXPECT_WK_STREQ("https://www.webkit.org/", [webView stringByEvaluatingJavaScript:@"urlData.textContent"]);
    EXPECT_WK_STREQ("", [webView stringByEvaluatingJavaScript:@"textData.textContent"]);
    EXPECT_WK_STREQ("HELLO WORLD", [webView stringByEvaluatingJavaScript:@"htmlData.textContent"]);
    EXPECT_FALSE([[webView stringByEvaluatingJavaScript:@"rawHTMLData.textContent"] containsString:@"script"]);
}

TEST(PasteMixedContent, ImageDataAndPlainText)
{
    auto webView = setUpWebView();
    writeTypesAndDataToPasteboard(NSPasteboardTypeString, @"Hello world", NSPasteboardTypePNG, [NSData dataWithContentsOfFile:imagePath()], nil);
    [webView paste:nil];

    EXPECT_WK_STREQ("Files, text/plain", [webView stringByEvaluatingJavaScript:@"types.textContent"]);
    EXPECT_WK_STREQ("(STRING, text/plain), (FILE, image/png)", [webView stringByEvaluatingJavaScript:@"items.textContent"]);
    EXPECT_WK_STREQ("('image.png', image/png)", [webView stringByEvaluatingJavaScript:@"files.textContent"]);
    EXPECT_WK_STREQ("", [webView stringByEvaluatingJavaScript:@"urlData.textContent"]);
    EXPECT_WK_STREQ("Hello world", [webView stringByEvaluatingJavaScript:@"textData.textContent"]);
    EXPECT_WK_STREQ("", [webView stringByEvaluatingJavaScript:@"htmlData.textContent"]);
}

TEST(PasteMixedContent, ImageDataAndPlainTextAndURL)
{
    auto webView = setUpWebView();
    writeTypesAndDataToPasteboard(NSPasteboardTypeString, imagePath(), NSURLPboardType, imagePath(), NSPasteboardTypePNG, [NSData dataWithContentsOfFile:imagePath()], nil);
    [webView paste:nil];

    EXPECT_WK_STREQ("Files", [webView stringByEvaluatingJavaScript:@"types.textContent"]);
    EXPECT_WK_STREQ("(FILE, image/png)", [webView stringByEvaluatingJavaScript:@"items.textContent"]);
    EXPECT_WK_STREQ("('image.png', image/png)", [webView stringByEvaluatingJavaScript:@"files.textContent"]);
    EXPECT_WK_STREQ("", [webView stringByEvaluatingJavaScript:@"urlData.textContent"]);
    EXPECT_WK_STREQ("", [webView stringByEvaluatingJavaScript:@"textData.textContent"]);
    EXPECT_WK_STREQ("", [webView stringByEvaluatingJavaScript:@"htmlData.textContent"]);
}

TEST(PasteMixedContent, ImageDataAndPlainTextAndURLAndHTML)
{
    auto webView = setUpWebView();
    writeTypesAndDataToPasteboard(NSPasteboardTypeString, imagePath(), NSURLPboardType, imagePath(), NSPasteboardTypeHTML, markupString(), NSPasteboardTypePNG, [NSData dataWithContentsOfFile:imagePath()], nil);
    [webView paste:nil];

    EXPECT_WK_STREQ("Files, text/html", [webView stringByEvaluatingJavaScript:@"types.textContent"]);
    EXPECT_WK_STREQ("(STRING, text/html), (FILE, image/png)", [webView stringByEvaluatingJavaScript:@"items.textContent"]);
    EXPECT_WK_STREQ("('image.png', image/png)", [webView stringByEvaluatingJavaScript:@"files.textContent"]);
    EXPECT_WK_STREQ("", [webView stringByEvaluatingJavaScript:@"urlData.textContent"]);
    EXPECT_WK_STREQ("", [webView stringByEvaluatingJavaScript:@"textData.textContent"]);
    EXPECT_WK_STREQ("HELLO WORLD", [webView stringByEvaluatingJavaScript:@"htmlData.textContent"]);
    EXPECT_FALSE([[webView stringByEvaluatingJavaScript:@"rawHTMLData.textContent"] containsString:@"script"]);
}

} // namespace TestWebKitAPI

#endif // PLATFORM(MAC) && WK_API_ENABLED
