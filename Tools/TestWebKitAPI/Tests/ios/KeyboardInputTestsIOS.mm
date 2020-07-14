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

#if PLATFORM(IOS_FAMILY)

#import "PlatformUtilities.h"
#import "TestCocoa.h"
#import "TestInputDelegate.h"
#import "TestNavigationDelegate.h"
#import "TestProtocol.h"
#import "TestWKWebView.h"
#import "UIKitSPI.h"
#import "UserInterfaceSwizzler.h"
#import <WebKit/WKProcessPoolPrivate.h>
#import <WebKit/WKWebViewPrivate.h>
#import <WebKit/_WKProcessPoolConfiguration.h>
#import <WebKitLegacy/WebEvent.h>
#import <cmath>

@interface WKContentView ()
- (BOOL)_shouldSimulateKeyboardInputOnTextInsertion;
@end

@interface InputAssistantItemTestingWebView : TestWKWebView
+ (UIBarButtonItemGroup *)leadingItemsForWebView:(WKWebView *)webView;
+ (UIBarButtonItemGroup *)trailingItemsForWebView:(WKWebView *)webView;
@end

@implementation InputAssistantItemTestingWebView {
    RetainPtr<UIBarButtonItemGroup> _leadingItems;
    RetainPtr<UIBarButtonItemGroup> _trailingItems;
}

- (void)fakeLeadingBarButtonItemAction
{
}

- (void)fakeTrailingBarButtonItemAction
{
}

+ (UIImage *)barButtonIcon
{
    return [UIImage imageNamed:@"TestWebKitAPI.resources/icon.png"];
}

+ (UIBarButtonItemGroup *)leadingItemsForWebView:(WKWebView *)webView
{
    static dispatch_once_t onceToken;
    static UIBarButtonItemGroup *sharedItems;
    dispatch_once(&onceToken, ^{
        auto leadingItem = adoptNS([[UIBarButtonItem alloc] initWithImage:self.barButtonIcon style:UIBarButtonItemStylePlain target:webView action:@selector(fakeLeadingBarButtonItemAction)]);
        sharedItems = [[UIBarButtonItemGroup alloc] initWithBarButtonItems:@[ leadingItem.get() ] representativeItem:nil];
    });
    return sharedItems;
}

+ (UIBarButtonItemGroup *)trailingItemsForWebView:(WKWebView *)webView
{
    static dispatch_once_t onceToken;
    static UIBarButtonItemGroup *sharedItems;
    dispatch_once(&onceToken, ^{
        auto trailingItem = adoptNS([[UIBarButtonItem alloc] initWithImage:self.barButtonIcon style:UIBarButtonItemStylePlain target:webView action:@selector(fakeTrailingBarButtonItemAction)]);
        sharedItems = [[UIBarButtonItemGroup alloc] initWithBarButtonItems:@[ trailingItem.get() ] representativeItem:nil];
    });
    return sharedItems;
}

- (UITextInputAssistantItem *)inputAssistantItem
{
    auto assistantItem = adoptNS([[UITextInputAssistantItem alloc] init]);
    [assistantItem setLeadingBarButtonGroups:@[[InputAssistantItemTestingWebView leadingItemsForWebView:self]]];
    [assistantItem setTrailingBarButtonGroups:@[[InputAssistantItemTestingWebView trailingItemsForWebView:self]]];
    return assistantItem.autorelease();
}

@end

@implementation TestWKWebView (KeyboardInputTests)

static CGRect rounded(CGRect rect)
{
    return CGRectMake(std::round(rect.origin.x), std::round(rect.origin.y), std::round(rect.size.width), std::round(rect.size.height));
}

- (void)waitForCaretViewFrameToBecome:(CGRect)frame
{
    BOOL hasEmittedWarning = NO;
    NSTimeInterval secondsToWaitUntilWarning = 2;
    NSTimeInterval startTime = [NSDate timeIntervalSinceReferenceDate];
    while ([[NSRunLoop currentRunLoop] runMode:NSDefaultRunLoopMode beforeDate:[NSDate distantPast]]) {
        CGRect currentFrame = rounded(self.caretViewRectInContentCoordinates);
        if (CGRectEqualToRect(currentFrame, frame))
            break;

        if (hasEmittedWarning || startTime + secondsToWaitUntilWarning >= [NSDate timeIntervalSinceReferenceDate])
            continue;

        NSLog(@"Expected a caret rect of %@, but still observed %@", NSStringFromCGRect(frame), NSStringFromCGRect(currentFrame));
        hasEmittedWarning = YES;
    }
}

- (void)waitForSelectionViewRectsToBecome:(NSArray<NSValue *> *)selectionRects
{
    BOOL hasEmittedWarning = NO;
    NSTimeInterval secondsToWaitUntilWarning = 2;
    NSTimeInterval startTime = [NSDate timeIntervalSinceReferenceDate];
    while ([[NSRunLoop currentRunLoop] runMode:NSDefaultRunLoopMode beforeDate:[NSDate distantPast]]) {
        NSArray<NSValue *> *currentRects = self.selectionViewRectsInContentCoordinates;
        BOOL selectionRectsMatch = YES;
        if (currentRects.count == selectionRects.count) {
            for (NSUInteger index = 0; index < selectionRects.count; ++index)
                selectionRectsMatch |= CGRectEqualToRect(selectionRects[index].CGRectValue, rounded(currentRects[index].CGRectValue));
        } else
            selectionRectsMatch = NO;

        if (selectionRectsMatch)
            break;

        if (hasEmittedWarning || startTime + secondsToWaitUntilWarning >= [NSDate timeIntervalSinceReferenceDate])
            continue;

        NSLog(@"Expected a selection rects of %@, but still observed %@", selectionRects, currentRects);
        hasEmittedWarning = YES;
    }
}

