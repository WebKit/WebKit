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

#import "PlatformUtilities.h"
#import "Test.h"
#import "TestNavigationDelegate.h"
#import "TestProtocol.h"
#import "TestResourceLoadDelegate.h"
#import "TestWKWebView.h"
#import "UIKitSPIForTesting.h"
#import <WebCore/ColorCocoa.h>
#import <WebCore/FontCocoa.h>
#import <WebKit/NSAttributedStringPrivate.h>
#import <WebKit/WKWebsiteDataStorePrivate.h>
#import <WebKit/_WKResourceLoadInfo.h>
#import <WebKit/_WKTextRun.h>
#import <WebKit/_WKWebsiteDataStoreConfiguration.h>
#import <pal/spi/cocoa/NSAttributedStringSPI.h>
#import <wtf/cocoa/TypeCastsCocoa.h>
#import <wtf/text/WTFString.h>

@implementation WKWebView (WKWebViewGetContents)

- (NSAttributedString *)_contentsAsAttributedString
{
    __block bool finished = false;
    __block RetainPtr<NSAttributedString> result;
    [self _getContentsAsAttributedStringWithCompletionHandler:^(NSAttributedString *string, NSDictionary *, NSError *) {
        result = string;
        finished = true;
    }];
    TestWebKitAPI::Util::run(&finished);
    return result.autorelease();
}

- (NSArray<_WKTextRun *> *)_allTextRuns
{
    __block bool finished = false;
    __block RetainPtr<NSArray<_WKTextRun *>> result;
    [self _requestAllTextWithCompletionHandler:^(NSArray<_WKTextRun *> *textRuns) {
        result = textRuns;
        finished = true;
    }];
    TestWebKitAPI::Util::run(&finished);
    return result.autorelease();
}

@end

namespace TestWebKitAPI {

TEST(WKWebView, GetContentsShouldReturnString)
{
    RetainPtr<TestWKWebView> webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600)]);

    [webView synchronouslyLoadTestPageNamed:@"simple"];

    __block bool finished = false;

    [webView _getContentsAsStringWithCompletionHandler:^(NSString *string, NSError *error) {
        EXPECT_NULL(error);
        EXPECT_WK_STREQ(@"Simple HTML file.", string);
        finished = true;
    }];

    Util::run(&finished);
}

TEST(WKWebView, GetContentsShouldFailWhenClosingPage)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600)]);

    [webView synchronouslyLoadTestPageNamed:@"simple"];

    __block bool finished = false;

    [webView _getContentsAsStringWithCompletionHandlerKeepIPCConnectionAliveForTesting:^(NSString *string, NSError *error) {
        finished = true;
    }];

    [webView _close];

    Util::run(&finished);
}

TEST(WKWebView, GetContentsOfAllFramesShouldReturnString)
{
    RetainPtr<TestWKWebView> webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600)]);

    [webView synchronouslyLoadHTMLString:@"<body>beep<iframe srcdoc=\"meep\">herp</iframe><iframe srcdoc=\"moop\">derp</iframe></body>"];

    __block bool finished = false;

    [webView _getContentsOfAllFramesAsStringWithCompletionHandler:^(NSString *string) {
        EXPECT_WK_STREQ(@"beep\n\nmeep\n\nmoop", string);
        finished = true;
    }];

    Util::run(&finished);
}

