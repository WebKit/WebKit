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

#import "PlatformUtilities.h"
#import "TestInputDelegate.h"
#import "TestWKWebView.h"
#import "UIKitSPIForTesting.h"
#import "UserInterfaceSwizzler.h"
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
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 400, 400)]);
    [webView synchronouslyLoadHTMLString:@"<body style='font-size: 20px;'>Hello world</body>"];
    [webView selectTextWithGranularity:UITextGranularityCharacter atPoint:CGPointMake(10, 20)];
    [webView updateSelectionWithExtentPoint:CGPointMake(300, 20) withBoundary:UITextGranularityCharacter];
    EXPECT_WK_STREQ("Hello world", [webView stringByEvaluatingJavaScript:@"getSelection().toString()"]);
}

TEST(UIWKInteractionViewProtocol, UpdateSelectionWithExtentPoint)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 400, 400)]);
    [webView synchronouslyLoadHTMLString:@"<body contenteditable style='font-size: 20px;'>Hello world</body>"];

    [webView evaluateJavaScript:@"getSelection().setPosition(document.body, 1)" completionHandler:nil];
    [webView updateSelectionWithExtentPoint:CGPointMake(5, 20)];
    EXPECT_WK_STREQ("Hello world", [webView stringByEvaluatingJavaScript:@"getSelection().toString()"]);

    [webView evaluateJavaScript:@"getSelection().setPosition(document.body, 0)" completionHandler:nil];
    [webView updateSelectionWithExtentPoint:CGPointMake(300, 20)];
    EXPECT_WK_STREQ("Hello world", [webView stringByEvaluatingJavaScript:@"getSelection().toString()"]);
}

TEST(UIWKInteractionViewProtocol, SelectPositionAtPointAfterBecomingFirstResponder)
{
    IPhoneUserInterfaceSwizzler userInterfaceSwizzler;

    auto inputDelegate = adoptNS([TestInputDelegate new]);
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 400, 400)]);
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
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 400, 400)]);
    auto inputDelegate = adoptNS([TestInputDelegate new]);
    [webView _setInputDelegate:inputDelegate.get()];

    bool didCallDecidePolicyForFocusedElement = false;
    [inputDelegate setFocusStartsInputSessionPolicyHandler:[&] (WKWebView *, id <_WKFocusedElementInfo>) -> _WKFocusStartsInputSessionPolicy {
        didCallDecidePolicyForFocusedElement = true;
        return _WKFocusStartsInputSessionPolicyDisallow;
    }];

    // 1. Focus element
    [webView synchronouslyLoadHTMLString:@"<body style='margin: 0; padding: 0'><div contenteditable='true' style='height: 200px; width: 200px'></div></body>"];
    [webView stringByEvaluatingJavaScript:@"document.querySelector('div').focus()"];
    TestWebKitAPI::Util::run(&didCallDecidePolicyForFocusedElement);

    // 2. Focus the element again via selecting a position at a point inside it.
    didCallDecidePolicyForFocusedElement = false;
    [webView becomeFirstResponder];
    [webView selectPositionAtPoint:CGPointMake(8, 8)];
    TestWebKitAPI::Util::run(&didCallDecidePolicyForFocusedElement);
}

TEST(UIWKInteractionViewProtocol, SelectPositionAtPointInElementInNonFocusedFrame)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 400, 400)]);
    auto inputDelegate = adoptNS([TestInputDelegate new]);
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
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 320)]);
    auto inputDelegate = adoptNS([TestInputDelegate new]);
    [webView _setInputDelegate:inputDelegate.get()];

    bool didStartInputSession = false;
    [inputDelegate setFocusStartsInputSessionPolicyHandler:[&] (WKWebView *, id <_WKFocusedElementInfo>) {
        didStartInputSession = true;
        return _WKFocusStartsInputSessionPolicyAllow;
    }];

    [webView synchronouslyLoadTestPageNamed:@"editable-responsive-body"];
    TestWebKitAPI::Util::run(&didStartInputSession);
    return { WTFMove(webView), WTFMove(inputDelegate) };
}

TEST(UIWKInteractionViewProtocol, TextInteractionCanBeginInExistingSelection)
{
    auto [webView, inputDelegate] = setUpEditableWebViewAndWaitForInputSession();
    auto contentView = [webView textInputContentView];
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
    auto contentView = [webView textInputContentView];
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
    auto contentView = [webView textInputContentView];
    [contentView selectAll:nil];
    [contentView insertText:@"Hello world"];
    [webView waitForNextPresentationUpdate];

    auto observer = adoptNS([[EditorStateObserver alloc] initWithWebView:webView.get()]);
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

} // namespace TestWebKitAPI

#endif