- (UIBarButtonItemGroup *)lastTrailingBarButtonGroup
{
    return self.firstResponder.inputAssistantItem.trailingBarButtonGroups.lastObject;
}

@end

@interface CustomInputWebView : TestWKWebView
- (instancetype)initWithFrame:(CGRect)frame configuration:(WKWebViewConfiguration *)configuration inputView:(UIView *)inputView inputAccessoryView:(UIView *)inputAccessoryView;
@end

@implementation CustomInputWebView {
    RetainPtr<UIView> _customInputView;
    RetainPtr<UIView> _customInputAccessoryView;
}

- (instancetype)initWithFrame:(CGRect)frame configuration:(WKWebViewConfiguration *)configuration inputView:(UIView *)inputView inputAccessoryView:(UIView *)inputAccessoryView
{
    if (self = [super initWithFrame:frame configuration:configuration]) {
        _customInputView = inputView;
        _customInputAccessoryView = inputAccessoryView;
    }
    return self;
}

- (UIView *)inputView
{
    return _customInputView.get();
}

- (UIView *)inputAccessoryView
{
    return _customInputAccessoryView.get();
}

@end

static RetainPtr<TestWKWebView> webViewWithAutofocusedInput(const RetainPtr<TestInputDelegate>& inputDelegate)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);

    __block bool doneWaiting = false;
    [inputDelegate setFocusStartsInputSessionPolicyHandler:^_WKFocusStartsInputSessionPolicy(WKWebView *, id <_WKFocusedElementInfo>) {
        doneWaiting = true;
        return _WKFocusStartsInputSessionPolicyAllow;
    }];
    [webView _setInputDelegate:inputDelegate.get()];
    [webView synchronouslyLoadHTMLString:@"<meta name='viewport' content='width=device-width, initial-scale=1'><input autofocus>"];

    TestWebKitAPI::Util::run(&doneWaiting);
    doneWaiting = false;
    return webView;
}

static std::pair<RetainPtr<TestWKWebView>, RetainPtr<TestInputDelegate>> webViewAndInputDelegateWithAutofocusedInput()
{
    auto inputDelegate = adoptNS([TestInputDelegate new]);
    return { webViewWithAutofocusedInput(inputDelegate), inputDelegate };
}