TEST(WKWebView, GetContentsShouldReturnAttributedString)
{
    RetainPtr<TestWKWebView> webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600)]);

    [webView synchronouslyLoadHTMLString:@"<body bgcolor='red'>Hello <b>World!</b>"];

    __block bool finished = false;

    [webView _getContentsAsAttributedStringWithCompletionHandler:^(NSAttributedString *attributedString, NSDictionary<NSAttributedStringDocumentAttributeKey, id> *documentAttributes, NSError *error) {
        EXPECT_NOT_NULL(attributedString);
        EXPECT_NOT_NULL(documentAttributes);
        EXPECT_NULL(error);

        __block size_t i = 0;
        [attributedString enumerateAttributesInRange:NSMakeRange(0, attributedString.length) options:0 usingBlock:^(NSDictionary *attributes, NSRange attributeRange, BOOL *stop) {
            auto* substring = [attributedString attributedSubstringFromRange:attributeRange];

            if (!i) {
                EXPECT_WK_STREQ(@"Hello ", substring.string);
#if USE(APPKIT)
                EXPECT_WK_STREQ(@"Times-Roman", dynamic_objc_cast<NSFont>(attributes[NSFontAttributeName]).fontName);
#else
                EXPECT_WK_STREQ(@"TimesNewRomanPSMT", dynamic_objc_cast<UIFont>(attributes[NSFontAttributeName]).fontName);
#endif
            } else if (i == 1) {
                EXPECT_WK_STREQ(@"World!", substring.string);
#if USE(APPKIT)
                EXPECT_WK_STREQ(@"Times-Bold", dynamic_objc_cast<NSFont>(attributes[NSFontAttributeName]).fontName);
#else
                EXPECT_WK_STREQ(@"TimesNewRomanPS-BoldMT", dynamic_objc_cast<UIFont>(attributes[NSFontAttributeName]).fontName);
#endif
            } else
                ASSERT_NOT_REACHED();

            ++i;
        }];

#if USE(APPKIT)
        EXPECT_WK_STREQ(@"sRGB IEC61966-2.1 colorspace 1 0 0 1", dynamic_objc_cast<NSColor>(documentAttributes[NSBackgroundColorDocumentAttribute]).description);
#else
        EXPECT_WK_STREQ(@"kCGColorSpaceModelRGB 1 0 0 1 ", dynamic_objc_cast<UIColor>(documentAttributes[NSBackgroundColorDocumentAttribute]).description);
#endif

        finished = true;
    }];

    Util::run(&finished);
}

TEST(WKWebView, GetContentsWithOpticallySizedFontShouldReturnAttributedString)
{
    RetainPtr<TestWKWebView> webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600)]);

    [webView synchronouslyLoadHTMLString:@"<body style='font-family: system-ui; font-weight: 100; font-size: 16px; text-rendering: optimizeLegibility'>Hello</body>"];

    __block bool finished = false;

    [webView _getContentsAsAttributedStringWithCompletionHandler:^(NSAttributedString *attributedString, NSDictionary<NSAttributedStringDocumentAttributeKey, id> *documentAttributes, NSError *error) {
        EXPECT_NOT_NULL(attributedString);
        EXPECT_NOT_NULL(documentAttributes);
        EXPECT_NULL(error);

        __block size_t i = 0;
        [attributedString enumerateAttributesInRange:NSMakeRange(0, attributedString.length) options:0 usingBlock:^(NSDictionary *attributes, NSRange attributeRange, BOOL* stop) {
            auto *substring = [attributedString attributedSubstringFromRange:attributeRange];

            if (!i) {
                EXPECT_WK_STREQ(@"Hello", substring.string);

#if USE(APPKIT)
                EXPECT_EQ([dynamic_objc_cast<NSFont>(attributes[NSFontAttributeName]) pointSize], 16);
#else
                EXPECT_EQ([dynamic_objc_cast<UIFont>(attributes[NSFontAttributeName]) pointSize], 16);
#endif
            } else
                ASSERT_NOT_REACHED();

            ++i;
        }];

        EXPECT_EQ(i, 1UL);

        finished = true;
    }];

    Util::run(&finished);
}

TEST(WKWebView, AttributedStringAccessibilityLabel)
{
    auto webView = adoptNS([TestWKWebView new]);

    NSString *imagePath = [NSBundle.test_resourcesBundle pathForResource:@"icon" ofType:@"png"];
    [webView synchronouslyLoadHTMLString:[NSString stringWithFormat:@"<html><body><b>Hello</b> <img src='file://%@' width='100' height='100' alt='alt text'> <img src='file://%@' width='100' height='100' alt='aria label text'></body></html>", imagePath, imagePath]];

    __block bool finished = false;

    [webView _getContentsAsAttributedStringWithCompletionHandler:^(NSAttributedString *attributedString, NSDictionary<NSAttributedStringDocumentAttributeKey, id> *documentAttributes, NSError *error) {
        EXPECT_NOT_NULL(attributedString);
        EXPECT_NOT_NULL(documentAttributes);
        EXPECT_NULL(error);

        __block bool foundImage1 { NO };
        __block bool foundImage2 { NO };
        [attributedString enumerateAttribute:NSAttachmentAttributeName inRange:NSMakeRange(0, attributedString.length) options:0 usingBlock:^(id value, NSRange attributeRange, BOOL* stop) {
            if ([value isKindOfClass:NSTextAttachment.class]) {
                if ([[value accessibilityLabel] isEqualToString:@"alt text"])
                    foundImage1 = YES;
                if ([[value accessibilityLabel] isEqualToString:@"aria label text"])
                    foundImage2 = YES;
            }
        }];

        EXPECT_TRUE(foundImage1);
        EXPECT_TRUE(foundImage2);

        finished = true;
    }];

    Util::run(&finished);
}

