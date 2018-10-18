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

#import "config.h"

#if PLATFORM(IOS_FAMILY) && WK_API_ENABLED

#import "PlatformUtilities.h"
#import "TestWKWebView.h"
#import "UIKitSPI.h"
#import <UIKit/UIFontDescriptor.h>
#import <WebKit/WKWebViewPrivate.h>

@interface TestWKWebView (EditActionTesting)
- (BOOL)querySelectorExists:(NSString *)querySelector;
@end

@implementation TestWKWebView (EditActionTesting)

- (BOOL)querySelectorExists:(NSString *)querySelector
{
    return [[self objectByEvaluatingJavaScript:[NSString stringWithFormat:@"!!document.querySelector(`%@`)", querySelector]] boolValue];
}

@end

namespace TestWebKitAPI {

static RetainPtr<TestWKWebView> webViewForEditActionTesting()
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 400, 400)]);
    [webView synchronouslyLoadHTMLString:@"<div>WebKit</div>"];
    [webView _setEditable:YES];
    [webView stringByEvaluatingJavaScript:@"getSelection().setPosition(document.body, 1)"];
    return webView;
}

TEST(WKWebViewEditActions, ListInsertion)
{
    auto webView = webViewForEditActionTesting();

    [webView insertOrderedList:nil];
    EXPECT_TRUE([webView querySelectorExists:@"ol"]);
    [webView insertOrderedList:nil];
    EXPECT_FALSE([webView querySelectorExists:@"ol"]);

    [webView insertUnorderedList:nil];
    EXPECT_TRUE([webView querySelectorExists:@"ul"]);
    [webView insertUnorderedList:nil];
    EXPECT_FALSE([webView querySelectorExists:@"ul"]);
}

TEST(WKWebViewEditActions, ChangeIndentation)
{
    auto webView = webViewForEditActionTesting();

    [webView indent:nil];
    EXPECT_TRUE([webView querySelectorExists:@"blockquote"]);
    [webView indent:nil];
    EXPECT_TRUE([webView querySelectorExists:@"blockquote > blockquote"]);

    [webView outdent:nil];
    EXPECT_TRUE([webView querySelectorExists:@"blockquote"]);
    [webView outdent:nil];
    EXPECT_FALSE([webView querySelectorExists:@"blockquote"]);
}

TEST(WKWebViewEditActions, SetAlignment)
{
    auto webView = webViewForEditActionTesting();
    auto runTest = [webView] {
        [webView alignCenter:nil];
        EXPECT_WK_STREQ("center", [webView stylePropertyAtSelectionStart:@"text-align"]);
        [webView alignLeft:nil];
        EXPECT_WK_STREQ("left", [webView stylePropertyAtSelectionStart:@"text-align"]);
        [webView alignRight:nil];
        EXPECT_WK_STREQ("right", [webView stylePropertyAtSelectionStart:@"text-align"]);
        [webView alignJustified:nil];
        EXPECT_WK_STREQ("justify", [webView stylePropertyAtSelectionStart:@"text-align"]);
    };

    [webView evaluateJavaScript:@"document.body.dir = 'rtl'" completionHandler:nil];
    runTest();

    [webView evaluateJavaScript:@"document.body.dir = 'ltr'" completionHandler:nil];
    runTest();
}

TEST(WKWebViewEditActions, ToggleStrikeThrough)
{
    auto webView = webViewForEditActionTesting();
    [webView selectAll:nil];
    [webView toggleStrikeThrough:nil];
    EXPECT_WK_STREQ("line-through", [webView stylePropertyAtSelectionStart:@"-webkit-text-decorations-in-effect"]);
    EXPECT_WK_STREQ("line-through", [webView stylePropertyAtSelectionEnd:@"-webkit-text-decorations-in-effect"]);

    [webView toggleStrikeThrough:nil];
    EXPECT_WK_STREQ("none", [webView stylePropertyAtSelectionStart:@"-webkit-text-decorations-in-effect"]);
    EXPECT_WK_STREQ("none", [webView stylePropertyAtSelectionEnd:@"-webkit-text-decorations-in-effect"]);

    [webView collapseToEnd];
    [webView toggleStrikeThrough:nil];
    [[webView textInputContentView] insertText:@"Hello"];
    EXPECT_WK_STREQ("line-through", [webView stylePropertyAtSelectionStart:@"-webkit-text-decorations-in-effect"]);

    [webView toggleStrikeThrough:nil];
    [[webView textInputContentView] insertText:@"Hello"];
    EXPECT_WK_STREQ("none", [webView stylePropertyAtSelectionStart:@"-webkit-text-decorations-in-effect"]);
}

