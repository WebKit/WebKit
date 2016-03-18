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
#import <WebKit/WKWebViewPrivate.h>
#import <wtf/RetainPtr.h>

#if WK_API_ENABLED && !PLATFORM(IOS)

typedef enum : NSUInteger {
    NSTextFinderAsynchronousDocumentFindOptionsBackwards = 1 << 0,
} NSTextFinderAsynchronousDocumentFindOptions;

@protocol NSTextFinderAsynchronousDocumentFindMatch <NSObject>
@property (retain, nonatomic, readonly) NSArray *textRects;
- (void)generateTextImage:(void (^)(NSImage *generatedImage))completionHandler;
@end

@interface WKWebView (NSTextFinderSupport)

- (void)findMatchesForString:(NSString *)targetString relativeToMatch:(id <NSTextFinderAsynchronousDocumentFindMatch>)relativeMatch findOptions:(NSTextFinderAsynchronousDocumentFindOptions)findOptions maxResults:(NSUInteger)maxResults resultCollector:(void (^)(NSArray *matches, BOOL didWrap))resultCollector;

@end

static bool navigationDone;
static bool findMatchesDone;

@interface FindInPageNavigationDelegate : NSObject <WKNavigationDelegate>
@end

@implementation FindInPageNavigationDelegate

- (void)webView:(WKWebView *)webView didFinishNavigation:(WKNavigation *)navigation
{
    navigationDone = true;
}

@end

TEST(WebKit2, FindInPage)
{
    RetainPtr<WKWebView> webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 100, 100)]);
    [webView _setOverrideDeviceScaleFactor:2];

    RetainPtr<FindInPageNavigationDelegate> delegate = adoptNS([[FindInPageNavigationDelegate alloc] init]);
    [webView setNavigationDelegate:delegate.get()];

    NSURLRequest *request = [NSURLRequest requestWithURL:[[NSBundle mainBundle] URLForResource:@"lots-of-text" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"]];
    [webView loadRequest:request];

    TestWebKitAPI::Util::run(&navigationDone);

    NSTextFinderAsynchronousDocumentFindOptions noFindOptions = (NSTextFinderAsynchronousDocumentFindOptions)0;

    // Find all matches.
    [webView findMatchesForString:@"Birthday" relativeToMatch:nil findOptions:noFindOptions maxResults:NSUIntegerMax resultCollector:^(NSArray *matches, BOOL didWrap) {
        EXPECT_EQ((NSUInteger)360, matches.count);

        id <NSTextFinderAsynchronousDocumentFindMatch> firstMatch = [matches objectAtIndex:0];
        EXPECT_EQ((NSUInteger)1, firstMatch.textRects.count);

        findMatchesDone = true;
    }];

    TestWebKitAPI::Util::run(&findMatchesDone);
    findMatchesDone = false;

    // Find one match, doing an incremental search.
    [webView findMatchesForString:@"Birthday" relativeToMatch:nil findOptions:noFindOptions maxResults:1 resultCollector:^(NSArray *matches, BOOL didWrap) {
        EXPECT_EQ((NSUInteger)1, matches.count);

        id <NSTextFinderAsynchronousDocumentFindMatch> firstMatch = [matches objectAtIndex:0];
        EXPECT_EQ((NSUInteger)1, firstMatch.textRects.count);

        // Find the next match in incremental mode.
        [webView findMatchesForString:@"Birthday" relativeToMatch:nil findOptions:noFindOptions maxResults:1 resultCollector:^(NSArray *matches, BOOL didWrap) {
            EXPECT_EQ((NSUInteger)1, matches.count);

            id <NSTextFinderAsynchronousDocumentFindMatch> secondMatch = [matches objectAtIndex:0];
            EXPECT_EQ((NSUInteger)1, secondMatch.textRects.count);

            EXPECT_FALSE(NSEqualRects([firstMatch.textRects.lastObject rectValue], [secondMatch.textRects.lastObject rectValue]));

            // Find the previous match in incremental mode.
            [webView findMatchesForString:@"Birthday" relativeToMatch:nil findOptions:NSTextFinderAsynchronousDocumentFindOptionsBackwards maxResults:1 resultCollector:^(NSArray *matches, BOOL didWrap) {
                EXPECT_EQ((NSUInteger)1, matches.count);

                id <NSTextFinderAsynchronousDocumentFindMatch> firstMatchAgain = [matches objectAtIndex:0];
                EXPECT_EQ((NSUInteger)1, firstMatchAgain.textRects.count);

                EXPECT_TRUE(NSEqualRects([firstMatch.textRects.lastObject rectValue], [firstMatchAgain.textRects.lastObject rectValue]));
                
                findMatchesDone = true;
            }];
        }];

    }];

    TestWebKitAPI::Util::run(&findMatchesDone);
    findMatchesDone = false;

    // Ensure that the generated image has the correct DPI.
    [webView findMatchesForString:@"Birthday" relativeToMatch:nil findOptions:noFindOptions maxResults:NSUIntegerMax resultCollector:^(NSArray *matches, BOOL didWrap) {
        EXPECT_EQ((NSUInteger)360, matches.count);

        id <NSTextFinderAsynchronousDocumentFindMatch> firstMatch = [matches objectAtIndex:0];
        [firstMatch generateTextImage:^(NSImage *image) {
            CGImageRef CGImage = [image CGImageForProposedRect:nil context:nil hints:nil];
            EXPECT_EQ(image.size.width, CGImageGetWidth(CGImage) / 2);
            EXPECT_EQ(image.size.height, CGImageGetHeight(CGImage) / 2);

            findMatchesDone = true;
        }];
    }];

    TestWebKitAPI::Util::run(&findMatchesDone);
    findMatchesDone = false;
}

#endif