TEST(WKWebView, AttributedStringAttributeTypes)
{
    NSString *html = @"<html>"
    "<head>"
    "    <meta name='CreationTime' content='2023-12-01T12:23:34Z'/>"
    "    <meta name='Keywords' content='a b c'/>"
    "</head>"
    "<body>"
    "    <p style='text-shadow: 0 1px black'>text shadow paragraph</p>"
    "    <p style='display: inline; unicode-bidi: bidi-override'>bidi paragraph</p>"
    "</body>"
    "</html>";

    auto webView = adoptNS([TestWKWebView new]);
    [webView synchronouslyLoadHTMLString:html];

    __block bool finished { false };

    [webView _getContentsAsAttributedStringWithCompletionHandler:^(NSAttributedString *attributedString, NSDictionary<NSAttributedStringDocumentAttributeKey, id> *documentAttributes, NSError *error) {
        bool foundNSDate = [documentAttributes[@"NSCreationTimeDocumentAttribute"] isKindOfClass:NSDate.class];
        EXPECT_TRUE(foundNSDate);
        bool foundNSArrayOfNSStrings = [documentAttributes[@"NSKeywordsDocumentAttribute"] isEqualToArray:@[@"a", @"b", @"c"]];
        EXPECT_TRUE(foundNSArrayOfNSStrings);

        __block bool foundNSShadow { false };
        __block bool foundNSParagraphStyle { false };
        __block bool foundNSArrayOfNSNumbers { false };
        [attributedString enumerateAttributesInRange:NSMakeRange(0, [attributedString length]) options:0 usingBlock: ^(NSDictionary<NSAttributedStringKey, id> *attributes, NSRange range, BOOL *) {
            if ([attributes[@"NSShadow"] isKindOfClass:NSShadow.class])
                foundNSShadow = true;
            if ([attributes[@"NSParagraphStyle"] isKindOfClass:NSParagraphStyle.class])
                foundNSParagraphStyle = true;
            if ([attributes[@"NSWritingDirection"] isKindOfClass:NSArray.class])
                foundNSArrayOfNSNumbers = [attributes[@"NSWritingDirection"][0] doubleValue] == 2;
        }];
        EXPECT_TRUE(foundNSShadow);
        EXPECT_TRUE(foundNSParagraphStyle);
        EXPECT_TRUE(foundNSArrayOfNSNumbers);
        finished = true;
    }];

    Util::run(&finished);
}