namespace TestWebKitAPI {

TEST(KeyboardInputTests, FormNavigationAssistantBarButtonItems)
{
    IPadUserInterfaceSwizzler iPadUserInterface;

    auto inputDelegate = adoptNS([TestInputDelegate new]);
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    [webView _setInputDelegate:inputDelegate.get()];
    [inputDelegate setFocusStartsInputSessionPolicyHandler:[&] (WKWebView *, id <_WKFocusedElementInfo>) -> _WKFocusStartsInputSessionPolicy {
        return _WKFocusStartsInputSessionPolicyAllow;
    }];
    [webView synchronouslyLoadHTMLString:@"<body contenteditable>"];
    [webView evaluateJavaScriptAndWaitForInputSessionToChange:@"document.body.focus()"];

    EXPECT_EQ(2U, [webView lastTrailingBarButtonGroup].barButtonItems.count);
    EXPECT_FALSE([webView lastTrailingBarButtonGroup].hidden);

    if (![UIWebFormAccessory instancesRespondToSelector:@selector(setNextPreviousItemsVisible:)]) {
        // The rest of this test requires UIWebFormAccessory to be able to show or hide its next and previous items.
        return;
    }

    [webView _setEditable:YES];
    EXPECT_TRUE([webView lastTrailingBarButtonGroup].hidden);

    [webView _setEditable:NO];
    EXPECT_FALSE([webView lastTrailingBarButtonGroup].hidden);
}

TEST(KeyboardInputTests, ModifyInputAssistantItemBarButtonGroups)
{
    auto inputDelegate = adoptNS([TestInputDelegate new]);
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    [webView _setInputDelegate:inputDelegate.get()];
    [webView synchronouslyLoadHTMLString:@"<body contenteditable>"];
    UITextInputAssistantItem *item = [webView inputAssistantItem];
    UIBarButtonItemGroup *leadingItems = [InputAssistantItemTestingWebView leadingItemsForWebView:webView.get()];
    UIBarButtonItemGroup *trailingItems = [InputAssistantItemTestingWebView trailingItemsForWebView:webView.get()];
    item.leadingBarButtonGroups = @[ leadingItems ];
    item.trailingBarButtonGroups = @[ trailingItems ];

    [inputDelegate setFocusStartsInputSessionPolicyHandler:[&] (WKWebView *, id <_WKFocusedElementInfo>) -> _WKFocusStartsInputSessionPolicy {
        return _WKFocusStartsInputSessionPolicyAllow;
    }];
    [webView evaluateJavaScriptAndWaitForInputSessionToChange:@"document.body.focus()"];

    EXPECT_EQ([webView firstResponder], [webView textInputContentView]);
    EXPECT_TRUE([[webView firstResponder].inputAssistantItem.leadingBarButtonGroups containsObject:leadingItems]);
    EXPECT_TRUE([[webView firstResponder].inputAssistantItem.trailingBarButtonGroups containsObject:trailingItems]);

    // Now blur and refocus the editable area, and check that the same leading and trailing button items are present.
    [webView evaluateJavaScriptAndWaitForInputSessionToChange:@"document.body.blur()"];
    [webView evaluateJavaScriptAndWaitForInputSessionToChange:@"document.body.focus()"];

    EXPECT_EQ([webView firstResponder], [webView textInputContentView]);
    EXPECT_TRUE([[webView firstResponder].inputAssistantItem.leadingBarButtonGroups containsObject:leadingItems]);
    EXPECT_TRUE([[webView firstResponder].inputAssistantItem.trailingBarButtonGroups containsObject:trailingItems]);
}

TEST(KeyboardInputTests, OverrideInputAssistantItemBarButtonGroups)
{
    auto inputDelegate = adoptNS([TestInputDelegate new]);
    auto webView = adoptNS([[InputAssistantItemTestingWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    [webView _setInputDelegate:inputDelegate.get()];
    [webView synchronouslyLoadHTMLString:@"<body contenteditable>"];
    [inputDelegate setFocusStartsInputSessionPolicyHandler:[&] (WKWebView *, id <_WKFocusedElementInfo>) -> _WKFocusStartsInputSessionPolicy {
        return _WKFocusStartsInputSessionPolicyAllow;
    }];
    [webView evaluateJavaScriptAndWaitForInputSessionToChange:@"document.body.focus()"];

    UIBarButtonItemGroup *leadingItems = [InputAssistantItemTestingWebView leadingItemsForWebView:webView.get()];
    UIBarButtonItemGroup *trailingItems = [InputAssistantItemTestingWebView trailingItemsForWebView:webView.get()];
    EXPECT_EQ([webView firstResponder], [webView textInputContentView]);
    EXPECT_TRUE([[webView firstResponder].inputAssistantItem.leadingBarButtonGroups containsObject:leadingItems]);
    EXPECT_TRUE([[webView firstResponder].inputAssistantItem.trailingBarButtonGroups containsObject:trailingItems]);

    // Now blur and refocus the editable area, and check that the same leading and trailing button items are present.
    [webView evaluateJavaScriptAndWaitForInputSessionToChange:@"document.body.blur()"];
    [webView evaluateJavaScriptAndWaitForInputSessionToChange:@"document.body.focus()"];

    EXPECT_EQ([webView firstResponder], [webView textInputContentView]);
    EXPECT_TRUE([[webView firstResponder].inputAssistantItem.leadingBarButtonGroups containsObject:leadingItems]);
    EXPECT_TRUE([[webView firstResponder].inputAssistantItem.trailingBarButtonGroups containsObject:trailingItems]);
}

TEST(KeyboardInputTests, CustomInputViewAndInputAccessoryView)
{
    auto inputView = adoptNS([[UIView alloc] init]);
    auto inputAccessoryView = adoptNS([[UIView alloc] init]);
    auto inputDelegate = adoptNS([TestInputDelegate new]);
    [inputDelegate setWillStartInputSessionHandler:[inputView, inputAccessoryView] (WKWebView *, id<_WKFormInputSession> session) {
        session.customInputView = inputView.get();
        session.customInputAccessoryView = inputAccessoryView.get();
    }];

    auto webView = webViewWithAutofocusedInput(inputDelegate);
    EXPECT_EQ(inputView.get(), [webView firstResponder].inputView);
    EXPECT_EQ(inputAccessoryView.get(), [webView firstResponder].inputAccessoryView);
}

TEST(KeyboardInputTests, CanHandleKeyEventInCompletionHandler)
{
    auto [webView, inputDelegate] = webViewAndInputDelegateWithAutofocusedInput();
    bool doneWaiting = false;

    id <UITextInputPrivate> contentView = (id <UITextInputPrivate>)[webView firstResponder];
    auto firstWebEvent = adoptNS([[WebEvent alloc] initWithKeyEventType:WebEventKeyDown timeStamp:CFAbsoluteTimeGetCurrent() characters:@"a" charactersIgnoringModifiers:@"a" modifiers:0 isRepeating:NO withFlags:0 withInputManagerHint:nil keyCode:0 isTabKey:NO]);
    auto secondWebEvent = adoptNS([[WebEvent alloc] initWithKeyEventType:WebEventKeyUp timeStamp:CFAbsoluteTimeGetCurrent() characters:@"a" charactersIgnoringModifiers:@"a" modifiers:0 isRepeating:NO withFlags:0 withInputManagerHint:nil keyCode:0 isTabKey:NO]);
    [contentView handleKeyWebEvent:firstWebEvent.get() withCompletionHandler:[&] (WebEvent *event, BOOL) {
        EXPECT_TRUE([event isEqual:firstWebEvent.get()]);
        [contentView handleKeyWebEvent:secondWebEvent.get() withCompletionHandler:[&] (WebEvent *event, BOOL) {
            EXPECT_TRUE([event isEqual:secondWebEvent.get()]);
            [contentView insertText:@"a"];
            doneWaiting = true;
        }];
    }];

    TestWebKitAPI::Util::run(&doneWaiting);
    EXPECT_WK_STREQ("a", [webView stringByEvaluatingJavaScript:@"document.querySelector('input').value"]);
}

TEST(KeyboardInputTests, ResigningFirstResponderCancelsKeyEvents)
{
    auto [webView, inputDelegate] = webViewAndInputDelegateWithAutofocusedInput();
    auto contentView = [webView textInputContentView];
    auto keyDownEvent = adoptNS([[WebEvent alloc] initWithKeyEventType:WebEventKeyDown timeStamp:CFAbsoluteTimeGetCurrent() characters:@"a" charactersIgnoringModifiers:@"a" modifiers:0 isRepeating:NO withFlags:0 withInputManagerHint:nil keyCode:0 isTabKey:NO]);

    [webView becomeFirstResponder];
    [webView evaluateJavaScript:@"while(1);" completionHandler:nil];

    bool doneWaiting = false;
    [contentView handleKeyWebEvent:keyDownEvent.get() withCompletionHandler:[&] (WebEvent *event, BOOL handled) {
        EXPECT_TRUE([event isEqual:keyDownEvent.get()]);
        EXPECT_TRUE(handled);
        doneWaiting = true;
    }];

    EXPECT_TRUE([webView resignFirstResponder]);
    TestWebKitAPI::Util::run(&doneWaiting);
}

TEST(KeyboardInputTests, WaitForKeyEventHandlerInFirstResponder)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    auto contentView = [webView textInputContentView];
    auto keyDownEvent = adoptNS([[WebEvent alloc] initWithKeyEventType:WebEventKeyDown timeStamp:CFAbsoluteTimeGetCurrent() characters:@"a" charactersIgnoringModifiers:@"a" modifiers:0 isRepeating:NO withFlags:0 withInputManagerHint:nil keyCode:0 isTabKey:NO]);

    [webView becomeFirstResponder];
    [webView synchronouslyLoadHTMLString:@"<body></body>"];
    [webView evaluateJavaScript:@"start = Date.now(); while(Date.now() - start < 500);" completionHandler:nil];

    bool doneWaiting = false;
    [contentView handleKeyWebEvent:keyDownEvent.get() withCompletionHandler:[&] (WebEvent *event, BOOL handled) {
        EXPECT_TRUE([event isEqual:keyDownEvent.get()]);
        EXPECT_FALSE(handled);
        doneWaiting = true;
    }];

    TestWebKitAPI::Util::run(&doneWaiting);
}

TEST(KeyboardInputTests, HandleKeyEventsInCrashedOrUninitializedWebProcess)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    auto contentView = [webView textInputContentView];
    {
        auto keyDownEvent = adoptNS([[WebEvent alloc] initWithKeyEventType:WebEventKeyDown timeStamp:CFAbsoluteTimeGetCurrent() characters:@"a" charactersIgnoringModifiers:@"a" modifiers:0 isRepeating:NO withFlags:0 withInputManagerHint:nil keyCode:65 isTabKey:NO]);
        bool doneWaiting = false;
        [webView synchronouslyLoadHTMLString:@"<body></body>"];
        [webView evaluateJavaScript:@"while (1);" completionHandler:nil];
        [contentView handleKeyWebEvent:keyDownEvent.get() withCompletionHandler:[&](WebEvent *event, BOOL handled) {
            EXPECT_TRUE([event isEqual:keyDownEvent.get()]);
            EXPECT_FALSE(handled);
            doneWaiting = true;
        }];
        [webView _killWebContentProcessAndResetState];
        TestWebKitAPI::Util::run(&doneWaiting);
    }
    {
        auto keyUpEvent = adoptNS([[WebEvent alloc] initWithKeyEventType:WebEventKeyUp timeStamp:CFAbsoluteTimeGetCurrent() characters:@"a" charactersIgnoringModifiers:@"a" modifiers:0 isRepeating:NO withFlags:0 withInputManagerHint:nil keyCode:65 isTabKey:NO]);
        bool doneWaiting = false;
        [webView _close];
        [contentView handleKeyWebEvent:keyUpEvent.get() withCompletionHandler:[&](WebEvent *event, BOOL handled) {
            EXPECT_TRUE([event isEqual:keyUpEvent.get()]);
            EXPECT_FALSE(handled);
            doneWaiting = true;
        }];
        TestWebKitAPI::Util::run(&doneWaiting);
    }
}

TEST(KeyboardInputTests, HandleKeyEventsWhileSwappingWebProcess)
{
    [TestProtocol registerWithScheme:@"https"];

    auto processPoolConfiguration = adoptNS([[_WKProcessPoolConfiguration alloc] init]);
    [processPoolConfiguration setProcessSwapsOnNavigation:YES];
    auto processPool = adoptNS([[WKProcessPool alloc] _initWithConfiguration:processPoolConfiguration.get()]);
    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [configuration setProcessPool:processPool.get()];

    auto navigationDelegate = adoptNS([[TestNavigationDelegate alloc] init]);
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
    [webView setNavigationDelegate:navigationDelegate.get()];
    [webView loadHTMLString:@"<body>webkit.org</body>" baseURL:[NSURL URLWithString:@"https://webkit.org"]];
    [navigationDelegate waitForDidFinishNavigation];

    [webView loadHTMLString:@"<body>apple.com</body>" baseURL:[NSURL URLWithString:@"https://apple.com"]];
    [navigationDelegate waitForDidStartProvisionalNavigation];

    bool done = false;
    auto keyEvent = adoptNS([[WebEvent alloc] initWithKeyEventType:WebEventKeyDown timeStamp:CFAbsoluteTimeGetCurrent() characters:@"a" charactersIgnoringModifiers:@"a" modifiers:0 isRepeating:NO withFlags:0 withInputManagerHint:nil keyCode:65 isTabKey:NO]);
    dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(0.01 * NSEC_PER_SEC)), dispatch_get_main_queue(), [keyEvent, webView, &done] {
        [[webView textInputContentView] handleKeyWebEvent:keyEvent.get() withCompletionHandler:[keyEvent, &done](WebEvent *event, BOOL handled) {
            EXPECT_TRUE([event isEqual:keyEvent.get()]);
            EXPECT_FALSE(handled);
            done = true;
        }];
    });

