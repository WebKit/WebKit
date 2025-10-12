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

#import "CGImagePixelReader.h"
#import "ClassMethodSwizzler.h"
#import "HTTPServer.h"
#import "InstanceMethodSwizzler.h"
#import "PlatformUtilities.h"
#import "TestCocoa.h"
#import "TestInputDelegate.h"
#import "TestNavigationDelegate.h"
#import "TestProtocol.h"
#import "TestUIDelegate.h"
#import "TestWKWebView.h"
#import "UIKitSPIForTesting.h"
#import "UserInterfaceSwizzler.h"
#import "WKWebViewConfigurationExtras.h"
#import <WebCore/Color.h>
#import <WebKit/WKFrameInfoPrivate.h>
#import <WebKit/WKProcessPoolPrivate.h>
#import <WebKit/WKWebViewConfigurationPrivate.h>
#import <WebKit/WKWebViewPrivate.h>
#import <WebKit/WKWebViewPrivateForTesting.h>
#import <WebKit/WKWebViewPrivateForTestingIOS.h>
#import <WebKit/_WKProcessPoolConfiguration.h>
#import <WebKitLegacy/WebEvent.h>
#import <cmath>
#import <pal/spi/cocoa/NSAttributedStringSPI.h>
#import <wtf/darwin/DispatchExtras.h>

#import <pal/cocoa/CoreTelephonySoftLink.h>

namespace TestWebKitAPI {

enum class CaretVisibility : bool { Hidden, Visible };

}

@interface WKContentView ()
@property (nonatomic, readonly) NSUndoManager *undoManagerForWebView;
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
    return [UIImage imageNamed:@"TestWebKitAPIResources.bundle/icon.png"];
}

+ (UIBarButtonItemGroup *)leadingItemsForWebView:(WKWebView *)webView
{
    static dispatch_once_t onceToken;
    static RetainPtr<UIBarButtonItemGroup> sharedItems;
    dispatch_once(&onceToken, ^{
        auto leadingItem = adoptNS([[UIBarButtonItem alloc] initWithImage:self.barButtonIcon style:UIBarButtonItemStylePlain target:webView action:@selector(fakeLeadingBarButtonItemAction)]);
        sharedItems = adoptNS([[UIBarButtonItemGroup alloc] initWithBarButtonItems:@[ leadingItem.get() ] representativeItem:nil]);
    });
    return sharedItems.get();
}