TEST(WKWebView, AttributedStringFromTable)
{
    auto webView = adoptNS([TestWKWebView new]);
    [webView synchronouslyLoadHTMLString:@"<html>"
        "  <head>"
        "  <style>"
        "  .background {"
        "      background-color: red;"
        "  }"
        "  .border {"
        "      border: 2px solid red;"
        "  }"
        "  </style>"
        "  </head>"
        "  <body>"
        "    <table>"
        "      <tbody>"
        "        <tr><td class='background'>One</td><td class='background'>Two</td></tr>"
        "        <tr><td class='background'>Three</td><td class='background'>Four</td></tr>"
        "      </tbody>"
        "    </table>"
        "    <table>"
        "      <tbody>"
        "        <tr><td class='border'>Five</td><td class='border'>Six</td></tr>"
        "        <tr><td class='border'>Seven</td><td class='border'>Eight</td></tr>"
        "      </tbody>"
        "    </table>"
        "  </body>"
        "</html>"];

    __block Vector<std::pair<NSString *, NSTextTableBlock *>> allTableCells;
    RetainPtr string = [webView _contentsAsAttributedString];
    [string enumerateAttributesInRange:NSMakeRange(0, [string length]) options:0 usingBlock:^(NSDictionary<NSAttributedStringKey, id> *attributes, NSRange range, BOOL *) {
        auto trimmedSubstring = [[[string string] substringWithRange:range] stringByTrimmingCharactersInSet:NSCharacterSet.whitespaceAndNewlineCharacterSet];
        auto textBlocks = [attributes[NSParagraphStyleAttributeName] textBlocks];
        EXPECT_EQ(textBlocks.count, 1U);
        EXPECT_TRUE([textBlocks[0] isKindOfClass:NSClassFromString(@"NSTextTableBlock")]);
        allTableCells.append({ trimmedSubstring, static_cast<NSTextTableBlock *>(textBlocks[0]) });
    }];

    auto checkCellAtIndex = ^(size_t index, NSString *expectedText, NSInteger expectedColumn, NSInteger expectedRow, NSTextTable *expectedTable,
        CGFloat expectedBorderWidth, WebCore::CocoaColor *expectedBackgroundColor) {
        auto [text, cell] = allTableCells[index];
        EXPECT_WK_STREQ(expectedText, text);
        EXPECT_EQ(cell.startingColumn, expectedColumn);
        EXPECT_EQ(cell.startingRow, expectedRow);
        EXPECT_EQ(cell.columnSpan, static_cast<NSInteger>(1));
        EXPECT_EQ(cell.rowSpan, static_cast<NSInteger>(1));
        EXPECT_EQ(cell.table, expectedTable);
#if PLATFORM(IOS_FAMILY)
        auto leftEdge = CGRectMinXEdge;
#else
        auto leftEdge = NSRectEdgeMinX;
#endif
        EXPECT_EQ([cell widthForLayer:NSTextBlockBorder edge:leftEdge], expectedBorderWidth);

        if (!expectedBackgroundColor)
            EXPECT_NULL(cell.backgroundColor);
        else {
            auto cellColor = WebCore::colorFromCocoaColor([cell backgroundColor]);
            auto expectedColor = WebCore::colorFromCocoaColor(expectedBackgroundColor);
            EXPECT_EQ(cellColor, expectedColor);
        }
    };

    EXPECT_EQ(allTableCells.size(), 8U);
    auto firstTable = allTableCells.first().second.table;
    auto secondTable = allTableCells.last().second.table;

    checkCellAtIndex(0, @"One", 0, 0, firstTable, 0, [WebCore::CocoaColor redColor]);
    checkCellAtIndex(1, @"Two", 1, 0, firstTable, 0, [WebCore::CocoaColor redColor]);
    checkCellAtIndex(2, @"Three", 0, 1, firstTable, 0, [WebCore::CocoaColor redColor]);
    checkCellAtIndex(3, @"Four", 1, 1, firstTable, 0, [WebCore::CocoaColor redColor]);
    checkCellAtIndex(4, @"Five", 0, 0, secondTable, 2., nil);
    checkCellAtIndex(5, @"Six", 1, 0, secondTable, 2., nil);
    checkCellAtIndex(6, @"Seven", 0, 1, secondTable, 2., nil);
    checkCellAtIndex(7, @"Eight", 1, 1, secondTable, 2., nil);
}

TEST(WKWebView, AttributedStringWithLinksInTableCell)
{
    auto webView = adoptNS([TestWKWebView new]);
    [webView synchronouslyLoadHTMLString:@"<html>"
        "  <body>"
        "    <table>"
        "      <tbody>"
        "        <tr>"
        "          <td><a href='https://webkit.org'>WebKit</a> One</td>"
        "          <td><a href='https://apple.com'>Apple</a> Two</td>"
        "        </tr>"
        "      </tbody>"
        "    </table>"
        "  </body>"
        "</html>"];

    __block Vector<std::pair<NSString *, NSTextTableBlock *>> allTableCells;
    RetainPtr string = [webView _contentsAsAttributedString];
    [string enumerateAttributesInRange:NSMakeRange(0, [string length]) options:0 usingBlock:^(NSDictionary<NSAttributedStringKey, id> *attributes, NSRange range, BOOL *) {
        auto trimmedSubstring = [[[string string] substringWithRange:range] stringByTrimmingCharactersInSet:NSCharacterSet.whitespaceAndNewlineCharacterSet];
        auto textBlocks = [attributes[NSParagraphStyleAttributeName] textBlocks];
        EXPECT_EQ(textBlocks.count, 1U);
        EXPECT_TRUE([textBlocks[0] isKindOfClass:NSClassFromString(@"NSTextTableBlock")]);
        allTableCells.append({ trimmedSubstring, static_cast<NSTextTableBlock *>(textBlocks[0]) });
    }];

    auto checkCellAtIndex = ^(size_t index, NSString *expectedText, NSInteger expectedColumn, NSInteger expectedRow, NSTextTable *expectedTable) {
        auto [text, cell] = allTableCells[index];
        EXPECT_WK_STREQ(expectedText, text);
        EXPECT_EQ(cell.startingColumn, expectedColumn);
        EXPECT_EQ(cell.startingRow, expectedRow);
        EXPECT_EQ(cell.columnSpan, static_cast<NSInteger>(1));
        EXPECT_EQ(cell.rowSpan, static_cast<NSInteger>(1));
        EXPECT_EQ(cell.table, expectedTable);
    };

    EXPECT_EQ(allTableCells.size(), 4U);
    auto table = allTableCells.first().second.table;
    checkCellAtIndex(0, @"WebKit", 0, 0, table);
    checkCellAtIndex(1, @"One", 0, 0, table);
    checkCellAtIndex(2, @"Apple", 1, 0, table);
    checkCellAtIndex(3, @"Two", 1, 0, table);

    EXPECT_EQ(allTableCells[0].second, allTableCells[1].second);
    EXPECT_FALSE(allTableCells[1].second == allTableCells[2].second);
    EXPECT_EQ(allTableCells[2].second, allTableCells[3].second);
}

