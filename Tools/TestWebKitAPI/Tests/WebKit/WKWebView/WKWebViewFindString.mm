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

#import "Helpers/DeprecatedGlobalValues.h"
#import "Helpers/PlatformUtilities.h"
#import "Helpers/cocoa/TestWKWebView.h"
#import "WKWebViewFindStringFindDelegate.h"
#import <WebKit/WKWebViewPrivate.h>
#import <WebKit/_WKInputDelegate.h>

#if PLATFORM(IOS_FAMILY)
#import "ClassMethodSwizzler.h"
#import "UIKitSPIForTesting.h"
#endif

static bool focusDidStartInputSession;
static const NSUInteger maxCount = 100;

@interface WKWebViewFindStringInputDelegate : NSObject <_WKInputDelegate>
@end

@implementation WKWebViewFindStringInputDelegate

- (void)_webView:(WKWebView *)webView didStartInputSession:(id <_WKFormInputSession>)inputSession
{
    focusDidStartInputSession = YES;
}

@end

#if PLATFORM(IOS_FAMILY)
static BOOL returnNo()
{
    return NO;
}

static BOOL returnYes()
{
    return YES;
}

static BOOL viewIsFirstResponder(UIView *view)
{
    return [view isFirstResponder];
}
#else
static BOOL viewIsFirstResponder(NSView *view)
{
    return view.window.firstResponder == view;
}
#endif

namespace TestWebKitAPI {

TEST(WKWebViewFindString, DoNotFocusMatchWhenWebViewResignedAndHardwareKeyboardAttached)
{
#if PLATFORM(IOS_FAMILY)
    ClassMethodSwizzler swizzler([UIKeyboard class], @selector(isInHardwareKeyboardMode), reinterpret_cast<IMP>(returnYes));
#endif

    RetainPtr inputDelegate = adoptNS([[WKWebViewFindStringInputDelegate alloc] init]);
    RetainPtr findDelegate = adoptNS([[WKWebViewFindStringFindDelegate alloc] init]);
    RetainPtr configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    RetainPtr firstWebView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 300, 200) configuration:configuration.get() addToWindow:YES]);
    [firstWebView synchronouslyLoadHTMLString:@"<input type='text' value='hello'>"];
    [firstWebView _setInputDelegate:inputDelegate.get()];
    [firstWebView _setFindDelegate:findDelegate.get()];

    RetainPtr secondWebView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(300, 0, 300, 200) configuration:configuration.get() addToWindow:YES]);
    EXPECT_TRUE([secondWebView becomeFirstResponder]);
    EXPECT_FALSE(viewIsFirstResponder(firstWebView.get()));

    [firstWebView _findString:@"hello" options:0 maxCount:maxCount];
    Util::run(&isDone);

    EXPECT_FALSE(viewIsFirstResponder(firstWebView.get()));
    EXPECT_WK_STREQ("hello", [findDelegate findString]);
    EXPECT_FALSE(focusDidStartInputSession);
}

#if PLATFORM(IOS_FAMILY)
TEST(WKWebViewFindString, DoNotFocusMatchWhenWebViewResigned)
{
    ClassMethodSwizzler swizzler([UIKeyboard class], @selector(isInHardwareKeyboardMode), reinterpret_cast<IMP>(returnNo));

    RetainPtr inputDelegate = adoptNS([[WKWebViewFindStringInputDelegate alloc] init]);
    RetainPtr findDelegate = adoptNS([[WKWebViewFindStringFindDelegate alloc] init]);
    RetainPtr configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    RetainPtr firstWebView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 300, 200) configuration:configuration.get() addToWindow:YES]);
    [firstWebView synchronouslyLoadHTMLString:@"<input type='text' value='hello'>"];
    [firstWebView _setInputDelegate:inputDelegate.get()];
    [firstWebView _setFindDelegate:findDelegate.get()];

    RetainPtr secondWebView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(300, 0, 300, 200) configuration:configuration.get() addToWindow:YES]);
    EXPECT_TRUE([secondWebView becomeFirstResponder]);
    EXPECT_FALSE([firstWebView isFirstResponder]);

    [firstWebView _findString:@"hello" options:0 maxCount:maxCount];
    Util::run(&isDone);

    EXPECT_FALSE([firstWebView isFirstResponder]);
    EXPECT_WK_STREQ("hello", [findDelegate findString]);
    EXPECT_FALSE(focusDidStartInputSession);
}
#endif

TEST(WKWebViewFindString, DoNotUpdateMatchIndexWhenGivenNoIndexChangeOption)
{
    RetainPtr findDelegate = adoptNS([[WKWebViewFindStringFindDelegate alloc] init]);
    RetainPtr configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    RetainPtr firstWebView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 300, 200) configuration:configuration.get() addToWindow:YES]);
    [firstWebView synchronouslyLoadHTMLString:@"<p>hello</p><p>hello</p>"];
    [firstWebView _setFindDelegate:findDelegate.get()];

    [firstWebView _findString:@"hello" options:0 maxCount:maxCount];
    Util::run(&isDone);

    EXPECT_EQ(0, [findDelegate matchIndex]);

    [firstWebView _findString:@"hello" options:_WKFindOptionsNoIndexChange maxCount:maxCount];
    Util::run(&isDone);

    EXPECT_EQ(0, [findDelegate matchIndex]);
}

