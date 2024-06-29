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

#import "ClassMethodSwizzler.h"
#import "PlatformUtilities.h"
#import "TestInputDelegate.h"
#import "TestWKWebView.h"
#import "UIKitSPIForTesting.h"
#import "WKWebViewConfigurationExtras.h"
#import <WebKit/WKWebViewPrivate.h>
#import <WebKit/WKWebViewPrivateForTesting.h>
#import <WebKit/_WKProcessPoolConfiguration.h>
#import <pal/spi/ios/BrowserEngineKitSPI.h>

@implementation UIFont (TestWebKitAPI)

+ (instancetype)_test_systemFontWithSize:(CGFloat)size traits:(UIFontDescriptorSymbolicTraits)traits
{
    auto font = [self systemFontOfSize:size];
    auto newDescriptor = [font.fontDescriptor fontDescriptorWithSymbolicTraits:traits];
    return [self fontWithDescriptor:newDescriptor size:0];
}

@end

static void checkCGRectIsNotEmpty(CGRect rect)
{
    BOOL isEmpty = CGRectIsEmpty(rect);
    EXPECT_FALSE(isEmpty);
    if (isEmpty)
        NSLog(@"Expected %@ to be non-empty", NSStringFromCGRect(rect));
}

static UIFont *returnNil(Class, SEL)
{
    return nil;
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
    [webView synchronouslyLoadHTMLString:@"<meta name='viewport' content='width=device-width, initial-scale=1'>"
        "<body style='font-size: 16px; font-family: UICTFontTextStyleBody; font-weight: bold;'>"
        "<em>Wulk</em>"
        "</body>"];
    [webView evaluateJavaScriptAndWaitForInputSessionToChange:@"document.body.focus()"];
    [webView _executeEditCommand:@"MoveToEndOfLine" argument:nil completion:nil];

    auto [firstRect, lastRect] = [webView autocorrectionRectsForString:@"Wulk"];
    checkCGRectIsNotEmpty(firstRect);
    checkCGRectIsNotEmpty(lastRect);

    auto contentView = [webView textInputContentView];
    auto selectedTextRangeBeforeScaling = contentView.selectedTextRange;
    auto stylingDictionaryBeforeScaling = [contentView textStylingAtPosition:selectedTextRangeBeforeScaling.start inDirection:UITextStorageDirectionForward];
    UIFont *fontBeforeScaling = [stylingDictionaryBeforeScaling objectForKey:NSFontAttributeName];
    UIFont *size16SystemFont = [UIFont _test_systemFontWithSize:16 traits:UIFontDescriptorTraitBold | UIFontDescriptorTraitItalic];
    EXPECT_WK_STREQ(size16SystemFont.fontName, fontBeforeScaling.fontName);
    EXPECT_WK_STREQ(size16SystemFont.familyName, fontBeforeScaling.familyName);
    EXPECT_EQ(16, fontBeforeScaling.pointSize);

    auto traitsBeforeScaling = fontBeforeScaling.fontDescriptor.symbolicTraits;
    EXPECT_TRUE(traitsBeforeScaling & UIFontDescriptorTraitBold);
    EXPECT_TRUE(traitsBeforeScaling & UIFontDescriptorTraitItalic);

    {
        // Verify that -textStylingAtPosition:inDirection: is robust in the case where +fontWithDescriptor:size: is nil.
        ClassMethodSwizzler swizzler { UIFont.class, @selector(fontWithDescriptor:size:), reinterpret_cast<IMP>(returnNil) };
        NSDictionary *result = [contentView textStylingAtPosition:contentView.selectedTextRange.start inDirection:UITextStorageDirectionForward];
        EXPECT_TRUE(!!result[NSFontAttributeName]);
    }

    [webView scrollView].zoomScale = 2;
    auto selectedTextRangeAfterScaling = contentView.selectedTextRange;
    auto stylingDictionaryAfterScaling = [contentView textStylingAtPosition:selectedTextRangeAfterScaling.start inDirection:UITextStorageDirectionForward];
    UIFont *fontAfterScaling = [stylingDictionaryAfterScaling objectForKey:NSFontAttributeName];
    UIFont *size32SystemFont = [UIFont _test_systemFontWithSize:32 traits:UIFontDescriptorTraitBold | UIFontDescriptorTraitItalic];
    EXPECT_WK_STREQ(size32SystemFont.fontName, fontAfterScaling.fontName);
    EXPECT_WK_STREQ(size32SystemFont.familyName, fontAfterScaling.familyName);
    EXPECT_EQ(32, fontAfterScaling.pointSize);

    auto traitsAfterScaling = fontBeforeScaling.fontDescriptor.symbolicTraits;
    EXPECT_TRUE(traitsAfterScaling & UIFontDescriptorTraitBold);
    EXPECT_TRUE(traitsAfterScaling & UIFontDescriptorTraitItalic);
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
    [webView autocorrectionContext];
}

