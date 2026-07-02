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

#if PLATFORM(IOS_FAMILY)

#import "Helpers/PlatformUtilities.h"
#import "TestInputDelegate.h"
#import "Helpers/cocoa/TestWKWebView.h"
#import "UIKitSPIForTesting.h"
#import "Helpers/ios/UIKitTestingHelpers.h"
#import "Helpers/ios/UserInterfaceSwizzler.h"
#import <WebKit/WKUIDelegatePrivate.h>
#import <WebKit/WKWebViewPrivate.h>
#import <WebKit/WKWebViewPrivateForTesting.h>
#import <wtf/RetainPtr.h>

@interface TestWKWebView (UIWKInteractionViewTesting)
- (void)selectPositionAtPoint:(CGPoint)point;
- (void)selectTextWithGranularity:(UITextGranularity)granularity atPoint:(CGPoint)point;
- (void)updateSelectionWithExtentPoint:(CGPoint)point;
- (void)updateSelectionWithExtentPoint:(CGPoint)point withBoundary:(UITextGranularity)granularity;
@end

@implementation TestWKWebView (UIWKInteractionViewTesting)

- (void)selectTextWithGranularity:(UITextGranularity)granularity atPoint:(CGPoint)point
{
    __block bool done = false;
    [self.textInputContentView selectTextWithGranularity:granularity atPoint:point completionHandler:^{
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
}

- (void)updateSelectionWithExtentPoint:(CGPoint)point
{
    __block bool done = false;
    [self.textInputContentView updateSelectionWithExtentPoint:point completionHandler:^(BOOL) {
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
}

- (void)updateSelectionWithExtentPoint:(CGPoint)point withBoundary:(UITextGranularity)granularity
{
    __block bool done = false;
    [self.textInputContentView updateSelectionWithExtentPoint:point withBoundary:granularity completionHandler:^(BOOL) {
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
}

- (void)selectPositionAtPoint:(CGPoint)point
{
    __block bool done = false;
    [self.textInputContentView selectPositionAtPoint:point completionHandler:^{
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
}

@end

@interface EditorStateObserver : NSObject <WKUIDelegatePrivate>
- (instancetype)initWithWebView:(WKWebView *)webView;
@property (nonatomic, readonly) NSUInteger changeCount;
@end

@implementation EditorStateObserver {
    __weak WKWebView *_webView;
}

- (instancetype)initWithWebView:(WKWebView *)webView
{
    if (!(self = [super init]))
        return nil;

    webView.UIDelegate = self;
    _changeCount = 0;
    return self;
}

- (void)_webView:(WKWebView *)webView editorStateDidChange:(NSDictionary *)editorState
{
    _changeCount++;
}

@end

namespace TestWebKitAPI {

TEST(UIWKInteractionViewProtocol, SelectTextWithCharacterGranularity)
{
    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 400, 400)]);
    [webView synchronouslyLoadHTMLString:@"<body style='font-size: 20px;'>Hello world</body>"];
    [webView selectTextWithGranularity:UITextGranularityCharacter atPoint:CGPointMake(10, 20)];
    [webView updateSelectionWithExtentPoint:CGPointMake(300, 20) withBoundary:UITextGranularityCharacter];
    EXPECT_WK_STREQ("Hello world", [webView stringByEvaluatingJavaScript:@"getSelection().toString()"]);
}

TEST(UIWKInteractionViewProtocol, UpdateSelectionWithExtentPoint)
{
    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 400, 400)]);
    [webView synchronouslyLoadHTMLString:@"<body contenteditable style='font-size: 20px;'>Hello world</body>"];

    RetainPtr mouseTouchGesture = [&] -> UIGestureRecognizer * {
        for (UIGestureRecognizer *gestureRecognizer in [webView textInputContentView].gestureRecognizers) {
            if ([gestureRecognizer.name isEqualToString:@"WKMouseTouch"])
                return gestureRecognizer;
        }
        return nil;
    }();

    [webView evaluateJavaScript:@"getSelection().setPosition(document.body, 1)" completionHandler:nil];
    [mouseTouchGesture _setStateForTesting:UIGestureRecognizerStateEnded];
    [webView updateSelectionWithExtentPoint:CGPointMake(5, 20)];
    [mouseTouchGesture _clearOverriddenStateForTesting];
    EXPECT_WK_STREQ("Hello world", [webView stringByEvaluatingJavaScript:@"getSelection().toString()"]);

    [webView evaluateJavaScript:@"getSelection().setPosition(document.body, 0)" completionHandler:nil];
    [mouseTouchGesture _setStateForTesting:UIGestureRecognizerStateEnded];
    [webView updateSelectionWithExtentPoint:CGPointMake(300, 20)];
    [mouseTouchGesture _clearOverriddenStateForTesting];
    EXPECT_WK_STREQ("Hello world", [webView stringByEvaluatingJavaScript:@"getSelection().toString()"]);
}

TEST(UIWKInteractionViewProtocol, SelectPositionAtPointAfterBecomingFirstResponder)
{
    IPhoneUserInterfaceSwizzler userInterfaceSwizzler;

    RetainPtr inputDelegate = adoptNS([TestInputDelegate new]);
    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 400, 400)]);
    [webView _setInputDelegate:inputDelegate.get()];
    [inputDelegate setFocusStartsInputSessionPolicyHandler:[&] (WKWebView *, id <_WKFocusedElementInfo>) -> _WKFocusStartsInputSessionPolicy {
        return _WKFocusStartsInputSessionPolicyAllow;
    }];
    // 1. Ensure that the WKWebView is not first responder.
    [webView synchronouslyLoadHTMLString:@"<body style='margin: 0; padding: 0'><div contenteditable='true' style='height: 200px; width: 200px'></div></body>"];
    [webView evaluateJavaScriptAndWaitForInputSessionToChange:@"document.querySelector('div').focus()"];

    // We explicitly dismiss the form accessory view to ensure that a DOM blur event is dispatched
    // regardless of the test device used as -resignFirstResponder may not do this (e.g. it does
    // not do this on iPad when there is a hardware keyboard attached).
    [webView dismissFormAccessoryView];
    [webView resignFirstResponder];
    EXPECT_WK_STREQ("BODY", [webView stringByEvaluatingJavaScript:@"document.activeElement.tagName"]);

    // 2. Make it first responder and perform selection.
    [webView becomeFirstResponder];
    [webView selectPositionAtPoint:CGPointMake(8, 8)];
    EXPECT_WK_STREQ("DIV", [webView stringByEvaluatingJavaScript:@"document.activeElement.tagName"]);
}

