/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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

#import "InstanceMethodSwizzler.h"
#import "TestWKWebView.h"
#import "Utilities.h"
#import <WebKit/WKWebViewPrivate.h>

namespace TestWebKitAPI {

static NSInteger lastDocumentSpellCheckingTag = 0;
static bool handledCheckStringRequest = false;
static NSArray<NSTextCheckingResult *> *swizzledCheckString(id, SEL, NSString *, NSRange, NSTextCheckingTypes, NSDictionary<NSTextCheckingOptionKey, id> *, NSInteger tag, NSOrthography **, NSInteger *)
{
    lastDocumentSpellCheckingTag = tag;
    handledCheckStringRequest = true;
    return @[ ];
}

TEST(SpellCheckerDocumentTag, SpellCheckerDocumentTagWhenCheckingString)
{
    InstanceMethodSwizzler spellCheckingSwizzler {
        NSSpellChecker.sharedSpellChecker.class,
        @selector(checkString:range:types:options:inSpellDocumentWithTag:orthography:wordCount:),
        reinterpret_cast<IMP>(swizzledCheckString)
    };

    auto webView = adoptNS([[TestWKWebView<NSTextInputClient> alloc] initWithFrame:CGRectMake(0, 0, 400, 400)]);
    [webView synchronouslyLoadHTMLString:@"<body contenteditable>"];
    [webView objectByEvaluatingJavaScript:@"getSelection().setPosition(document.body)"];
    [webView insertText:@"Testing.\n" replacementRange:NSMakeRange(0, 0)];
    Util::run(&handledCheckStringRequest);

    EXPECT_GT(lastDocumentSpellCheckingTag, 0);
    EXPECT_GT([webView _spellCheckerDocumentTag], 0);
    EXPECT_EQ(lastDocumentSpellCheckingTag, [webView _spellCheckerDocumentTag]);
}

} // namespace TestWebKitAPI

#endif // PLATFORM(MAC)
