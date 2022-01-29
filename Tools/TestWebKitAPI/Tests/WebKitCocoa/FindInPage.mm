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

#import "config.h"

#import "PlatformUtilities.h"
#import "TestNavigationDelegate.h"
#import "TestWKWebView.h"
#import "WKWebViewConfigurationExtras.h"
#import <WebKit/WKWebViewPrivate.h>
#import <wtf/BlockPtr.h>
#import <wtf/RetainPtr.h>

#if PLATFORM(IOS_FAMILY)
#import "UIKitSPI.h"
#endif

#if !PLATFORM(IOS_FAMILY)

typedef enum : NSUInteger {
    NSTextFinderAsynchronousDocumentFindOptionsBackwards = 1 << 0,
    NSTextFinderAsynchronousDocumentFindOptionsWrap = 1 << 1,
} NSTextFinderAsynchronousDocumentFindOptions;

constexpr auto noFindOptions = (NSTextFinderAsynchronousDocumentFindOptions)0;
constexpr auto backwardsFindOptions = NSTextFinderAsynchronousDocumentFindOptionsBackwards;
constexpr auto wrapFindOptions = NSTextFinderAsynchronousDocumentFindOptionsWrap;
constexpr auto wrapBackwardsFindOptions = (NSTextFinderAsynchronousDocumentFindOptions)(NSTextFinderAsynchronousDocumentFindOptionsWrap | NSTextFinderAsynchronousDocumentFindOptionsBackwards);

@protocol NSTextFinderAsynchronousDocumentFindMatch <NSObject>
@property (retain, nonatomic, readonly) NSArray *textRects;
- (void)generateTextImage:(void (^)(NSImage *generatedImage))completionHandler;
@end

typedef id <NSTextFinderAsynchronousDocumentFindMatch> FindMatch;

@interface WKWebView (NSTextFinderSupport)

- (void)findMatchesForString:(NSString *)targetString relativeToMatch:(FindMatch)relativeMatch findOptions:(NSTextFinderAsynchronousDocumentFindOptions)findOptions maxResults:(NSUInteger)maxResults resultCollector:(void (^)(NSArray *matches, BOOL didWrap))resultCollector;
- (void)replaceMatches:(NSArray<FindMatch> *)matches withString:(NSString *)replacementString inSelectionOnly:(BOOL)selectionOnly resultCollector:(void (^)(NSUInteger replacementCount))resultCollector;

@end

struct FindResult {
    RetainPtr<NSArray> matches;
    BOOL didWrap { NO };
};

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