TEST(UIWKInteractionViewProtocol, SelectPositionAtPointInFocusedElementStartsInputSession)
{
    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 400, 400)]);
    RetainPtr inputDelegate = adoptNS([TestInputDelegate new]);
    [webView _setInputDelegate:inputDelegate.get()];

    bool didCallDecidePolicyForFocusedElement = false;
    [inputDelegate setFocusStartsInputSessionPolicyHandler:[&] (WKWebView *, id <_WKFocusedElementInfo>) -> _WKFocusStartsInputSessionPolicy {
        didCallDecidePolicyForFocusedElement = true;
        return _WKFocusStartsInputSessionPolicyDisallow;
    }];

    // 1. Focus element
    [webView synchronouslyLoadHTMLString:@"<body style='margin: 0; padding: 0'><div contenteditable='true' style='height: 200px; width: 200px'></div></body>"];
    [webView focusInWindow];
    [webView stringByEvaluatingJavaScript:@"document.querySelector('div').focus()"];
    TestWebKitAPI::Util::run(&didCallDecidePolicyForFocusedElement);

    // 2. Focus the element again via selecting a position at a point inside it.
    didCallDecidePolicyForFocusedElement = false;

    // Resign first so that the next focusInWindow will re-fire the focus event that induces DecidePolicyForFocusedElement.
    [webView resignFirstResponder];
    [webView focusInWindow];
    [webView selectPositionAtPoint:CGPointMake(8, 8)];
    TestWebKitAPI::Util::run(&didCallDecidePolicyForFocusedElement);
}

