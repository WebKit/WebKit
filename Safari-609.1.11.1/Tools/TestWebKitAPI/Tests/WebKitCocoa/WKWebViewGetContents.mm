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
#import <WebKit/WebKit.h>
#import <wtf/RetainPtr.h>

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
