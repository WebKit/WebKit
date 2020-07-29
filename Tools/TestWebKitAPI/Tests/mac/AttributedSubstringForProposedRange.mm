/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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

#import "AppKitSPI.h"
#import "PlatformUtilities.h"
#import "TestWKWebView.h"
#import <wtf/RetainPtr.h>

TEST(AttributedSubstringForProposedRange, TextAlignmentParagraphStyles)
{
    NSString *markup = @"<!DOCTYPE html>"
        "<meta charset='utf8'>"
        "<body contenteditable>"
        "<div>Default</div>"
        "<div style='text-align: right;'>Right</div>"
        "<div style='text-align: center;'>Center</div>"
        "<div style='text-align: left;'>Left</div>"
        "<div style='text-align: justify;'>Justify</div>"
        "<div style='text-align: start;' dir='rtl'>Start + RTL</div>"
        "<div style='text-align: end;' dir='rtl'>End + RTL</div>"
        "<div style='text-align: end;' dir='ltr'>End + LTR</div>"
        "<div style='text-align: start;' dir='ltr'>Start + LTR</div>"
        "</body>"
        "<script>getSelection().setPosition(document.body)</script>";

    auto webView = adoptNS([[TestWKWebView<NSTextInputClient_Async> alloc] initWithFrame:NSMakeRect(0, 0, 800, 600)]);
    [webView synchronouslyLoadHTMLString:markup];

    __block bool finished = false;
    [webView attributedSubstringForProposedRange:NSMakeRange(0, NSUIntegerMax) completionHandler:^(NSAttributedString *string, NSRange actualRange) {
        EXPECT_EQ(0U, actualRange.location);
        EXPECT_EQ(77U, actualRange.length);

        auto textAlignmentForSubstring = [&](NSString *substring) -> NSTextAlignment {
            NSRange range = [string.string rangeOfString:substring];
            EXPECT_GT(range.length, 0U);

            __block RetainPtr<NSParagraphStyle> result;
            [string enumerateAttribute:NSParagraphStyleAttributeName inRange:range options:0 usingBlock:^(NSParagraphStyle *style, NSRange, BOOL *) {
                result = style;
            }];
            return result ? [result alignment] : NSTextAlignmentNatural;
        };

        EXPECT_EQ(NSTextAlignmentNatural, textAlignmentForSubstring(@"Default"));
        EXPECT_EQ(NSTextAlignmentRight, textAlignmentForSubstring(@"Right"));
        EXPECT_EQ(NSTextAlignmentCenter, textAlignmentForSubstring(@"Center"));
        EXPECT_EQ(NSTextAlignmentLeft, textAlignmentForSubstring(@"Left"));
        EXPECT_EQ(NSTextAlignmentJustified, textAlignmentForSubstring(@"Justify"));
        EXPECT_EQ(NSTextAlignmentRight, textAlignmentForSubstring(@"Start + RTL"));
        EXPECT_EQ(NSTextAlignmentLeft, textAlignmentForSubstring(@"End + RTL"));
        EXPECT_EQ(NSTextAlignmentRight, textAlignmentForSubstring(@"End + LTR"));
        EXPECT_EQ(NSTextAlignmentLeft, textAlignmentForSubstring(@"Start + LTR"));

        finished = true;
    }];
    TestWebKitAPI::Util::run(&finished);
}

#endif // PLATFORM(MAC)