static NSUInteger replaceMatches(WKWebView *webView, NSArray<FindMatch> *matchesToReplace, NSString *replacementText)
{
    __block NSUInteger result;
    __block bool done = false;

    [webView replaceMatches:matchesToReplace withString:replacementText inSelectionOnly:NO resultCollector:^(NSUInteger replacementCount) {
        result = replacementCount;
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

TEST(WebKit, FindInPageWithPlatformPresentation)
{
    // This should be the same as above, but does not generate rects or images, so that AppKit won't paint its find UI.

    RetainPtr<WKWebView> webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 200, 200)]);
    [webView _setOverrideDeviceScaleFactor:2];
    [webView _setUsePlatformFindUI:NO];

    NSURLRequest *request = [NSURLRequest requestWithURL:[[NSBundle mainBundle] URLForResource:@"lots-of-text" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"]];
    [webView loadRequest:request];
    [webView _test_waitForDidFinishNavigation];

    // Find all matches, but recieve no rects.
    auto result = findMatches(webView.get(), @"Birthday");
    EXPECT_EQ((NSUInteger)360, [result.matches count]);
    RetainPtr<FindMatch> match = [result.matches objectAtIndex:0];
    EXPECT_EQ((NSUInteger)0, [match textRects].count);

    // Ensure that no image is generated.
    __block bool generateTextImageDone = false;
    [match generateTextImage:^(NSImage *image) {
        EXPECT_EQ(image, nullptr);
        generateTextImageDone = true;
    }];
    TestWebKitAPI::Util::run(&generateTextImageDone);

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

TEST(WebKit, FindInPageBackwardsFirst)
{
    RetainPtr<WKWebView> webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 100, 100)]);
    [webView _setOverrideDeviceScaleFactor:2];

    [webView loadHTMLString:@"word word" baseURL:nil];
    [webView _test_waitForDidFinishNavigation];

    // Find one match, doing an incremental search.
    auto result = findMatches(webView.get(), @"word", wrapBackwardsFindOptions, 1);
    EXPECT_EQ((NSUInteger)1, [result.matches count]);

    result = findMatches(webView.get(), @"word", wrapBackwardsFindOptions, 1);
    EXPECT_EQ((NSUInteger)1, [result.matches count]);
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

TEST(WebKit, FindAndReplace)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 400, 400)]);
    [webView synchronouslyLoadHTMLString:@"<body contenteditable><input id='first' value='hello'>hello world<input id='second' value='world'></body>"];

    auto result = findMatches(webView.get(), @"hello");
    EXPECT_EQ(2U, [result.matches count]);
    EXPECT_EQ(2U, replaceMatches(webView.get(), result.matches.get(), @"hi"));
    EXPECT_WK_STREQ("hi", [webView stringByEvaluatingJavaScript:@"first.value"]);
    EXPECT_WK_STREQ("world", [webView stringByEvaluatingJavaScript:@"second.value"]);
    EXPECT_WK_STREQ("hi world", [webView stringByEvaluatingJavaScript:@"document.body.textContent"]);

    result = findMatches(webView.get(), @"world");
    EXPECT_EQ(2U, [result.matches count]);
    EXPECT_EQ(1U, replaceMatches(webView.get(), @[ [result.matches firstObject] ], @"hi"));
    EXPECT_WK_STREQ("hi", [webView stringByEvaluatingJavaScript:@"first.value"]);
    EXPECT_WK_STREQ("world", [webView stringByEvaluatingJavaScript:@"second.value"]);
    EXPECT_WK_STREQ("hi hi", [webView stringByEvaluatingJavaScript:@"document.body.textContent"]);

    result = findMatches(webView.get(), @"world");
    EXPECT_EQ(1U, [result.matches count]);
    EXPECT_EQ(1U, replaceMatches(webView.get(), @[ [result.matches firstObject] ], @"hi"));
    EXPECT_WK_STREQ("hi", [webView stringByEvaluatingJavaScript:@"first.value"]);
    EXPECT_WK_STREQ("hi", [webView stringByEvaluatingJavaScript:@"second.value"]);
    EXPECT_WK_STREQ("hi hi", [webView stringByEvaluatingJavaScript:@"document.body.textContent"]);
}

#if ENABLE(IMAGE_ANALYSIS)

TEST(WebKit, FindTextInImageOverlay)
{
    auto configuration = retainPtr([WKWebViewConfiguration _test_configurationWithTestPlugInClassName:@"WebProcessPlugInWithInternals" configureJSCForTesting:YES]);
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 400, 400) configuration:configuration.get()]);
    [webView synchronouslyLoadTestPageNamed:@"simple-image-overlay"];
    {
        auto [matches, didWrap] = findMatches(webView.get(), @"foobar");
        EXPECT_EQ(1U, [matches count]);
        EXPECT_FALSE(didWrap);
    }

    [webView evaluateJavaScript:@"document.body.appendChild(document.createTextNode('foobar'))" completionHandler:nil];

    {
        auto [matches, didWrap] = findMatches(webView.get(), @"foobar");
        EXPECT_EQ(2U, [matches count]);
        EXPECT_FALSE(didWrap);
    }
}

#endif // ENABLE(IMAGE_ANALYSIS)

#endif // !PLATFORM(IOS_FAMILY)

#if HAVE(UIFINDINTERACTION)

@interface TestTextSearchOptions : NSObject
@property (nonatomic) _UITextSearchMatchMethod wordMatchMethod;
@property (nonatomic) NSStringCompareOptions stringCompareOptions;
@end

@implementation TestTextSearchOptions
@end

@interface TestSearchAggregator : NSObject <_UITextSearchAggregator>

@property (readonly) NSUInteger count;
@property (nonatomic, readonly) NSArray<UITextRange *> *foundRanges;

- (instancetype)initWithCompletionHandler:(dispatch_block_t)completionHandler;

@end

@implementation TestSearchAggregator {
    RetainPtr<NSMutableArray<UITextRange *>> _foundRanges;
    BlockPtr<void()> _completionHandler;
}

- (instancetype)initWithCompletionHandler:(dispatch_block_t)completionHandler
{
    if (!(self = [super init]))
        return nil;

    _foundRanges = adoptNS([[NSMutableArray alloc] init]);
    _completionHandler = makeBlockPtr(completionHandler);

    return self;
}

