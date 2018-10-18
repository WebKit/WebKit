/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
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
#import "TestNavigationDelegate.h"
#import <WebKit/WKWebViewPrivate.h>
#import <wtf/RetainPtr.h>

#if WK_API_ENABLED && !PLATFORM(IOS_FAMILY)

typedef enum : NSUInteger {
    NSTextFinderAsynchronousDocumentFindOptionsBackwards = 1 << 0,
    NSTextFinderAsynchronousDocumentFindOptionsWrap = 1 << 1,
} NSTextFinderAsynchronousDocumentFindOptions;

NSTextFinderAsynchronousDocumentFindOptions noFindOptions = (NSTextFinderAsynchronousDocumentFindOptions)0;
NSTextFinderAsynchronousDocumentFindOptions backwardsFindOptions =NSTextFinderAsynchronousDocumentFindOptionsBackwards;
NSTextFinderAsynchronousDocumentFindOptions wrapFindOptions =NSTextFinderAsynchronousDocumentFindOptionsWrap;
NSTextFinderAsynchronousDocumentFindOptions wrapBackwardsFindOptions = (NSTextFinderAsynchronousDocumentFindOptions)(NSTextFinderAsynchronousDocumentFindOptionsWrap | NSTextFinderAsynchronousDocumentFindOptionsBackwards);

@protocol NSTextFinderAsynchronousDocumentFindMatch <NSObject>
@property (retain, nonatomic, readonly) NSArray *textRects;
- (void)generateTextImage:(void (^)(NSImage *generatedImage))completionHandler;
@end

typedef id <NSTextFinderAsynchronousDocumentFindMatch> FindMatch;

@interface WKWebView (NSTextFinderSupport)

- (void)findMatchesForString:(NSString *)targetString relativeToMatch:(FindMatch)relativeMatch findOptions:(NSTextFinderAsynchronousDocumentFindOptions)findOptions maxResults:(NSUInteger)maxResults resultCollector:(void (^)(NSArray *matches, BOOL didWrap))resultCollector;

@end

typedef struct {
    RetainPtr<NSArray> matches;
    BOOL didWrap;
} FindResult;

static FindResult findMatches(WKWebView *webView, NSString *findString, NSTextFinderAsynchronousDocumentFindOptions findOptions = noFindOptions, NSUInteger maxResults = NSUIntegerMax)
{
    __block FindResult result;
    __block bool done = false;

    [webView findMatchesForString:findString relativeToMatch:nil findOptions:findOptions maxResults:maxResults resultCollector:^(NSArray *matches, BOOL didWrap) {
        result.matches = matches;
        result.didWrap = didWrap;
        done = true;
    }];

    TestWebKitAPI::Util::run(&done);

    return result;
}