TEST(WKWebViewFindString, MatchIndexIsCorrectWhenNavigatingForwardAndBackward)
{
    RetainPtr findDelegate = adoptNS([[WKWebViewFindStringFindDelegate alloc] init]);
    RetainPtr configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 300, 200) configuration:configuration.get() addToWindow:YES]);
    [webView synchronouslyLoadHTMLString:@"<p>hello</p><p>hello</p><p>hello</p>"];
    [webView _setFindDelegate:findDelegate.get()];

    [webView _findString:@"hello" options:_WKFindOptionsDetermineMatchIndex maxCount:maxCount];
    Util::run(&isDone);
    EXPECT_EQ(0, [findDelegate matchIndex]);

    isDone = false;
    [webView _findString:@"hello" options:_WKFindOptionsDetermineMatchIndex maxCount:maxCount];
    Util::run(&isDone);
    EXPECT_EQ(1, [findDelegate matchIndex]);

    isDone = false;
    [webView _findString:@"hello" options:_WKFindOptionsDetermineMatchIndex maxCount:maxCount];
    Util::run(&isDone);
    EXPECT_EQ(2, [findDelegate matchIndex]);

    isDone = false;
    [webView _findString:@"hello" options:_WKFindOptionsBackwards | _WKFindOptionsDetermineMatchIndex  maxCount:maxCount];
    Util::run(&isDone);
    EXPECT_EQ(1, [findDelegate matchIndex]);
}

TEST(WKWebViewFindString, MatchIndexDoesNotUpdateWithoutDetermineMatchIndexOption)
{
    RetainPtr findDelegate = adoptNS([[WKWebViewFindStringFindDelegate alloc] init]);
    RetainPtr configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 300, 200) configuration:configuration.get() addToWindow:YES]);
    [webView synchronouslyLoadHTMLString:@"<p>hello</p><p>hello</p><p>hello</p>"];
    [webView _setFindDelegate:findDelegate.get()];

    [webView _findString:@"hello" options:_WKFindOptionsDetermineMatchIndex maxCount:maxCount];
    Util::run(&isDone);
    EXPECT_EQ(0, [findDelegate matchIndex]);

    isDone = false;
    [webView _findString:@"hello" options:_WKFindOptionsDetermineMatchIndex maxCount:maxCount];
    Util::run(&isDone);
    EXPECT_EQ(1, [findDelegate matchIndex]);

    isDone = false;
    [webView _findString:@"hello" options:_WKFindOptionsDetermineMatchIndex maxCount:maxCount];
    Util::run(&isDone);
    EXPECT_EQ(2, [findDelegate matchIndex]);

    isDone = false;
    [webView _findString:@"hello" options:_WKFindOptionsBackwards | _WKFindOptionsDetermineMatchIndex  maxCount:maxCount];
    Util::run(&isDone);
    EXPECT_EQ(1, [findDelegate matchIndex]);
}

TEST(WKWebViewFindString, MatchIndexIsCorrectNavigatingWrapAround)
{
    RetainPtr findDelegate = adoptNS([[WKWebViewFindStringFindDelegate alloc] init]);
    RetainPtr configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 300, 200) configuration:configuration.get() addToWindow:YES]);
    [webView synchronouslyLoadHTMLString:@"<p>hello</p><p>hello</p><p>hello</p>"];
    [webView _setFindDelegate:findDelegate.get()];

    auto findOptions = _WKFindOptionsWrapAround | _WKFindOptionsDetermineMatchIndex;

    [webView _findString:@"hello" options:findOptions maxCount:maxCount];
    Util::run(&isDone);
    EXPECT_EQ(0, [findDelegate matchIndex]);

    isDone = false;
    [webView _findString:@"hello" options:findOptions maxCount:maxCount];
    Util::run(&isDone);
    EXPECT_EQ(1, [findDelegate matchIndex]);

    isDone = false;
    [webView _findString:@"hello" options:findOptions maxCount:maxCount];
    Util::run(&isDone);
    EXPECT_EQ(2, [findDelegate matchIndex]);

    isDone = false;
    [webView _findString:@"hello" options:findOptions maxCount:maxCount];
    Util::run(&isDone);
    EXPECT_EQ(0, [findDelegate matchIndex]);
}

TEST(WKWebViewFindString, MatchIndexIsCorrectNavigatingWrapAroundBackwards)
{
    RetainPtr findDelegate = adoptNS([[WKWebViewFindStringFindDelegate alloc] init]);
    RetainPtr configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 300, 200) configuration:configuration.get() addToWindow:YES]);
    [webView synchronouslyLoadHTMLString:@"<p>hello</p><p>hello</p><p>hello</p>"];
    [webView _setFindDelegate:findDelegate.get()];

    auto findOptions = _WKFindOptionsWrapAround | _WKFindOptionsDetermineMatchIndex;
    auto findOptionsBackwards =  findOptions | _WKFindOptionsBackwards;

    [webView _findString:@"hello" options:findOptions maxCount:maxCount];
    Util::run(&isDone);
    EXPECT_EQ(0, [findDelegate matchIndex]);

    isDone = false;
    [webView _findString:@"hello" options:findOptionsBackwards maxCount:maxCount];
    Util::run(&isDone);
    EXPECT_EQ(2, [findDelegate matchIndex]);

    isDone = false;
    [webView _findString:@"hello" options:findOptionsBackwards maxCount:maxCount];
    Util::run(&isDone);
    EXPECT_EQ(1, [findDelegate matchIndex]);

    isDone = false;
    [webView _findString:@"hello" options:findOptionsBackwards maxCount:maxCount];
    Util::run(&isDone);
    EXPECT_EQ(0, [findDelegate matchIndex]);
}

} // namespace TestWebKitAPI