TEST(WKWebView, AttributedStringFromList)
{
    auto webView = adoptNS([TestWKWebView new]);
    [webView synchronouslyLoadHTMLString:@"<html>"
        "  <body>"
        "    <ol>"
        "      <li>One</li><li>Two</li>"
        "    </ol>"
        "    <ul>"
        "      <li>Three</li><li>Four</li>"
        "    </ul>"
        "  </body>"
        "</html>"];

    __block Vector<std::pair<NSString *, NSTextList *>> allTextLists;
    RetainPtr string = [webView _contentsAsAttributedString];
    [string enumerateAttributesInRange:NSMakeRange(0, [string length]) options:0 usingBlock:^(NSDictionary<NSAttributedStringKey, id> *attributes, NSRange range, BOOL *) {
        auto trimmedSubstring = [[[string string] substringWithRange:range] stringByTrimmingCharactersInSet:NSCharacterSet.whitespaceAndNewlineCharacterSet];
        auto textLists = [attributes[NSParagraphStyleAttributeName] textLists];
        EXPECT_EQ(textLists.count, 1U);
        allTextLists.append({ trimmedSubstring, textLists[0] });
    }];

    auto checkListAtIndex = ^(size_t index, NSString *expectedText, NSTextList *expectedList) {
        auto [text, list] = allTextLists[index];
        EXPECT_WK_STREQ(expectedText, text);
        EXPECT_EQ(list, expectedList);
    };

    EXPECT_EQ(allTextLists.size(), 8U);
    auto firstList = allTextLists.first().second;
    auto secondList = allTextLists.last().second;

    checkListAtIndex(0, @"1", firstList);
    checkListAtIndex(1, @"One", firstList);
    checkListAtIndex(2, @"2", firstList);
    checkListAtIndex(3, @"Two", firstList);
    checkListAtIndex(4, @"•", secondList);
    checkListAtIndex(5, @"Three", secondList);
    checkListAtIndex(6, @"•", secondList);
    checkListAtIndex(7, @"Four", secondList);
}

TEST(WKWebView, AttributedStringWithoutNetworkLoads)
{
    [TestProtocol registerWithScheme:@"https"];

    NSString *markup = @""
        "<body>"
        "<strong>Hello</strong>"
        "<img src='https://webkit.org/nonexistent-image.png'>"
        "</body>";

    __block bool attemptedImageLoad = false;
    auto resourceLoadDelegate = adoptNS([TestResourceLoadDelegate new]);
    [resourceLoadDelegate setDidSendRequest:^(WKWebView *, _WKResourceLoadInfo *info, NSURLRequest *) {
        if (info.resourceType == _WKResourceLoadInfoResourceTypeImage)
            attemptedImageLoad = true;
    }];

    __block bool done = false;
    __block RetainPtr<NSAttributedString> resultString;
    __block RetainPtr<NSError> resultError;
    [NSAttributedString _loadFromHTMLWithOptions:@{ _WKAllowNetworkLoadsOption : @NO } contentLoader:^(WKWebView *webView) {
        webView._resourceLoadDelegate = resourceLoadDelegate.get();
        return [webView loadHTMLString:markup baseURL:nil];
    } completionHandler:^(NSAttributedString *string, NSDictionary *, NSError *error) {
        resultString = string;
        resultError = error;
        done = true;
    }];
    Util::run(&done);

    EXPECT_NULL(resultError);

    auto helloRange = [[resultString string] rangeOfString:@"Hello"];
    EXPECT_EQ(helloRange.location, 0U);
    EXPECT_EQ(helloRange.length, 5U);
    EXPECT_FALSE(attemptedImageLoad);
}