+ (UIBarButtonItemGroup *)trailingItemsForWebView:(WKWebView *)webView
{
    static dispatch_once_t onceToken;
    static RetainPtr<UIBarButtonItemGroup> sharedItems;
    dispatch_once(&onceToken, ^{
        auto trailingItem = adoptNS([[UIBarButtonItem alloc] initWithImage:self.barButtonIcon style:UIBarButtonItemStylePlain target:webView action:@selector(fakeTrailingBarButtonItemAction)]);
        sharedItems = adoptNS([[UIBarButtonItemGroup alloc] initWithBarButtonItems:@[ trailingItem.get() ] representativeItem:nil]);
    });
    return sharedItems.get();
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

- (void)waitForCaretVisibility:(TestWebKitAPI::CaretVisibility)visibility
{
    BOOL hasEmittedWarning = NO;
    NSTimeInterval secondsToWaitUntilWarning = 2;
    NSTimeInterval startTime = [NSDate timeIntervalSinceReferenceDate];
    while ([[NSRunLoop currentRunLoop] runMode:NSDefaultRunLoopMode beforeDate:[NSDate distantPast]]) {
        BOOL sizeIsEmpty = CGSizeEqualToSize(self.caretViewRectInContentCoordinates.size, CGSizeZero);
        if ((visibility == TestWebKitAPI::CaretVisibility::Hidden) == sizeIsEmpty)
            break;

        if (hasEmittedWarning || startTime + secondsToWaitUntilWarning >= [NSDate timeIntervalSinceReferenceDate])
            continue;

        NSLog(@"Expected the caret to %s", visibility == TestWebKitAPI::CaretVisibility::Hidden ? "disappear" : "appear");
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

@interface CustomTextInputTraitsWebView : TestWKWebView
- (instancetype)initWithFrame:(CGRect)frame keyboardType:(UIKeyboardType)keyboardType;
@end

@implementation CustomTextInputTraitsWebView {
    UIKeyboardType _keyboardType;
}

- (instancetype)initWithFrame:(CGRect)frame keyboardType:(UIKeyboardType)keyboardType
{
    if (self = [super initWithFrame:frame])
        _keyboardType = keyboardType;

    return self;
}

- (UITextInputTraits *)_textInputTraits
{
    UITextInputTraits *traits = [super _textInputTraits];
    traits.keyboardType = _keyboardType;
    return traits;
}

@end

@interface CustomUndoManagerWebView : TestWKWebView
@property (nonatomic, strong) NSUndoManager *customUndoManager;
@end

@implementation CustomUndoManagerWebView

- (NSUndoManager *)undoManager
{
    return _customUndoManager ?: super.undoManager;
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

    auto firstWebEvent = adoptNS([[WebEvent alloc] initWithKeyEventType:WebEventKeyDown timeStamp:CFAbsoluteTimeGetCurrent() characters:@"a" charactersIgnoringModifiers:@"a" modifiers:0 isRepeating:NO withFlags:0 withInputManagerHint:nil keyCode:0 isTabKey:NO]);
    auto secondWebEvent = adoptNS([[WebEvent alloc] initWithKeyEventType:WebEventKeyUp timeStamp:CFAbsoluteTimeGetCurrent() characters:@"a" charactersIgnoringModifiers:@"a" modifiers:0 isRepeating:NO withFlags:0 withInputManagerHint:nil keyCode:0 isTabKey:NO]);
    [webView handleKeyEvent:firstWebEvent.get() completion:[&](WebEvent *event, BOOL) {
        EXPECT_TRUE([event isEqual:firstWebEvent.get()]);
        [webView handleKeyEvent:secondWebEvent.get() completion:[&] (WebEvent *event, BOOL) {
            EXPECT_TRUE([event isEqual:secondWebEvent.get()]);
            [(id<UITextInput>)[webView firstResponder] insertText:@"a"];
            doneWaiting = true;
        }];
    }];

    TestWebKitAPI::Util::run(&doneWaiting);
    EXPECT_WK_STREQ("a", [webView stringByEvaluatingJavaScript:@"document.querySelector('input').value"]);
}

TEST(KeyboardInputTests, ResigningFirstResponderCancelsKeyEvents)
{
    auto [webView, inputDelegate] = webViewAndInputDelegateWithAutofocusedInput();
    auto keyDownEvent = adoptNS([[WebEvent alloc] initWithKeyEventType:WebEventKeyDown timeStamp:CFAbsoluteTimeGetCurrent() characters:@"a" charactersIgnoringModifiers:@"a" modifiers:0 isRepeating:NO withFlags:0 withInputManagerHint:nil keyCode:0 isTabKey:NO]);

    [webView becomeFirstResponder];
    [webView evaluateJavaScript:@"while(1);" completionHandler:nil];

    bool doneWaiting = false;
    [webView handleKeyEvent:keyDownEvent.get() completion:[&] (WebEvent *event, BOOL handled) {
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
    auto keyDownEvent = adoptNS([[WebEvent alloc] initWithKeyEventType:WebEventKeyDown timeStamp:CFAbsoluteTimeGetCurrent() characters:@"a" charactersIgnoringModifiers:@"a" modifiers:0 isRepeating:NO withFlags:0 withInputManagerHint:nil keyCode:0 isTabKey:NO]);

    [webView becomeFirstResponder];
    [webView synchronouslyLoadHTMLString:@"<body></body>"];
    [webView evaluateJavaScript:@"start = Date.now(); while(Date.now() - start < 500);" completionHandler:nil];

    bool doneWaiting = false;
    [webView handleKeyEvent:keyDownEvent.get() completion:[&] (WebEvent *event, BOOL handled) {
        EXPECT_TRUE([event isEqual:keyDownEvent.get()]);
        EXPECT_FALSE(handled);
        doneWaiting = true;
    }];

    TestWebKitAPI::Util::run(&doneWaiting);
}

TEST(KeyboardInputTests, HandleKeyEventsInCrashedOrUninitializedWebProcess)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    {
        auto keyDownEvent = adoptNS([[WebEvent alloc] initWithKeyEventType:WebEventKeyDown timeStamp:CFAbsoluteTimeGetCurrent() characters:@"a" charactersIgnoringModifiers:@"a" modifiers:0 isRepeating:NO withFlags:0 withInputManagerHint:nil keyCode:65 isTabKey:NO]);
        bool doneWaiting = false;
        [webView synchronouslyLoadHTMLString:@"<body></body>"];
        [webView evaluateJavaScript:@"while (1);" completionHandler:nil];
        [webView handleKeyEvent:keyDownEvent.get() completion:[&](WebEvent *event, BOOL handled) {
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
        [webView handleKeyEvent:keyUpEvent.get() completion:[&](WebEvent *event, BOOL handled) {
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
    dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(0.01 * NSEC_PER_SEC)), mainDispatchQueueSingleton(), [keyEvent, webView, &done] {
        [webView handleKeyEvent:keyEvent.get() completion:[keyEvent, &done](WebEvent *event, BOOL handled) {
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
    auto [webView, inputDelegate] = webViewAndInputDelegateWithAutofocusedInput();
    EXPECT_WK_STREQ("INPUT", [webView stringByEvaluatingJavaScript:@"document.activeElement.tagName"]);
    [webView waitForCaretVisibility:CaretVisibility::Visible];

    dispatch_block_t restoreActiveFocusState = [webView _retainActiveFocusedState];
    [webView resignFirstResponder];
    restoreActiveFocusState();
    [webView waitForCaretVisibility:CaretVisibility::Hidden];

    [webView becomeFirstResponder];
    [webView waitForCaretVisibility:CaretVisibility::Visible];
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
    auto [webView, inputDelegate] = webViewAndInputDelegateWithAutofocusedInput();
    EXPECT_WK_STREQ("INPUT", [webView stringByEvaluatingJavaScript:@"document.activeElement.tagName"]);
    [webView waitForCaretVisibility:CaretVisibility::Visible];

    [webView resignFirstResponder];
    [webView waitForCaretVisibility:CaretVisibility::Hidden];

    [webView becomeFirstResponder];
    [webView waitForCaretVisibility:CaretVisibility::Visible];
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
    EXPECT_TRUE([webView effectiveTextInputTraits].isSingleLineDocument);

    // Text area
    [webView evaluateJavaScriptAndWaitForInputSessionToChange:@"document.getElementById('second').focus()"];
    EXPECT_FALSE([webView effectiveTextInputTraits].isSingleLineDocument);

    // Content editable
    [webView evaluateJavaScriptAndWaitForInputSessionToChange:@"document.getElementById('third').focus()"];
    EXPECT_FALSE([webView effectiveTextInputTraits].isSingleLineDocument);
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

        UIKeyboardType keyboardType = [webView effectiveTextInputTraits].keyboardType;

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

TEST(KeyboardInputTests, OverrideTextInputTraits)
{
    UIKeyboardType keyboardType = UIKeyboardTypeNumberPad;

    auto webView = adoptNS([[CustomTextInputTraitsWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500) keyboardType:keyboardType]);
    auto inputDelegate = adoptNS([[TestInputDelegate alloc] init]);
    [inputDelegate setFocusStartsInputSessionPolicyHandler:[&] (WKWebView *, id<_WKFocusedElementInfo>) -> _WKFocusStartsInputSessionPolicy {
        return _WKFocusStartsInputSessionPolicyAllow;
    }];
    [webView _setInputDelegate:inputDelegate.get()];

    [webView synchronouslyLoadHTMLString:@"<body><div id='editor' contenteditable='true'></div></body>"];

    [webView evaluateJavaScriptAndWaitForInputSessionToChange:@"document.getElementById('editor').focus()"];
    EXPECT_EQ(keyboardType, [webView effectiveTextInputTraits].keyboardType);
}

TEST(KeyboardInputTests, ClearSelectedTextRange)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    auto inputDelegate = adoptNS([[TestInputDelegate alloc] init]);
    [inputDelegate setFocusStartsInputSessionPolicyHandler:[&] (WKWebView *, id<_WKFocusedElementInfo>) -> _WKFocusStartsInputSessionPolicy {
        return _WKFocusStartsInputSessionPolicyAllow;
    }];

    [webView _setInputDelegate:inputDelegate.get()];
    [webView synchronouslyLoadHTMLString:@"<input id='textField' value='hello' />"];
    [webView evaluateJavaScriptAndWaitForInputSessionToChange:@"textField.focus()"];
    [webView objectByEvaluatingJavaScript:@"textField.select()"];
    auto selectedTextBeforeClearing = [webView selectedText];
    EXPECT_WK_STREQ(selectedTextBeforeClearing, "hello");

    [webView textInputContentView].selectedTextRange = nil;
    auto selectedTextAfterClearing = [webView selectedText];
    EXPECT_WK_STREQ(selectedTextAfterClearing, "");
}

TEST(KeyboardInputTests, DisableSpellChecking)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    auto inputDelegate = adoptNS([[TestInputDelegate alloc] init]);
    [inputDelegate setFocusStartsInputSessionPolicyHandler:[&] (WKWebView *, id <_WKFocusedElementInfo>) -> _WKFocusStartsInputSessionPolicy {
        return _WKFocusStartsInputSessionPolicyAllow;
    }];
    [webView _setInputDelegate:inputDelegate.get()];

    auto checkSmartQuotesAndDashesType = [&] (UITextSmartDashesType dashesType, UITextSmartQuotesType quotesType, UITextSpellCheckingType spellCheckingType) {
        auto traits = [webView effectiveTextInputTraits];
        EXPECT_EQ(dashesType, traits.smartDashesType);
        EXPECT_EQ(quotesType, traits.smartQuotesType);
        EXPECT_EQ(spellCheckingType, traits.spellCheckingType);
    };

    [webView synchronouslyLoadHTMLString:@"<div id='foo' contenteditable spellcheck='false'></div><textarea id='bar' spellcheck='false'></textarea><input id='baz' spellcheck='false'>"];
    [webView evaluateJavaScriptAndWaitForInputSessionToChange:@"foo.focus()"];
    checkSmartQuotesAndDashesType(UITextSmartDashesTypeNo, UITextSmartQuotesTypeNo, UITextSpellCheckingTypeNo);
    [webView evaluateJavaScriptAndWaitForInputSessionToChange:@"bar.focus()"];
    checkSmartQuotesAndDashesType(UITextSmartDashesTypeNo, UITextSmartQuotesTypeNo, UITextSpellCheckingTypeNo);
    [webView evaluateJavaScriptAndWaitForInputSessionToChange:@"baz.focus()"];
    checkSmartQuotesAndDashesType(UITextSmartDashesTypeNo, UITextSmartQuotesTypeNo, UITextSpellCheckingTypeNo);

    [webView synchronouslyLoadHTMLString:@"<div id='foo' contenteditable></div><textarea id='bar' spellcheck='true'></textarea><input id='baz'>"];
    [webView evaluateJavaScriptAndWaitForInputSessionToChange:@"foo.focus()"];
    checkSmartQuotesAndDashesType(UITextSmartDashesTypeDefault, UITextSmartQuotesTypeDefault, UITextSpellCheckingTypeDefault);
    [webView evaluateJavaScriptAndWaitForInputSessionToChange:@"bar.focus()"];
    checkSmartQuotesAndDashesType(UITextSmartDashesTypeDefault, UITextSmartQuotesTypeDefault, UITextSpellCheckingTypeDefault);
    [webView evaluateJavaScriptAndWaitForInputSessionToChange:@"baz.focus()"];
    checkSmartQuotesAndDashesType(UITextSmartDashesTypeDefault, UITextSmartQuotesTypeDefault, UITextSpellCheckingTypeDefault);
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
        selectionClipRect = [webView selectionClipRect];
    }];
    [webView _setInputDelegate:inputDelegate.get()];
    [webView synchronouslyLoadHTMLString:@"<meta name='viewport' content='width=device-width, initial-scale=1'><input>"];
    [webView evaluateJavaScriptAndWaitForInputSessionToChange:@"document.querySelector('input').focus()"];

    EXPECT_EQ(9, selectionClipRect.origin.x);
    EXPECT_EQ(9, selectionClipRect.origin.y);
    EXPECT_EQ(153, selectionClipRect.size.width);
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

    bool verifiedFrame { false };
    [inputDelegate setFocusRequiresStrongPasswordAssistanceHandler:[&] (WKWebView *, id<_WKFocusedElementInfo> info, void(^completionHandler)(BOOL)) {
        EXPECT_NOT_NULL(info.frame);
        verifiedFrame = true;
        completionHandler(YES);
    }];

    [webView _setInputDelegate:inputDelegate.get()];

    [webView synchronouslyLoadHTMLString:@"<input type='password' id='input'>"];
    [webView evaluateJavaScriptAndWaitForInputSessionToChange:@"document.getElementById('input').focus()"];

    NSDictionary *actual = [[webView textInputContentView] _autofillContext];
    EXPECT_TRUE([[actual allValues] containsObject:expected]);
    EXPECT_TRUE([actual[@"_automaticPasswordKeyboard"] boolValue]);
    EXPECT_TRUE(verifiedFrame);
}

TEST(KeyboardInputTests, TestWebViewAdditionalContextForNonAutofillCredentialType)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    auto inputDelegate = adoptNS([[TestInputDelegate alloc] init]);

    [inputDelegate setFocusStartsInputSessionPolicyHandler:[&] (WKWebView *, id<_WKFocusedElementInfo>) -> _WKFocusStartsInputSessionPolicy {
        return _WKFocusStartsInputSessionPolicyAllow;
    }];

    [inputDelegate setFocusRequiresStrongPasswordAssistanceHandler:^(WKWebView *, id<_WKFocusedElementInfo>, void(^completionHandler)(BOOL)) {
        completionHandler(YES);
    }];

    [webView _setInputDelegate:inputDelegate.get()];

    [webView synchronouslyLoadHTMLString:@"<input type='text' id='input' autocomplete='username webauthn'>"];
    [webView evaluateJavaScriptAndWaitForInputSessionToChange:@"document.getElementById('input').focus()"];

    NSDictionary *actual = [[webView textInputContentView] _autofillContext];
    EXPECT_TRUE([actual[@"_page_id"] boolValue]);
    EXPECT_TRUE([actual[@"_frame_id"] boolValue]);
    EXPECT_WK_STREQ("webauthn", actual[@"_credential_type"]);
}

TEST(KeyboardInputTests, AsyncFocusRequiresStrongPasswordAssistanceAfterBlurNoStartSession)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    auto inputDelegate = adoptNS([[TestInputDelegate alloc] init]);

    [inputDelegate setFocusStartsInputSessionPolicyHandler:[&] (WKWebView *, id<_WKFocusedElementInfo>) -> _WKFocusStartsInputSessionPolicy {
        return _WKFocusStartsInputSessionPolicyAllow;
    }];

    __block bool calledCompletionHandler { false };
    [inputDelegate setFocusRequiresStrongPasswordAssistanceHandler:^(WKWebView *webView, id<_WKFocusedElementInfo>, void(^completionHandler)(BOOL)) {
        [webView evaluateJavaScript:@"document.getElementById('input').blur()" completionHandler:^(id, NSError *) {
            [webView evaluateJavaScript:@"let didAnotherRountTrip = true" completionHandler:^(id, NSError *) {
                completionHandler(YES);
                calledCompletionHandler = true;
            }];
        }];
    }];

    [inputDelegate setDidStartInputSessionHandler:^(WKWebView *, id<_WKFormInputSession>) {
        EXPECT_FALSE(true);
    }];

    [webView _setInputDelegate:inputDelegate.get()];

    [webView synchronouslyLoadHTMLString:@"<input type='text' id='input' autocomplete='username webauthn'>"];
    [webView evaluateJavaScript:@"document.getElementById('input').focus()" completionHandler:nil];

    Util::run(&calledCompletionHandler);
    Util::runFor(0.1_s);
}

TEST(KeyboardInputTests, TestWebViewAccessoryDoneDuringStrongPasswordAssistance)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    auto inputDelegate = adoptNS([[TestInputDelegate alloc] init]);

    [inputDelegate setFocusStartsInputSessionPolicyHandler:[&] (WKWebView *, id<_WKFocusedElementInfo>) -> _WKFocusStartsInputSessionPolicy {
        return _WKFocusStartsInputSessionPolicyAllow;
    }];

    [inputDelegate setFocusRequiresStrongPasswordAssistanceHandler:[&] (WKWebView *, id<_WKFocusedElementInfo>, void(^completionHandler)(BOOL)) {
        completionHandler(YES);
    }];

    [webView _setInputDelegate:inputDelegate.get()];

    [webView synchronouslyLoadHTMLString:@"<input type='password' id='input'>"];
    [webView evaluateJavaScriptAndWaitForInputSessionToChange:@"document.getElementById('input').focus()"];
    EXPECT_WK_STREQ("INPUT", [webView stringByEvaluatingJavaScript:@"document.activeElement.tagName"]);
    [webView dismissFormAccessoryView];
    EXPECT_WK_STREQ("BODY", [webView stringByEvaluatingJavaScript:@"document.activeElement.tagName"]);
    EXPECT_TRUE([webView _contentViewIsFirstResponder]);
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

    RetainPtr<NSURL> testURL = [NSBundle.test_resourcesBundle URLForResource:@"insert-text" withExtension:@"html"];
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

    RetainPtr<NSURL> testURL = [NSBundle.test_resourcesBundle URLForResource:@"insert-text" withExtension:@"html"];
    [webView synchronouslyLoadRequest:[NSURLRequest requestWithURL:testURL.get()]];
    [webView evaluateJavaScriptAndWaitForInputSessionToChange:@"document.body.focus()"];
    [[webView textInputContentView] insertText:@"hello" alternatives:@[ @"helo" ] style:UITextAlternativeStyleNone];
    EXPECT_NS_EQUAL((@[@"keydown", @"beforeinput", @"input", @"keyup", @"change"]), [webView objectByEvaluatingJavaScript:@"firedEvents"]);
}

TEST(KeyboardInputTests, OverrideUndoManager)
{
    auto webView = adoptNS([[CustomUndoManagerWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    auto contentView = [webView wkContentView];
    EXPECT_EQ(contentView.undoManager, contentView.undoManagerForWebView);

    auto undoManager = adoptNS([[NSUndoManager alloc] init]);
    [webView setCustomUndoManager:undoManager.get()];
    EXPECT_EQ(contentView.undoManager, undoManager);
}

TEST(KeyboardInputTests, DoNotRegisterActionsInOverriddenUndoManager)
{
    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [configuration _setUndoManagerAPIEnabled:YES];

    auto webView = adoptNS([[CustomUndoManagerWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500) configuration:configuration.get()]);
    auto contentView = [webView wkContentView];
    EXPECT_FALSE([contentView.undoManagerForWebView canUndo]);

    auto overrideUndoManager = adoptNS([[NSUndoManager alloc] init]);
    [webView setCustomUndoManager:overrideUndoManager.get()];

    __block bool doneWaiting = false;
    [webView synchronouslyLoadHTMLString:@"<body></body>"];
    [webView evaluateJavaScript:@"document.undoManager.addItem(new UndoItem({ label: '', undo: () => debug(\"Performed undo.\"), redo: () => debug(\"Performed redo.\") }))" completionHandler:^(id, NSError *) {
        doneWaiting = true;
    }];

    TestWebKitAPI::Util::run(&doneWaiting);
    EXPECT_TRUE([contentView.undoManagerForWebView canUndo]);
    EXPECT_FALSE([overrideUndoManager canUndo]);
}

TEST(KeyboardInputTests, NewUndoGroupClosesPreviousTypingCommand)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 400, 400)]);
    auto inputDelegate = adoptNS([TestInputDelegate new]);
    [inputDelegate setFocusStartsInputSessionPolicyHandler:[](WKWebView *, id<_WKFocusedElementInfo>) {
        return _WKFocusStartsInputSessionPolicyAllow;
    }];
    __block bool didStartInputSession = false;
    [inputDelegate setDidStartInputSessionHandler:^(WKWebView *, id<_WKFormInputSession>) {
        didStartInputSession = true;
    }];
    [webView _setInputDelegate:inputDelegate.get()];
    [webView synchronouslyLoadHTMLString:@"<body contenteditable>"];
    [webView objectByEvaluatingJavaScript:@"document.body.focus()"];
    Util::run(&didStartInputSession);

    RetainPtr contentView = [webView textInputContentView];
    auto insertText = ^(NSString *text) {
        [contentView insertText:text];
        [webView waitForNextPresentationUpdate];
    };

    insertText(@"Foo");

    [[contentView undoManager] beginUndoGrouping];
    [webView waitForNextPresentationUpdate];

    insertText(@" ");
    insertText(@"bar");

    [[contentView undoManager] endUndoGrouping];
    [webView waitForNextPresentationUpdate];
    EXPECT_WK_STREQ("Foo bar", [webView contentsAsString]);

    [[contentView undoManager] undo];
    [webView waitForNextPresentationUpdate];
    EXPECT_WK_STREQ("Foo", [webView contentsAsString]);

    [[contentView undoManager] undo];
    [webView waitForNextPresentationUpdate];
    EXPECT_WK_STREQ("", [webView contentsAsString]);
}

static UIView * nilResizableSnapshotViewFromRect(id, SEL, CGRect, BOOL, UIEdgeInsets)
{
    return nil;
}

TEST(KeyboardInputTests, DoNotCrashWhenFocusingSelectWithoutViewSnapshot)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    auto delegate = adoptNS([TestInputDelegate new]);
    [webView _setInputDelegate:delegate.get()];
    [delegate setFocusStartsInputSessionPolicyHandler:[](WKWebView *, id <_WKFocusedElementInfo>) {
        return _WKFocusStartsInputSessionPolicyAllow;
    }];

    [webView synchronouslyLoadHTMLString:@"<select id='select'><option>foo</option><option>bar</option></select>"];

    InstanceMethodSwizzler swizzler { UIView.class, @selector(resizableSnapshotViewFromRect:afterScreenUpdates:withCapInsets:), reinterpret_cast<IMP>(nilResizableSnapshotViewFromRect) };
    [webView stringByEvaluatingJavaScript:@"select.focus()"];
    [webView waitForNextPresentationUpdate];
}

TEST(KeyboardInputTests, EditableWebViewRequiresKeyboardWhenFirstResponder)
{
    auto returnNo = imp_implementationWithBlock(^{
        return NO;
    });

    InstanceMethodSwizzler hardwareKeyboardAttachedSwizzler { UIKeyboardImpl.class, @selector(hardwareKeyboardAttached), returnNo };
    ClassMethodSwizzler hardwareKeyboardModeSwizzler { UIKeyboard.class, @selector(isInHardwareKeyboardMode), returnNo };

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    auto delegate = adoptNS([TestInputDelegate new]);
    [webView _setInputDelegate:delegate.get()];
    [delegate setFocusStartsInputSessionPolicyHandler:[](WKWebView *, id <_WKFocusedElementInfo>) {
        return _WKFocusStartsInputSessionPolicyAllow;
    }];

    auto contentView = [webView textInputContentView];
    [webView synchronouslyLoadHTMLString:@"<input value='foo' readonly>"];
    EXPECT_FALSE([contentView _requiresKeyboardWhenFirstResponder]);

    [webView _setEditable:YES];
    [webView waitForNextPresentationUpdate];
    EXPECT_TRUE([contentView _requiresKeyboardWhenFirstResponder]);

    [webView stringByEvaluatingJavaScript:@"document.querySelector('input').focus()"];
    [webView waitForNextPresentationUpdate];
    EXPECT_FALSE([contentView _requiresKeyboardWhenFirstResponder]);
}

TEST(KeyboardInputTests, InputSessionWhenEvaluatingJavaScript)
{
    __block bool done = false;
    auto inputDelegate = adoptNS([[TestInputDelegate alloc] init]);
    bool willStartInputSession = false;
    [inputDelegate setWillStartInputSessionHandler:[&] (WKWebView *, id<_WKFormInputSession>) {
        willStartInputSession = true;
    }];
    bool didStartInputSession = false;
    [inputDelegate setDidStartInputSessionHandler:[&] (WKWebView *, id<_WKFormInputSession>) {
        didStartInputSession = true;
    }];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    [webView _setInputDelegate:inputDelegate.get()];
    [webView synchronouslyLoadHTMLString:@"<input value='foo'>"];

    NSString *focusInputScript = @"document.querySelector('input').focus()";

    done = false;
    [webView _evaluateJavaScriptWithoutUserGesture:focusInputScript completionHandler:^(id, NSError *error) {
        EXPECT_NULL(error);
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
    [webView waitForNextPresentationUpdate];
    EXPECT_FALSE(willStartInputSession);
    EXPECT_FALSE(didStartInputSession);

    done = false;
    [webView callAsyncJavaScript:focusInputScript arguments:nil inFrame:nil inContentWorld:WKContentWorld.pageWorld completionHandler:^(id, NSError *error) {
        EXPECT_NULL(error);
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
    [webView waitForNextPresentationUpdate];
    EXPECT_TRUE(willStartInputSession);
    EXPECT_TRUE(didStartInputSession);

    [webView objectByEvaluatingJavaScript:@"document.activeElement.blur()"];
    [webView waitForNextPresentationUpdate];

    willStartInputSession = false;
    didStartInputSession = false;
    done = false;
    [webView evaluateJavaScript:focusInputScript completionHandler:^(id, NSError *error) {
        EXPECT_NULL(error);
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
    [webView waitForNextPresentationUpdate];
    EXPECT_TRUE(willStartInputSession);
    EXPECT_TRUE(didStartInputSession);
}

TEST(KeyboardInputTests, NoCrashWhenDiscardingMarkedText)
{
    auto processPoolConfiguration = adoptNS([[_WKProcessPoolConfiguration alloc] init]);
    [processPoolConfiguration setProcessSwapsOnNavigation:YES];

    auto processPool = adoptNS([[WKProcessPool alloc] _initWithConfiguration:processPoolConfiguration.get()]);
    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [configuration setProcessPool:processPool.get()];

    auto navigationDelegate = adoptNS([[TestNavigationDelegate alloc] init]);
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 800, 600) configuration:configuration.get()]);
    [webView setNavigationDelegate:navigationDelegate.get()];
    [webView _setEditable:YES];

    auto navigateAndSetMarkedText = [&](const String& urlString) {
        RetainPtr request = adoptNS([[NSURLRequest alloc] initWithURL:adoptNS([[NSURL alloc] initWithString:urlString.createNSString().get()]).get()]);
        [webView loadSimulatedRequest:request.get() responseHTMLString:@"<body>Hello world</body>"];
        [navigationDelegate waitForDidFinishNavigation];
        [webView selectAll:nil];
        [[webView textInputContentView] setMarkedText:@"Hello" selectedRange:NSMakeRange(0, 5)];
        [webView waitForNextPresentationUpdate];
    };

    navigateAndSetMarkedText("https://foo.com"_s);
    navigateAndSetMarkedText("https://bar.com"_s);
    navigateAndSetMarkedText("https://baz.com"_s);
    navigateAndSetMarkedText("https://foo.com"_s);
    [webView _close];

    Util::runFor(100_ms);
}

TEST(KeyboardInputTests, NoCrashWithEmptyAttributedMarkedText)
{
    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    [webView _setEditable:YES];
    [webView synchronouslyLoadHTMLString:@"<meta name='viewport' content='width=device-width'><meta charset='utf-8'><body></body>"];
    [webView selectAll:nil];

    RetainPtr attributes = @{
        NSMarkedClauseSegmentAttributeName: @(0),
        NSUnderlineStyleAttributeName: @(NSUnderlineStyleSingle),
        NSUnderlineColorAttributeName: UIColor.tintColor
    };

    RetainPtr composition = adoptNS([[NSAttributedString alloc] initWithString:@"あ" attributes:attributes.get()]);
    [[webView textInputContentView] setAttributedMarkedText:composition.get() selectedRange:NSMakeRange(0, 1)];

    RetainPtr finalComposition = adoptNS([[NSMutableAttributedString alloc] initWithString:@"あs" attributes:attributes.get()]);
    [[webView textInputContentView] setAttributedMarkedText:finalComposition.get() selectedRange:NSMakeRange(0, 2)];

    [finalComposition setAttributes:nil range:NSMakeRange(0, 2)];
    [finalComposition replaceCharactersInRange:NSMakeRange(0, 2) withString:@"明日"];

    [[webView textInputContentView] setAttributedMarkedText:finalComposition.get() selectedRange:NSMakeRange(0, 2)];

    [finalComposition deleteCharactersInRange:NSMakeRange(0, 2)];
    [[webView textInputContentView] setAttributedMarkedText:finalComposition.get() selectedRange:NSMakeRange(0, 0)];
}

TEST(KeyboardInputTests, CharactersAroundCaretSelection)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    auto delegate = adoptNS([TestInputDelegate new]);
    [webView _setInputDelegate:delegate.get()];

    bool focused = false;
    [delegate setFocusStartsInputSessionPolicyHandler:[&](WKWebView *, id<_WKFocusedElementInfo>) {
        focused = true;
        return _WKFocusStartsInputSessionPolicyAllow;
    }];

    [webView synchronouslyLoadHTMLString:@"<input autofocus autocapitalize='words' value='foo bar' type='text' />"];
    Util::run(&focused);

    auto testSelection = [&](unsigned selectionOffset, UTF32Char twoCharactersBefore, UTF32Char characterBefore, UTF32Char characterAfter) {
        auto script = [NSString stringWithFormat:@"document.querySelector('input').setSelectionRange(%u, %u)", selectionOffset, selectionOffset];
        [webView objectByEvaluatingJavaScript:script];
        [webView waitForNextPresentationUpdate];

        auto contentView = [webView textInputContentView];
        EXPECT_EQ([contentView _characterInRelationToCaretSelection:-2], twoCharactersBefore);
        EXPECT_EQ([contentView _characterInRelationToCaretSelection:-1], characterBefore);
        EXPECT_EQ([contentView _characterInRelationToCaretSelection:0], characterAfter);
    };

    testSelection(1, '\0', 'f', 'o');
    testSelection(3, 'o', 'o', ' ');
    testSelection(7, 'a', 'r', '\0');
}

#if HAVE(REDESIGNED_TEXT_CURSOR)

TEST(KeyboardInputTests, MarkedTextSegmentsWithUnderlines)
{
    auto frame = CGRectMake(0, 0, 100, 100);
    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:frame configuration:configuration.get() addToWindow:NO]);

    auto window = adoptNS([[UIWindow alloc] initWithFrame:[webView frame]]);
    [window addSubview:webView.get()];

    [webView _setEditable:YES];
    [webView synchronouslyLoadHTMLString:@"<meta name='viewport' content='width=device-width'><meta charset='utf-8'><body>なんですか？</body>"];
    [webView selectAll:nil];

    auto setMarkedTextWithUnderlines = [&](NSUnderlineStyle firstUnderlineStyle, NSUnderlineStyle secondUnderlineStyle) {
        auto composition = adoptNS([[NSMutableAttributedString alloc] initWithString:@"なんですか？"]);
        [composition addAttributes:@{
            NSMarkedClauseSegmentAttributeName: @(0),
            NSUnderlineStyleAttributeName: @(firstUnderlineStyle),
            NSUnderlineColorAttributeName: UIColor.tintColor
        } range:NSMakeRange(0, 5)];

        [composition addAttributes:@{
            NSMarkedClauseSegmentAttributeName: @(1),
            NSUnderlineStyleAttributeName: @(secondUnderlineStyle),
            NSUnderlineColorAttributeName: UIColor.tintColor
        } range:NSMakeRange(5, 1)];

        [[webView textInputContentView] setAttributedMarkedText:composition.get() selectedRange:NSMakeRange(0, 6)];
    };

    setMarkedTextWithUnderlines(NSUnderlineStyleSingle, NSUnderlineStyleThick);
    [webView waitForNextPresentationUpdate];

    auto snapshotConfiguration = adoptNS([[WKSnapshotConfiguration alloc] init]);
    [snapshotConfiguration setAfterScreenUpdates:YES];

    auto takeSnapshot = [&] {
        __block RetainPtr<CGImage> result;
        __block bool done = false;
        [webView takeSnapshotWithConfiguration:snapshotConfiguration.get() completionHandler:^(UIImage *snapshot, NSError *error) {
            result = snapshot.CGImage;
            done = true;
        }];
        Util::run(&done);
        return result;
    };

    auto snapshotBefore = takeSnapshot();

    setMarkedTextWithUnderlines(NSUnderlineStyleThick, NSUnderlineStyleSingle);
    [webView waitForNextPresentationUpdate];

    auto snapshotAfter = takeSnapshot();

    CGImagePixelReader snapshotReaderBefore { snapshotBefore.get() };
    CGImagePixelReader snapshotReaderAfter { snapshotAfter.get() };

    unsigned numberOfDifferentPixels = 0;
    for (int x = 0; x < 200; ++x) {
        for (int y = 0; y < 200; ++y) {
            if (snapshotReaderBefore.at(x, y) != snapshotReaderAfter.at(x, y))
                numberOfDifferentPixels++;
        }
    }
    EXPECT_GT(numberOfDifferentPixels, 0U);
}

TEST(KeyboardInputTests, HasTextAfterFocusingTextField)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 600)]);

    enum class HasText : bool { No, Yes };
    __block Vector<std::pair<WKInputType, HasText>, 3> results;
    __block bool doneWaitingForInputSession = false;

    auto inputDelegate = adoptNS([[TestInputDelegate alloc] init]);
    [inputDelegate setDidStartInputSessionHandler:^(WKWebView *webView, id<_WKFormInputSession> session) {
        results.append({
            session.focusedElementInfo.type,
            webView.textInputContentView.hasText ? HasText::Yes : HasText::No
        });
        doneWaitingForInputSession = true;
    }];

    [inputDelegate setFocusStartsInputSessionPolicyHandler:^(WKWebView *, id<_WKFocusedElementInfo>) {
        return _WKFocusStartsInputSessionPolicyAllow;
    }];
    [webView _setInputDelegate:inputDelegate.get()];
    [webView synchronouslyLoadHTMLString:@"<!DOCTYPE html>"
        "<html>"
        "<meta name='viewport' content='width=device-width'>"
        "<body>"
        "  <input id='emailField' type='email' value='foo@bar.com'>"
        "  <input id='passwordField' type='password'>"
        "  <div id='richTextEditor' contentEditable='true'><span>Hello world</span></div>"
        "</body>"
        "</html>"];

    auto waitForInputSessionAfterFocusing = ^(NSString *elementID) {
        doneWaitingForInputSession = false;
        [webView objectByEvaluatingJavaScript:[NSString stringWithFormat:@"%@.focus()", elementID]];
        Util::run(&doneWaitingForInputSession);
    };

    waitForInputSessionAfterFocusing(@"emailField");
    waitForInputSessionAfterFocusing(@"passwordField");
    waitForInputSessionAfterFocusing(@"richTextEditor");

    EXPECT_EQ(results.size(), 3U);
    EXPECT_EQ(results[0].first, WKInputTypeEmail);
    EXPECT_EQ(results[0].second, HasText::Yes);
    EXPECT_EQ(results[1].first, WKInputTypePassword);
    EXPECT_EQ(results[1].second, HasText::No);
    EXPECT_EQ(results[2].first, WKInputTypeContentEditable);
    EXPECT_EQ(results[2].second, HasText::Yes);
}