    [navigationDelegate waitForDidFinishNavigation];
    TestWebKitAPI::Util::run(&done);
}

TEST(KeyboardInputTests, CaretSelectionRectAfterRestoringFirstResponderWithRetainActiveFocusedState)
{
    // This difference in caret width is due to the fact that we don't zoom in to the input field on iPad, but do on iPhone.
    auto expectedCaretRect = CGRectMake(16, 13, UIDevice.currentDevice.userInterfaceIdiom == UIUserInterfaceIdiomPad ? 3 : 2, 15);
    auto [webView, inputDelegate] = webViewAndInputDelegateWithAutofocusedInput();
    EXPECT_WK_STREQ("INPUT", [webView stringByEvaluatingJavaScript:@"document.activeElement.tagName"]);
    [webView waitForCaretViewFrameToBecome:expectedCaretRect];

    dispatch_block_t restoreActiveFocusState = [webView _retainActiveFocusedState];
    [webView resignFirstResponder];
    restoreActiveFocusState();
    [webView waitForCaretViewFrameToBecome:CGRectZero];

    [webView becomeFirstResponder];
    [webView waitForCaretViewFrameToBecome:expectedCaretRect];
}

TEST(KeyboardInputTests, RangedSelectionRectAfterRestoringFirstResponderWithRetainActiveFocusedState)
{
    NSArray *expectedSelectionRects = @[ [NSValue valueWithCGRect:CGRectMake(16, 13, 24, 15)] ];

    auto [webView, inputDelegate] = webViewAndInputDelegateWithAutofocusedInput();
    [[webView textInputContentView] insertText:@"hello"];
    [webView selectAll:nil];
    EXPECT_WK_STREQ("INPUT", [webView stringByEvaluatingJavaScript:@"document.activeElement.tagName"]);
    [webView waitForSelectionViewRectsToBecome:expectedSelectionRects];

    dispatch_block_t restoreActiveFocusState = [webView _retainActiveFocusedState];
    [webView resignFirstResponder];
    restoreActiveFocusState();
    [webView waitForSelectionViewRectsToBecome:@[ ]];

    [webView becomeFirstResponder];
    [webView waitForSelectionViewRectsToBecome:expectedSelectionRects];
}

