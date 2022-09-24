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

#import "PlatformUtilities.h"
#import "TestWKWebView.h"
#import <WebKit/WKWebViewPrivate.h>
#import <WebKit/WKWebViewPrivateForTesting.h>

#if PLATFORM(IOS_FAMILY)
#import "UIKitSPI.h"
#import <UIKit/UIFontDescriptor.h>
#endif

@interface TestWKWebView (EditActionTesting)
- (BOOL)querySelectorExists:(NSString *)querySelector;
- (void)insertString:(NSString *)string;
- (void)setPosition:(NSString *)container offset:(NSUInteger)offset;
- (void)setBase:(NSString *)base baseOffset:(NSUInteger)baseOffset extent:(NSString *)extent extentOffset:(NSUInteger)extentOffset;
@end

@implementation TestWKWebView (EditActionTesting)

- (BOOL)querySelectorExists:(NSString *)querySelector
{
    return [[self objectByEvaluatingJavaScript:[NSString stringWithFormat:@"!!document.querySelector(`%@`)", querySelector]] boolValue];
}

- (void)insertString:(NSString *)string
{
#if PLATFORM(IOS_FAMILY)
    [[self textInputContentView] insertText:string];
#else
    [self insertText:string];
#endif
}

- (void)setPosition:(NSString *)container offset:(NSUInteger)offset
{
    [self evaluateJavaScript:[NSString stringWithFormat:@"getSelection().setPosition(%@, %tu)", container, offset] completionHandler:nil];
}

- (void)setBase:(NSString *)base baseOffset:(NSUInteger)baseOffset extent:(NSString *)extent extentOffset:(NSUInteger)extentOffset
{
    [self evaluateJavaScript:[NSString stringWithFormat:@"getSelection().setBaseAndExtent(%@, %tu, %@, %tu)", base, baseOffset, extent, extentOffset] completionHandler:nil];
}

@end

#if PLATFORM(IOS_FAMILY)

@interface WKWebView (InternalIOS)
- (void)_translate:(id)sender;
@end

#endif // PLATFORM(IOS_FAMILY)

namespace TestWebKitAPI {

static RetainPtr<TestWKWebView> webViewForEditActionTesting(NSString *markup)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 400, 400)]);
    [webView synchronouslyLoadHTMLString:markup];
    [webView _setEditable:YES];
    [webView becomeFirstResponder];
    [webView stringByEvaluatingJavaScript:@"getSelection().setPosition(document.body, 1)"];
    return webView;
}

static RetainPtr<TestWKWebView> webViewForEditActionTestingWithPageNamed(NSString *testPageName)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 400, 400)]);
    [webView synchronouslyLoadTestPageNamed:testPageName];
    [webView _setEditable:YES];
    [webView becomeFirstResponder];
    [webView stringByEvaluatingJavaScript:@"getSelection().setPosition(document.body, 1)"];
    return webView;
}

static RetainPtr<TestWKWebView> webViewForEditActionTesting()
{
    return webViewForEditActionTesting(@"<div>WebKit</div>");
}

TEST(WKWebViewEditActions, ModifyListLevel)
{
    auto webView = webViewForEditActionTesting(@"<ol><li>Foo</li><ol><li id='bar'>Bar</li></ol><ul><li id='baz'>Baz</li></ul></ol>");

    [webView setPosition:@"bar" offset:0];
    [webView _decreaseListLevel:nil];
    EXPECT_TRUE([webView querySelectorExists:@"ol > li#bar"]);
    EXPECT_TRUE([webView querySelectorExists:@"ol > ul > li#baz"]);

    [webView setPosition:@"baz" offset:0];
    [webView _decreaseListLevel:nil];
    EXPECT_TRUE([webView querySelectorExists:@"ol > li#bar"]);
    EXPECT_TRUE([webView querySelectorExists:@"ol > li#baz"]);

    [webView setBase:@"bar" baseOffset:0 extent:@"baz" extentOffset:1];
    [webView _increaseListLevel:nil];
    EXPECT_TRUE([webView querySelectorExists:@"ol > ol > li#bar"]);
    EXPECT_TRUE([webView querySelectorExists:@"ol > ol > li#baz"]);

    [webView _decreaseListLevel:nil];
    EXPECT_TRUE([webView querySelectorExists:@"ol > li#bar"]);
    EXPECT_TRUE([webView querySelectorExists:@"ol > li#baz"]);
}

