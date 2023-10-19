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

#import "config.h"
#import "Test.h"

#if PLATFORM(IOS_FAMILY)

#import "PlatformUtilities.h"
#import "TestInputDelegate.h"
#import "TestWKWebView.h"
#import "UIKitSPIForTesting.h"
#import "WKWebViewConfigurationExtras.h"
#import <WebKit/WKWebViewPrivate.h>
#import <WebKit/WKWebViewPrivateForTesting.h>
#import <WebKit/_WKProcessPoolConfiguration.h>

@interface TestWKWebView (AutocorrectionTests)
- (UIWKAutocorrectionRects *)autocorrectionRectsForString:(NSString *)string;
- (UIWKAutocorrectionContext *)synchronouslyRequestAutocorrectionContext;
@end

@implementation TestWKWebView (AutocorrectionTests)

- (UIWKAutocorrectionRects *)autocorrectionRectsForString:(NSString *)string
{
    RetainPtr<UIWKAutocorrectionRects> result;
    bool done = false;
    [self.textInputContentView requestAutocorrectionRectsForString:string withCompletionHandler:[&] (UIWKAutocorrectionRects *rects) {
        result = rects;
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
    return result.autorelease();
}

- (UIWKAutocorrectionContext *)synchronouslyRequestAutocorrectionContext
{
    RetainPtr<UIWKAutocorrectionContext> result;
    bool done = false;
    [self.textInputContentView requestAutocorrectionContextWithCompletionHandler:[&] (UIWKAutocorrectionContext *context) {
        result = context;
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
    return result.autorelease();
}

@end

static void checkCGRectIsNotEmpty(CGRect rect)
{
    BOOL isEmpty = CGRectIsEmpty(rect);
    EXPECT_FALSE(isEmpty);
    if (isEmpty)
        NSLog(@"Expected %@ to be non-empty", NSStringFromCGRect(rect));
}

TEST(AutocorrectionTests, FontAtCaretWhenUsingUICTFontTextStyle)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 568)]);
    auto inputDelegate = adoptNS([[TestInputDelegate alloc] init]);
    [inputDelegate setFocusStartsInputSessionPolicyHandler:[] (WKWebView *, id <_WKFocusedElementInfo>) -> _WKFocusStartsInputSessionPolicy {
        return _WKFocusStartsInputSessionPolicyAllow;
    }];

    [webView _setInputDelegate:inputDelegate.get()];
    [webView _setEditable:YES];
    [webView synchronouslyLoadHTMLString:@"<meta name='viewport' content='width=device-width, initial-scale=1'><body style='font-size: 16px; font-family: UICTFontTextStyleBody'>Wulk</body>"];
    [webView evaluateJavaScriptAndWaitForInputSessionToChange:@"document.body.focus()"];
    [webView _executeEditCommand:@"MoveToEndOfLine" argument:nil completion:nil];

    RetainPtr autocorrectionRects = [webView autocorrectionRectsForString:@"Wulk"];
    checkCGRectIsNotEmpty([autocorrectionRects firstRect]);
    checkCGRectIsNotEmpty([autocorrectionRects lastRect]);

    auto contentView = [webView textInputContentView];
    UIFont *fontBeforeScaling = [contentView fontForCaretSelection];
    UIFont *size16SystemFont = [UIFont systemFontOfSize:16];
    EXPECT_WK_STREQ(size16SystemFont.fontName, fontBeforeScaling.fontName);
    EXPECT_WK_STREQ(size16SystemFont.familyName, fontBeforeScaling.familyName);
    EXPECT_EQ(16, fontBeforeScaling.pointSize);

    [webView scrollView].zoomScale = 2;
    UIFont *fontAfterScaling = [contentView fontForCaretSelection];
    UIFont *size32SystemFont = [UIFont systemFontOfSize:32];
    EXPECT_WK_STREQ(size32SystemFont.fontName, fontAfterScaling.fontName);
    EXPECT_WK_STREQ(size32SystemFont.familyName, fontAfterScaling.familyName);
    EXPECT_EQ(32, fontAfterScaling.pointSize);
}