TEST(WebKit, FindInPage)
{
    RetainPtr<WKWebView> webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 200, 200)]);
    [webView _setOverrideDeviceScaleFactor:2];

    NSURLRequest *request = [NSURLRequest requestWithURL:[[NSBundle mainBundle] URLForResource:@"lots-of-text" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"]];
    [webView loadRequest:request];
    [webView _test_waitForDidFinishNavigation];

    // Find all matches.
    auto result = findMatches(webView.get(), @"Birthday");
    EXPECT_EQ((NSUInteger)360, [result.matches count]);
    RetainPtr<FindMatch> match = [result.matches objectAtIndex:0];
    EXPECT_EQ((NSUInteger)1, [match textRects].count);

    // Ensure that the generated image has the correct DPI.
    __block bool generateTextImageDone = false;
    [match generateTextImage:^(NSImage *image) {
        CGImageRef CGImage = [image CGImageForProposedRect:nil context:nil hints:nil];
        EXPECT_EQ(image.size.width, CGImageGetWidth(CGImage) / 2);
        EXPECT_EQ(image.size.height, CGImageGetHeight(CGImage) / 2);
        generateTextImageDone = true;
    }];
    TestWebKitAPI::Util::run(&generateTextImageDone);

    // Find one match, doing an incremental search.
    result = findMatches(webView.get(), @"Birthday", noFindOptions, 1);
    EXPECT_EQ((NSUInteger)1, [result.matches count]);
    RetainPtr<FindMatch> firstMatch = [result.matches firstObject];
    EXPECT_EQ((NSUInteger)1, [firstMatch textRects].count);

    // Find the next match in incremental mode.
    result = findMatches(webView.get(), @"Birthday", noFindOptions, 1);
    EXPECT_EQ((NSUInteger)1, [result.matches count]);
    RetainPtr<FindMatch> secondMatch = [result.matches firstObject];
    EXPECT_EQ((NSUInteger)1, [secondMatch textRects].count);
    EXPECT_FALSE(NSEqualRects([[firstMatch textRects].lastObject rectValue], [[secondMatch textRects].lastObject rectValue]));

    // Find the previous match in incremental mode.
    result = findMatches(webView.get(), @"Birthday", backwardsFindOptions, 1);
    EXPECT_EQ((NSUInteger)1, [result.matches count]);
    RetainPtr<FindMatch> firstMatchAgain = [result.matches firstObject];
    EXPECT_EQ((NSUInteger)1, [firstMatchAgain textRects].count);
    EXPECT_TRUE(NSEqualRects([[firstMatch textRects].lastObject rectValue], [[firstMatchAgain textRects].lastObject rectValue]));

    // Ensure that we cap the number of matches. There are actually 1600, but we only get the first 1000.
    result = findMatches(webView.get(), @" ");
    EXPECT_EQ((NSUInteger)1000, [result.matches count]);
}

TEST(WebKit, FindInPageWrapping)
{
    RetainPtr<WKWebView> webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 100, 100)]);
    [webView _setOverrideDeviceScaleFactor:2];

    [webView loadHTMLString:@"word word" baseURL:nil];
    [webView _test_waitForDidFinishNavigation];

    // Find one match, doing an incremental search.
    auto result = findMatches(webView.get(), @"word", wrapFindOptions, 1);
    EXPECT_EQ((NSUInteger)1, [result.matches count]);
    EXPECT_FALSE(result.didWrap);

    result = findMatches(webView.get(), @"word", wrapFindOptions, 1);
    EXPECT_EQ((NSUInteger)1, [result.matches count]);
    EXPECT_FALSE(result.didWrap);

    // The next match should wrap.
    result = findMatches(webView.get(), @"word", wrapFindOptions, 1);
    EXPECT_EQ((NSUInteger)1, [result.matches count]);
    EXPECT_TRUE(result.didWrap);

    // Going backward after wrapping should wrap again.
    result = findMatches(webView.get(), @"word", wrapBackwardsFindOptions, 1);
    EXPECT_EQ((NSUInteger)1, [result.matches count]);
    EXPECT_TRUE(result.didWrap);
}

TEST(WebKit, FindInPageWrappingDisabled)
{
    RetainPtr<WKWebView> webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 100, 100)]);
    [webView _setOverrideDeviceScaleFactor:2];

    [webView loadHTMLString:@"word word" baseURL:nil];
    [webView _test_waitForDidFinishNavigation];

    // Find one match, doing an incremental search.
    auto result = findMatches(webView.get(), @"word", noFindOptions, 1);
    EXPECT_EQ((NSUInteger)1, [result.matches count]);
    EXPECT_FALSE(result.didWrap);

    result = findMatches(webView.get(), @"word", noFindOptions, 1);
    EXPECT_EQ((NSUInteger)1, [result.matches count]);
    EXPECT_FALSE(result.didWrap);

    // The next match should fail, because wrapping is disabled.
    result = findMatches(webView.get(), @"word", noFindOptions, 1);
    EXPECT_EQ((NSUInteger)0, [result.matches count]);
    EXPECT_FALSE(result.didWrap);
}

TEST(WebKit, FindInPageWrappingSubframe)
{
    RetainPtr<WKWebView> webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 100, 100)]);
    [webView _setOverrideDeviceScaleFactor:2];

    [webView loadHTMLString:@"word <iframe srcdoc='word'>" baseURL:nil];
    [webView _test_waitForDidFinishNavigation];

    // Find one match, doing an incremental search.
    auto result = findMatches(webView.get(), @"word", wrapFindOptions, 1);
    EXPECT_EQ((NSUInteger)1, [result.matches count]);
    EXPECT_FALSE(result.didWrap);

    result = findMatches(webView.get(), @"word", wrapFindOptions, 1);
    EXPECT_EQ((NSUInteger)1, [result.matches count]);
    EXPECT_FALSE(result.didWrap);

    // The next match should wrap.
    result = findMatches(webView.get(), @"word", wrapFindOptions, 1);
    EXPECT_EQ((NSUInteger)1, [result.matches count]);
    EXPECT_TRUE(result.didWrap);

    // Going backward after wrapping should wrap again.
    result = findMatches(webView.get(), @"word", wrapBackwardsFindOptions, 1);
    EXPECT_EQ((NSUInteger)1, [result.matches count]);
    EXPECT_TRUE(result.didWrap);
}

#endif
