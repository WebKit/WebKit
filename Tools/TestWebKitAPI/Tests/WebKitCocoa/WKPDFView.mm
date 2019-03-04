/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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
#import <WebKit/WKFoundation.h>

// FIXME: Enable these tests on iOS 12 once rdar://problem/39475542 is resolved.
#if TARGET_OS_IPHONE && __IPHONE_OS_VERSION_MIN_REQUIRED < 120000

#import "PlatformUtilities.h"
#import "TestNavigationDelegate.h"
#import "Utilities.h"
#import <WebKit/WKWebViewPrivate.h>
#import <WebKit/_WKFindDelegate.h>
#import <wtf/RetainPtr.h>

static void runTest(NSURL *pdfURL)
{
    auto webView = adoptNS([[WKWebView alloc] init]);
    [webView loadRequest:[NSURLRequest requestWithURL:pdfURL]];
    [webView _test_waitForDidFinishNavigation];

    NSData *expected = [NSData dataWithContentsOfURL:pdfURL];
    NSData *actual = [webView _dataForDisplayedPDF];
    EXPECT_TRUE([expected isEqualToData:actual]);
}

TEST(WKPDFView, DataForDisplayedPDF)
{
    runTest([[NSBundle mainBundle] URLForResource:@"test" withExtension:@"pdf" subdirectory:@"TestWebKitAPI.resources"]);
}

TEST(WKPDFView, DataForDisplayedPDFEncrypted)
{
    runTest([[NSBundle mainBundle] URLForResource:@"encrypted" withExtension:@"pdf" subdirectory:@"TestWebKitAPI.resources"]);
}

static BOOL isDone;
static const NSUInteger maxCount = 100;

@interface TestFindDelegate : NSObject <_WKFindDelegate>
@property (nonatomic, readonly) NSString *findString;
@property (nonatomic, readonly) NSUInteger matchesCount;
@property (nonatomic, readonly) NSInteger matchIndex;
@property (nonatomic, readonly) BOOL didFail;
@end