TEST(AutocorrectionTests, RequestAutocorrectionContextAfterClosingPage)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 568)]);
    [webView synchronouslyLoadTestPageNamed:@"autofocused-text-input"];

    auto contentView = [webView textInputContentView];
    [contentView resignFirstResponder];
    [webView _close];

    // This test just verifies that attempting to request an autocorrection context in this state
    // does not trigger a crash or hang, and also still invokes the given completion handler.
    [webView synchronouslyRequestAutocorrectionContext];
}

TEST(AutocorrectionTests, AutocorrectionContextDoesNotIncludeNewlineInTextField)
{
    if ([UIKeyboard usesInputSystemUI])
        return;
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 568)]);
    [webView synchronouslyLoadTestPageNamed:@"autofocused-text-input"];

    RetainPtr contextBeforeTyping = [webView synchronouslyRequestAutocorrectionContext];
    EXPECT_EQ(0U, [contextBeforeTyping contextBeforeSelection].length);
    EXPECT_EQ(0U, [contextBeforeTyping selectedText].length);
    EXPECT_EQ(0U, [contextBeforeTyping contextAfterSelection].length);

    [[webView textInputContentView] insertText:@"a"];
    [webView waitForNextPresentationUpdate];

    RetainPtr contextAfterTyping = [webView synchronouslyRequestAutocorrectionContext];
    EXPECT_WK_STREQ(@"a", [contextAfterTyping contextBeforeSelection]);
    EXPECT_EQ(0U, [contextAfterTyping selectedText].length);
    EXPECT_EQ(0U, [contextAfterTyping contextAfterSelection].length);
}

#if HAVE(AUTOCORRECTION_ENHANCEMENTS)

TEST(AutocorrectionTests, AutocorrectionIndicatorsDismissAfterNextWord)
{
    auto configuration = [WKWebViewConfiguration _test_configurationWithTestPlugInClassName:@"WebProcessPlugInWithInternals" configureJSCForTesting:YES];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 568) configuration:configuration]);
    auto inputDelegate = adoptNS([[TestInputDelegate alloc] init]);
    [inputDelegate setFocusStartsInputSessionPolicyHandler:[] (WKWebView *, id<_WKFocusedElementInfo>) -> _WKFocusStartsInputSessionPolicy {
        return _WKFocusStartsInputSessionPolicyAllow;
    }];

    [webView _setInputDelegate:inputDelegate.get()];
    [webView synchronouslyLoadTestPageNamed:@"autofocused-text-input"];

    auto *contentView = [webView textInputContentView];
    [contentView insertText:@"Is it diferent"];

    [webView waitForNextPresentationUpdate];

    NSString *hasCorrectionIndicatorMarkerJavaScript = @"internals.hasCorrectionIndicatorMarker(6, 9);";

    __block bool done = false;

    [webView applyAutocorrection:@"different" toString:@"diferent" isCandidate:YES withCompletionHandler:^{
        NSString *hasCorrectionIndicatorMarker = [webView stringByEvaluatingJavaScript:hasCorrectionIndicatorMarkerJavaScript];
        EXPECT_WK_STREQ("1", hasCorrectionIndicatorMarker);
        done = true;
    }];

    TestWebKitAPI::Util::run(&done);

    [contentView insertText:@" than"];
    [webView waitForNextPresentationUpdate];

    [contentView insertText:@" "];
    [webView waitForNextPresentationUpdate];

    TestWebKitAPI::Util::runFor(3_s);

    NSString *hasCorrectionIndicatorMarker = [webView stringByEvaluatingJavaScript:hasCorrectionIndicatorMarkerJavaScript];
    EXPECT_WK_STREQ("0", hasCorrectionIndicatorMarker);
}