TEST(WKWebViewEditActions, ChangeListType)
{
    auto webView = webViewForEditActionTestingWithPageNamed(@"editable-nested-lists");

    [webView setPosition:@"one" offset:1];
    [webView _changeListType:nil];
    EXPECT_TRUE([webView querySelectorExists:@"ol.list > li#one"]);
    EXPECT_TRUE([webView querySelectorExists:@"ol.list > li#four"]);

    [webView setBase:@"two" baseOffset:0 extent:@"two" extentOffset:1];
    [webView _changeListType:nil];
    EXPECT_TRUE([webView querySelectorExists:@"ul.list > li#two"]);
    EXPECT_TRUE([webView querySelectorExists:@"ul.list > li#three"]);
    EXPECT_WK_STREQ("rgb(255, 0, 0)", [webView stringByEvaluatingJavaScript:@"getComputedStyle(document.querySelector('#two')).color"]);
    EXPECT_WK_STREQ("rgb(255, 0, 0)", [webView stringByEvaluatingJavaScript:@"getComputedStyle(document.querySelector('#three')).color"]);
}

TEST(WKWebViewEditActions, NestedListInsertion)
{
    auto webView = webViewForEditActionTesting();

    [webView _insertNestedOrderedList:nil];
    EXPECT_TRUE([webView querySelectorExists:@"ol"]);
    EXPECT_TRUE([webView querySelectorExists:@"ol > li"]);

    [webView _insertNestedOrderedList:nil];
    EXPECT_TRUE([webView querySelectorExists:@"ol > ol"]);
    EXPECT_TRUE([webView querySelectorExists:@"ol > ol > li"]);

    [webView _insertNestedUnorderedList:nil];
    EXPECT_TRUE([webView querySelectorExists:@"ol > ol > ul"]);
    EXPECT_TRUE([webView querySelectorExists:@"ol > ol > ul > li"]);

    [webView _insertNestedUnorderedList:nil];
    EXPECT_TRUE([webView querySelectorExists:@"ol > ol > ul > ul"]);
    EXPECT_TRUE([webView querySelectorExists:@"ol > ol > ul > ul > li"]);
}

TEST(WKWebViewEditActions, ListInsertion)
{
    auto webView = webViewForEditActionTesting();

    [webView _insertOrderedList:nil];
    EXPECT_TRUE([webView querySelectorExists:@"ol"]);
    [webView _insertOrderedList:nil];
    EXPECT_FALSE([webView querySelectorExists:@"ol"]);

    [webView _insertUnorderedList:nil];
    EXPECT_TRUE([webView querySelectorExists:@"ul"]);
    [webView _insertUnorderedList:nil];
    EXPECT_FALSE([webView querySelectorExists:@"ul"]);
}

TEST(WKWebViewEditActions, ChangeIndentation)
{
    auto webView = webViewForEditActionTesting();

    [webView _indent:nil];
    EXPECT_TRUE([webView querySelectorExists:@"blockquote"]);
    [webView _indent:nil];
    EXPECT_TRUE([webView querySelectorExists:@"blockquote > blockquote"]);

    [webView _outdent:nil];
    EXPECT_TRUE([webView querySelectorExists:@"blockquote"]);
    [webView _outdent:nil];
    EXPECT_FALSE([webView querySelectorExists:@"blockquote"]);
}