- (void)foundRange:(UITextRange *)range forSearchString:(NSString *)string inDocument:(_UITextSearchDocumentIdentifier)document
{
    [_foundRanges addObject:range];
}

- (void)finishedSearching
{
    if (_completionHandler)
        _completionHandler();
}

- (NSArray<UITextRange *>*)foundRanges
{
    return _foundRanges.get();
}

- (NSUInteger)count
{
    return [_foundRanges count];
}

@end

static void testPerformTextSearchWithQueryStringInWebView(WKWebView *webView, NSString *query, TestTextSearchOptions *searchOptions, NSUInteger expectedMatches)
{
    __block bool finishedSearching = false;
    RetainPtr aggregator = adoptNS([[TestSearchAggregator alloc] initWithCompletionHandler:^{
        finishedSearching = true;
    }]);

    // FIXME: (rdar://86140914) Use _UITextSearchOptions directly when the symbol is exported.
    [webView performTextSearchWithQueryString:query usingOptions:(_UITextSearchOptions *)searchOptions resultAggregator:aggregator.get()];

    TestWebKitAPI::Util::run(&finishedSearching);

    EXPECT_EQ([aggregator count], expectedMatches);
}

static RetainPtr<NSArray<UITextRange *>> textRangesForQueryString(WKWebView *webView, NSString *query)
{
    __block bool finishedSearching = false;
    auto aggregator = adoptNS([[TestSearchAggregator alloc] initWithCompletionHandler:^{
        finishedSearching = true;
    }]);

    // FIXME: (rdar://86140914) Use _UITextSearchOptions directly when the symbol is exported.
    auto options = adoptNS([[TestTextSearchOptions alloc] init]);
    [webView performTextSearchWithQueryString:query usingOptions:(_UITextSearchOptions *)options.get() resultAggregator:aggregator.get()];

    TestWebKitAPI::Util::run(&finishedSearching);

    return adoptNS([[aggregator foundRanges] copy]);
}

