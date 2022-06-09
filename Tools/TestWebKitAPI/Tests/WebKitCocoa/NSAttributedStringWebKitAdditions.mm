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