#if HAVE(AUTOCORRECTION_ENHANCEMENTS)
TEST(KeyboardInputTests, AutocorrectionIndicatorColorNotAffectedByAuthorDefinedAncestorColorProperty)
{
    auto frame = CGRectMake(0, 0, 320, 568);
    auto window = adoptNS([[UIWindow alloc] initWithFrame:frame]);

    auto createSnapshotForHTMLString = ^(NSString *HTMLString) {
        auto configuration = [WKWebViewConfiguration _test_configurationWithTestPlugInClassName:@"WebProcessPlugInWithInternals" configureJSCForTesting:YES];

        auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:frame configuration:configuration addToWindow:NO]);

        [window addSubview:webView.get()];

        auto inputDelegate = adoptNS([[TestInputDelegate alloc] init]);
        [inputDelegate setFocusStartsInputSessionPolicyHandler:[] (WKWebView *, id<_WKFocusedElementInfo>) -> _WKFocusStartsInputSessionPolicy {
            return _WKFocusStartsInputSessionPolicyAllow;
        }];

        [webView _setInputDelegate:inputDelegate.get()];
        [webView synchronouslyLoadHTMLString:HTMLString];

        [webView waitForNextPresentationUpdate];

        [webView stringByEvaluatingJavaScript:@"document.getElementById('input').focus();"];

        [webView waitForNextPresentationUpdate];

        auto *contentView = [webView textInputContentView];
        [contentView insertText:@"Is it diferent"];

        [webView waitForNextPresentationUpdate];

        NSString *hasCorrectionIndicatorMarkerJavaScript = @"internals.hasCorrectionIndicatorMarker(6, 9);";

        __block bool done = false;

        [webView replaceText:@"diferent" withText:@"different" shouldUnderline:YES completion:^{
            NSString *hasCorrectionIndicatorMarker = [webView stringByEvaluatingJavaScript:hasCorrectionIndicatorMarkerJavaScript];
            EXPECT_WK_STREQ("1", hasCorrectionIndicatorMarker);
            done = true;
        }];

        TestWebKitAPI::Util::run(&done);

        [webView waitForNextPresentationUpdate];

        auto snapshotConfiguration = adoptNS([[WKSnapshotConfiguration alloc] init]);
        [snapshotConfiguration setAfterScreenUpdates:YES];

        __block RetainPtr<CGImage> result;
        done = false;
        [webView takeSnapshotWithConfiguration:snapshotConfiguration.get() completionHandler:^(UIImage *snapshot, NSError *error) {
            EXPECT_NULL(error);
            result = snapshot.CGImage;
            done = true;
        }];

        Util::run(&done);

        [webView removeFromSuperview];

        return result;
    };

    auto expected = createSnapshotForHTMLString(@"<body><div id='input' contenteditable/></body>");

    auto actual = createSnapshotForHTMLString(@"<body style='color: black'><div id='input' contenteditable/></body>");

    CGImagePixelReader snapshotReaderExpected { expected.get() };
    CGImagePixelReader snapshotReaderActual { actual.get() };

ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    // FIXME: <rdar://155548417> ([ Build-Failure ] [ iOS26+ ] error: 'mainScreen' is deprecated: first deprecated in iOS 26.0)
    auto scale = UIScreen.mainScreen.scale;