TEST(UIWKInteractionViewProtocol, SelectPositionAtPointInElementInNonFocusedFrame)
{
    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 400, 400)]);
    RetainPtr inputDelegate = adoptNS([TestInputDelegate new]);
    [webView _setInputDelegate:inputDelegate.get()];

    bool didStartInputSession = false;
    [inputDelegate setFocusStartsInputSessionPolicyHandler:[&] (WKWebView *, id <_WKFocusedElementInfo>) {
        didStartInputSession = true;
        return _WKFocusStartsInputSessionPolicyAllow;
    }];

    [webView synchronouslyLoadHTMLString:@"<body style='margin: 0; padding: 0'><iframe height='100' width='100%' style='border: none; padding: 0; margin: 0' srcdoc='<body style=\"margin: 0; padding: 0\"><div contenteditable=\"true\" style=\"width: 200px; height: 200px\"></body>'></iframe></body>"];
    EXPECT_WK_STREQ("BODY", [webView stringByEvaluatingJavaScript:@"document.querySelector('iframe').contentDocument.activeElement.tagName"]);

    [webView becomeFirstResponder];
    [webView selectPositionAtPoint:CGPointMake(0, 0)];
    TestWebKitAPI::Util::run(&didStartInputSession);
    EXPECT_WK_STREQ("DIV", [webView stringByEvaluatingJavaScript:@"document.querySelector('iframe').contentDocument.activeElement.tagName"]);
}

static std::pair<RetainPtr<TestWKWebView>, RetainPtr<TestInputDelegate>> setUpEditableWebViewAndWaitForInputSession()
{
    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 320)]);
    RetainPtr inputDelegate = adoptNS([TestInputDelegate new]);
    [webView _setInputDelegate:inputDelegate.get()];

    bool didStartInputSession = false;
    [inputDelegate setFocusStartsInputSessionPolicyHandler:[&] (WKWebView *, id <_WKFocusedElementInfo>) {
        didStartInputSession = true;
        return _WKFocusStartsInputSessionPolicyAllow;
    }];

    [webView synchronouslyLoadTestPageNamed:@"editable-responsive-body"];
    [webView focusInWindow];
    TestWebKitAPI::Util::run(&didStartInputSession);
    return { WTF::move(webView), WTF::move(inputDelegate) };
}

// Clears the editable body, then records composition events so a test can assert on the exact
// sequence emitted while dictation streams hypotheses.
static void installCompositionEventRecorder(TestWKWebView *webView)
{
    [webView stringByEvaluatingJavaScript:@"(() => {"
        "    document.body.innerHTML = '';"
        "    window.events = [];"
        "    for (const type of ['compositionstart', 'compositionupdate', 'compositionend'])"
        "        document.addEventListener(type, event => window.events.push(`${event.type}:${event.data ?? ''}`), true);"
        "    getSelection().setPosition(document.body, 0);"
        "})()"];
}

static RetainPtr<NSArray> recordedCompositionEvents(TestWKWebView *webView)
{
    return [webView objectByEvaluatingJavaScript:@"window.events"];
}

TEST(UIWKInteractionViewProtocol, TextInteractionCanBeginInExistingSelection)
{
    auto [webView, inputDelegate] = setUpEditableWebViewAndWaitForInputSession();
    RetainPtr contentView = [webView textInputContentView];
    BOOL allowsTextInteractionOutsideOfSelection = [contentView textInteractionGesture:UIWKGestureLoupe shouldBeginAtPoint:CGPointMake(50, 50)];
    EXPECT_TRUE(allowsTextInteractionOutsideOfSelection);

    [webView selectAll:nil];
    [webView waitForNextPresentationUpdate];

    BOOL allowsTextInteractionInsideSelection = [contentView textInteractionGesture:UIWKGestureLoupe shouldBeginAtPoint:CGPointMake(50, 50)];
    EXPECT_TRUE(allowsTextInteractionInsideSelection);
}

TEST(UIWKInteractionViewProtocol, ReplaceDictatedTextContainingEmojis)
{
    auto [webView, inputDelegate] = setUpEditableWebViewAndWaitForInputSession();
    RetainPtr contentView = [webView textInputContentView];
    [contentView selectAll:nil];
    [contentView insertText:@"Hello world. This 👉🏻 is a good boy"];
    [webView waitForNextPresentationUpdate];

    [contentView replaceDictatedText:@"This 👉🏻 is a good boy" withText:@"This 👉🏻 is a 🦮"];
    [webView waitForNextPresentationUpdate];
    EXPECT_WK_STREQ(@"Hello world. This 👉🏻 is a 🦮", [webView contentsAsString]);
}