TEST(AutocorrectionTests, AutocorrectionIndicatorsMultiWord)
{
    auto configuration = [WKWebViewConfiguration _test_configurationWithTestPlugInClassName:@"WebProcessPlugInWithInternals" configureJSCForTesting:YES];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 568) configuration:configuration]);
    auto inputDelegate = adoptNS([[TestInputDelegate alloc] init]);
    [inputDelegate setFocusStartsInputSessionPolicyHandler:[] (WKWebView *, id<_WKFocusedElementInfo>) -> _WKFocusStartsInputSessionPolicy {
        return _WKFocusStartsInputSessionPolicyAllow;
    }];

    [webView _setInputDelegate:inputDelegate.get()];
    [webView synchronouslyLoadTestPageNamed:@"autofocused-text-input"];

    auto *contentView = [webView textInputContentView];
    [contentView insertText:@"tomorrownight"];

    [webView waitForNextPresentationUpdate];

    NSString *hasCorrectionIndicatorMarkerJavaScript = @"internals.hasCorrectionIndicatorMarker(0, 14);";

    __block bool done = false;

    [webView applyAutocorrection:@"tomorrow night" toString:@"tomorrownight" isCandidate:YES withCompletionHandler:^{
        NSString *hasCorrectionIndicatorMarker = [webView stringByEvaluatingJavaScript:hasCorrectionIndicatorMarkerJavaScript];
        EXPECT_WK_STREQ("1", hasCorrectionIndicatorMarker);
        done = true;
    }];

    TestWebKitAPI::Util::run(&done);

    EXPECT_WK_STREQ("", [webView selectedText]);

    [contentView selectWordForReplacement];
    [webView waitForNextPresentationUpdate];

    EXPECT_WK_STREQ("tomorrow night", [webView selectedText]);
}

#endif // HAVE(AUTOCORRECTION_ENHANCEMENTS)

TEST(AutocorrectionTests, AutocorrectionContextBeforeAndAfterEditing)
{
    if ([UIKeyboard usesInputSystemUI])
        return;
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 568)]);
    auto inputDelegate = adoptNS([[TestInputDelegate alloc] init]);
    [inputDelegate setFocusStartsInputSessionPolicyHandler:[] (WKWebView *, id <_WKFocusedElementInfo>) -> _WKFocusStartsInputSessionPolicy {
        return _WKFocusStartsInputSessionPolicyAllow;
    }];

    [webView _setInputDelegate:inputDelegate.get()];
    [webView synchronouslyLoadTestPageNamed:@"autofocused-text-input"];

    auto *contentView = [webView textInputContentView];
    RetainPtr contextBeforeInsertingText = [webView synchronouslyRequestAutocorrectionContext];
    EXPECT_EQ(0U, [contextBeforeInsertingText contextBeforeSelection].length);
    EXPECT_EQ(0U, [contextBeforeInsertingText selectedText].length);
    EXPECT_EQ(0U, [contextBeforeInsertingText contextAfterSelection].length);

    [contentView insertText:@"hello"];
    RetainPtr contextAfterInsertingText = [webView synchronouslyRequestAutocorrectionContext];
    EXPECT_WK_STREQ("hello", [contextAfterInsertingText contextBeforeSelection]);
    EXPECT_EQ(0U, [contextAfterInsertingText selectedText].length);
    EXPECT_EQ(0U, [contextAfterInsertingText contextAfterSelection].length);

    [contentView selectAll:nil];
    RetainPtr contextAfterSelecting = [webView synchronouslyRequestAutocorrectionContext];
    EXPECT_EQ(0U, [contextAfterSelecting contextBeforeSelection].length);
    EXPECT_WK_STREQ("hello", [contextAfterSelecting selectedText]);
    EXPECT_EQ(0U, [contextAfterSelecting contextAfterSelection].length);
}

TEST(AutocorrectionTests, AvoidDeadlockWithGPUProcessCreationInEmptyView)
{
    auto poolConfiguration = adoptNS([_WKProcessPoolConfiguration new]);
    [poolConfiguration setIgnoreSynchronousMessagingTimeoutsForTesting:YES];
    auto configuration = adoptNS([WKWebViewConfiguration new]);
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectZero configuration:configuration.get() processPoolConfiguration:poolConfiguration.get()]);
    auto inputDelegate = adoptNS([[TestInputDelegate alloc] init]);
    [inputDelegate setFocusStartsInputSessionPolicyHandler:[] (WKWebView *, id<_WKFocusedElementInfo>) -> _WKFocusStartsInputSessionPolicy {
        return _WKFocusStartsInputSessionPolicyAllow;
    }];

    [webView _setInputDelegate:inputDelegate.get()];
    [webView synchronouslyLoadTestPageNamed:@"autofocused-text-input"];
    [webView synchronouslyRequestAutocorrectionContext];
}

#endif // PLATFORM(IOS_FAMILY)