ALLOW_DEPRECATED_DECLARATIONS_END

    for (int x = 0; x < frame.size.width * scale; ++x) {
        for (int y = 0; y < frame.size.height * scale; ++y)
            EXPECT_EQ(snapshotReaderExpected.at(x, y), snapshotReaderActual.at(x, y));
    }
}
#endif

#endif // HAVE(REDESIGNED_TEXT_CURSOR)

#if HAVE(UI_CONVERSATION_CONTEXT)

TEST(KeyboardInputTests, SetConversationContext)
{
    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 400, 400)]);
    RetainPtr conversationContext = adoptNS([UIMailConversationContext new]);
    [webView setConversationContext:conversationContext.get()];

    RetainPtr suggestionForTesting = adoptNS([UIInputSuggestion new]);
    RetainPtr uiDelegate = adoptNS([TestUIDelegate new]);
    __block bool didInsertInputSuggestion = false;
    [uiDelegate setInsertInputSuggestion:^(WKWebView *view, UIInputSuggestion *suggestion) {
        EXPECT_EQ(view, webView.get());
        EXPECT_EQ(suggestion, suggestionForTesting.get());
        didInsertInputSuggestion = true;
    }];
    RetainPtr inputDelegate = adoptNS([TestInputDelegate new]);
    [inputDelegate setFocusStartsInputSessionPolicyHandler:[](WKWebView *, id<_WKFocusedElementInfo>) {
        return _WKFocusStartsInputSessionPolicyAllow;
    }];
    __block bool didStartInputSession = false;
    [inputDelegate setDidStartInputSessionHandler:^(WKWebView *, id<_WKFormInputSession>) {
        didStartInputSession = true;
    }];
    [webView setUIDelegate:uiDelegate.get()];
    [webView _setInputDelegate:inputDelegate.get()];
    [webView synchronouslyLoadHTMLString:@"<textarea autofocus>"];

    Util::run(&didStartInputSession);
    EXPECT_EQ([webView effectiveTextInputTraits].conversationContext, conversationContext.get());

    [[webView textInputContentView] insertInputSuggestion:suggestionForTesting.get()];
    EXPECT_TRUE(didInsertInputSuggestion);
}

