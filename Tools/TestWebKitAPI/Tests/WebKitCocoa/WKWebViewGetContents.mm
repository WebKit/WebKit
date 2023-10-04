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
#import "TestWKWebView.h"
#import "UIKitSPIForTesting.h"
#import <pal/spi/cocoa/NSAttributedStringSPI.h>
#import <wtf/cocoa/TypeCastsCocoa.h>

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

@end

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

    TestWebKitAPI::Util::run(&finished);
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

    TestWebKitAPI::Util::run(&finished);
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

    TestWebKitAPI::Util::run(&finished);
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

    TestWebKitAPI::Util::run(&finished);
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

    TestWebKitAPI::Util::run(&finished);
}

TEST(WKWebView, AttributedStringAccessibilityLabel)
{
    auto webView = adoptNS([TestWKWebView new]);

    NSString *imagePath = [[NSBundle mainBundle] pathForResource:@"icon" ofType:@"png" inDirectory:@"TestWebKitAPI.resources"];
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

    TestWebKitAPI::Util::run(&finished);
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

    TestWebKitAPI::Util::run(&finished);
}

TEST(WKWebView, AttributedStringFromTable)
{
    auto webView = adoptNS([TestWKWebView new]);
    [webView synchronouslyLoadHTMLString:@"<html>"
        "  <body>"
        "    <table>"
        "      <tbody>"
        "        <tr><td>One</td><td>Two</td></tr>"
        "        <tr><td>Three</td><td>Four</td></tr>"
        "      </tbody>"
        "    </table>"
        "    <table>"
        "      <tbody>"
        "        <tr><td>Five</td><td>Six</td></tr>"
        "        <tr><td>Seven</td><td>Eight</td></tr>"
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

    EXPECT_EQ(allTableCells.size(), 8U);
    auto firstTable = allTableCells.first().second.table;
    auto secondTable = allTableCells.last().second.table;

    checkCellAtIndex(0, @"One", 0, 0, firstTable);
    checkCellAtIndex(1, @"Two", 1, 0, firstTable);
    checkCellAtIndex(2, @"Three", 0, 1, firstTable);
    checkCellAtIndex(3, @"Four", 1, 1, firstTable);
    checkCellAtIndex(4, @"Five", 0, 0, secondTable);
    checkCellAtIndex(5, @"Six", 1, 0, secondTable);
    checkCellAtIndex(6, @"Seven", 0, 1, secondTable);
    checkCellAtIndex(7, @"Eight", 1, 1, secondTable);
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
