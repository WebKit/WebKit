/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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

#import "TestCocoa.h"
#import "TestWKWebView.h"
#import <WebKit/NSAttributedString.h>
#import <wtf/RetainPtr.h>

TEST(NSAttributedStringWebKitAdditions, DefaultFontSize)
{
    __block bool done = false;
    [NSAttributedString loadFromHTMLWithString:@"Hello World" options:@{ } completionHandler:^(NSAttributedString *attributedString, NSDictionary<NSAttributedStringDocumentAttributeKey, id> *attributes, NSError *error) {
        EXPECT_EQ([[attributedString attribute:NSFontAttributeName atIndex:0 effectiveRange:nil] pointSize], 12.0);

        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
}

TEST(NSAttributedStringWebKitAdditions, MultipleParagraphs)
{
    __block bool done = false;
    [NSAttributedString loadFromHTMLWithString:@"<p style='margin-bottom:1px'>Hello</p><p style='margin-bottom:2px'>World</p>" options:@{ } completionHandler:^(NSAttributedString *attributedString, NSDictionary<NSAttributedStringDocumentAttributeKey, id> *attributes, NSError *error) {
        EXPECT_EQ([[attributedString attribute:NSParagraphStyleAttributeName atIndex:0 effectiveRange:nil] paragraphSpacing], 1.0);
        EXPECT_EQ([[attributedString attribute:NSParagraphStyleAttributeName atIndex:6 effectiveRange:nil] paragraphSpacing], 2.0);

        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
}

TEST(NSAttributedStringWebKitAdditions, BlockQuotes)
{
    __block bool done = false;
    [NSAttributedString loadFromHTMLWithString:@"<p>Hello</p><blockquote>Down<blockquote>under</blockquote></blockquote>" options:@{ } completionHandler:^(NSAttributedString *attributedString, NSDictionary<NSAttributedStringDocumentAttributeKey, id> *attributes, NSError *error) {
        EXPECT_EQ([[attributedString attribute:NSParagraphStyleAttributeName atIndex:0 effectiveRange:nil] headerLevel], 0);
        EXPECT_EQ([[attributedString attribute:NSParagraphStyleAttributeName atIndex:6 effectiveRange:nil] headerLevel], 0);
        NSPresentationIntent* quote1 = [attributedString attribute:NSPresentationIntentAttributeName atIndex:6 effectiveRange:nil];
        ASSERT_NOT_NULL(quote1);
        EXPECT_EQ([quote1 indentationLevel], 0);
        EXPECT_EQ([[attributedString attribute:NSParagraphStyleAttributeName atIndex:11 effectiveRange:nil] headerLevel], 0);
        NSPresentationIntent* quote2 = [attributedString attribute:NSPresentationIntentAttributeName atIndex:11 effectiveRange:nil];
        ASSERT_NOT_NULL(quote2);
        EXPECT_EQ([quote2 indentationLevel], 1);
        EXPECT_NE([quote1 identity], [quote2 identity]);
        EXPECT_EQ([quote1 identity], [[quote2 parentIntent] identity]);

        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
}

#if PLATFORM(IOS_FAMILY)
TEST(NSAttributedStringWebKitAdditions, DirectoriesNotCreated)
{
    NSString *directory = [NSHomeDirectory() stringByAppendingString:@"/Library/Cookies"];
    NSFileManager *fileManager = [NSFileManager defaultManager];
    [fileManager removeItemAtPath:directory error:nil];

    __block bool done = false;
    [NSAttributedString loadFromHTMLWithData:[NSData data] options:@{ } completionHandler:^(NSAttributedString *attributedString, NSDictionary<NSAttributedStringDocumentAttributeKey, id> *attributes, NSError *error) {
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);

    auto cookieDirectoryExists = [&] {
        return [[NSFileManager defaultManager] fileExistsAtPath:directory];
    };
    EXPECT_FALSE(cookieDirectoryExists());

    auto webView = adoptNS([TestWKWebView new]);
    [webView synchronouslyLoadHTMLString:@"hi"];
    EXPECT_TRUE(cookieDirectoryExists());
}
#endif

TEST(NSAttributedStringWebKitAdditions, FontDataURL)
{
    NSURL *fontURL = [[NSBundle mainBundle] URLForResource:@"Ahem" withExtension:@"ttf" subdirectory:@"TestWebKitAPI.resources"];
    NSData *fontData = [NSData dataWithContentsOfURL:fontURL];
    NSString *fontBase64 = [fontData base64EncodedStringWithOptions:0];

    NSString *html = [NSString stringWithFormat:@""
        "<html>"
        "<head>"
        "<style>"
        "@font-face { font-family: exampleFont; src: url(data:font/opentype;base64,%@); }"
        "div { font-family: exampleFont; }"
        "</style>"
        "</head>"
        "<body><div>hello!</div></body>"
        "</html>", fontBase64];

    __block bool done = false;
    [NSAttributedString loadFromHTMLWithString:html options:@{ } completionHandler:^(NSAttributedString *attributedString, NSDictionary<NSAttributedStringDocumentAttributeKey, id> *attributes, NSError *error) {
        __block bool foundFont { false };
        [attributedString enumerateAttributesInRange:NSMakeRange(0, attributedString.length) options:0 usingBlock:^(NSDictionary *attributes, NSRange attributeRange, BOOL *stop) {
            if (attributes[NSFontAttributeName])
                foundFont = true;
        }];
        EXPECT_TRUE(foundFont);
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
}