TEST(WKWebViewEditActions, SetAlignment)
{
    auto webView = webViewForEditActionTesting();
    auto runTest = [webView] {
        [webView _alignCenter:nil];
        EXPECT_WK_STREQ("center", [webView stylePropertyAtSelectionStart:@"text-align"]);
        [webView _alignLeft:nil];
        EXPECT_WK_STREQ("left", [webView stylePropertyAtSelectionStart:@"text-align"]);
        [webView _alignRight:nil];
        EXPECT_WK_STREQ("right", [webView stylePropertyAtSelectionStart:@"text-align"]);
        [webView _alignJustified:nil];
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
    [webView _toggleStrikeThrough:nil];
    EXPECT_WK_STREQ("line-through", [webView stylePropertyAtSelectionStart:@"-webkit-text-decorations-in-effect"]);
    EXPECT_WK_STREQ("line-through", [webView stylePropertyAtSelectionEnd:@"-webkit-text-decorations-in-effect"]);

    [webView _toggleStrikeThrough:nil];
    EXPECT_WK_STREQ("none", [webView stylePropertyAtSelectionStart:@"-webkit-text-decorations-in-effect"]);
    EXPECT_WK_STREQ("none", [webView stylePropertyAtSelectionEnd:@"-webkit-text-decorations-in-effect"]);

    [webView collapseToEnd];
    [webView _toggleStrikeThrough:nil];
    [webView insertString:@"Hello"];
    EXPECT_WK_STREQ("line-through", [webView stylePropertyAtSelectionStart:@"-webkit-text-decorations-in-effect"]);

    [webView _toggleStrikeThrough:nil];
    [webView insertString:@"Hello"];
    EXPECT_WK_STREQ("none", [webView stylePropertyAtSelectionStart:@"-webkit-text-decorations-in-effect"]);
}

TEST(WKWebViewEditActions, PasteAsQuotation)
{
    auto webView = webViewForEditActionTesting();
    [webView selectAll:nil];
    [webView _executeEditCommand:@"cut" argument:nil completion:nil];
    [webView _pasteAsQuotation:nil];
    EXPECT_TRUE([webView querySelectorExists:@"blockquote"]);
}

TEST(WKWebViewEditActions, PasteAndMatchStyle)
{
    auto source = webViewForEditActionTesting();
    [source selectAll:nil];
    [source evaluateJavaScript:@"document.execCommand('bold'); document.execCommand('underline'); document.execCommand('italic')" completionHandler:nil];
    [source _synchronouslyExecuteEditCommand:@"Copy" argument:nil];

    auto destination = webViewForEditActionTesting(@"<div><br></div>");
    [destination _pasteAndMatchStyle:nil];
    [destination selectAll:nil];
    EXPECT_FALSE([destination stringByEvaluatingJavaScript:@"document.queryCommandState('bold')"].boolValue);
    EXPECT_FALSE([destination stringByEvaluatingJavaScript:@"document.queryCommandState('italic')"].boolValue);
    EXPECT_FALSE([destination stringByEvaluatingJavaScript:@"document.queryCommandState('underline')"].boolValue);
    EXPECT_WK_STREQ("WebKit", [destination stringByEvaluatingJavaScript:@"getSelection().toString()"]);
}

#if PLATFORM(IOS_FAMILY)

TEST(WKWebViewEditActions, ModifyBaseWritingDirection)
{
    auto webView = webViewForEditActionTesting(@"<meta charset='utf8'><p id='english'>Hello world</p><p id='hebrew'>מקור השם עברית</p>");

    [webView evaluateJavaScript:@"getSelection().setPosition(english)" completionHandler:nil];
    [webView makeTextWritingDirectionRightToLeft:nil];
    [webView waitForNextPresentationUpdate];
    EXPECT_WK_STREQ("rtl", [webView stringByEvaluatingJavaScript:@"getComputedStyle(english).direction"]);
    EXPECT_FALSE([webView canPerformAction:@selector(makeTextWritingDirectionRightToLeft:) withSender:nil]);
    EXPECT_TRUE([webView canPerformAction:@selector(makeTextWritingDirectionLeftToRight:) withSender:nil]);

    [webView evaluateJavaScript:@"getSelection().setPosition(hebrew)" completionHandler:nil];
    [webView makeTextWritingDirectionLeftToRight:nil];
    [webView waitForNextPresentationUpdate];
    EXPECT_WK_STREQ("ltr", [webView stringByEvaluatingJavaScript:@"getComputedStyle(hebrew).direction"]);
    EXPECT_FALSE([webView canPerformAction:@selector(makeTextWritingDirectionLeftToRight:) withSender:nil]);

    [webView evaluateJavaScript:@"getSelection().setPosition(english)" completionHandler:nil];
    [webView makeTextWritingDirectionNatural:nil];
    [webView waitForNextPresentationUpdate];
    EXPECT_WK_STREQ("ltr", [webView stringByEvaluatingJavaScript:@"getComputedStyle(english).direction"]);
    EXPECT_FALSE([webView canPerformAction:@selector(makeTextWritingDirectionLeftToRight:) withSender:nil]);
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

    [webView _setFontSize:20 sender:nil];
    EXPECT_EQ(20, [[webView stylePropertyAtSelectionStart:@"font-size"] floatValue]);
}

TEST(WKWebViewEditActions, SetTextColor)
{
    auto webView = webViewForEditActionTesting();
    [webView selectAll:nil];

    [webView _setTextColor:[UIColor colorWithRed:1 green:0 blue:0 alpha:1] sender:nil];
    EXPECT_WK_STREQ("rgb(255, 0, 0)", [webView stylePropertyAtSelectionStart:@"color"]);
    EXPECT_TRUE([webView querySelectorExists:@"font"]);

    [webView _setTextColor:[UIColor colorWithRed:0 green:1 blue:0 alpha:0.2] sender:nil];
    EXPECT_WK_STREQ("rgba(0, 255, 0, 0.2)", [webView stylePropertyAtSelectionStart:@"color"]);
    EXPECT_FALSE([webView querySelectorExists:@"font"]);
}

TEST(WKWebViewEditActions, SetFontFamily)
{
    auto webView = webViewForEditActionTesting();
    [webView selectAll:nil];

    UIFontDescriptor *fontDescriptor = [UIFontDescriptor fontDescriptorWithFontAttributes:@{ UIFontDescriptorFamilyAttribute: @"Helvetica" }];
    [webView _setFont:[UIFont fontWithDescriptor:fontDescriptor size:24] sender:nil];
    EXPECT_WK_STREQ("Helvetica", [webView stylePropertyAtSelectionStart:@"font-family"]);
    EXPECT_WK_STREQ("24px", [webView stylePropertyAtSelectionStart:@"font-size"]);
    EXPECT_WK_STREQ("400", [webView stylePropertyAtSelectionStart:@"font-weight"]);
    EXPECT_WK_STREQ("normal", [webView stylePropertyAtSelectionStart:@"font-style"]);

    [webView _setFont:[UIFont fontWithName:@"TimesNewRomanPS-BoldMT" size:12] sender:nil];
    EXPECT_WK_STREQ("\"Times New Roman\"", [webView stylePropertyAtSelectionStart:@"font-family"]);
    EXPECT_WK_STREQ("12px", [webView stylePropertyAtSelectionStart:@"font-size"]);
    EXPECT_WK_STREQ("700", [webView stylePropertyAtSelectionStart:@"font-weight"]);
    EXPECT_WK_STREQ("normal", [webView stylePropertyAtSelectionStart:@"font-style"]);

    fontDescriptor = [fontDescriptor fontDescriptorWithSymbolicTraits:UIFontDescriptorTraitItalic | UIFontDescriptorTraitBold];
    [webView _setFont:[UIFont fontWithDescriptor:fontDescriptor size:20] sender:nil];
    EXPECT_WK_STREQ("Helvetica", [webView stylePropertyAtSelectionStart:@"font-family"]);
    EXPECT_WK_STREQ("20px", [webView stylePropertyAtSelectionStart:@"font-size"]);
    EXPECT_WK_STREQ("700", [webView stylePropertyAtSelectionStart:@"font-weight"]);
    EXPECT_WK_STREQ("italic", [webView stylePropertyAtSelectionStart:@"font-style"]);
}

TEST(WebKit, CanInvokeTranslateWithTextSelection)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 568)]);
    [webView synchronouslyLoadTestPageNamed:@"simple"];
    EXPECT_FALSE([webView canPerformAction:@selector(_translate:) withSender:nil]);

    [webView selectAll:nil];
    [webView waitForNextPresentationUpdate];
    EXPECT_TRUE([webView canPerformAction:@selector(_translate:) withSender:nil]);

    [webView collapseToEnd];
    [webView waitForNextPresentationUpdate];
    EXPECT_FALSE([webView canPerformAction:@selector(_translate:) withSender:nil]);
}