#endif // HAVE(UI_CONVERSATION_CONTEXT)

#if HAVE(ESIM_AUTOFILL_SYSTEM_SUPPORT)

static BOOL allowESIMAutoFillForWebKit(id, SEL, NSString *host, NSError **)
{
    return [host isEqualToString:@"login.webkit.org"];
}

TEST(KeyboardInputTests, DeviceEIDAndIMEIAutoFill)
{
    InstanceMethodSwizzler swizzler {
#if HAVE(DELAY_INIT_LINKING)
        CoreTelephonyClient.class,
#else
        PAL::getCoreTelephonyClientClassSingleton(),
#endif
        @selector(isAutofilleSIMIdAllowedForDomain:error:),
        reinterpret_cast<IMP>(allowESIMAutoFillForWebKit)
    };
    [WKWebView _setApplicationBundleIdentifier:@"org.webkit.SomeTelephonyApp"];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    auto navigationDelegate = adoptNS([TestNavigationDelegate new]);
    auto inputDelegate = adoptNS([TestInputDelegate new]);
    [inputDelegate setFocusStartsInputSessionPolicyHandler:[](WKWebView *, id<_WKFocusedElementInfo>) {
        return _WKFocusStartsInputSessionPolicyAllow;
    }];
    __block bool didStartInputSession = false;
    [inputDelegate setDidStartInputSessionHandler:^(WKWebView *, id<_WKFormInputSession>) {
        didStartInputSession = true;
    }];

    [webView setNavigationDelegate:navigationDelegate.get()];
    [webView _setInputDelegate:inputDelegate.get()];

    auto loadSimulatedRequest = ^(NSString *urlString) {
        [webView loadSimulatedRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:urlString]] responseHTMLString:@"<body>"
            "<input id='imei' type='number' placeholder='imei' autocomplete='device-imei' />"
            "<input id='eid' type='number' placeholder='eid' autocomplete='device-eid' />"
            "</body>"];
        [navigationDelegate waitForDidFinishNavigation];
    };

    auto focusElementWithID = ^(NSString *identifier) {
        [webView objectByEvaluatingJavaScript:[NSString stringWithFormat:@"document.getElementById('%@').focus()", identifier]];
        Util::run(&didStartInputSession);
    };

    auto blurActiveElement = ^{
        [webView objectByEvaluatingJavaScript:@"document.activeElement.blur()"];
        [webView waitForNextPresentationUpdate];
        didStartInputSession = false;
    };

    loadSimulatedRequest(@"https://login.webkit.org"); // AutoFill is allowed here.
    focusElementWithID(@"imei");
    EXPECT_WK_STREQ(UITextContentTypeCellularIMEI, [webView effectiveTextInputTraits].textContentType);

    blurActiveElement();
    focusElementWithID(@"eid");
    EXPECT_WK_STREQ(UITextContentTypeCellularEID, [webView effectiveTextInputTraits].textContentType);

    loadSimulatedRequest(@"https://apple.com"); // AutoFill is not allowed here.
    focusElementWithID(@"imei");
    EXPECT_NULL([webView effectiveTextInputTraits].textContentType);

    blurActiveElement();
    focusElementWithID(@"eid");
    EXPECT_NULL([webView effectiveTextInputTraits].textContentType);
}