TEST(WKWebViewEditActions, ChangeFontSize)
{
    auto webView = webViewForEditActionTesting();
    [webView selectAll:nil];
    EXPECT_EQ(16, [[webView stylePropertyAtSelectionStart:@"font-size"] floatValue]);

    [webView increaseSize:nil];
    EXPECT_EQ(17, [[webView stylePropertyAtSelectionStart:@"font-size"] floatValue]);
    [webView increaseSize:nil];
    EXPECT_EQ(18, [[webView stylePropertyAtSelectionStart:@"font-size"] floatValue]);

    [webView decreaseSize:nil];
    EXPECT_EQ(17, [[webView stylePropertyAtSelectionStart:@"font-size"] floatValue]);
    [webView decreaseSize:nil];
    EXPECT_EQ(16, [[webView stylePropertyAtSelectionStart:@"font-size"] floatValue]);

    [webView setFontSize:20 sender:nil];
    EXPECT_EQ(20, [[webView stylePropertyAtSelectionStart:@"font-size"] floatValue]);
}

TEST(WKWebViewEditActions, SetTextColor)
{
    auto webView = webViewForEditActionTesting();
    [webView selectAll:nil];

    [webView setTextColor:[UIColor colorWithRed:1 green:0 blue:0 alpha:1] sender:nil];
    EXPECT_WK_STREQ("rgb(255, 0, 0)", [webView stylePropertyAtSelectionStart:@"color"]);
    EXPECT_TRUE([webView querySelectorExists:@"font"]);

    [webView setTextColor:[UIColor colorWithRed:0 green:1 blue:0 alpha:0.2] sender:nil];
    EXPECT_WK_STREQ("rgba(0, 255, 0, 0.2)", [webView stylePropertyAtSelectionStart:@"color"]);
    EXPECT_FALSE([webView querySelectorExists:@"font"]);
}

TEST(WKWebViewEditActions, SetFontFamily)
{
    auto webView = webViewForEditActionTesting();
    [webView selectAll:nil];

    UIFontDescriptor *fontDescriptor = [UIFontDescriptor fontDescriptorWithFontAttributes:@{ UIFontDescriptorFamilyAttribute: @"Helvetica" }];
    [webView setFont:[UIFont fontWithDescriptor:fontDescriptor size:24] sender:nil];
    EXPECT_WK_STREQ("Helvetica", [webView stylePropertyAtSelectionStart:@"font-family"]);
    EXPECT_WK_STREQ("24px", [webView stylePropertyAtSelectionStart:@"font-size"]);
    EXPECT_WK_STREQ("normal", [webView stylePropertyAtSelectionStart:@"font-weight"]);
    EXPECT_WK_STREQ("normal", [webView stylePropertyAtSelectionStart:@"font-style"]);

    [webView setFont:[UIFont fontWithName:@"TimesNewRomanPS-BoldMT" size:12] sender:nil];
    EXPECT_WK_STREQ("\"Times New Roman\"", [webView stylePropertyAtSelectionStart:@"font-family"]);
    EXPECT_WK_STREQ("12px", [webView stylePropertyAtSelectionStart:@"font-size"]);
    EXPECT_WK_STREQ("bold", [webView stylePropertyAtSelectionStart:@"font-weight"]);
    EXPECT_WK_STREQ("normal", [webView stylePropertyAtSelectionStart:@"font-style"]);

    fontDescriptor = [fontDescriptor fontDescriptorWithSymbolicTraits:UIFontDescriptorTraitItalic | UIFontDescriptorTraitBold];
    [webView setFont:[UIFont fontWithDescriptor:fontDescriptor size:20] sender:nil];
    EXPECT_WK_STREQ("Helvetica", [webView stylePropertyAtSelectionStart:@"font-family"]);
    EXPECT_WK_STREQ("20px", [webView stylePropertyAtSelectionStart:@"font-size"]);
    EXPECT_WK_STREQ("bold", [webView stylePropertyAtSelectionStart:@"font-weight"]);
    EXPECT_WK_STREQ("italic", [webView stylePropertyAtSelectionStart:@"font-style"]);
}

} // namespace TestWebKitAPI

#endif // PLATFORM(IOS_FAMILY) && WK_API_ENABLED