#else

TEST(WKWebViewEditActions, ModifyTextWritingDirection)
{
    auto webView = webViewForEditActionTesting(@"<div id='text' style='direction: rtl; unicode-bidi: bidi-override;'>WebKit</div>");
    [webView selectAll:nil];
    [webView makeTextWritingDirectionNatural:nil];
    EXPECT_WK_STREQ("normal", [webView stringByEvaluatingJavaScript:@"getComputedStyle(text).unicodeBidi"]);
}

TEST(WKWebViewEditActions, CopyFontAtCaretSelection)
{
    auto webView = webViewForEditActionTesting(@"<p id='source' style='color: rgb(255, 0, 0);'>Source</p><p id='target'>Target</p>");
    [webView objectByEvaluatingJavaScript:@"getSelection().setPosition(source.childNodes[0], 3)"];

    auto validateAndPerformAction = [](TestWKWebView *webView, SEL action) {
        auto menu = adoptNS([NSMenu new]);
        auto item = adoptNS([NSMenuItem new]);
        [item setTarget:webView];
        [item setAction:action];
        [menu addItem:item.get()];
        [webView validateUserInterfaceItem:item.get()];
        [webView waitForNextPresentationUpdate];
        EXPECT_TRUE([item isEnabled]);

        [menu performActionForItemAtIndex:0];
        [webView waitForNextPresentationUpdate];
    };

    validateAndPerformAction(webView.get(), @selector(copyFont:));
    [webView objectByEvaluatingJavaScript:@"getSelection().selectAllChildren(target)"];
    validateAndPerformAction(webView.get(), @selector(pasteFont:));

    auto getComputedColor = @"let deepestChild = target;"
        "while (deepestChild.children[0])"
        "  deepestChild = deepestChild.children[0];"
        "getComputedStyle(deepestChild).color";
    EXPECT_WK_STREQ("rgb(255, 0, 0)", [webView stringByEvaluatingJavaScript:getComputedColor]);
}

#endif

} // namespace TestWebKitAPI