TEST(KeyboardInputTests, CaretSelectionRectAfterRestoringFirstResponder)
{
    // This difference in caret width is due to the fact that we don't zoom in to the input field on iPad, but do on iPhone.
    auto expectedCaretRect = CGRectMake(16, 13, UIDevice.currentDevice.userInterfaceIdiom == UIUserInterfaceIdiomPad ? 3 : 2, 15);
    auto [webView, inputDelegate] = webViewAndInputDelegateWithAutofocusedInput();
    EXPECT_WK_STREQ("INPUT", [webView stringByEvaluatingJavaScript:@"document.activeElement.tagName"]);
    [webView waitForCaretViewFrameToBecome:expectedCaretRect];

    [webView resignFirstResponder];
    [webView waitForCaretViewFrameToBecome:CGRectZero];

    [webView becomeFirstResponder];
    [webView waitForCaretViewFrameToBecome:expectedCaretRect];
}

TEST(KeyboardInputTests, RangedSelectionRectAfterRestoringFirstResponder)
{
    NSArray *expectedSelectionRects = @[ [NSValue valueWithCGRect:CGRectMake(16, 13, 24, 15)] ];

    auto [webView, inputDelegate] = webViewAndInputDelegateWithAutofocusedInput();
    [[webView textInputContentView] insertText:@"hello"];
    [webView selectAll:nil];
    EXPECT_WK_STREQ("INPUT", [webView stringByEvaluatingJavaScript:@"document.activeElement.tagName"]);
    [webView waitForSelectionViewRectsToBecome:expectedSelectionRects];

    [webView resignFirstResponder];
    [webView waitForSelectionViewRectsToBecome:@[ ]];

    [webView becomeFirstResponder];
    [webView waitForSelectionViewRectsToBecome:expectedSelectionRects];
}

