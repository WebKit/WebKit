/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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

#if PLATFORM(MAC)

#import "PlatformUtilities.h"
#import "TestWKWebView.h"

#import <Carbon/Carbon.h>
#import <WebKit/WebKitPrivate.h>

static NSString *GetInputValueJSExpression = @"document.querySelector('input').value";
static NSString *GetDocumentScrollTopJSExpression = @"document.body.scrollTop";

@interface TestCandidate : NSTextCheckingResult
@end

@implementation TestCandidate {
    NSString *_string;
    NSRange _range;
}

- (instancetype)initWithReplacementString:(NSString *)string inRange:(NSRange)range
{
    if (self = [super init]) {
        _string = string;
        _range = range;
    }
    return self;
}

- (NSString *)replacementString
{
    return _string;
}

- (NSTextCheckingType)resultType
{
    return NSTextCheckingTypeReplacement;
}

- (NSRange)range
{
    return _range;
}

@end

@interface CandidateTestWebView : TestWKWebView
@property (nonatomic, readonly, getter=isCandidateListVisible) BOOL candidateListVisible;
@end

@implementation CandidateTestWebView {
    bool _isDoneWaitingForCandidate;
    bool _isListeningForCandidateListVisibilityChanges;
    NSUInteger _candidateListVisibilityChangeCount;
}

- (void)insertCandidatesAndWaitForResponse:(NSString *)replacementString range:(NSRange)range
{
    _isDoneWaitingForCandidate = false;
    [self _handleAcceptedCandidate:[[TestCandidate alloc] initWithReplacementString:replacementString inRange:range]];
    TestWebKitAPI::Util::run(&_isDoneWaitingForCandidate);
}

- (void)_didHandleAcceptedCandidate
{
    _isDoneWaitingForCandidate = true;
}

- (void)expectCandidateListVisibilityUpdates:(NSUInteger)expectedUpdateCount whenPerformingActions:(dispatch_block_t)actions
{
    _candidateListVisibilityChangeCount = 0;
    _isListeningForCandidateListVisibilityChanges = YES;

    actions();

    _isListeningForCandidateListVisibilityChanges = NO;
    EXPECT_EQ(expectedUpdateCount, _candidateListVisibilityChangeCount);
}

- (void)_didUpdateCandidateListVisibility:(BOOL)visible
{
    if (_candidateListVisible == visible)
        return;

    _candidateListVisible = visible;
    if (_isListeningForCandidateListVisibilityChanges)
        _candidateListVisibilityChangeCount++;
}

- (void)typeString:(NSString *)string inputMessage:(NSString *)inputMessage
{
    for (uint64_t i = 0; i < string.length; ++i) {
        dispatch_async(dispatch_get_main_queue(), ^()
        {
            [self typeCharacter:[string characterAtIndex:i]];
        });
        [self waitForMessage:inputMessage];
        [self waitForNextPresentationUpdate];
    }
}

+ (instancetype)setUpWithFrame:(NSRect)frame testPage:(NSString *)testPageName
{
    CandidateTestWebView *wkWebView = [[CandidateTestWebView alloc] initWithFrame:frame];

    [wkWebView loadTestPageNamed:testPageName];
    [wkWebView waitForMessage:@"focused"];
    [wkWebView waitForNextPresentationUpdate];
    [wkWebView _forceRequestCandidates];

    return wkWebView;
}

@end

TEST(WKWebViewCandidateTests, SoftSpaceReplacementAfterCandidateInsertionWithoutReplacement)
{
    CandidateTestWebView *wkWebView = [CandidateTestWebView setUpWithFrame:NSMakeRect(0, 0, 800, 600) testPage:@"input-field-in-scrollable-document"];

    [wkWebView insertCandidatesAndWaitForResponse:@"apple " range:NSMakeRange(0, 0)];
    EXPECT_WK_STREQ("apple ", [wkWebView stringByEvaluatingJavaScript:GetInputValueJSExpression]);

    [wkWebView expectCandidateListVisibilityUpdates:0 whenPerformingActions:^()
    {
        [wkWebView typeString:@" " inputMessage:@"input"];
    }];
    EXPECT_WK_STREQ("apple ", [wkWebView stringByEvaluatingJavaScript:GetInputValueJSExpression]);
    EXPECT_EQ([[wkWebView stringByEvaluatingJavaScript:GetDocumentScrollTopJSExpression] doubleValue], 0);
}

TEST(WKWebViewCandidateTests, InsertCharactersAfterCandidateInsertionWithSoftSpace)
{
    CandidateTestWebView *wkWebView = [CandidateTestWebView setUpWithFrame:NSMakeRect(0, 0, 800, 600) testPage:@"input-field-in-scrollable-document"];

    [wkWebView insertCandidatesAndWaitForResponse:@"foo " range:NSMakeRange(0, 0)];
    EXPECT_WK_STREQ("foo ", [wkWebView stringByEvaluatingJavaScript:GetInputValueJSExpression]);

    [wkWebView typeString:@"a" inputMessage:@"input"];
    EXPECT_WK_STREQ("foo a", [wkWebView stringByEvaluatingJavaScript:GetInputValueJSExpression]);
}