TEST(UIWKInteractionViewProtocol, SuppressSelectionChangesDuringDictation)
{
    auto [webView, inputDelegate] = setUpEditableWebViewAndWaitForInputSession();
    RetainPtr contentView = [webView textInputContentView];
    [contentView selectAll:nil];
    [contentView insertText:@"Hello world"];
    [webView waitForNextPresentationUpdate];

    RetainPtr observer = adoptNS([[EditorStateObserver alloc] initWithWebView:webView.get()]);
    [contentView willInsertFinalDictationResult];
    [contentView replaceDictatedText:@"Hello world" withText:@""];
    [contentView insertText:@"Foo"];
    [contentView insertText:@" "];
    [contentView insertText:@"Bar"];
    [contentView didInsertFinalDictationResult];
    [webView waitForNextPresentationUpdate];
    EXPECT_WK_STREQ("Foo Bar", [webView contentsAsString]);
    EXPECT_EQ(1U, [observer changeCount]);
}

TEST(UIWKInteractionViewProtocol, ReplaceDictatedTextStartsComposition)
{
    auto [webView, inputDelegate] = setUpEditableWebViewAndWaitForInputSession();
    RetainPtr contentView = [webView textInputContentView];
    installCompositionEventRecorder(webView.get());

    // The initial hypothesis arrives as a plain insertText: (the seed).
    [contentView insertText:@"Te"];
    [webView waitForNextPresentationUpdate];

    // The first streaming update replaces the seed and starts the composition.
    [contentView replaceDictatedText:@"Te" withText:@"Test"];
    [webView waitForNextPresentationUpdate];

    RetainPtr events = recordedCompositionEvents(webView.get());
    EXPECT_TRUE([events containsObject:@"compositionstart:"]);
    EXPECT_TRUE([events containsObject:@"compositionupdate:Test"]);
    EXPECT_WK_STREQ("Test", [webView stringByEvaluatingJavaScript:@"document.body.textContent"]);
}

TEST(UIWKInteractionViewProtocol, ReplaceDictatedTextContinuesComposition)
{
    auto [webView, inputDelegate] = setUpEditableWebViewAndWaitForInputSession();
    RetainPtr contentView = [webView textInputContentView];
    installCompositionEventRecorder(webView.get());

    // Stream a longer phrase one word at a time, as dictation does in practice.
    [contentView insertText:@"th"];
    [contentView replaceDictatedText:@"th" withText:@"the"];
    [contentView replaceDictatedText:@"the" withText:@"the quick"];
    [contentView replaceDictatedText:@"the quick" withText:@"the quick brown"];
    [contentView replaceDictatedText:@"the quick brown" withText:@"the quick brown fox"];
    [contentView replaceDictatedText:@"the quick brown fox" withText:@"the quick brown fox jumps"];
    [webView waitForNextPresentationUpdate];

    RetainPtr events = recordedCompositionEvents(webView.get());
    NSUInteger compositionStartCount = 0;
    NSUInteger compositionEndCount = 0;
    for (NSString *event in events.get()) {
        if ([event isEqualToString:@"compositionstart:"])
            compositionStartCount++;
        else if ([event hasPrefix:@"compositionend:"])
            compositionEndCount++;
    }

    EXPECT_EQ(1U, compositionStartCount);
    EXPECT_EQ(0U, compositionEndCount);
    EXPECT_TRUE([events containsObject:@"compositionupdate:the quick brown fox jumps"]);
    EXPECT_WK_STREQ("the quick brown fox jumps", [webView stringByEvaluatingJavaScript:@"document.body.textContent"]);
}

TEST(UIWKInteractionViewProtocol, ReplaceDictatedTextClearingEmitsCompositionEnd)
{
    auto [webView, inputDelegate] = setUpEditableWebViewAndWaitForInputSession();
    RetainPtr contentView = [webView textInputContentView];
    installCompositionEventRecorder(webView.get());

    [contentView insertText:@"Te"];
    [contentView replaceDictatedText:@"Te" withText:@"Test 123"];
    [webView waitForNextPresentationUpdate];

    [contentView willInsertFinalDictationResult];
    [contentView replaceDictatedText:@"Test 123" withText:@""];
    [contentView insertText:@"Test 123"];
    [contentView didInsertFinalDictationResult];
    [webView waitForNextPresentationUpdate];

    RetainPtr events = recordedCompositionEvents(webView.get());
    EXPECT_TRUE([events containsObject:@"compositionend:"]);
    EXPECT_WK_STREQ("Test 123", [webView stringByEvaluatingJavaScript:@"document.body.textContent"]);
}