TEST(AutocorrectionTests, AutocorrectionContextDoesNotIncludeNewlineInTextField)
{
    if ([UIKeyboard usesInputSystemUI])
        return;
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 568)]);
    [webView synchronouslyLoadTestPageNamed:@"autofocused-text-input"];

    auto contextBeforeTyping = [webView autocorrectionContext];
    EXPECT_TRUE(contextBeforeTyping.contextBeforeSelection.isEmpty());
    EXPECT_TRUE(contextBeforeTyping.selectedText.isEmpty());
    EXPECT_TRUE(contextBeforeTyping.contextAfterSelection.isEmpty());

    [[webView textInputContentView] insertText:@"a"];
    [webView waitForNextPresentationUpdate];

    auto contextAfterTyping = [webView autocorrectionContext];
    EXPECT_WK_STREQ(@"a", contextAfterTyping.contextBeforeSelection);
    EXPECT_TRUE(contextAfterTyping.selectedText.isEmpty());
    EXPECT_TRUE(contextAfterTyping.contextAfterSelection.isEmpty());
}

TEST(AutocorrectionTests, MultiWordAutocorrectionFromStartOfText)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 568)]);
    auto inputDelegate = adoptNS([TestInputDelegate new]);
    bool startedInputSession = false;
    [inputDelegate setFocusStartsInputSessionPolicyHandler:[&] (WKWebView *, id<_WKFocusedElementInfo>) -> _WKFocusStartsInputSessionPolicy {
        startedInputSession = true;
        return _WKFocusStartsInputSessionPolicyAllow;
    }];
    [webView _setInputDelegate:inputDelegate.get()];
    [webView synchronouslyLoadHTMLString:@"<input id='input' value='Helol wordl'></input>"];
    [webView objectByEvaluatingJavaScript:@"input.focus(); input.setSelectionRange(0, 0)"];
    TestWebKitAPI::Util::run(&startedInputSession);

    __block bool done = false;
    [webView replaceText:@"Helol wordl" withText:@"Hello world" shouldUnderline:NO completion:^{
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);

    EXPECT_WK_STREQ("Hello world", [webView stringByEvaluatingJavaScript:@"input.value"]);
}

TEST(AutocorrectionTests, DoNotLearnCorrectionsAfterChangingInputTypeFromPassword)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 568)]);
    auto inputDelegate = adoptNS([TestInputDelegate new]);

    bool startedInputSession = false;
    [inputDelegate setFocusStartsInputSessionPolicyHandler:[&] (WKWebView *, id<_WKFocusedElementInfo>) -> _WKFocusStartsInputSessionPolicy {
        startedInputSession = true;
        return _WKFocusStartsInputSessionPolicyAllow;
    }];
    [webView _setInputDelegate:inputDelegate.get()];
    [webView synchronouslyLoadHTMLString:@"<input id='first' type='password'></input><input id='second'></input>"];
    [webView stringByEvaluatingJavaScript:@"let first = document.querySelector('#first'); first.type = 'text'; first.focus();"];
    TestWebKitAPI::Util::run(&startedInputSession);

    auto learnsCorrections = [&]() -> BOOL {
#if USE(BROWSERENGINEKIT)
        if ([webView hasAsyncTextInput])
            return [webView extendedTextInputTraits].typingAdaptationEnabled;
#endif
        return [webView effectiveTextInputTraits].learnsCorrections;
    };
    EXPECT_FALSE(learnsCorrections());

    startedInputSession = false;
    [webView stringByEvaluatingJavaScript:@"document.querySelector('#second').focus()"];
    TestWebKitAPI::Util::run(&startedInputSession);
    EXPECT_TRUE(learnsCorrections());
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

    [webView replaceText:@"diferent" withText:@"different" shouldUnderline:YES completion:^{
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

    [webView replaceText:@"tomorrownight" withText:@"tomorrow night" shouldUnderline:YES completion:^{
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
    auto contextBeforeInsertingText = [webView autocorrectionContext];
    EXPECT_TRUE(contextBeforeInsertingText.contextBeforeSelection.isEmpty());
    EXPECT_TRUE(contextBeforeInsertingText.selectedText.isEmpty());
    EXPECT_TRUE(contextBeforeInsertingText.contextAfterSelection.isEmpty());
    EXPECT_TRUE(contextBeforeInsertingText.markedText.isNull());
    EXPECT_TRUE(NSEqualRanges(contextBeforeInsertingText.selectedRangeInMarkedText, NSMakeRange(NSNotFound, 0)));

    [contentView insertText:@"hello"];
    auto contextAfterInsertingText = [webView autocorrectionContext];
    EXPECT_WK_STREQ("hello", contextAfterInsertingText.contextBeforeSelection);
    EXPECT_TRUE(contextAfterInsertingText.selectedText.isEmpty());
    EXPECT_TRUE(contextAfterInsertingText.contextAfterSelection.isEmpty());
    EXPECT_TRUE(contextAfterInsertingText.markedText.isNull());
    EXPECT_TRUE(NSEqualRanges(contextAfterInsertingText.selectedRangeInMarkedText, NSMakeRange(NSNotFound, 0)));

    [contentView selectAll:nil];
    auto contextAfterSelecting = [webView autocorrectionContext];
    EXPECT_TRUE(contextAfterSelecting.contextBeforeSelection.isEmpty());
    EXPECT_WK_STREQ("hello", contextAfterSelecting.selectedText);
    EXPECT_TRUE(contextAfterSelecting.contextAfterSelection.isEmpty());
    EXPECT_TRUE(contextAfterSelecting.markedText.isNull());
    EXPECT_TRUE(NSEqualRanges(contextAfterSelecting.selectedRangeInMarkedText, NSMakeRange(NSNotFound, 0)));
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
    [webView autocorrectionContext];
}

#endif // PLATFORM(IOS_FAMILY)