TEST(WKWebView, AttributedStringWithSourceApplicationBundleID)
{
    [TestProtocol registerWithScheme:@"https"];

    RetainPtr markup = @"<p>Remote image</p><img src='https://bundle-file/icon.png'>";
    RetainPtr bundleID = @"com.apple.Safari";
    auto options = @{
        _WKAllowNetworkLoadsOption : @YES,
        _WKSourceApplicationBundleIdentifierOption : bundleID.get()
    };

    __block bool done = false;
    __block RetainPtr<NSAttributedString> resultString;
    __block RetainPtr<NSError> resultError;
    __block RetainPtr<WKWebView> loaderWebView;
    [NSAttributedString _loadFromHTMLWithOptions:options contentLoader:^(WKWebView *webView) {
        loaderWebView = webView;
        return [webView loadHTMLString:markup.get() baseURL:nil];
    } completionHandler:^(NSAttributedString *string, NSDictionary *, NSError *error) {
        resultString = string;
        resultError = error;
        done = true;
    }];
    Util::run(&done);

    EXPECT_NULL(resultError);

    auto dataStore = [loaderWebView configuration].websiteDataStore;
    EXPECT_FALSE(dataStore.isPersistent);
    EXPECT_WK_STREQ(bundleID.get(), dataStore._configuration.sourceApplicationBundleIdentifier);

    auto textRange = [[resultString string] rangeOfString:@"Remote image"];
    EXPECT_EQ(textRange.location, 0U);
}

TEST(WKWebView, TextWithWebFontAsAttributedString)
{
    auto archiveURL = [NSBundle.test_resourcesBundle URLForResource:@"text-with-web-font" withExtension:@"webarchive"];
    auto archiveData = adoptNS([[NSData alloc] initWithContentsOfURL:archiveURL]);

    RetainPtr<NSAttributedString> result;
    bool done = false;
    [NSAttributedString _loadFromHTMLWithOptions:@{ } contentLoader:[&](WKWebView *webView) -> WKNavigation * {
        return [webView loadData:archiveData.get() MIMEType:@"application/x-webarchive" characterEncodingName:@"" baseURL:NSBundle.mainBundle.bundleURL];
    } completionHandler:[&](NSAttributedString *string, NSDictionary<NSAttributedStringDocumentAttributeKey, id> *attributes, NSError *error) {
        EXPECT_NULL(error);
        result = string;
        done = true;
    }];
    Util::run(&done);

    __auto_type whitespaceSet = NSCharacterSet.whitespaceAndNewlineCharacterSet;
    __block Vector<std::pair<RetainPtr<NSString>, WebCore::CocoaFont *>, 3> substringsAndFonts;
    [result enumerateAttributesInRange:NSMakeRange(0, [result length]) options:0 usingBlock:^(NSDictionary<NSAttributedStringKey, id> *attributes, NSRange range, BOOL *) {
        substringsAndFonts.append({
            { [[[result string] substringWithRange:range] stringByTrimmingCharactersInSet:whitespaceSet] },
            dynamic_objc_cast<WebCore::CocoaFont>(attributes[NSFontAttributeName])
        });
    }];

    EXPECT_EQ(3U, substringsAndFonts.size());
    EXPECT_WK_STREQ(@"Hello world in system font.", substringsAndFonts[0].first.get());
    EXPECT_WK_STREQ(@"Hello world in Times.", substringsAndFonts[1].first.get());
    EXPECT_WK_STREQ(@"Hello world in Ahem.", substringsAndFonts[2].first.get());

    EXPECT_TRUE([substringsAndFonts[1].second.fontName containsString:@"Times"]);
    EXPECT_WK_STREQ(substringsAndFonts[0].second.fontName, substringsAndFonts[2].second.fontName);
    EXPECT_FALSE([substringsAndFonts[2].second.fontName containsString:@"LastResort"]);
}