TEST(UIWKInteractionViewProtocol, ReplaceDictatedTextReplacesSelectedText)
{
    auto [webView, inputDelegate] = setUpEditableWebViewAndWaitForInputSession();
    RetainPtr contentView = [webView textInputContentView];
    installCompositionEventRecorder(webView);

    // Start with some existing text and select all of it, as if the user selected text
    // before invoking dictation.
    [webView stringByEvaluatingJavaScript:@"document.body.innerHTML = 'Hello world'; getSelection().selectAllChildren(document.body)"];
    [webView waitForNextPresentationUpdate];
    EXPECT_WK_STREQ("Hello world", [webView stringByEvaluatingJavaScript:@"getSelection().toString()"]);

    [contentView insertText:@"Te"];
    [webView waitForNextPresentationUpdate];

    [contentView replaceDictatedText:@"Te" withText:@"Test"];
    [webView waitForNextPresentationUpdate];

    RetainPtr events = recordedCompositionEvents(webView);
    EXPECT_TRUE([events containsObject:@"compositionstart:"]);
    EXPECT_TRUE([events containsObject:@"compositionupdate:Test"]);
    EXPECT_WK_STREQ("Test", [webView stringByEvaluatingJavaScript:@"document.body.textContent"]);
}

TEST(UIWKInteractionViewProtocol, ReplaceDictatedTextReplacesSelectedPortion)
{
    auto [webView, inputDelegate] = setUpEditableWebViewAndWaitForInputSession();
    RetainPtr contentView = [webView textInputContentView];
    installCompositionEventRecorder(webView);

    // Start with some existing text and select only the last word ("world").
    [webView stringByEvaluatingJavaScript:@"(() => {"
        "    document.body.innerHTML = 'Hello world';"
        "    const text = document.body.firstChild;"
        "    const range = document.createRange();"
        "    range.setStart(text, 6);"
        "    range.setEnd(text, 11);"
        "    getSelection().removeAllRanges();"
        "    getSelection().addRange(range);"
        "})()"];
    [webView waitForNextPresentationUpdate];
    EXPECT_WK_STREQ("world", [webView stringByEvaluatingJavaScript:@"getSelection().toString()"]);

    [contentView insertText:@"Te"];
    [webView waitForNextPresentationUpdate];

    [contentView replaceDictatedText:@"Te" withText:@"Test"];
    [webView waitForNextPresentationUpdate];

    RetainPtr events = recordedCompositionEvents(webView);
    EXPECT_TRUE([events containsObject:@"compositionstart:"]);
    EXPECT_TRUE([events containsObject:@"compositionupdate:Test"]);
    EXPECT_WK_STREQ("Hello Test", [webView stringByEvaluatingJavaScript:@"document.body.textContent"]);
}

TEST(UIWKInteractionViewProtocol, ReplaceDictatedTextReplacesMultiWordSelection)
{
    auto [webView, inputDelegate] = setUpEditableWebViewAndWaitForInputSession();
    RetainPtr contentView = [webView textInputContentView];
    installCompositionEventRecorder(webView);

    // Select multiple words in the middle of the text, leaving words (and spaces) on both sides.
    [webView stringByEvaluatingJavaScript:@"(() => {"
        "    document.body.innerHTML = 'one two three four';"
        "    const text = document.body.firstChild;"
        "    const range = document.createRange();"
        "    range.setStart(text, 4);"
        "    range.setEnd(text, 13);"
        "    getSelection().removeAllRanges();"
        "    getSelection().addRange(range);"
        "})()"];
    [webView waitForNextPresentationUpdate];
    EXPECT_WK_STREQ("two three", [webView stringByEvaluatingJavaScript:@"getSelection().toString()"]);

    [contentView insertText:@"Te"];
    [webView waitForNextPresentationUpdate];

    [contentView replaceDictatedText:@"Te" withText:@"Test"];
    [webView waitForNextPresentationUpdate];

    RetainPtr events = recordedCompositionEvents(webView);
    EXPECT_TRUE([events containsObject:@"compositionstart:"]);
    EXPECT_TRUE([events containsObject:@"compositionupdate:Test"]);
    EXPECT_WK_STREQ("one Test four", [webView stringByEvaluatingJavaScript:@"document.body.textContent"]);
}