TEST(KeyboardInputTests, IsSingleLineDocument)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    auto inputDelegate = adoptNS([[TestInputDelegate alloc] init]);
    [inputDelegate setFocusStartsInputSessionPolicyHandler:[&] (WKWebView *, id <_WKFocusedElementInfo>) { return _WKFocusStartsInputSessionPolicyAllow; }];
    [webView _setInputDelegate:inputDelegate.get()];

    [webView synchronouslyLoadHTMLString:@"<body><input id='first' type='text'><textarea id='second'></textarea><div id='third' contenteditable='true'></div></body>"];

    // Text field
    [webView evaluateJavaScriptAndWaitForInputSessionToChange:@"document.getElementById('first').focus()"];
    EXPECT_TRUE([webView textInputContentView].textInputTraits.isSingleLineDocument);

    // Text area
    [webView evaluateJavaScriptAndWaitForInputSessionToChange:@"document.getElementById('second').focus()"];
    EXPECT_FALSE([webView textInputContentView].textInputTraits.isSingleLineDocument);

    // Content editable
    [webView evaluateJavaScriptAndWaitForInputSessionToChange:@"document.getElementById('third').focus()"];
    EXPECT_FALSE([webView textInputContentView].textInputTraits.isSingleLineDocument);
}

TEST(KeyboardInputTests, KeyboardTypeForInput)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    auto inputDelegate = adoptNS([[TestInputDelegate alloc] init]);

    [inputDelegate setFocusStartsInputSessionPolicyHandler:[&] (WKWebView *, id <_WKFocusedElementInfo>) -> _WKFocusStartsInputSessionPolicy {
        return _WKFocusStartsInputSessionPolicyAllow;
    }];
    [webView _setInputDelegate:inputDelegate.get()];
    [webView synchronouslyLoadHTMLString:@"<input id='input'>"];
    [webView evaluateJavaScriptAndWaitForInputSessionToChange:@"input.focus()"];

    auto runTest = ^(NSString *inputType, NSString *inputMode, NSString *pattern, UIKeyboardType expectedKeyboardType) {
        [webView evaluateJavaScriptAndWaitForInputSessionToChange:@"input.blur()"];
        [webView evaluateJavaScriptAndWaitForInputSessionToChange:[NSString stringWithFormat:@"input.type = '%@'; input.inputMode = '%@'; input.pattern = '%@'; input.focus()", inputType, inputMode, pattern]];

        UIView<UITextInputPrivate> *textInput = (UIView<UITextInputPrivate> *)[webView textInputContentView];
        UIKeyboardType keyboardType = [textInput textInputTraits].keyboardType;

        bool success = keyboardType == expectedKeyboardType;
        if (!success)
            NSLog(@"Displayed %li for <input type='%@' inputmode='%@' pattern='%@'>. Expected %li.", (long)keyboardType, inputType, inputMode, pattern, (long)expectedKeyboardType);

        return success;
    };

    NSDictionary *expectedKeyboardTypeForInputType = @{
        @"text": @(UIKeyboardTypeDefault),
        @"password": @(UIKeyboardTypeDefault),
        @"search": @(UIKeyboardTypeDefault),
        @"email": @(UIKeyboardTypeEmailAddress),
        @"tel": @(UIKeyboardTypePhonePad),
        @"number": @(UIKeyboardTypeNumbersAndPunctuation),
        @"url": @(UIKeyboardTypeURL)
    };

    NSDictionary *expectedKeyboardTypeForInputMode = @{
        @"": @(-1),
        @"text": @(UIKeyboardTypeDefault),
        @"tel": @(UIKeyboardTypePhonePad),
        @"url": @(UIKeyboardTypeURL),
        @"email": @(UIKeyboardTypeEmailAddress),
        @"numeric": @(UIKeyboardTypeNumberPad),
        @"decimal": @(UIKeyboardTypeDecimalPad),
        @"search": @(UIKeyboardTypeWebSearch)
    };

    NSDictionary *expectedKeyboardTypeForPattern = @{
        @"": @(-1),
        @"\\\\d*": @(UIKeyboardTypeNumberPad),
        @"[0-9]*": @(UIKeyboardTypeNumberPad)
    };

    for (NSString *inputType in expectedKeyboardTypeForInputType) {
        BOOL isNumberOrTextInput = [inputType isEqual:@"text"] || [inputType isEqual:@"number"];
        for (NSString *inputMode in expectedKeyboardTypeForInputMode) {
            for (NSString *pattern in expectedKeyboardTypeForPattern) {
                NSNumber *keyboardType;
                if (inputMode.length) {
                    // inputmode has the highest priority.
                    keyboardType = expectedKeyboardTypeForInputMode[inputMode];
                } else {
                    // Special case for text and number inputs that have a numeric pattern. Otherwise, the input type determines the keyboard type.
                    keyboardType = pattern.length && isNumberOrTextInput ? expectedKeyboardTypeForPattern[pattern] : expectedKeyboardTypeForInputType[inputType];
                }

                EXPECT_TRUE(runTest(inputType, inputMode, pattern, (UIKeyboardType)keyboardType.intValue));
            }
        }
    }
}

TEST(KeyboardInputTests, OverrideInputViewAndInputAccessoryView)
{
    auto inputView = adoptNS([[UIView alloc] init]);
    auto inputAccessoryView = adoptNS([[UIView alloc] init]);
    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    auto webView = adoptNS([[CustomInputWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 568) configuration:configuration.get() inputView:inputView.get() inputAccessoryView:inputAccessoryView.get()]);
    auto contentView = [webView textInputContentView];

    EXPECT_EQ(inputAccessoryView.get(), [contentView inputAccessoryView]);
    EXPECT_EQ(inputView.get(), [contentView inputView]);
}