TEST(WKWebView, AttributedStringAndCDATASection)
{
    RetainPtr<TestWKWebView> webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600)]);

    [webView synchronouslyLoadHTMLString:@"<body>1<script>document.body.append('2', new Document().createCDATASection('3'));</script>4"];

    __block bool finished = false;

    [webView _getContentsAsAttributedStringWithCompletionHandler:^(NSAttributedString *attributedString, NSDictionary<NSAttributedStringDocumentAttributeKey, id> *documentAttributes, NSError *error) {
        EXPECT_EQ([attributedString length], 4U);
        auto expectedSubstring = [[attributedString string] rangeOfString:@"1234"];
        EXPECT_EQ(expectedSubstring.location, 0U);
        EXPECT_EQ(expectedSubstring.length, 4U);
        finished = true;
    }];

    Util::run(&finished);
}

TEST(WKWebView, AttributedStringIncludesUserSelectNoneContent)
{
    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 400, 300)]);
    [webView synchronouslyLoadHTMLString:@"<body><p style='-webkit-user-select: none;'>Hello</p></body>"];

    RetainPtr string = [[webView _contentsAsAttributedString] string];
    EXPECT_WK_STREQ("Hello", [string stringByTrimmingCharactersInSet:NSCharacterSet.newlineCharacterSet]);
}

TEST(WKWebView, RequestAllTextRunsWithSubframes)
{
    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 400, 300)]);
    [webView synchronouslyLoadTestPageNamed:@"subframes"];

    Util::waitForConditionWithLogging([&] -> bool {
        return [[webView objectByEvaluatingJavaScript:@"subframeLoaded"] boolValue];
    }, 5, @"Timed out waiting for subframes to load.");

    RetainPtr allTextRuns = [webView _allTextRuns];

    Vector<std::pair<const char*, CGRect>> expectedResults {
#if PLATFORM(MAC)
        { "Here's to the crazy", CGRectMake(0, 18, 298, 33) },
        { "ones.", CGRectMake(0, 18, 298, 33) },
        { "The round", CGRectMake(9, 84, 160, 64) },
        { "pegs in", CGRectMake(9, 84, 160, 64) },
        { "the square", CGRectMake(9, 84, 160, 64) },
        { "holes.", CGRectMake(9, 84, 160, 64) },
        { "The", CGRectMake(18, 157, 192, 96) },
        { "ones", CGRectMake(18, 157, 192, 96) },
        { "who", CGRectMake(18, 157, 192, 96) },
        { "see", CGRectMake(18, 157, 192, 96) },
        { "things", CGRectMake(18, 157, 192, 96) },
        { "differently.", CGRectMake(18, 157, 192, 96) },
#else
        { "Here's to the crazy ones.", CGRectMake(0, 18, 394, 17) },
        { "The round", CGRectMake(9, 68, 176, 68) },
        { "pegs in the", CGRectMake(9, 68, 176, 68) },
        { "square", CGRectMake(9, 68, 176, 68) },
        { "holes.", CGRectMake(9, 68, 176, 68) },
        { "The", CGRectMake(18, 145, 192, 102) },
        { "ones", CGRectMake(18, 145, 192, 102) },
        { "who", CGRectMake(18, 145, 192, 102) },
        { "see", CGRectMake(18, 145, 192, 102) },
        { "things", CGRectMake(18, 145, 192, 102) },
        { "differently.", CGRectMake(18, 145, 192, 102) },
#endif
    };

    EXPECT_EQ([allTextRuns count], expectedResults.size());

    for (size_t i = 0; i < expectedResults.size(); ++i) {
        auto [expectedString, expectedRect] = expectedResults[i];
        RetainPtr textRun = [allTextRuns objectAtIndex:i];
        CGRect rectInWebView = [textRun rectInWebView];
        EXPECT_EQ(expectedRect.origin.x, rectInWebView.origin.x);
        EXPECT_EQ(expectedRect.origin.y, rectInWebView.origin.y);
        EXPECT_EQ(expectedRect.size.width, rectInWebView.size.width);
        EXPECT_EQ(expectedRect.size.height, rectInWebView.size.height);
        EXPECT_WK_STREQ(expectedString, [textRun text]);
    }
}

} // namespace TestWebKitAPI