TEST(WebKit, FindInPage)
{
    auto webView = adoptNS([[WKWebView alloc] initWithFrame:CGRectMake(0, 0, 200, 200)]);

    NSURLRequest *request = [NSURLRequest requestWithURL:[[NSBundle mainBundle] URLForResource:@"lots-of-text" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"]];
    [webView loadRequest:request];
    [webView _test_waitForDidFinishNavigation];

    RetainPtr searchOptions = adoptNS([[TestTextSearchOptions alloc] init]);
    testPerformTextSearchWithQueryStringInWebView(webView.get(), @"Birthday", searchOptions.get(), 360UL);
}

TEST(WebKit, FindInPageCaseInsensitive)
{
    auto webView = adoptNS([[WKWebView alloc] initWithFrame:CGRectMake(0, 0, 200, 200)]);

    NSURLRequest *request = [NSURLRequest requestWithURL:[[NSBundle mainBundle] URLForResource:@"lots-of-text" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"]];
    [webView loadRequest:request];
    [webView _test_waitForDidFinishNavigation];

    RetainPtr searchOptions = adoptNS([[TestTextSearchOptions alloc] init]);
    testPerformTextSearchWithQueryStringInWebView(webView.get(), @"birthday", searchOptions.get(), 0UL);

    [searchOptions setStringCompareOptions:NSCaseInsensitiveSearch];
    testPerformTextSearchWithQueryStringInWebView(webView.get(), @"birthday", searchOptions.get(), 360UL);
}

TEST(WebKit, FindInPageStartsWith)
{
    auto webView = adoptNS([[WKWebView alloc] initWithFrame:CGRectMake(0, 0, 200, 200)]);

    NSURLRequest *request = [NSURLRequest requestWithURL:[[NSBundle mainBundle] URLForResource:@"lots-of-text" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"]];
    [webView loadRequest:request];
    [webView _test_waitForDidFinishNavigation];

    RetainPtr searchOptions = adoptNS([[TestTextSearchOptions alloc] init]);

    testPerformTextSearchWithQueryStringInWebView(webView.get(), @"Birth", searchOptions.get(), 360UL);
    testPerformTextSearchWithQueryStringInWebView(webView.get(), @"day", searchOptions.get(), 360UL);

    [searchOptions setWordMatchMethod:_UITextSearchMatchMethodStartsWith];

    testPerformTextSearchWithQueryStringInWebView(webView.get(), @"Birth", searchOptions.get(), 360UL);
    testPerformTextSearchWithQueryStringInWebView(webView.get(), @"day", searchOptions.get(), 0UL);
}

TEST(WebKit, FindInPageFullWord)
{
    auto webView = adoptNS([[WKWebView alloc] initWithFrame:CGRectMake(0, 0, 200, 200)]);

    NSURLRequest *request = [NSURLRequest requestWithURL:[[NSBundle mainBundle] URLForResource:@"lots-of-text" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"]];
    [webView loadRequest:request];
    [webView _test_waitForDidFinishNavigation];

    RetainPtr searchOptions = adoptNS([[TestTextSearchOptions alloc] init]);

    testPerformTextSearchWithQueryStringInWebView(webView.get(), @"Birth", searchOptions.get(), 360UL);

    [searchOptions setWordMatchMethod:_UITextSearchMatchMethodFullWord];

    testPerformTextSearchWithQueryStringInWebView(webView.get(), @"Birthday", searchOptions.get(), 360UL);
    testPerformTextSearchWithQueryStringInWebView(webView.get(), @"Birth", searchOptions.get(), 0UL);
}

TEST(WebKit, FindInteraction)
{
    auto webView = adoptNS([[WKWebView alloc] initWithFrame:CGRectMake(0, 0, 200, 200)]);

    EXPECT_NULL([webView _findInteraction]);

    [webView _setFindInteractionEnabled:YES];
    EXPECT_NOT_NULL([webView _findInteraction]);

    [webView _setFindInteractionEnabled:NO];
    EXPECT_NULL([webView _findInteraction]);
}

TEST(WebKit, RequestRectForFoundTextRange)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600)]);
    [webView synchronouslyLoadHTMLString:@"<iframe srcdoc='<p>Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed do eiusmod tempor incididunt ut labore et dolore magna aliqua. Tellus in metus vulputate eu scelerisque felis imperdiet. Mi quis hendrerit dolor magna eget est lorem ipsum dolor. In cursus turpis massa tincidunt dui ut ornare. Sapien et ligula ullamcorper malesuada. Maecenas volutpat blandit aliquam etiam erat. Turpis egestas integer eget aliquet nibh praesent tristique. Ipsum dolor sit amet consectetur adipiscing. Tellus cras adipiscing enim eu turpis egestas pretium aenean pharetra. Sem fringilla ut morbi tincidunt augue interdum velit euismod. Habitant morbi tristique senectus et netus. Aenean euismod elementum nisi quis. Facilisi nullam vehicula ipsum a. Elementum facilisis leo vel fringilla. Molestie nunc non blandit massa enim. Orci ac auctor augue mauris. Pellentesque pulvinar pellentesque habitant morbi tristique senectus et. Magnis dis parturient montes nascetur ridiculus mus mauris vitae. Id leo in vitae turpis massa sed. Netus et malesuada fames ac turpis egestas sed tempus. Morbi quis commodo odio aenean sed adipiscing diam donec. Sit amet purus gravida quis blandit turpis. Odio euismod lacinia at quis risus sed vulputate. Varius duis at consectetur lorem donec massa. Sit amet consectetur adipiscing elit pellentesque habitant. Feugiat in fermentum posuere urna nec tincidunt praesent.</p>'></iframe>"];

    auto ranges = textRangesForQueryString(webView.get(), @"Sapien");

    __block bool done = false;
    [webView _requestRectForFoundTextRange:[ranges firstObject] completionHandler:^(CGRect rect) {
        EXPECT_TRUE(CGRectEqualToRect(rect, CGRectMake(252, 146, 44, 19)));
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);

    ranges = textRangesForQueryString(webView.get(), @"fermentum");

    done = false;
    [webView _requestRectForFoundTextRange:[ranges firstObject] completionHandler:^(CGRect rect) {
        EXPECT_TRUE(CGRectEqualToRect(rect, CGRectMake(229, 646, 72, 19)));
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);

    [webView scrollRangeToVisible:[ranges firstObject] inDocument:nil];
    done = false;
    [webView _requestRectForFoundTextRange:[ranges firstObject] completionHandler:^(CGRect rect) {
        EXPECT_TRUE(CGRectEqualToRect(rect, CGRectMake(229, 104, 72, 19)));
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
}

#endif // HAVE(UIFINDINTERACTION)