TEST(KeyboardInputTests, DisableSmartQuotesAndDashes)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    auto inputDelegate = adoptNS([[TestInputDelegate alloc] init]);
    [inputDelegate setFocusStartsInputSessionPolicyHandler:[&] (WKWebView *, id <_WKFocusedElementInfo>) -> _WKFocusStartsInputSessionPolicy {
        return _WKFocusStartsInputSessionPolicyAllow;
    }];
    [webView _setInputDelegate:inputDelegate.get()];

    auto checkSmartQuotesAndDashesType = [&] (UITextSmartDashesType dashesType, UITextSmartQuotesType quotesType) {
        UITextInputTraits *traits = [[webView textInputContentView] textInputTraits];
        EXPECT_EQ(dashesType, traits.smartDashesType);
        EXPECT_EQ(quotesType, traits.smartQuotesType);
    };

    [webView synchronouslyLoadHTMLString:@"<div id='foo' contenteditable spellcheck='false'></div><textarea id='bar' spellcheck='false'></textarea><input id='baz' spellcheck='false'>"];
    [webView evaluateJavaScriptAndWaitForInputSessionToChange:@"foo.focus()"];
    checkSmartQuotesAndDashesType(UITextSmartDashesTypeNo, UITextSmartQuotesTypeNo);
    [webView evaluateJavaScriptAndWaitForInputSessionToChange:@"bar.focus()"];
    checkSmartQuotesAndDashesType(UITextSmartDashesTypeNo, UITextSmartQuotesTypeNo);
    [webView evaluateJavaScriptAndWaitForInputSessionToChange:@"baz.focus()"];
    checkSmartQuotesAndDashesType(UITextSmartDashesTypeNo, UITextSmartQuotesTypeNo);

    [webView synchronouslyLoadHTMLString:@"<div id='foo' contenteditable></div><textarea id='bar' spellcheck='true'></textarea><input id='baz'>"];
    [webView evaluateJavaScriptAndWaitForInputSessionToChange:@"foo.focus()"];
    checkSmartQuotesAndDashesType(UITextSmartDashesTypeDefault, UITextSmartQuotesTypeDefault);
    [webView evaluateJavaScriptAndWaitForInputSessionToChange:@"bar.focus()"];
    checkSmartQuotesAndDashesType(UITextSmartDashesTypeDefault, UITextSmartQuotesTypeDefault);
    [webView evaluateJavaScriptAndWaitForInputSessionToChange:@"baz.focus()"];
    checkSmartQuotesAndDashesType(UITextSmartDashesTypeDefault, UITextSmartQuotesTypeDefault);
}

TEST(KeyboardInputTests, SelectionClipRectsWhenPresentingInputView)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    auto inputDelegate = adoptNS([[TestInputDelegate alloc] init]);
    [inputDelegate setFocusStartsInputSessionPolicyHandler:[&] (WKWebView *, id <_WKFocusedElementInfo>) -> _WKFocusStartsInputSessionPolicy {
        return _WKFocusStartsInputSessionPolicyAllow;
    }];

    CGRect selectionClipRect = CGRectNull;
    [inputDelegate setDidStartInputSessionHandler:[&] (WKWebView *, id <_WKFormInputSession>) {
        selectionClipRect = [[webView textInputContentView] _selectionClipRect];
    }];
    [webView _setInputDelegate:inputDelegate.get()];
    [webView synchronouslyLoadHTMLString:@"<meta name='viewport' content='width=device-width, initial-scale=1'><input>"];
    [webView evaluateJavaScriptAndWaitForInputSessionToChange:@"document.querySelector('input').focus()"];

    EXPECT_EQ(11, selectionClipRect.origin.x);
    EXPECT_EQ(11, selectionClipRect.origin.y);
    EXPECT_EQ(136, selectionClipRect.size.width);
    EXPECT_EQ(20, selectionClipRect.size.height);
}

TEST(KeyboardInputTests, TestWebViewAdditionalContextForStrongPasswordAssistance)
{
    NSDictionary *expected = @{ @"strongPasswordAdditionalContext" : @"testUUID" };

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    auto inputDelegate = adoptNS([[TestInputDelegate alloc] init]);

    [inputDelegate setFocusStartsInputSessionPolicyHandler:[&] (WKWebView *, id <_WKFocusedElementInfo>) -> _WKFocusStartsInputSessionPolicy {
        return _WKFocusStartsInputSessionPolicyAllow;
    }];

    [inputDelegate setWebViewAdditionalContextForStrongPasswordAssistanceHandler:[&] (WKWebView *) {
        return expected;
    }];

    [inputDelegate setFocusRequiresStrongPasswordAssistanceHandler:[&] (WKWebView *, id <_WKFocusedElementInfo>) {
        return YES;
    }];

    [webView _setInputDelegate:inputDelegate.get()];

    [webView synchronouslyLoadHTMLString:@"<input type='password' id='input'>"];
    [webView evaluateJavaScriptAndWaitForInputSessionToChange:@"document.getElementById('input').focus()"];

    NSDictionary *actual = [[webView textInputContentView] _autofillContext];
    EXPECT_TRUE([[actual allValues] containsObject:expected]);
}