@implementation TestFindDelegate {
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

static void loadWebView(WKWebView *webView, TestFindDelegate *findDelegate)
{
    [webView _setFindDelegate:findDelegate];
    NSURL *pdfURL = [[NSBundle mainBundle] URLForResource:@"find" withExtension:@"pdf" subdirectory:@"TestWebKitAPI.resources"];
    [webView loadRequest:[NSURLRequest requestWithURL:pdfURL]];
    [webView _test_waitForDidFinishNavigation];
}

TEST(WKPDFView, CountString)
{
    auto webView = adoptNS([[WKWebView alloc] init]);
    auto findDelegate = adoptNS([[TestFindDelegate alloc] init]);
    loadWebView(webView.get(), findDelegate.get());

    NSString *expectedString = @"Two";
    [webView _countStringMatches:expectedString options:0 maxCount:maxCount];
    TestWebKitAPI::Util::run(&isDone);

    EXPECT_EQ(1U, [findDelegate matchesCount]);
    EXPECT_FALSE([findDelegate didFail]);
    EXPECT_WK_STREQ(expectedString, [findDelegate findString]);
}

TEST(WKPDFView, CountStringMissing)
{
    auto webView = adoptNS([[WKWebView alloc] init]);
    auto findDelegate = adoptNS([[TestFindDelegate alloc] init]);
    loadWebView(webView.get(), findDelegate.get());

    NSString *expectedString = @"One";
    [webView _countStringMatches:expectedString options:0 maxCount:maxCount];
    TestWebKitAPI::Util::run(&isDone);

    EXPECT_EQ(0U, [findDelegate matchesCount]);
    EXPECT_FALSE([findDelegate didFail]);
    EXPECT_WK_STREQ(expectedString, [findDelegate findString]);
}

TEST(WKPDFView, CountStringCaseInsensitive)
{
    auto webView = adoptNS([[WKWebView alloc] init]);
    auto findDelegate = adoptNS([[TestFindDelegate alloc] init]);
    loadWebView(webView.get(), findDelegate.get());

    NSString *expectedString = @"t";
    [webView _countStringMatches:expectedString options:_WKFindOptionsCaseInsensitive maxCount:maxCount];
    TestWebKitAPI::Util::run(&isDone);

    EXPECT_EQ(5U, [findDelegate matchesCount]);
    EXPECT_FALSE([findDelegate didFail]);
    EXPECT_WK_STREQ(expectedString, [findDelegate findString]);
}

TEST(WKPDFView, FindString)
{
    auto webView = adoptNS([[WKWebView alloc] init]);
    auto findDelegate = adoptNS([[TestFindDelegate alloc] init]);
    loadWebView(webView.get(), findDelegate.get());

    NSString *expectedString = @"one";
    [webView _findString:expectedString options:0 maxCount:maxCount];
    TestWebKitAPI::Util::run(&isDone);

    EXPECT_EQ(1U, [findDelegate matchesCount]);
    EXPECT_EQ(0, [findDelegate matchIndex]);
    EXPECT_FALSE([findDelegate didFail]);
    EXPECT_WK_STREQ(expectedString, [findDelegate findString]);
}

TEST(WKPDFView, FindStringMissing)
{
    auto webView = adoptNS([[WKWebView alloc] init]);
    auto findDelegate = adoptNS([[TestFindDelegate alloc] init]);
    loadWebView(webView.get(), findDelegate.get());

    NSString *expectedString = @"One";
    [webView _findString:expectedString options:0 maxCount:maxCount];
    TestWebKitAPI::Util::run(&isDone);

    EXPECT_TRUE([findDelegate didFail]);
    EXPECT_WK_STREQ(expectedString, [findDelegate findString]);
}

TEST(WKPDFView, FindStringCaseInsensitive)
{
    auto webView = adoptNS([[WKWebView alloc] init]);
    auto findDelegate = adoptNS([[TestFindDelegate alloc] init]);
    loadWebView(webView.get(), findDelegate.get());

    NSString *expectedString = @"t";
    [webView _findString:expectedString options:_WKFindOptionsCaseInsensitive maxCount:maxCount];
    TestWebKitAPI::Util::run(&isDone);

    EXPECT_EQ(5U, [findDelegate matchesCount]);
    EXPECT_EQ(0, [findDelegate matchIndex]);
    EXPECT_FALSE([findDelegate didFail]);
    EXPECT_WK_STREQ(expectedString, [findDelegate findString]);

    isDone = NO;
    [webView _findString:expectedString options:_WKFindOptionsCaseInsensitive maxCount:maxCount];
    TestWebKitAPI::Util::run(&isDone);

    EXPECT_EQ(5U, [findDelegate matchesCount]);
    EXPECT_EQ(1, [findDelegate matchIndex]);
    EXPECT_FALSE([findDelegate didFail]);
    EXPECT_WK_STREQ(expectedString, [findDelegate findString]);

    isDone = NO;
    [webView _findString:expectedString options:_WKFindOptionsCaseInsensitive | _WKFindOptionsBackwards maxCount:maxCount];
    TestWebKitAPI::Util::run(&isDone);

    EXPECT_EQ(5U, [findDelegate matchesCount]);
    EXPECT_EQ(0, [findDelegate matchIndex]);
    EXPECT_FALSE([findDelegate didFail]);
    EXPECT_WK_STREQ(expectedString, [findDelegate findString]);
}

TEST(WKPDFView, FindStringBackward)
{
    auto webView = adoptNS([[WKWebView alloc] init]);
    auto findDelegate = adoptNS([[TestFindDelegate alloc] init]);
    loadWebView(webView.get(), findDelegate.get());

    NSString *expectedString = @"t";
    [webView _findString:expectedString options:_WKFindOptionsCaseInsensitive | _WKFindOptionsBackwards maxCount:maxCount];
    TestWebKitAPI::Util::run(&isDone);

    EXPECT_EQ(5U, [findDelegate matchesCount]);
    EXPECT_EQ(4, [findDelegate matchIndex]);
    EXPECT_FALSE([findDelegate didFail]);
    EXPECT_WK_STREQ(expectedString, [findDelegate findString]);

    isDone = NO;
    [webView _findString:expectedString options:_WKFindOptionsCaseInsensitive | _WKFindOptionsBackwards maxCount:maxCount];
    TestWebKitAPI::Util::run(&isDone);

    EXPECT_EQ(5U, [findDelegate matchesCount]);
    EXPECT_EQ(3, [findDelegate matchIndex]);
    EXPECT_FALSE([findDelegate didFail]);
    EXPECT_WK_STREQ(expectedString, [findDelegate findString]);

    isDone = NO;
    [webView _findString:expectedString options:_WKFindOptionsCaseInsensitive maxCount:maxCount];
    TestWebKitAPI::Util::run(&isDone);

    EXPECT_EQ(5U, [findDelegate matchesCount]);
    EXPECT_EQ(4, [findDelegate matchIndex]);
    EXPECT_FALSE([findDelegate didFail]);
    EXPECT_WK_STREQ(expectedString, [findDelegate findString]);
}

TEST(WKPDFView, FindStringPastEnd)
{
    auto webView = adoptNS([[WKWebView alloc] init]);
    auto findDelegate = adoptNS([[TestFindDelegate alloc] init]);
    loadWebView(webView.get(), findDelegate.get());

    NSString *expectedString = @"two";
    [webView _findString:expectedString options:_WKFindOptionsCaseInsensitive maxCount:maxCount];
    TestWebKitAPI::Util::run(&isDone);

    EXPECT_EQ(2U, [findDelegate matchesCount]);
    EXPECT_EQ(0, [findDelegate matchIndex]);
    EXPECT_FALSE([findDelegate didFail]);
    EXPECT_WK_STREQ(expectedString, [findDelegate findString]);

    isDone = NO;
    [webView _findString:expectedString options:_WKFindOptionsCaseInsensitive maxCount:maxCount];
    TestWebKitAPI::Util::run(&isDone);

    EXPECT_EQ(2U, [findDelegate matchesCount]);
    EXPECT_EQ(1, [findDelegate matchIndex]);
    EXPECT_FALSE([findDelegate didFail]);
    EXPECT_WK_STREQ(expectedString, [findDelegate findString]);

    isDone = NO;
    [webView _findString:expectedString options:_WKFindOptionsCaseInsensitive maxCount:maxCount];
    TestWebKitAPI::Util::run(&isDone);

    EXPECT_TRUE([findDelegate didFail]);
    EXPECT_WK_STREQ(expectedString, [findDelegate findString]);
}

TEST(WKPDFView, FindStringBackwardPastStart)
{
    auto webView = adoptNS([[WKWebView alloc] init]);
    auto findDelegate = adoptNS([[TestFindDelegate alloc] init]);
    loadWebView(webView.get(), findDelegate.get());

    NSString *expectedString = @"two";
    [webView _findString:expectedString options:_WKFindOptionsCaseInsensitive | _WKFindOptionsBackwards maxCount:maxCount];
    TestWebKitAPI::Util::run(&isDone);

    EXPECT_EQ(2U, [findDelegate matchesCount]);
    EXPECT_EQ(1, [findDelegate matchIndex]);
    EXPECT_FALSE([findDelegate didFail]);
    EXPECT_WK_STREQ(expectedString, [findDelegate findString]);

    isDone = NO;
    [webView _findString:expectedString options:_WKFindOptionsCaseInsensitive | _WKFindOptionsBackwards maxCount:maxCount];
    TestWebKitAPI::Util::run(&isDone);

    EXPECT_EQ(2U, [findDelegate matchesCount]);
    EXPECT_EQ(0, [findDelegate matchIndex]);
    EXPECT_FALSE([findDelegate didFail]);
    EXPECT_WK_STREQ(expectedString, [findDelegate findString]);

    isDone = NO;
    [webView _findString:expectedString options:_WKFindOptionsCaseInsensitive | _WKFindOptionsBackwards maxCount:maxCount];
    TestWebKitAPI::Util::run(&isDone);

    EXPECT_TRUE([findDelegate didFail]);
    EXPECT_WK_STREQ(expectedString, [findDelegate findString]);
}

#endif