#endif // HAVE(ESIM_AUTOFILL_SYSTEM_SUPPORT)

TEST(KeyboardInputTests, ImplementAllOptionalTextInputTraits)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 400, 400)]);
    auto traits = [webView effectiveTextInputTraits];
    EXPECT_EQ(traits.autocapitalizationType, UITextAutocapitalizationTypeSentences);
    EXPECT_EQ(traits.spellCheckingType, UITextSpellCheckingTypeDefault);
    EXPECT_EQ(traits.smartQuotesType, UITextSmartQuotesTypeDefault);
    EXPECT_EQ(traits.smartDashesType, UITextSmartDashesTypeDefault);
    EXPECT_EQ(traits.smartInsertDeleteType, UITextSmartInsertDeleteTypeDefault);
    EXPECT_EQ(traits.keyboardType, UIKeyboardTypeDefault);
#if HAVE(ALLOW_NUMBERPAD_POPOVER_TEXT_INPUT_TRAITS)
    EXPECT_FALSE(traits.allowsNumberPadPopover);
#endif
    EXPECT_EQ(traits.keyboardAppearance, UIKeyboardAppearanceDefault);
    EXPECT_EQ(traits.returnKeyType, UIReturnKeyDefault);
    EXPECT_FALSE(traits.enablesReturnKeyAutomatically);
    EXPECT_FALSE(traits.secureTextEntry);
    EXPECT_NULL(traits.textContentType);
    EXPECT_NULL(traits.passwordRules);
