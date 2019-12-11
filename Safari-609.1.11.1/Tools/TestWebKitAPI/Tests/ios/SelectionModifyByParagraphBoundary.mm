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
#import <WebKit/WKWebViewPrivate.h>

typedef UITextInputArrowKeyHistory *(*MoveParagraphSelectorType)(id, SEL, BOOL, UITextInputArrowKeyHistory *);

TEST(SelectionTests, ModifyByParagraphBoundary)
{
    auto webView = adoptNS([[TestWKWebView alloc] init]);
    auto inputDelegate = adoptNS([[TestInputDelegate alloc] init]);
    [inputDelegate setFocusStartsInputSessionPolicyHandler:[] (WKWebView *, id <_WKFocusedElementInfo>) -> _WKFocusStartsInputSessionPolicy {
        return _WKFocusStartsInputSessionPolicyAllow;
    }];

    [webView _setInputDelegate:inputDelegate.get()];
    [webView _setEditable:YES];
    [webView synchronouslyLoadHTMLString:@"<meta name='viewport' content='width=device-width, initial-scale=1'><body contenteditable>hello<div id='blankLine'><br></div><div id='lastLine'>world</div></body>"];
    [webView evaluateJavaScriptAndWaitForInputSessionToChange:@"document.body.focus()"];

    EXPECT_TRUE([webView stringByEvaluatingJavaScript:@"getSelection().isCollapsed"].boolValue);
    EXPECT_TRUE([webView stringByEvaluatingJavaScript:@"getSelection().anchorNode == document.body.firstChild"].boolValue);
    EXPECT_EQ([webView stringByEvaluatingJavaScript:@"getSelection().anchorOffset"].intValue, 0);

    UIView *contentView = [webView valueForKeyPath:@"_currentContentView"];
    SEL moveToEndOfParagraphSelector = NSSelectorFromString(@"_moveToEndOfParagraph:withHistory:");
    auto moveToEndOfParagraphMethod = (MoveParagraphSelectorType)[contentView methodForSelector:moveToEndOfParagraphSelector];
    SEL moveToStartOfParagraphSelector = NSSelectorFromString(@"_moveToStartOfParagraph:withHistory:");
    auto moveToStartOfParagraphMethod = (MoveParagraphSelectorType)[contentView methodForSelector:moveToStartOfParagraphSelector];

    moveToEndOfParagraphMethod(contentView, moveToEndOfParagraphSelector, FALSE, nil);
    EXPECT_TRUE([webView stringByEvaluatingJavaScript:@"getSelection().isCollapsed"].boolValue);
    EXPECT_TRUE([webView stringByEvaluatingJavaScript:@"getSelection().anchorNode == document.body.firstChild"].boolValue);
    EXPECT_EQ([webView stringByEvaluatingJavaScript:@"getSelection().anchorOffset"].intValue, 5);

    moveToEndOfParagraphMethod(contentView, moveToEndOfParagraphSelector, FALSE, nil);
    EXPECT_TRUE([webView stringByEvaluatingJavaScript:@"getSelection().isCollapsed"].boolValue);
    EXPECT_TRUE([webView stringByEvaluatingJavaScript:@"getSelection().anchorNode == document.getElementById('blankLine')"].boolValue);
    EXPECT_EQ([webView stringByEvaluatingJavaScript:@"getSelection().anchorOffset"].intValue, 0);

    moveToEndOfParagraphMethod(contentView, moveToEndOfParagraphSelector, FALSE, nil);
    EXPECT_TRUE([webView stringByEvaluatingJavaScript:@"getSelection().isCollapsed"].boolValue);
    EXPECT_TRUE([webView stringByEvaluatingJavaScript:@"getSelection().anchorNode == document.getElementById('lastLine').lastChild"].boolValue);
    EXPECT_EQ([webView stringByEvaluatingJavaScript:@"getSelection().anchorOffset"].intValue, 5);

    moveToStartOfParagraphMethod(contentView, moveToStartOfParagraphSelector, FALSE, nil);
    EXPECT_TRUE([webView stringByEvaluatingJavaScript:@"getSelection().isCollapsed"].boolValue);
    EXPECT_TRUE([webView stringByEvaluatingJavaScript:@"getSelection().anchorNode == document.getElementById('lastLine').firstChild"].boolValue);
    EXPECT_EQ([webView stringByEvaluatingJavaScript:@"getSelection().anchorOffset"].intValue, 0);

    moveToStartOfParagraphMethod(contentView, moveToStartOfParagraphSelector, FALSE, nil);
    EXPECT_TRUE([webView stringByEvaluatingJavaScript:@"getSelection().isCollapsed"].boolValue);
    EXPECT_TRUE([webView stringByEvaluatingJavaScript:@"getSelection().anchorNode == document.getElementById('blankLine')"].boolValue);
    EXPECT_EQ([webView stringByEvaluatingJavaScript:@"getSelection().anchorOffset"].intValue, 0);

    moveToStartOfParagraphMethod(contentView, moveToStartOfParagraphSelector, FALSE, nil);
    EXPECT_TRUE([webView stringByEvaluatingJavaScript:@"getSelection().isCollapsed"].boolValue);
    EXPECT_TRUE([webView stringByEvaluatingJavaScript:@"getSelection().anchorNode == document.body.firstChild"].boolValue);
    EXPECT_EQ([webView stringByEvaluatingJavaScript:@"getSelection().anchorOffset"].intValue, 0);

    moveToEndOfParagraphMethod(contentView, moveToEndOfParagraphSelector, TRUE, nil);
    EXPECT_FALSE([webView stringByEvaluatingJavaScript:@"getSelection().isCollapsed"].boolValue);
    EXPECT_WK_STREQ([webView stringByEvaluatingJavaScript:@"getSelection().toString()"], "hello");

    moveToEndOfParagraphMethod(contentView, moveToEndOfParagraphSelector, TRUE, nil);
    EXPECT_FALSE([webView stringByEvaluatingJavaScript:@"getSelection().isCollapsed"].boolValue);
    EXPECT_WK_STREQ([webView stringByEvaluatingJavaScript:@"getSelection().toString()"], "hello\n");

    moveToEndOfParagraphMethod(contentView, moveToEndOfParagraphSelector, TRUE, nil);
    EXPECT_FALSE([webView stringByEvaluatingJavaScript:@"getSelection().isCollapsed"].boolValue);
    EXPECT_WK_STREQ([webView stringByEvaluatingJavaScript:@"getSelection().toString()"], "hello\n\nworld");

    [webView stringByEvaluatingJavaScript:@"getSelection().setPosition(document.body, document.body.childNodes.length)"];

    moveToStartOfParagraphMethod(contentView, moveToStartOfParagraphSelector, TRUE, nil);
    EXPECT_FALSE([webView stringByEvaluatingJavaScript:@"getSelection().isCollapsed"].boolValue);
    EXPECT_WK_STREQ([webView stringByEvaluatingJavaScript:@"getSelection().toString()"], "world");

    moveToStartOfParagraphMethod(contentView, moveToStartOfParagraphSelector, TRUE, nil);
    EXPECT_FALSE([webView stringByEvaluatingJavaScript:@"getSelection().isCollapsed"].boolValue);
    EXPECT_WK_STREQ([webView stringByEvaluatingJavaScript:@"getSelection().toString()"], "\nworld");

    moveToStartOfParagraphMethod(contentView, moveToStartOfParagraphSelector, TRUE, nil);
    EXPECT_FALSE([webView stringByEvaluatingJavaScript:@"getSelection().isCollapsed"].boolValue);
    EXPECT_WK_STREQ([webView stringByEvaluatingJavaScript:@"getSelection().toString()"], "hello\n\nworld");
}

#endif // PLATFORM(IOS_FAMILY)