TEST(UIWKInteractionViewProtocol, ReplaceDictatedTextEmitsCompositionEventSequence)
{
    auto [webView, inputDelegate] = setUpEditableWebViewAndWaitForInputSession();
    RetainPtr contentView = [webView textInputContentView];
    installCompositionEventRecorder(webView);

    [contentView insertText:@"th"];
    [contentView replaceDictatedText:@"th" withText:@"the"];
    [contentView replaceDictatedText:@"the" withText:@"the quick"];
    [contentView replaceDictatedText:@"the quick" withText:@"the quick brown"];
    [contentView willInsertFinalDictationResult];
    [contentView replaceDictatedText:@"the quick brown" withText:@""];
    [contentView insertText:@"the quick brown"];
    [contentView didInsertFinalDictationResult];
    [webView waitForNextPresentationUpdate];

    RetainPtr events = recordedCompositionEvents(webView);
    NSArray *expected = @[
        @"compositionstart:",
        @"compositionupdate:the",
        @"compositionupdate:the quick",
        @"compositionupdate:the quick brown",
        @"compositionend:",
    ];
    EXPECT_TRUE([events isEqualToArray:expected]);
    EXPECT_WK_STREQ("the quick brown", [webView stringByEvaluatingJavaScript:@"document.body.textContent"]);
}

TEST(UIWKInteractionViewProtocol, ReplaceDictatedTextPreservesSurroundingText)
{
    auto [webView, inputDelegate] = setUpEditableWebViewAndWaitForInputSession();
    RetainPtr contentView = [webView textInputContentView];
    installCompositionEventRecorder(webView);

    // Place the caret in the middle of existing text, as if dictating between two characters.
    [webView stringByEvaluatingJavaScript:@"document.body.innerHTML = '()'; getSelection().setPosition(document.body.firstChild, 1)"];
    [webView waitForNextPresentationUpdate];

    [contentView insertText:@"Te"];
    [contentView replaceDictatedText:@"Te" withText:@"Test"];
    [webView waitForNextPresentationUpdate];

    RetainPtr events = recordedCompositionEvents(webView);
    EXPECT_TRUE([events containsObject:@"compositionstart:"]);
    EXPECT_TRUE([events containsObject:@"compositionupdate:Test"]);
    EXPECT_WK_STREQ("(Test)", [webView stringByEvaluatingJavaScript:@"document.body.textContent"]);
}

TEST(UIWKInteractionViewProtocol, ReplaceDictatedTextHandlesShrinkingHypothesis)
{
    auto [webView, inputDelegate] = setUpEditableWebViewAndWaitForInputSession();
    RetainPtr contentView = [webView textInputContentView];
    installCompositionEventRecorder(webView);

    [contentView insertText:@"Te"];
    [contentView replaceDictatedText:@"Te" withText:@"Test"];
    [contentView replaceDictatedText:@"Test" withText:@"Test one"];
    [contentView replaceDictatedText:@"Test one" withText:@"Test"];
    [webView waitForNextPresentationUpdate];

    RetainPtr events = recordedCompositionEvents(webView);
    NSUInteger compositionStartCount = 0;
    NSUInteger compositionEndCount = 0;
    for (NSString *event in events.get()) {
        if ([event isEqualToString:@"compositionstart:"])
            compositionStartCount++;
        else if ([event hasPrefix:@"compositionend:"])
            compositionEndCount++;
    }

    EXPECT_EQ(1U, compositionStartCount);
    EXPECT_EQ(0U, compositionEndCount);
    EXPECT_WK_STREQ("compositionupdate:Test", [events lastObject]);
    EXPECT_WK_STREQ("Test", [webView stringByEvaluatingJavaScript:@"document.body.textContent"]);
}

} // namespace TestWebKitAPI

#endif