TEST(KeyboardInputTests, SupportsImagePaste)
{
    auto inputDelegate = adoptNS([[TestInputDelegate alloc] init]);
    [inputDelegate setFocusStartsInputSessionPolicyHandler:[&] (WKWebView *, id <_WKFocusedElementInfo>) -> _WKFocusStartsInputSessionPolicy {
        return _WKFocusStartsInputSessionPolicyAllow;
    }];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 568)]);
    auto contentView = (id <UITextInputPrivate>)[webView textInputContentView];
    [webView synchronouslyLoadHTMLString:@"<input id='input'></input><select id='select'></select><div contenteditable id='editor'></div><textarea id='textarea'></textarea>"];
    [webView _setInputDelegate:inputDelegate.get()];

    [webView stringByEvaluatingJavaScript:@"input.focus()"];
    EXPECT_TRUE(contentView.supportsImagePaste);

    [webView stringByEvaluatingJavaScript:@"document.activeElement.blur()"];
    [webView waitForNextPresentationUpdate];
    [webView stringByEvaluatingJavaScript:@"select.focus()"];
    EXPECT_FALSE(contentView.supportsImagePaste);

    [webView stringByEvaluatingJavaScript:@"editor.focus()"];
    EXPECT_TRUE(contentView.supportsImagePaste);

    [webView stringByEvaluatingJavaScript:@"document.activeElement.blur(); input.type = 'color'"];
    [webView waitForNextPresentationUpdate];
    [webView stringByEvaluatingJavaScript:@"input.focus()"];
    EXPECT_FALSE(contentView.supportsImagePaste);

    [webView stringByEvaluatingJavaScript:@"textarea.focus()"];
    EXPECT_TRUE(contentView.supportsImagePaste);
}

TEST(KeyboardInputTests, SuppressSoftwareKeyboard)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    [webView _setSuppressSoftwareKeyboard:YES];
    [[webView window] makeKeyWindow];

    auto inputDelegate = adoptNS([[TestInputDelegate alloc] init]);
    [inputDelegate setFocusStartsInputSessionPolicyHandler:[&](WKWebView *, id <_WKFocusedElementInfo>) {
        return _WKFocusStartsInputSessionPolicyAllow;
    }];

    [webView _setInputDelegate:inputDelegate.get()];
    [webView synchronouslyLoadHTMLString:@"<body contenteditable>"];
    [webView evaluateJavaScriptAndWaitForInputSessionToChange:@"document.body.focus()"];
    EXPECT_TRUE(UIKeyboardImpl.sharedInstance._shouldSuppressSoftwareKeyboard);
}

static BOOL shouldSimulateKeyboardInputOnTextInsertionOverride(id, SEL)
{
    return YES;
}

TEST(KeyboardInputTests, InsertTextSimulatingKeyboardInput)
{
    InstanceMethodSwizzler overrideShouldSimulateKeyboardInputOnTextInsertion { NSClassFromString(@"WKContentView"), @selector(_shouldSimulateKeyboardInputOnTextInsertion), reinterpret_cast<IMP>(shouldSimulateKeyboardInputOnTextInsertionOverride) };

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    auto inputDelegate = adoptNS([[TestInputDelegate alloc] init]);
    [inputDelegate setFocusStartsInputSessionPolicyHandler:[&](WKWebView *, id <_WKFocusedElementInfo>) { return _WKFocusStartsInputSessionPolicyAllow; }];
    [webView _setInputDelegate:inputDelegate.get()];

    RetainPtr<NSURL> testURL = [[NSBundle mainBundle] URLForResource:@"insert-text" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"];
    [webView synchronouslyLoadRequest:[NSURLRequest requestWithURL:testURL.get()]];
    [webView evaluateJavaScriptAndWaitForInputSessionToChange:@"document.body.focus()"];
    [[webView textInputContentView] insertText:@"hello"];
    EXPECT_NS_EQUAL((@[@"keydown", @"beforeinput", @"input", @"keyup", @"change"]), [webView objectByEvaluatingJavaScript:@"firedEvents"]);
}

TEST(KeyboardInputTests, InsertDictationAlternativesSimulatingKeyboardInput)
{
    InstanceMethodSwizzler overrideShouldSimulateKeyboardInputOnTextInsertion { NSClassFromString(@"WKContentView"), @selector(_shouldSimulateKeyboardInputOnTextInsertion), reinterpret_cast<IMP>(shouldSimulateKeyboardInputOnTextInsertionOverride) };

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    auto inputDelegate = adoptNS([[TestInputDelegate alloc] init]);
    [inputDelegate setFocusStartsInputSessionPolicyHandler:[&](WKWebView *, id <_WKFocusedElementInfo>) { return _WKFocusStartsInputSessionPolicyAllow; }];
    [webView _setInputDelegate:inputDelegate.get()];

    RetainPtr<NSURL> testURL = [[NSBundle mainBundle] URLForResource:@"insert-text" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"];
    [webView synchronouslyLoadRequest:[NSURLRequest requestWithURL:testURL.get()]];
    [webView evaluateJavaScriptAndWaitForInputSessionToChange:@"document.body.focus()"];
    [[webView textInputContentView] insertText:@"hello" alternatives:@[ @"helo" ] style:UITextAlternativeStyleNone];
    EXPECT_NS_EQUAL((@[@"keydown", @"beforeinput", @"input", @"keyup", @"change"]), [webView objectByEvaluatingJavaScript:@"firedEvents"]);
}

} // namespace TestWebKitAPI

#endif // PLATFORM(IOS_FAMILY)