TEST(WKWebViewCandidateTests, InsertCandidateFromPartiallyTypedPhraseWithSoftSpace)
{
    CandidateTestWebView *wkWebView = [CandidateTestWebView setUpWithFrame:NSMakeRect(0, 0, 800, 600) testPage:@"input-field-in-scrollable-document"];

    [wkWebView typeString:@"hel" inputMessage:@"input"];
    [wkWebView insertCandidatesAndWaitForResponse:@"hello " range:NSMakeRange(0, 3)];
    EXPECT_WK_STREQ("hello ", [wkWebView stringByEvaluatingJavaScript:GetInputValueJSExpression]);

    [wkWebView expectCandidateListVisibilityUpdates:0 whenPerformingActions:^()
    {
        [wkWebView typeString:@" " inputMessage:@"input"];
        EXPECT_WK_STREQ("hello ", [wkWebView stringByEvaluatingJavaScript:GetInputValueJSExpression]);
        EXPECT_EQ([[wkWebView stringByEvaluatingJavaScript:GetDocumentScrollTopJSExpression] doubleValue], 0);

        [wkWebView typeString:@" " inputMessage:@"input"];
        EXPECT_WK_STREQ("hello  ", [wkWebView stringByEvaluatingJavaScript:GetInputValueJSExpression]);
        EXPECT_EQ([[wkWebView stringByEvaluatingJavaScript:GetDocumentScrollTopJSExpression] doubleValue], 0);
    }];
}

TEST(WKWebViewCandidateTests, ClickingInTextFieldDoesNotThrashCandidateVisibility)
{
    CandidateTestWebView *wkWebView = [CandidateTestWebView setUpWithFrame:NSMakeRect(0, 0, 800, 600) testPage:@"large-input-field-focus-onload"];

    [wkWebView typeString:@"test" inputMessage:@"input"];
    [wkWebView expectCandidateListVisibilityUpdates:0 whenPerformingActions:^()
    {
        dispatch_async(dispatch_get_main_queue(), ^()
        {
            [wkWebView mouseDownAtPoint:NSMakePoint(100, 300) simulatePressure:YES];
        });
        [wkWebView waitForMessage:@"mousedown"];
    }];
    EXPECT_WK_STREQ("test", [wkWebView stringByEvaluatingJavaScript:GetInputValueJSExpression]);
}

TEST(WKWebViewCandidateTests, ShouldNotRequestCandidatesInPasswordField)
{
    CandidateTestWebView *wkWebView = [[CandidateTestWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600)];
    [wkWebView loadTestPageNamed:@"text-and-password-inputs"];
    [wkWebView waitForMessage:@"loaded"];
    [wkWebView _forceRequestCandidates];

    dispatch_async(dispatch_get_main_queue(), ^()
    {
        [wkWebView mouseDownAtPoint:NSMakePoint(400, 150) simulatePressure:YES];
    });
    [wkWebView waitForMessage:@"password-focused"];

    [wkWebView typeString:@"foo" inputMessage:@"password-input"];
    EXPECT_FALSE([wkWebView _shouldRequestCandidates]);

    NSString *passwordFieldValue = [wkWebView stringByEvaluatingJavaScript:@"document.querySelector('#password').value"];
    EXPECT_STREQ(passwordFieldValue.UTF8String, "foo");
}

#if USE(APPLE_INTERNAL_SDK)

TEST(WKWebViewCandidateTests, ShouldRequestCandidatesInTextField)
{
    CandidateTestWebView *wkWebView = [[CandidateTestWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600)];
    [wkWebView loadTestPageNamed:@"text-and-password-inputs"];
    [wkWebView waitForMessage:@"loaded"];
    [wkWebView _forceRequestCandidates];

    dispatch_async(dispatch_get_main_queue(), ^()
    {
        [wkWebView mouseDownAtPoint:NSMakePoint(400, 450) simulatePressure:YES];
    });
    [wkWebView waitForMessage:@"text-focused"];

    [wkWebView typeString:@"bar" inputMessage:@"text-input"];
    EXPECT_TRUE([wkWebView _shouldRequestCandidates]);

    NSString *textFieldValue = [wkWebView stringByEvaluatingJavaScript:@"document.querySelector('#text').value"];
    EXPECT_STREQ(textFieldValue.UTF8String, "bar");
}

#endif

TEST(WKWebViewCandidateTests, CandidateRectForEmptyParagraph)
{
    CandidateTestWebView *wkWebView = [CandidateTestWebView setUpWithFrame:NSMakeRect(0, 0, 800, 600) testPage:@"input-field-in-scrollable-document"];
    NSRect candidateRect = [wkWebView _candidateRect];
    EXPECT_NE(0, candidateRect.origin.y);
}

#endif // PLATFORM(MAC)
