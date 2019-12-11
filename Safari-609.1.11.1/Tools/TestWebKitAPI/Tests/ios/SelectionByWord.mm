/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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

#if PLATFORM(IOS_FAMILY)

#import "PlatformUtilities.h"
#import "TestInputDelegate.h"
#import "TestWKWebView.h"
#import "UIKitSPI.h"

@interface WKContentView (Private)
- (void)selectWordForReplacement;
@end

TEST(SelectionTests, ByWordAtEndOfDocument)
{
    auto webView = adoptNS([[TestWKWebView alloc] init]);

    [webView synchronouslyLoadHTMLString:@"<body><p>Paragraph One</p><p style='-webkit-user-select: none'>Paragraph Two</p><p>Paragraph Three</p><p></p></body>"];

    [webView stringByEvaluatingJavaScript:@"getSelection().setPosition(document.body.lastChild, 0)"];

    EXPECT_TRUE([webView stringByEvaluatingJavaScript:@"getSelection().isCollapsed"].boolValue);

    // This method triggers the wordRangeFromPosition() code to be tested.
    [[webView wkContentView] selectWordForReplacement];

    EXPECT_FALSE([webView stringByEvaluatingJavaScript:@"getSelection().isCollapsed"].boolValue);
    EXPECT_WK_STREQ([webView stringByEvaluatingJavaScript:@"getSelection().anchorNode.nodeValue"], "Paragraph Three");
    EXPECT_WK_STREQ([webView stringByEvaluatingJavaScript:@"getSelection().anchorOffset"], "10");
    EXPECT_WK_STREQ([webView stringByEvaluatingJavaScript:@"getSelection().focusNode.nodeValue"], "Paragraph Three");
    EXPECT_WK_STREQ([webView stringByEvaluatingJavaScript:@"getSelection().focusOffset"], "15");

    EXPECT_WK_STREQ([webView stringByEvaluatingJavaScript:@"getSelection().toString()"], "Three");
}

#endif // PLATFORM(IOS_FAMILY)
