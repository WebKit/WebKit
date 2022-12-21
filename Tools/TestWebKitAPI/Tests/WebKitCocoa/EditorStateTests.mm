/*
 * Copyright (C) 2017-2020 Apple Inc. All rights reserved.
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

#import "EditingTestHarness.h"
#import "PlatformUtilities.h"
#import "TestCocoa.h"
#import "TestInputDelegate.h"
#import "TestWKWebView.h"
#import "UserInterfaceSwizzler.h"
#import <WebKit/WKWebViewPrivate.h>
#import <wtf/Vector.h>

#if PLATFORM(IOS_FAMILY)
#import "UIKitSPI.h"
#endif

static void* const SelectionAttributesObservationContext = (void*)&SelectionAttributesObservationContext;

@interface SelectionChangeObserver : NSObject
- (instancetype)initWithWebView:(TestWKWebView *)webView;
@property (nonatomic, readonly) TestWKWebView *webView;
@property (nonatomic, readonly) _WKSelectionAttributes currentSelectionAttributes;
@end

@implementation SelectionChangeObserver {
    RetainPtr<TestWKWebView> _webView;
    Vector<_WKSelectionAttributes> _observedSelectionAttributes;
}

- (instancetype)initWithWebView:(TestWKWebView *)webView
{
    if (!(self = [super init]))
        return nil;

    _webView = webView;
    [_webView addObserver:self forKeyPath:@"_selectionAttributes" options:NSKeyValueObservingOptionOld | NSKeyValueObservingOptionNew context:SelectionAttributesObservationContext];
    return self;
}

- (TestWKWebView *)webView
{
    return _webView.get();
}

- (void)observeValueForKeyPath:(NSString *)keyPath ofObject:(id)object change:(NSDictionary<NSKeyValueChangeKey, id> *)change context:(void *)context
{
    if (context == SelectionAttributesObservationContext) {
        if (!_observedSelectionAttributes.isEmpty())
            EXPECT_EQ(_observedSelectionAttributes.last(), [change[NSKeyValueChangeOldKey] unsignedIntValue]);
        _observedSelectionAttributes.append([change[NSKeyValueChangeNewKey] unsignedIntValue]);
        return;
    }
    [super observeValueForKeyPath:keyPath ofObject:object change:change context:context];
}

- (_WKSelectionAttributes)currentSelectionAttributes
{
    return _observedSelectionAttributes.isEmpty() ? _WKSelectionAttributeNoSelection : _observedSelectionAttributes.last();
}

@end

namespace TestWebKitAPI {

static RetainPtr<EditingTestHarness> setUpEditorStateTestHarness()
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 400, 400)]);
    auto testHarness = adoptNS([[EditingTestHarness alloc] initWithWebView:webView.get()]);
    [webView synchronouslyLoadTestPageNamed:@"editor-state-test-harness"];
    return testHarness;
}

TEST(EditorStateTests, TypingAttributesBold)
{
    auto testHarness = setUpEditorStateTestHarness();

    [testHarness insertHTML:@"<b>first</b>" andExpectEditorStateWith:@{ @"bold": @YES }];
    [testHarness toggleBold];
    [testHarness insertText:@" second" andExpectEditorStateWith:@{ @"bold": @NO }];
    [testHarness insertHTML:@"<span style='font-weight: 700'> third</span>" andExpectEditorStateWith:@{ @"bold": @YES }];
    [testHarness insertHTML:@"<span style='font-weight: 300'> fourth</span>" andExpectEditorStateWith:@{ @"bold": @NO }];
    [testHarness insertHTML:@"<span style='font-weight: 800'> fifth</span>" andExpectEditorStateWith:@{ @"bold": @YES }];
    [testHarness insertHTML:@"<span style='font-weight: 400'> sixth</span>" andExpectEditorStateWith:@{ @"bold": @NO }];
    [testHarness insertHTML:@"<span style='font-weight: 900'> seventh</span>" andExpectEditorStateWith:@{ @"bold": @YES }];
    [testHarness toggleBold];
    [testHarness insertText:@" eighth" andExpectEditorStateWith:@{ @"bold": @NO }];
    [testHarness insertHTML:@"<strong> ninth</strong>" andExpectEditorStateWith:@{ @"bold": @YES }];
    [testHarness insertParagraphAndExpectEditorStateWith:@{ @"bold": @YES }];
    [testHarness deleteBackwardAndExpectEditorStateWith:@{ @"bold": @YES }];
    [testHarness moveWordBackwardAndExpectEditorStateWith:@{ @"bold": @YES }];
    [testHarness moveWordBackwardAndExpectEditorStateWith:@{ @"bold": @NO }];
    [testHarness moveWordBackwardAndExpectEditorStateWith:@{ @"bold": @YES }];
    [testHarness moveWordBackwardAndExpectEditorStateWith:@{ @"bold": @NO }];
    [testHarness selectAllAndExpectEditorStateWith:@{ @"bold": @YES }];
    EXPECT_WK_STREQ("first second third fourth fifth sixth seventh eighth ninth", [[testHarness webView] stringByEvaluatingJavaScript:@"getSelection().toString()"]);
}

TEST(EditorStateTests, TypingAttributesItalic)
{
    auto testHarness = setUpEditorStateTestHarness();

    [testHarness insertHTML:@"<i>first</i>" andExpectEditorStateWith:@{ @"italic": @YES }];
    [testHarness toggleItalic];
    [testHarness insertText:@" second" andExpectEditorStateWith:@{ @"italic": @NO }];
    [testHarness insertHTML:@"<span style='font-style: italic'> third</span>" andExpectEditorStateWith:@{ @"italic": @YES }];
    [testHarness toggleItalic];
    [testHarness insertText:@" fourth" andExpectEditorStateWith:@{ @"italic": @NO }];
    [testHarness toggleItalic];
    [testHarness insertText:@" fifth" andExpectEditorStateWith:@{ @"italic": @YES }];
    [testHarness insertHTML:@"<span style='font-style: normal'> sixth</span>" andExpectEditorStateWith:@{ @"italic": @NO }];
    [testHarness insertHTML:@"<span style='font-style: oblique'> seventh</span>" andExpectEditorStateWith:@{ @"italic": @YES }];
    [testHarness insertParagraphAndExpectEditorStateWith:@{ @"italic": @YES }];
    [testHarness deleteBackwardAndExpectEditorStateWith:@{ @"italic": @YES }];
    [testHarness moveWordBackwardAndExpectEditorStateWith:@{ @"italic": @YES }];
    [testHarness moveWordBackwardAndExpectEditorStateWith:@{ @"italic": @NO }];
    [testHarness moveWordBackwardAndExpectEditorStateWith:@{ @"italic": @YES }];
    [testHarness moveWordBackwardAndExpectEditorStateWith:@{ @"italic": @NO }];

    [testHarness selectAllAndExpectEditorStateWith:@{ @"italic": @YES }];
    EXPECT_WK_STREQ("first second third fourth fifth sixth seventh", [[testHarness webView] stringByEvaluatingJavaScript:@"getSelection().toString()"]);
}

TEST(EditorStateTests, TypingAttributesUnderline)
{
    auto testHarness = setUpEditorStateTestHarness();

    [testHarness insertHTML:@"<u>first</u>" andExpectEditorStateWith:@{ @"underline": @YES }];
    [testHarness toggleUnderline];
    [testHarness insertText:@" second" andExpectEditorStateWith:@{ @"underline": @NO }];
    [testHarness insertHTML:@"<span style='text-decoration: underline'> third</span>" andExpectEditorStateWith:@{ @"underline": @YES }];
    [testHarness insertHTML:@"<span style='text-decoration: line-through'> fourth</span>" andExpectEditorStateWith:@{ @"underline": @NO }];
    [testHarness insertHTML:@"<span style='text-decoration: underline overline line-through'> fifth</span>" andExpectEditorStateWith:@{ @"underline": @YES }];
    [testHarness insertHTML:@"<span style='text-decoration: none'> sixth</span>" andExpectEditorStateWith:@{ @"underline": @NO }];
    [testHarness toggleUnderline];
    [testHarness insertText:@" seventh" andExpectEditorStateWith:@{ @"underline": @YES }];
    [testHarness insertParagraphAndExpectEditorStateWith:@{ @"underline": @YES }];
    [testHarness deleteBackwardAndExpectEditorStateWith:@{ @"underline": @YES }];
    [testHarness moveWordBackwardAndExpectEditorStateWith:@{ @"underline": @YES }];
    [testHarness moveWordBackwardAndExpectEditorStateWith:@{ @"underline": @NO }];
    [testHarness moveWordBackwardAndExpectEditorStateWith:@{ @"underline": @YES }];
    [testHarness moveWordBackwardAndExpectEditorStateWith:@{ @"underline": @NO }];

    [testHarness selectAllAndExpectEditorStateWith:@{ @"underline": @YES }];
    EXPECT_WK_STREQ("first second third fourth fifth sixth seventh", [[testHarness webView] stringByEvaluatingJavaScript:@"getSelection().toString()"]);
}

TEST(EditorStateTests, TypingAttributesTextAlignmentAbsoluteAlignmentOptions)
{
    auto testHarness = setUpEditorStateTestHarness();
    TestWKWebView *webView = [testHarness webView];

    [webView stringByEvaluatingJavaScript:@"document.body.style.direction = 'ltr'"];

    [testHarness insertHTML:@"<div style='text-align: right;'>right</div>" andExpectEditorStateWith:@{ @"text-alignment": @(NSTextAlignmentRight) }];
    [testHarness insertParagraphAndExpectEditorStateWith:@{ @"text-alignment": @(NSTextAlignmentRight) }];

    [testHarness insertText:@"justified" andExpectEditorStateWith:@{ @"text-alignment": @(NSTextAlignmentRight) }];
    [testHarness alignJustifiedAndExpectEditorStateWith:@{ @"text-alignment": @(NSTextAlignmentJustified) }];
    [testHarness insertParagraphAndExpectEditorStateWith:@{ @"text-alignment": @(NSTextAlignmentJustified) }];

    [testHarness alignCenterAndExpectEditorStateWith:@{ @"text-alignment": @(NSTextAlignmentCenter) }];
    [testHarness insertText:@"center" andExpectEditorStateWith:@{ @"text-alignment": @(NSTextAlignmentCenter) }];
    [testHarness insertParagraphAndExpectEditorStateWith:@{ @"text-alignment": @(NSTextAlignmentCenter) }];

    [testHarness insertHTML:@"<span id='left'>left</span>" andExpectEditorStateWith:@{ @"text-alignment": @(NSTextAlignmentCenter) }];
    [webView stringByEvaluatingJavaScript:@"getSelection().setBaseAndExtent(left.childNodes[0], 0, left.childNodes[0], 4)"];
    [testHarness alignLeftAndExpectEditorStateWith:@{ @"text-alignment": @(NSTextAlignmentLeft) }];

    [testHarness selectAllAndExpectEditorStateWith:@{ @"text-alignment": @(NSTextAlignmentRight) }];
    EXPECT_WK_STREQ("right\njustified\ncenter\nleft", [webView stringByEvaluatingJavaScript:@"getSelection().toString()"]);
}

TEST(EditorStateTests, TypingAttributesTextAlignmentStartEnd)
{
    auto testHarness = setUpEditorStateTestHarness();
    TestWKWebView *webView = [testHarness webView];

    [webView stringByEvaluatingJavaScript:@"document.styleSheets[0].insertRule('.start { text-align: start; }')"];
    [webView stringByEvaluatingJavaScript:@"document.styleSheets[0].insertRule('.end { text-align: end; }')"];
    [webView stringByEvaluatingJavaScript:@"document.body.style.direction = 'rtl'"];

    [testHarness insertHTML:@"<div class='start'>rtl start</div>" andExpectEditorStateWith:@{ @"text-alignment": @(NSTextAlignmentRight) }];
    [testHarness insertParagraphAndExpectEditorStateWith:@{ @"text-alignment": @(NSTextAlignmentRight) }];

    [testHarness insertHTML:@"<div class='end'>rtl end</div>" andExpectEditorStateWith:@{ @"text-alignment": @(NSTextAlignmentLeft) }];
    [testHarness insertParagraphAndExpectEditorStateWith:@{ @"text-alignment": @(NSTextAlignmentLeft) }];

    [[testHarness webView] stringByEvaluatingJavaScript:@"document.body.style.direction = 'ltr'"];
    [testHarness insertHTML:@"<div class='start'>ltr start</div>" andExpectEditorStateWith:@{ @"text-alignment": @(NSTextAlignmentLeft) }];
    [testHarness insertParagraphAndExpectEditorStateWith:@{ @"text-alignment": @(NSTextAlignmentLeft) }];

    [testHarness insertHTML:@"<div class='end'>ltr end</div>" andExpectEditorStateWith:@{ @"text-alignment": @(NSTextAlignmentRight) }];
    [testHarness insertParagraphAndExpectEditorStateWith:@{ @"text-alignment": @(NSTextAlignmentRight) }];
}

TEST(EditorStateTests, TypingAttributesTextAlignmentDirectionalText)
{
    auto testHarness = setUpEditorStateTestHarness();
    [[testHarness webView] stringByEvaluatingJavaScript:@"document.body.setAttribute('dir', 'auto')"];

    [testHarness insertHTML:@"<div>מקור השם עברית</div>" andExpectEditorStateWith:@{ @"text-alignment": @(NSTextAlignmentRight) }];
    [testHarness insertParagraphAndExpectEditorStateWith:@{ @"text-alignment": @(NSTextAlignmentRight) }];
    [testHarness insertHTML:@"<div dir='ltr'>מקור השם עברית</div>" andExpectEditorStateWith:@{ @"text-alignment": @(NSTextAlignmentLeft) }];
    [testHarness insertParagraphAndExpectEditorStateWith:@{ @"text-alignment": @(NSTextAlignmentLeft) }];
    [testHarness insertHTML:@"<div dir='rtl'>מקור השם עברית</div>" andExpectEditorStateWith:@{ @"text-alignment": @(NSTextAlignmentRight) }];
    [testHarness insertParagraphAndExpectEditorStateWith:@{ @"text-alignment": @(NSTextAlignmentRight) }];

    [testHarness insertHTML:@"<div dir='auto'>This is English text</div>" andExpectEditorStateWith:@{ @"text-alignment": @(NSTextAlignmentLeft) }];
    [testHarness insertParagraphAndExpectEditorStateWith:@{ @"text-alignment": @(NSTextAlignmentLeft) }];
    [testHarness insertHTML:@"<div dir='rtl'>This is English text</div>" andExpectEditorStateWith:@{ @"text-alignment": @(NSTextAlignmentRight) }];
    [testHarness insertParagraphAndExpectEditorStateWith:@{ @"text-alignment": @(NSTextAlignmentRight) }];
    [testHarness insertHTML:@"<div dir='ltr'>This is English text</div>" andExpectEditorStateWith:@{ @"text-alignment": @(NSTextAlignmentLeft) }];
    [testHarness insertParagraphAndExpectEditorStateWith:@{ @"text-alignment": @(NSTextAlignmentLeft) }];
}

TEST(EditorStateTests, TypingAttributesTextColor)
{
    auto testHarness = setUpEditorStateTestHarness();

    [testHarness setForegroundColor:@"rgb(255, 0, 0)"];
    [testHarness insertText:@"red" andExpectEditorStateWith:@{ @"text-color": @"rgb(255, 0, 0)" }];

    [testHarness insertHTML:@"<span style='color: rgb(0, 255, 0)'>green</span>" andExpectEditorStateWith:@{ @"text-color": @"rgb(0, 255, 0)" }];
    [testHarness insertParagraphAndExpectEditorStateWith:@{ @"text-color": @"rgb(0, 255, 0)" }];

    [testHarness setForegroundColor:@"rgb(0, 0, 255)"];
    [testHarness insertText:@"blue" andExpectEditorStateWith:@{ @"text-color": @"rgb(0, 0, 255)" }];
}

TEST(EditorStateTests, TypingAttributesMixedStyles)
{
    auto testHarness = setUpEditorStateTestHarness();

    [testHarness alignCenterAndExpectEditorStateWith:@{ @"text-alignment": @(NSTextAlignmentCenter) }];
    [testHarness setForegroundColor:@"rgb(128, 128, 128)"];
    [testHarness toggleBold];
    [testHarness toggleItalic];
    [testHarness toggleUnderline];
    NSDictionary *expectedAttributes = @{
        @"bold": @YES,
        @"italic": @YES,
        @"underline": @YES,
        @"text-color": @"rgb(128, 128, 128)",
        @"text-alignment": @(NSTextAlignmentCenter)
    };
    BOOL containsProperties = [testHarness latestEditorStateContains:expectedAttributes];
    EXPECT_TRUE(containsProperties);
    if (!containsProperties)
        NSLog(@"Expected %@ to contain %@", [testHarness latestEditorState], expectedAttributes);
}

TEST(EditorStateTests, TypingAttributeLinkColor)
{
    auto testHarness = setUpEditorStateTestHarness();
    [testHarness insertHTML:@"<a href='https://www.apple.com/'>This is a link</a>" andExpectEditorStateWith:@{ @"text-color": @"rgb(0, 0, 238)" }];
    [testHarness selectAllAndExpectEditorStateWith:@{ @"text-color": @"rgb(0, 0, 238)" }];
    EXPECT_WK_STREQ("https://www.apple.com/", [[testHarness webView] stringByEvaluatingJavaScript:@"document.querySelector('a').href"]);
}

#if PLATFORM(IOS_FAMILY)

static void checkContentViewHasTextWithFailureDescription(TestWKWebView *webView, BOOL expectedToHaveText, NSString *description)
{
    BOOL hasText = webView.textInputContentView.hasText;
    if (expectedToHaveText)
        EXPECT_TRUE(hasText);
    else
        EXPECT_FALSE(hasText);

    if (expectedToHaveText != hasText)
        NSLog(@"Expected -[%@ hasText] to be %@, but observed: %@ (%@)", [webView.textInputContentView class], expectedToHaveText ? @"YES" : @"NO", hasText ? @"YES" : @"NO", description);
}

TEST(EditorStateTests, ContentViewHasTextInContentEditableElement)
{
    auto testHarness = setUpEditorStateTestHarness();
    TestWKWebView *webView = [testHarness webView];

    checkContentViewHasTextWithFailureDescription(webView, NO, @"before inserting any content");
    [testHarness insertHTML:@"<img src='icon.png'></img>"];
    checkContentViewHasTextWithFailureDescription(webView, NO, @"after inserting an image element");
    [testHarness insertText:@"A"];
    checkContentViewHasTextWithFailureDescription(webView, YES, @"after inserting text");
    [testHarness selectAll];
    checkContentViewHasTextWithFailureDescription(webView, YES, @"after selecting everything");
    [testHarness deleteBackwards];
    checkContentViewHasTextWithFailureDescription(webView, NO, @"after deleting everything");
    [testHarness insertParagraph];
    checkContentViewHasTextWithFailureDescription(webView, YES, @"after inserting a newline");
    [testHarness deleteBackwards];
    checkContentViewHasTextWithFailureDescription(webView, NO, @"after deleting the newline");
    [testHarness insertText:@"B"];
    checkContentViewHasTextWithFailureDescription(webView, YES, @"after inserting text again");
    [webView stringByEvaluatingJavaScript:@"document.body.blur()"];
    [webView waitForNextPresentationUpdate];
    checkContentViewHasTextWithFailureDescription(webView, NO, @"after losing focus");
}

TEST(EditorStateTests, ContentViewHasTextInTextarea)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 400, 400)]);
    auto testHarness = adoptNS([[EditingTestHarness alloc] initWithWebView:webView.get()]);
    [webView synchronouslyLoadHTMLString:@"<textarea id='textarea'></textarea>"];
    [webView stringByEvaluatingJavaScript:@"textarea.focus()"];
    [webView waitForNextPresentationUpdate];

    checkContentViewHasTextWithFailureDescription(webView.get(), NO, @"before inserting any content");
    [testHarness insertText:@"A"];
    checkContentViewHasTextWithFailureDescription(webView.get(), YES, @"after inserting text");
    [testHarness selectAll];
    checkContentViewHasTextWithFailureDescription(webView.get(), YES, @"after selecting everything");
    [testHarness deleteBackwards];
    checkContentViewHasTextWithFailureDescription(webView.get(), NO, @"after deleting everything");
    [testHarness insertParagraph];
    checkContentViewHasTextWithFailureDescription(webView.get(), YES, @"after inserting a newline");
    [testHarness deleteBackwards];
    checkContentViewHasTextWithFailureDescription(webView.get(), NO, @"after deleting the newline");
    [testHarness insertText:@"B"];
    checkContentViewHasTextWithFailureDescription(webView.get(), YES, @"after inserting text again");
    [webView stringByEvaluatingJavaScript:@"textarea.blur()"];
    [webView waitForNextPresentationUpdate];
    checkContentViewHasTextWithFailureDescription(webView.get(), NO, @"after losing focus");
}

TEST(EditorStateTests, CaretColorInContentEditable)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    [webView synchronouslyLoadHTMLString:@"<body style=\"caret-color: red;\" contenteditable=\"true\"></body>"];
    [webView stringByEvaluatingJavaScript:@"document.body.focus()"];
    [webView waitForNextPresentationUpdate];
    UIView<UITextInputTraits_Private> *textInput = (UIView<UITextInputTraits_Private> *) [webView textInputContentView];
    UIColor *insertionPointColor = textInput.insertionPointColor;
    UIColor *redColor = [UIColor redColor];
    auto colorSpace = adoptCF(CGColorSpaceCreateWithName(kCGColorSpaceGenericRGB));
    auto cgInsertionPointColor = adoptCF(CGColorCreateCopyByMatchingToColorSpace(colorSpace.get(), kCGRenderingIntentDefault, insertionPointColor.CGColor, NULL));
    auto cgRedColor = adoptCF(CGColorCreateCopyByMatchingToColorSpace(colorSpace.get(), kCGRenderingIntentDefault, redColor.CGColor, NULL));
    EXPECT_TRUE(CGColorEqualToColor(cgInsertionPointColor.get(), cgRedColor.get()));
}

TEST(EditorStateTests, ObserveSelectionAttributeChanges)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    auto editor = adoptNS([[EditingTestHarness alloc] initWithWebView:webView.get()]);
    [webView _setEditable:YES];
    [webView synchronouslyLoadHTMLString:@"<body></body>"];

    auto observer = adoptNS([[SelectionChangeObserver alloc] initWithWebView:webView.get()]);

    [webView evaluateJavaScript:@"document.body.focus()" completionHandler:nil];
    [webView waitForNextPresentationUpdate];
    EXPECT_EQ(_WKSelectionAttributeIsCaret, [observer currentSelectionAttributes]);

    [editor insertText:@"Hello"];
    EXPECT_EQ(_WKSelectionAttributeIsCaret, [observer currentSelectionAttributes]);

    [editor insertText:@"."];
    EXPECT_EQ(_WKSelectionAttributeIsCaret, [observer currentSelectionAttributes]);

    [editor moveBackward];
    EXPECT_EQ(_WKSelectionAttributeIsCaret, [observer currentSelectionAttributes]);

    [editor moveForward];
    EXPECT_EQ(_WKSelectionAttributeIsCaret, [observer currentSelectionAttributes]);

    [editor deleteBackwards];
    EXPECT_EQ(_WKSelectionAttributeIsCaret, [observer currentSelectionAttributes]);

    [editor insertParagraph];
    EXPECT_EQ(_WKSelectionAttributeIsCaret, [observer currentSelectionAttributes]);

    [editor selectAll];
    EXPECT_EQ(_WKSelectionAttributeIsRange, [observer currentSelectionAttributes]);

    [webView evaluateJavaScript:@"getSelection().removeAllRanges()" completionHandler:nil];
    [webView waitForNextPresentationUpdate];
    EXPECT_EQ(_WKSelectionAttributeNoSelection, [observer currentSelectionAttributes]);
}

TEST(EditorStateTests, ParagraphBoundary)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    [webView synchronouslyLoadHTMLString:@"<body contenteditable><p>Hello world.</p></body>"];
    [webView stringByEvaluatingJavaScript:@"document.body.focus()"];
    [webView waitForNextPresentationUpdate];

    auto textInput = [webView textInputContentView];
    auto editor = adoptNS([[EditingTestHarness alloc] initWithWebView:webView.get()]);
    [editor selectAll];

    EXPECT_TRUE([textInput isPosition:textInput.selectedTextRange.start atBoundary:UITextGranularityParagraph inDirection:UITextStorageDirectionBackward]);
    EXPECT_TRUE([textInput isPosition:textInput.selectedTextRange.end atBoundary:UITextGranularityParagraph inDirection:UITextStorageDirectionForward]);

    [editor moveForward];
    [editor moveBackward];
    [editor moveBackward];

    EXPECT_FALSE([textInput isPosition:textInput.selectedTextRange.start atBoundary:UITextGranularityParagraph inDirection:UITextStorageDirectionBackward]);
    EXPECT_FALSE([textInput isPosition:textInput.selectedTextRange.end atBoundary:UITextGranularityParagraph inDirection:UITextStorageDirectionForward]);
}

TEST(EditorStateTests, SelectedText)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    [webView synchronouslyLoadTestPageNamed:@"lots-of-text"];
    [webView _synchronouslyExecuteEditCommand:@"SelectAll" argument:nil];
    [webView waitForNextPresentationUpdate];
    EXPECT_GT([[webView textInputContentView] selectedText].length, 0U);
}

constexpr unsigned glyphWidth { 25 }; // pixels

static NSString *applyAhemStyle(NSString *htmlString)
{
    return [NSString stringWithFormat:@"<style>@font-face { font-family: Ahem; src: url(Ahem.ttf); } body { margin: 0; } * { font: %upx/1 Ahem; -webkit-text-size-adjust: none; }</style><meta name='viewport' content='width=980, initial-scale=1.0'>%@", glyphWidth, htmlString];
}

TEST(EditorStateTests, MarkedTextRange_HorizontalCaretSelection)
{
    IPhoneUserInterfaceSwizzler userInterfaceSwizzler;

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600)]);
    [webView synchronouslyLoadHTMLStringAndWaitUntilAllImmediateChildFramesPaint:applyAhemStyle(@"<body contenteditable='true'>.</body>")]; // . is dummy to force Ahem to load
    [webView stringByEvaluatingJavaScript:@"document.body.focus()"];

    auto *contentView = [webView textInputContentView];
    [contentView setMarkedText:@"hello" selectedRange:NSMakeRange(0, 0)];
    [webView waitForNextPresentationUpdate];

    UITextRange *markedTextRange = [contentView markedTextRange];
    NSArray<UITextSelectionRect *> *rects = [contentView selectionRectsForRange:markedTextRange];
    EXPECT_EQ(1U, rects.count);
    EXPECT_EQ(CGRectMake(0, 0, 5 * glyphWidth, glyphWidth), rects[0].rect);
    EXPECT_EQ(CGRectMake(0, 0, 2, glyphWidth), [contentView caretRectForPosition:markedTextRange.start]);
    EXPECT_EQ(CGRectMake(124, 0, 2, glyphWidth), [contentView caretRectForPosition:markedTextRange.end]);
    EXPECT_FALSE(rects[0].isVertical);
}

TEST(EditorStateTests, MarkedTextRange_HorizontalRangeSelection)
{
    IPhoneUserInterfaceSwizzler userInterfaceSwizzler;

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600)]);
    [webView synchronouslyLoadHTMLStringAndWaitUntilAllImmediateChildFramesPaint:applyAhemStyle(@"<body contenteditable='true'>.</body>")]; // . is dummy to force Ahem to load
    [webView stringByEvaluatingJavaScript:@"document.body.focus()"];
    [webView _synchronouslyExecuteEditCommand:@"InsertText" argument:@"Hello world"];

    auto *contentView = [webView textInputContentView];
    [contentView selectWordBackward];
    [contentView setMarkedText:@"world" selectedRange:NSMakeRange(0, 5)];
    [webView waitForNextPresentationUpdate];

    UITextRange *markedTextRange = [contentView markedTextRange];
    NSArray<UITextSelectionRect *> *rects = [contentView selectionRectsForRange:markedTextRange];
    EXPECT_EQ(1U, rects.count);
    EXPECT_EQ(CGRectMake(150, 0, 5 * glyphWidth, glyphWidth), rects[0].rect);
    EXPECT_EQ(CGRectMake(149, 0, 2, glyphWidth), [contentView caretRectForPosition:markedTextRange.start]);
    EXPECT_EQ(CGRectMake(274, 0, 2, glyphWidth), [contentView caretRectForPosition:markedTextRange.end]);
    EXPECT_FALSE(rects[0].isVertical);
}

TEST(EditorStateTests, MarkedTextRange_VerticalCaretSelection)
{
    IPhoneUserInterfaceSwizzler userInterfaceSwizzler;

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600)]);
    [webView synchronouslyLoadHTMLStringAndWaitUntilAllImmediateChildFramesPaint:applyAhemStyle(@"<body style='writing-mode: vertical-lr' contenteditable='true'>.</body>")]; // . is dummy to force Ahem to load
    [webView stringByEvaluatingJavaScript:@"document.body.focus()"];

    auto *contentView = [webView textInputContentView];
    [contentView setMarkedText:@"hello" selectedRange:NSMakeRange(0, 0)];
    [webView waitForNextPresentationUpdate];

    UITextRange *markedTextRange = [contentView markedTextRange];
    NSArray<UITextSelectionRect *> *rects = [contentView selectionRectsForRange:markedTextRange];
    EXPECT_EQ(1U, rects.count);
    EXPECT_EQ(CGRectMake(0, 0, glyphWidth, 5 * glyphWidth), rects[0].rect);
    EXPECT_EQ(CGRectMake(0, 0, glyphWidth, 2), [contentView caretRectForPosition:markedTextRange.start]);
    EXPECT_EQ(CGRectMake(0, 124, glyphWidth, 2), [contentView caretRectForPosition:markedTextRange.end]);
    EXPECT_TRUE(rects[0].isVertical);
}

TEST(EditorStateTests, MarkedTextRange_VerticalRangeSelection)
{
    IPhoneUserInterfaceSwizzler userInterfaceSwizzler;

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600)]);
    [webView synchronouslyLoadHTMLString:applyAhemStyle(@"<body style='writing-mode: vertical-lr' contenteditable='true'>.</body>")]; // . is dummy to force Ahem to load
    [webView stringByEvaluatingJavaScript:@"document.body.focus()"];
    [webView _synchronouslyExecuteEditCommand:@"InsertText" argument:@"Hello world"];

    auto *contentView = [webView textInputContentView];
    [contentView selectWordBackward];
    [contentView setMarkedText:@"world" selectedRange:NSMakeRange(0, 5)];
    [webView waitForNextPresentationUpdate];

    UITextRange *markedTextRange = [contentView markedTextRange];
    NSArray<UITextSelectionRect *> *rects = [contentView selectionRectsForRange:markedTextRange];
    EXPECT_EQ(1U, rects.count);
    EXPECT_EQ(CGRectMake(0, 150, glyphWidth, 5 * glyphWidth), rects[0].rect);
    EXPECT_EQ(CGRectMake(0, 149, glyphWidth, 2), [contentView caretRectForPosition:markedTextRange.start]);
    EXPECT_EQ(CGRectMake(0, 274, glyphWidth, 2), [contentView caretRectForPosition:markedTextRange.end]);
    EXPECT_TRUE(rects[0].isVertical);
}

#endif // PLATFORM(IOS_FAMILY)

} // namespace TestWebKitAPI
