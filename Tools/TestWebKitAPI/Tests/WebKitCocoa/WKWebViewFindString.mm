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

#include "config.h"

#import "PlatformUtilities.h"
#import "TestWKWebView.h"
#import <WebKit/WKWebViewPrivate.h>
#import <WebKit/_WKFindDelegate.h>
#import <WebKit/_WKInputDelegate.h>

#if PLATFORM(IOS_FAMILY)
#import "ClassMethodSwizzler.h"
#import "UIKitSPI.h"
#endif

static bool isDone;
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

@interface WKWebViewFindStringFindDelegate : NSObject <_WKFindDelegate>
@property (nonatomic, readonly) NSString *findString;
@property (nonatomic, readonly) NSUInteger matchesCount;
@property (nonatomic, readonly) NSInteger matchIndex;
@property (nonatomic, readonly) BOOL didFail;
@end

@implementation WKWebViewFindStringFindDelegate {
    RetainPtr<NSString> _findString;
}

- (NSString *)findString
{
    return _findString.get();
}

- (void)_webView:(WKWebView *)webView didCountMatches:(NSUInteger)matches forString:(NSString *)string
{
    _findString = string;
    _matchesCount = matches;
    _didFail = NO;
    isDone = YES;
}

- (void)_webView:(WKWebView *)webView didFindMatches:(NSUInteger)matches forString:(NSString *)string withMatchIndex:(NSInteger)matchIndex
{
    _findString = string;
    _matchesCount = matches;
    _matchIndex = matchIndex;
    _didFail = NO;
    isDone = YES;
}

- (void)_webView:(WKWebView *)webView didFailToFindString:(NSString *)string
{
    _findString = string;
    _didFail = YES;
    isDone = YES;
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

    auto inputDelegate = adoptNS([[WKWebViewFindStringInputDelegate alloc] init]);
    auto findDelegate = adoptNS([[WKWebViewFindStringFindDelegate alloc] init]);
    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    auto firstWebView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 300, 200) configuration:configuration.get() addToWindow:YES]);
    [firstWebView synchronouslyLoadHTMLString:@"<input type='text' value='hello'>"];
    [firstWebView _setInputDelegate:inputDelegate.get()];
    [firstWebView _setFindDelegate:findDelegate.get()];

    auto secondWebView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(300, 0, 300, 200) configuration:configuration.get() addToWindow:YES]);
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

    auto inputDelegate = adoptNS([[WKWebViewFindStringInputDelegate alloc] init]);
    auto findDelegate = adoptNS([[WKWebViewFindStringFindDelegate alloc] init]);
    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    auto firstWebView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 300, 200) configuration:configuration.get() addToWindow:YES]);
    [firstWebView synchronouslyLoadHTMLString:@"<input type='text' value='hello'>"];
    [firstWebView _setInputDelegate:inputDelegate.get()];
    [firstWebView _setFindDelegate:findDelegate.get()];

    auto secondWebView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(300, 0, 300, 200) configuration:configuration.get() addToWindow:YES]);
    EXPECT_TRUE([secondWebView becomeFirstResponder]);
    EXPECT_FALSE([firstWebView isFirstResponder]);

    [firstWebView _findString:@"hello" options:0 maxCount:maxCount];
    Util::run(&isDone);

    EXPECT_FALSE([firstWebView isFirstResponder]);
    EXPECT_WK_STREQ("hello", [findDelegate findString]);
    EXPECT_FALSE(focusDidStartInputSession);
}
#endif

} // namespace TestWebKitAPI