#if HAVE(UI_CONVERSATION_CONTEXT)
    EXPECT_NULL(traits.conversationContext);
#endif
#if USE(BROWSERENGINEKIT)
    auto extendedTraits = [webView extendedTextInputTraits];
    EXPECT_FALSE(extendedTraits.singleLineDocument);
    EXPECT_TRUE(extendedTraits.typingAdaptationEnabled);
    EXPECT_NULL(extendedTraits.insertionPointColor);
    EXPECT_NULL(extendedTraits.selectionHandleColor);
    EXPECT_NULL(extendedTraits.selectionHighlightColor);
#endif
}

TEST(KeyboardInputTests, TestFrameInfoServerTrustDuringStrongPasswordAssistance)
{
    HTTPServer server({
        { "/"_s, { "<script>alert('done')</script><input type='password' id='input'>"_s } },
    }, HTTPServer::Protocol::HttpsProxy);

    RetainPtr navigationDelegate = adoptNS([TestNavigationDelegate new]);
    [navigationDelegate allowAnyTLSCertificate];

    RetainPtr configuration = server.httpsProxyConfiguration();
    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500) configuration:configuration.get()]);
    [webView setNavigationDelegate:navigationDelegate.get()];

    RetainPtr inputDelegate = adoptNS([[TestInputDelegate alloc] init]);

    [inputDelegate setFocusStartsInputSessionPolicyHandler:^_WKFocusStartsInputSessionPolicy(WKWebView *, id<_WKFocusedElementInfo>) {
        return _WKFocusStartsInputSessionPolicyAllow;
    }];

    [inputDelegate setFocusRequiresStrongPasswordAssistanceHandler:[&] (WKWebView *, id<_WKFocusedElementInfo> info, void(^completionHandler)(BOOL)) {
        EXPECT_NOT_NULL(info.frame);
        EXPECT_NOT_NULL(info.frame._serverTrust);
        completionHandler(YES);
    }];

    [webView _setInputDelegate:inputDelegate.get()];

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/"]]];
    EXPECT_WK_STREQ([webView _test_waitForAlert], "done");

    [webView evaluateJavaScriptAndWaitForInputSessionToChange:@"document.getElementById('input').focus()"];
}

} // namespace TestWebKitAPI

#endif // PLATFORM(IOS_FAMILY)
