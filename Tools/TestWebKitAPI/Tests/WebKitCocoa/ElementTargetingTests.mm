/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
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

#import "CGImagePixelReader.h"
#import "PlatformUtilities.h"
#import "TestUIDelegate.h"
#import "TestWKWebView.h"
#import <WebKit/WKFrameInfoPrivate.h>
#import <WebKit/WKWebViewPrivate.h>
#import <WebKit/_WKFrameTreeNode.h>
#import <WebKit/_WKTargetedElementInfo.h>
#import <WebKit/_WKTargetedElementRequest.h>

@interface _WKTargetedElementInfo (TestWebKitAPI)
- (CGImageRef)takeSnapshot;
@end

@implementation _WKTargetedElementInfo (TestWebKitAPI)

- (CGImageRef)takeSnapshot
{
    __block bool done = false;
    __block RetainPtr<CGImageRef> result;
    [self takeSnapshotWithCompletionHandler:^(CGImageRef image) {
        result = image;
        done = true;
    }];
    Util::run(&done);
    return result.autorelease();
}

@end

@interface WKWebView (ElementTargeting)

- (NSArray<_WKTargetedElementInfo *> *)targetedElementInfoAt:(CGPoint)point;
- (NSArray<_WKTargetedElementInfo *> *)targetedElementInfoWithText:(NSString *)searchText;
- (NSArray<_WKTargetedElementInfo *> *)targetedElementInfoWithSelectors:(NSArray<NSSet<NSString *> *> *)selectors;
- (BOOL)adjustVisibilityForTargets:(NSArray<_WKTargetedElementInfo *> *)targets;
- (BOOL)resetVisibilityAdjustmentsForTargets:(NSArray<_WKTargetedElementInfo *> *)elements;
- (void)expectSingleTargetedSelector:(NSString *)expectedSelector at:(CGPoint)point;
#if PLATFORM(VISION)
- (NSArray<NSArray<_WKTargetedElementInfo *> *> *)allTargetableElementsWithHitTestInterval:(CGFloat)hitTestInterval;
#endif

@property (nonatomic, readonly) NSUInteger numberOfVisibilityAdjustmentRects;

@end

@implementation WKWebView (ElementTargeting)

- (NSArray<_WKTargetedElementInfo *> *)targetedElementInfo:(_WKTargetedElementRequest *)request
{
    __block RetainPtr<NSArray<_WKTargetedElementInfo *>> result;
    __block bool done = false;
    [self _requestTargetedElementInfo:request completionHandler:^(NSArray<_WKTargetedElementInfo *> *elements) {
        result = elements;
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
    return result.autorelease();
}

- (NSArray<_WKTargetedElementInfo *> *)targetedElementInfoAt:(CGPoint)point
{
    auto request = adoptNS([[_WKTargetedElementRequest alloc] initWithPoint:point]);
    return [self targetedElementInfo:request.get()];
}

- (NSArray<_WKTargetedElementInfo *> *)targetedElementInfoWithText:(NSString *)searchText
{
    auto request = adoptNS([[_WKTargetedElementRequest alloc] initWithSearchText:searchText]);
    return [self targetedElementInfo:request.get()];
}

- (NSArray<_WKTargetedElementInfo *> *)targetedElementInfoWithSelectors:(NSArray<NSSet<NSString *> *> *)selectors
{
    auto request = adoptNS([[_WKTargetedElementRequest alloc] initWithSelectors:selectors]);
    return [self targetedElementInfo:request.get()];
}

- (BOOL)adjustVisibilityForTargets:(NSArray<_WKTargetedElementInfo *> *)targets
{
    __block BOOL result = NO;
    __block bool done = false;
    [self _adjustVisibilityForTargetedElements:targets completionHandler:^(BOOL success) {
        result = success;
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
    return result;
}

- (NSUInteger)numberOfVisibilityAdjustmentRects
{
    __block NSUInteger result = 0;
    __block bool done = false;
    [self _numberOfVisibilityAdjustmentRectsWithCompletionHandler:^(NSUInteger count) {
        result = count;
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
    return result;
}

- (BOOL)resetVisibilityAdjustmentsForTargets:(NSArray<_WKTargetedElementInfo *> *)elements
{
    __block BOOL result = NO;
    __block bool done = false;
    [self _resetVisibilityAdjustmentsForTargetedElements:elements completionHandler:^(BOOL success) {
        result = success;
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
    return result;
}

- (void)expectSingleTargetedSelector:(NSString *)expectedSelector at:(CGPoint)point
{
    RetainPtr elements = [self targetedElementInfoAt:point];
    EXPECT_EQ([elements count], 1U);
    NSString *preferredSelector = [elements firstObject].selectors.firstObject;
    EXPECT_WK_STREQ(preferredSelector, expectedSelector);
}

#if PLATFORM(VISION)
- (NSArray<NSArray<_WKTargetedElementInfo *> *> *)allTargetableElementsWithHitTestInterval:(CGFloat)hitTestInterval
{
    __block RetainPtr<NSArray<NSArray<_WKTargetedElementInfo *> *>> result;
    __block bool done = false;
    [self _requestAllTargetableElementsInfo:hitTestInterval completionHandler:^(NSArray<NSArray<_WKTargetedElementInfo *> *> *elements) {
        result = elements;
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
    return result.autorelease();
}
#endif // PLATFORM(VISION)

@end

@interface _WKTargetedElementInfo (TestingAdditions)
@property (nonatomic, readonly) NSArray<_WKFrameTreeNode *> *childFrames;
@end

@implementation _WKTargetedElementInfo (TestingAdditions)

- (NSArray<_WKFrameTreeNode *> *)childFrames
{
    __block RetainPtr<NSArray<_WKFrameTreeNode *>> result;
    __block bool done = false;
    [self getChildFrames:^(NSArray<_WKFrameTreeNode *> *frames) {
        result = frames;
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
    return result.autorelease();
}

@end

namespace TestWebKitAPI {

TEST(ElementTargeting, BasicElementTargeting)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 800, 600)]);
    [webView synchronouslyLoadTestPageNamed:@"element-targeting-1"];

    Util::waitForConditionWithLogging([&] {
        return [[webView objectByEvaluatingJavaScript:@"window.subframeLoaded"] boolValue];
    }, 5, @"Timed out waiting for subframes to finish loading.");

    RetainPtr elements = [webView targetedElementInfoAt:CGPointMake(150, 150)];
    EXPECT_EQ([elements count], 3U);
    {
        auto element = [elements objectAtIndex:0];
        EXPECT_EQ(element.positionType, _WKTargetedElementPositionFixed);
        EXPECT_WK_STREQ("DIV.fixed.container", element.selectors.firstObject);
        EXPECT_TRUE([element.renderedText containsString:@"The round pegs"]);
        EXPECT_EQ(element.renderedText.length, 70U);
        EXPECT_EQ(element.offsetEdges, _WKRectEdgeLeft | _WKRectEdgeTop);

        RetainPtr childFrames = [element childFrames];
        EXPECT_EQ([childFrames count], 1U);

        auto childFrame = [childFrames firstObject];
        EXPECT_FALSE(childFrame.info.mainFrame);
        EXPECT_WK_STREQ(childFrame.info.request.URL.lastPathComponent, "nested-frames.html");
        EXPECT_WK_STREQ(childFrame.info._title, "Outer Subframe");
        EXPECT_EQ(childFrame.childFrames.count, 1U);

        auto nestedChildFrame = childFrame.childFrames.firstObject;
        EXPECT_FALSE(nestedChildFrame.info.mainFrame);
        EXPECT_FALSE(nestedChildFrame.info.mainFrame);
        EXPECT_WK_STREQ(nestedChildFrame.info.request.URL.scheme, "about");
        EXPECT_WK_STREQ(nestedChildFrame.info._title, "Inner Subframe");
        EXPECT_EQ(nestedChildFrame.childFrames.count, 0U);
    }
    {
        auto element = [elements objectAtIndex:1];
        EXPECT_EQ(element.positionType, _WKTargetedElementPositionAbsolute);
        EXPECT_WK_STREQ("#absolute", element.selectors.firstObject);
        EXPECT_TRUE([element.renderedText containsString:@"the crazy ones"]);
        EXPECT_EQ(element.renderedText.length, 64U);
        EXPECT_EQ(element.offsetEdges, _WKRectEdgeRight | _WKRectEdgeBottom);
        EXPECT_EQ(element.childFrames.count, 0U);
    }
    {
        auto element = [elements objectAtIndex:2];
        EXPECT_EQ(element.positionType, _WKTargetedElementPositionStatic);
        EXPECT_WK_STREQ("MAIN > SECTION:first-of-type", element.selectors.firstObject);
        EXPECT_TRUE([element.renderedText containsString:@"Lorem ipsum"]);
        EXPECT_EQ(element.renderedText.length, 896U);
        EXPECT_EQ(element.offsetEdges, _WKRectEdgeNone);
        EXPECT_EQ(element.childFrames.count, 0U);
    }
}

TEST(ElementTargeting, DoNotIgnorePointerEventsNone)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 800, 600)]);
    [webView synchronouslyLoadTestPageNamed:@"element-targeting-1"];

    Util::waitForConditionWithLogging([&] {
        return [[webView objectByEvaluatingJavaScript:@"window.subframeLoaded"] boolValue];
    }, 5, @"Timed out waiting for subframes to finish loading.");

    RetainPtr request = adoptNS([[_WKTargetedElementRequest alloc] initWithPoint:CGPointMake(150, 150)]);
    [request setShouldIgnorePointerEventsNone:NO];

    RetainPtr targets = [webView targetedElementInfo:request.get()];
    EXPECT_EQ([targets count], 2U);
    EXPECT_WK_STREQ("#absolute", [targets firstObject].selectors[0]);
    EXPECT_WK_STREQ("MAIN > SECTION:first-of-type", [targets lastObject].selectors[0]);
}

TEST(ElementTargeting, NearbyOutOfFlowElements)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 800, 600)]);
    [webView synchronouslyLoadTestPageNamed:@"element-targeting-2"];

    RetainPtr elements = [webView targetedElementInfoAt:CGPointMake(100, 100)];
    EXPECT_EQ([elements count], 5U);
    EXPECT_FALSE([elements objectAtIndex:0].nearbyTarget);
    EXPECT_FALSE([elements objectAtIndex:1].nearbyTarget);
    EXPECT_TRUE([elements objectAtIndex:2].nearbyTarget);
    EXPECT_TRUE([elements objectAtIndex:3].nearbyTarget);
    EXPECT_TRUE([elements objectAtIndex:4].nearbyTarget);
    // The two elements that are directly hit-tested should take precedence over nearby elements.
    EXPECT_WK_STREQ("DIV.fixed.container", [elements firstObject].selectors.firstObject);
    EXPECT_WK_STREQ("DIV.box", [elements objectAtIndex:1].selectors.firstObject);
    __auto_type nextThreeSelectors = [NSSet setWithArray:@[
        [elements objectAtIndex:2].selectors.firstObject,
        [elements objectAtIndex:3].selectors.firstObject,
        [elements objectAtIndex:4].selectors.firstObject,
    ]];
    EXPECT_TRUE([nextThreeSelectors containsObject:@"DIV.absolute.top-right"]);
    EXPECT_TRUE([nextThreeSelectors containsObject:@"DIV.absolute.bottom-left"]);
    EXPECT_TRUE([nextThreeSelectors containsObject:@"DIV.absolute.bottom-right"]);

    [webView adjustVisibilityForTargets:elements.get()];
    EXPECT_EQ([webView numberOfVisibilityAdjustmentRects], 1U);

    [webView resetVisibilityAdjustmentsForTargets:elements.get()];
    EXPECT_EQ([webView numberOfVisibilityAdjustmentRects], 0U);
}

static std::pair<RetainPtr<TestWKWebView>, RetainPtr<Util::PlatformWindow>> setUpWebViewForSnapshotting(CGRect frame)
{
#if PLATFORM(IOS_FAMILY)
    auto configuration = adoptNS([WKWebViewConfiguration new]);
    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:frame configuration:configuration.get() addToWindow:NO]);
    RetainPtr window = adoptNS([[UIWindow alloc] initWithFrame:frame]);
    [window addSubview:webView.get()];
    return { WTFMove(webView), WTFMove(window) };
#else
    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:frame]);
    return { WTFMove(webView), { [webView window] } };
#endif
}

TEST(ElementTargeting, AdjustVisibilityForUnparentedElement)
{
    auto webViewFrame = CGRectMake(0, 0, 800, 600);

    auto viewAndWindow = setUpWebViewForSnapshotting(webViewFrame);
    auto [webView, window] = viewAndWindow;
    [webView synchronouslyLoadTestPageNamed:@"element-targeting-2"];

    auto setOverlaysParented = [&](bool visible) {
        [viewAndWindow.first objectByEvaluatingJavaScript:visible ? @"addOverlays()" : @"removeOverlays()"];
    };

    RetainPtr elements = [webView targetedElementInfoAt:CGPointMake(100, 100)];
    setOverlaysParented(false);
    [webView targetedElementInfoAt:CGPointMake(100, 100)];
    [webView adjustVisibilityForTargets:elements.get()];
    setOverlaysParented(true);

    elements = [webView targetedElementInfoAt:CGPointMake(100, 100)];
    setOverlaysParented(false);
    [webView targetedElementInfoAt:CGPointMake(100, 100)];
    [webView adjustVisibilityForTargets:elements.get()];
    setOverlaysParented(true);

    [webView waitForNextPresentationUpdate];
    RetainPtr snapshot = [webView snapshotAfterScreenUpdates];
    CGImagePixelReader pixelReader { snapshot.get() };

    auto x = static_cast<unsigned>(100 * (pixelReader.width() / CGRectGetWidth(webViewFrame)));
    auto y = static_cast<unsigned>(100 * (pixelReader.height() / CGRectGetHeight(webViewFrame)));
    EXPECT_EQ(pixelReader.at(x, y), WebCore::Color::white);
}

TEST(ElementTargeting, AdjustVisibilityFromSelectors)
{
    auto webViewFrame = CGRectMake(0, 0, 800, 600);

    auto [webView, window] = setUpWebViewForSnapshotting(webViewFrame);
    RetainPtr preferences = adoptNS([WKWebpagePreferences new]);
    [preferences _setVisibilityAdjustmentSelectors:[NSSet setWithObjects:
        @".fixed.container"
        , @".absolute.bottom-right"
        , @".absolute.bottom-left"
        , @".absolute.top-right"
        , nil]];

    RetainPtr delegate = adoptNS([TestUIDelegate new]);
    RetainPtr adjustedSelectors = adoptNS([NSMutableSet new]);
    [delegate setWebViewDidAdjustVisibilityWithSelectors:^(WKWebView *, NSArray<NSString *> *selectors) {
        [adjustedSelectors addObjectsFromArray:selectors];
    }];
    [webView setUIDelegate:delegate.get()];
    [webView synchronouslyLoadTestPageNamed:@"element-targeting-2" preferences:preferences.get()];
    [webView waitForNextPresentationUpdate];
    {
        RetainPtr snapshot = [webView snapshotAfterScreenUpdates];
        CGImagePixelReader pixelReader { snapshot.get() };
        auto x = static_cast<unsigned>(100 * (pixelReader.width() / CGRectGetWidth(webViewFrame)));
        auto y = static_cast<unsigned>(100 * (pixelReader.height() / CGRectGetHeight(webViewFrame)));
        EXPECT_EQ(pixelReader.at(x, y), WebCore::Color::white);
        EXPECT_EQ([webView numberOfVisibilityAdjustmentRects], 1U);
        EXPECT_TRUE([adjustedSelectors containsObject:@".absolute.top-right"]);
        EXPECT_TRUE([adjustedSelectors containsObject:@".absolute.bottom-right"]);
        EXPECT_TRUE([adjustedSelectors containsObject:@".fixed.container"]);
        EXPECT_TRUE([adjustedSelectors containsObject:@".absolute.bottom-left"]);

        [webView objectByEvaluatingJavaScript:@"[...document.querySelectorAll('.fixed,.absolute')].map(e => e.style.display = 'none')"];
        [webView waitForNextPresentationUpdate];
        EXPECT_GT([webView numberOfVisibilityAdjustmentRects], 0U);
        [webView objectByEvaluatingJavaScript:@"[...document.querySelectorAll('.fixed,.absolute')].map(e => e.style.display = '')"];
        [webView waitForNextPresentationUpdate];
    }

    [webView resetVisibilityAdjustmentsForTargets:nil];
    [webView waitForNextPresentationUpdate];
    {
        RetainPtr snapshot = [webView snapshotAfterScreenUpdates];
        CGImagePixelReader pixelReader { snapshot.get() };
        auto x = static_cast<unsigned>(100 * (pixelReader.width() / CGRectGetWidth(webViewFrame)));
        auto y = static_cast<unsigned>(100 * (pixelReader.height() / CGRectGetHeight(webViewFrame)));
        EXPECT_FALSE(pixelReader.at(x, y) == WebCore::Color::white);
        EXPECT_EQ([webView numberOfVisibilityAdjustmentRects], 0U);
    }
}

TEST(ElementTargeting, RequestElementsFromSelectors)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 600, 480)]);

    RetainPtr preferences = adoptNS([WKWebpagePreferences new]);
    [preferences _setVisibilityAdjustmentSelectors:[NSSet setWithObjects:
        @".fixed.container"
        , @"DIV.absolute.bottom-right"
        , @"DIV.absolute.bottom-left"
        , @"DIV.absolute.top-right"
        , nil]];

    RetainPtr delegate = adoptNS([TestUIDelegate new]);
    __block bool didAdjustVisibility = false;
    [delegate setWebViewDidAdjustVisibilityWithSelectors:^(WKWebView *, NSArray<NSString *> *selectors) {
        didAdjustVisibility = true;
    }];
    [webView setUIDelegate:delegate.get()];
    [webView synchronouslyLoadTestPageNamed:@"element-targeting-2" preferences:preferences.get()];
    Util::run(&didAdjustVisibility);

    RetainPtr targets = [webView targetedElementInfoWithSelectors:@[
        [NSSet setWithObjects:@"DIV.absolute.bottom-right", @"#no-match", @".also-no-match", nil]
    ]];

    RetainPtr target = [targets firstObject];
    EXPECT_EQ(1U, [targets count]);
    EXPECT_WK_STREQ("DIV.absolute.bottom-right", [target selectorsIncludingShadowHosts].firstObject.firstObject);
    EXPECT_TRUE([target isInVisibilityAdjustmentSubtree]);
    EXPECT_WK_STREQ("Bottom Right", [target renderedText]);

    didAdjustVisibility = false;

    [webView resetVisibilityAdjustmentsForTargets:targets.get()];
    [webView waitForNextPresentationUpdate];
    EXPECT_FALSE(didAdjustVisibility);
}

TEST(ElementTargeting, SnapshotElementWithVisibilityAdjustment)
{
    auto webViewFrame = CGRectMake(0, 0, 800, 600);

    auto viewAndWindow = setUpWebViewForSnapshotting(webViewFrame);
    auto [webView, window] = viewAndWindow;
    [webView synchronouslyLoadTestPageNamed:@"element-targeting-2"];

    RetainPtr targets = [webView targetedElementInfoWithSelectors:@[
        [NSSet setWithObject:@".absolute.bottom-right"]
    ]];

    EXPECT_EQ([targets count], 1U);
    [webView adjustVisibilityForTargets:targets.get()];

    CGImagePixelReader reader { [[targets firstObject] takeSnapshot] };
    auto checkPixelColor = [&reader](unsigned x, unsigned y) {
        auto color = reader.at(x, y);
        EXPECT_FALSE(color == WebCore::Color::transparentBlack);
        EXPECT_FALSE(color == WebCore::Color::white);
    };

    checkPixelColor(10, 10);
    checkPixelColor(reader.width() - 10, 10);
    checkPixelColor(reader.width() - 10, reader.height() - 10);
    checkPixelColor(10, reader.height() - 10);
}

TEST(ElementTargeting, SkipSnapshotForNonReplacedElementWithoutChildren)
{
    auto webViewFrame = CGRectMake(0, 0, 800, 600);

    auto viewAndWindow = setUpWebViewForSnapshotting(webViewFrame);
    auto [webView, window] = viewAndWindow;
    [webView synchronouslyLoadTestPageNamed:@"element-targeting-10"];

    RetainPtr targets = [webView targetedElementInfoWithSelectors:@[ [NSSet setWithObject:@"DIV.fixed"] ]];

    EXPECT_EQ([targets count], 1U);
    [webView adjustVisibilityForTargets:targets.get()];

    EXPECT_NULL([[targets firstObject] takeSnapshot]);
}

TEST(ElementTargeting, AdjustVisibilityFromPseudoSelectors)
{
    auto webViewFrame = CGRectMake(0, 0, 800, 600);

    auto [webView, window] = setUpWebViewForSnapshotting(webViewFrame);
    RetainPtr preferences = adoptNS([WKWebpagePreferences new]);
    [preferences _setVisibilityAdjustmentSelectors:[NSSet setWithObjects:@"main::before", @"HTML::AFTER", nil]];
    RetainPtr delegate = adoptNS([TestUIDelegate new]);
    RetainPtr adjustedSelectors = adoptNS([NSMutableSet new]);
    [delegate setWebViewDidAdjustVisibilityWithSelectors:^(WKWebView *, NSArray<NSString *> *selectors) {
        [adjustedSelectors addObjectsFromArray:selectors];
    }];
    [webView setUIDelegate:delegate.get()];
    [webView synchronouslyLoadTestPageNamed:@"element-targeting-3" preferences:preferences.get()];
    [webView waitForNextPresentationUpdate];

    RetainPtr snapshot = [webView snapshotAfterScreenUpdates];
    CGImagePixelReader pixelReader { snapshot.get() };
    auto x = static_cast<unsigned>(100 * (pixelReader.width() / CGRectGetWidth(webViewFrame)));
    auto y = static_cast<unsigned>(100 * (pixelReader.height() / CGRectGetHeight(webViewFrame)));
    EXPECT_EQ(pixelReader.at(x, y), WebCore::Color::white);
    EXPECT_EQ([webView numberOfVisibilityAdjustmentRects], 1U);
    EXPECT_TRUE([adjustedSelectors containsObject:@"main::before"]);
    EXPECT_TRUE([adjustedSelectors containsObject:@"HTML::AFTER"]);
}

TEST(ElementTargeting, AdjustVisibilityForTargetsInShadowRoot)
{
    auto webViewFrame = CGRectMake(0, 0, 800, 600);
    auto [webView, window] = setUpWebViewForSnapshotting(webViewFrame);

    RetainPtr preferences = adoptNS([WKWebpagePreferences new]);
    [preferences _setVisibilityAdjustmentSelectorsIncludingShadowHosts:@[
        @[
            [NSSet setWithObject:@"MAIN"],
            [NSSet setWithObject:@"SECTION"],
            [NSSet setWithObject:@"DIV.green"]
        ]
    ]];

    __block bool didAdjustment = false;
    RetainPtr delegate = adoptNS([TestUIDelegate new]);
    [webView setUIDelegate:delegate.get()];
    [delegate setWebViewDidAdjustVisibilityWithSelectors:^(WKWebView *, NSArray<NSString *> *selectors) {
        didAdjustment = true;
    }];

    [webView synchronouslyLoadTestPageNamed:@"element-targeting-8" preferences:preferences.get()];
    Util::run(&didAdjustment);
    [webView waitForNextPresentationUpdate];

    {
        CGImagePixelReader pixelReader { [webView snapshotAfterScreenUpdates] };
        auto x = static_cast<unsigned>(100 * (pixelReader.width() / CGRectGetWidth(webViewFrame)));
        auto y = static_cast<unsigned>(300 * (pixelReader.height() / CGRectGetHeight(webViewFrame)));
        EXPECT_EQ(pixelReader.at(x, y), WebCore::Color::white);
        EXPECT_EQ([webView numberOfVisibilityAdjustmentRects], 1U);
    }

    RetainPtr elements = [webView targetedElementInfoAt:CGPointMake(100, 100)];
    EXPECT_EQ([elements count], 1U);

    RetainPtr firstTarget = [elements firstObject];
    EXPECT_TRUE([firstTarget isInShadowTree]);

    RetainPtr selectors = [firstTarget selectorsIncludingShadowHosts];
    EXPECT_EQ(3U, [selectors count]);
    EXPECT_WK_STREQ([selectors objectAtIndex:0][0], @"MAIN");
    EXPECT_WK_STREQ([selectors objectAtIndex:1][0], @"SECTION");
    EXPECT_WK_STREQ([selectors objectAtIndex:2][0], @"DIV.red");
}

TEST(ElementTargeting, TargetContainsShadowRoot)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 800, 600)]);
    [webView synchronouslyLoadTestPageNamed:@"element-targeting-4"];

    RetainPtr elements = [webView targetedElementInfoAt:CGPointMake(100, 150)];
    EXPECT_EQ([elements count], 1U);
    EXPECT_TRUE([[elements firstObject].selectors containsObject:@"#container"]);
}

TEST(ElementTargeting, ParentRelativeSelectors)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 800, 600)]);
    [webView synchronouslyLoadTestPageNamed:@"element-targeting-5"];
    [webView expectSingleTargetedSelector:@"BODY > DIV:first-of-type" at:CGPointMake(100, 50)];
    [webView expectSingleTargetedSelector:@"BODY > DIV:nth-child(3)" at:CGPointMake(100, 150)];
    [webView expectSingleTargetedSelector:@"BODY > DIV:last-of-type" at:CGPointMake(100, 250)];
    [webView expectSingleTargetedSelector:@"BODY > SECTION" at:CGPointMake(100, 350)];
}

TEST(ElementTargeting, TargetInFlowElements)
{
    auto center = CGPointMake(200, 200);
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 400, 400)]);
    [webView synchronouslyLoadTestPageNamed:@"element-targeting-6"];
    [webView expectSingleTargetedSelector:@"MAIN > P:first-of-type" at:center];

    [webView stringByEvaluatingJavaScript:@"scrollBy(0, 400)"];
    [webView waitForNextPresentationUpdate];
    [webView expectSingleTargetedSelector:@"IMG" at:center];

    [webView stringByEvaluatingJavaScript:@"scrollBy(0, 400)"];
    [webView waitForNextPresentationUpdate];
    [webView expectSingleTargetedSelector:@"P.bottom-text" at:center];
}

TEST(ElementTargeting, ReplacedRendererSizeIgnoresPageScaleAndZoom)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 800, 600)]);
    [webView synchronouslyLoadTestPageNamed:@"element-targeting-7"];
    RetainPtr targetBeforeScaling = [[webView targetedElementInfoAt:CGPointMake(100, 100)] firstObject];
#if PLATFORM(MAC)
    // Additionally test page zoom (i.e. ⌘+) on macOS.
    [webView _setPageZoomFactor:2];
    [webView _setPageScale:1.5 withOrigin:CGPointZero];
#else
    RetainPtr scrollView = [webView scrollView];
    [scrollView setZoomScale:3 animated:NO];
    [scrollView setContentOffset:CGPointZero];
    [webView waitForNextVisibleContentRectUpdate];
#endif
    [webView waitForNextPresentationUpdate];
    RetainPtr targetAfterScaling = [[webView targetedElementInfoAt:CGPointMake(100, 100)] firstObject];
    EXPECT_WK_STREQ([targetBeforeScaling renderedText], [targetAfterScaling renderedText]);
    EXPECT_FALSE([targetBeforeScaling hasLargeReplacedDescendant]);
    EXPECT_FALSE([targetAfterScaling hasLargeReplacedDescendant]);
}

TEST(ElementTargeting, RequestTargetedElementsBySearchableText)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 800, 600)]);
    [webView synchronouslyLoadTestPageNamed:@"element-targeting-7"];

    RetainPtr targetFromHitTest = [[webView targetedElementInfoAt:CGPointMake(100, 100)] firstObject];
    NSString *searchableText = [targetFromHitTest searchableText];
    EXPECT_GT(searchableText.length, 0U);
    EXPECT_TRUE([@"Image of a sunset over the 4th floor of Infinite Loop 2" containsString:searchableText]);
    EXPECT_WK_STREQ("sunset-in-cupertino-200px.png", [[[targetFromHitTest mediaAndLinkURLs] anyObject] lastPathComponent]);

    RetainPtr targetFromSearchText = [[webView targetedElementInfoWithText:searchableText] firstObject];
    EXPECT_TRUE([targetFromSearchText isSameElement:targetFromHitTest.get()]);
    EXPECT_WK_STREQ("sunset-in-cupertino-200px.png", [[[targetFromSearchText mediaAndLinkURLs] anyObject] lastPathComponent]);

    [webView adjustVisibilityForTargets:@[ targetFromSearchText.get() ]];
    EXPECT_TRUE([targetFromSearchText isSameElement:[[webView targetedElementInfoWithText:searchableText] firstObject]]);
}

TEST(ElementTargeting, AdjustVisibilityAfterRecreatingElement)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 800, 600)]);

    RetainPtr delegate = adoptNS([TestUIDelegate new]);
    [webView setUIDelegate:delegate.get()];
    [webView synchronouslyLoadTestPageNamed:@"element-targeting-7"];

    RetainPtr firstTarget = [[webView targetedElementInfoAt:CGPointMake(100, 100)] firstObject];
    [webView adjustVisibilityForTargets:@[ firstTarget.get() ]];

    __block bool didAdjustment = false;
    [delegate setWebViewDidAdjustVisibilityWithSelectors:^(WKWebView *, NSArray<NSString *> *selectors) {
        didAdjustment = true;
    }];

    [webView objectByEvaluatingJavaScript:@"recreateContainer()"];

    Util::run(&didAdjustment);
}

TEST(ElementTargeting, TargetedElementWithLargeImage)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 480, 600)]);
    [webView synchronouslyLoadTestPageNamed:@"element-targeting-9"];
    RetainPtr element = [[webView targetedElementInfoAt:CGPointMake(80, 80)] firstObject];

    EXPECT_WK_STREQ("{480,150}", [element renderedText]);
    EXPECT_EQ([[element screenReaderText] length], 0U);
    EXPECT_TRUE([element hasLargeReplacedDescendant]);
}

TEST(ElementTargeting, CountVisibilityAdjustmentsAfterNavigatingBack)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 800, 600)]);
    [webView synchronouslyLoadTestPageNamed:@"element-targeting-1"];

    Util::waitForConditionWithLogging([&] {
        return [[webView objectByEvaluatingJavaScript:@"window.subframeLoaded"] boolValue];
    }, 5, @"Timed out waiting for subframes to finish loading.");

    RetainPtr element = [[webView targetedElementInfoAt:CGPointMake(150, 150)] firstObject];
    EXPECT_WK_STREQ("DIV.fixed.container", [[[element selectorsIncludingShadowHosts] firstObject] firstObject]);
    [webView adjustVisibilityForTargets:@[ element.get() ]];
    EXPECT_EQ(1U, [webView numberOfVisibilityAdjustmentRects]);

    [webView synchronouslyLoadTestPageNamed:@"element-targeting-2"];
    EXPECT_EQ(0U, [webView numberOfVisibilityAdjustmentRects]);

    [webView synchronouslyGoBack];
    EXPECT_EQ(1U, [webView numberOfVisibilityAdjustmentRects]);

    [webView synchronouslyGoForward];
    EXPECT_EQ(0U, [webView numberOfVisibilityAdjustmentRects]);
}

#if PLATFORM(VISION)
TEST(ElementTargeting, RequestAllVisibleElements)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 800, 600)]);
    [webView synchronouslyLoadTestPageNamed:@"element-targeting-11"];

    RetainPtr elements = [webView allTargetableElementsWithHitTestInterval:CGFloat(40.0)];
    EXPECT_EQ([elements count], 4U);
    {
        auto subelements = [elements objectAtIndex: 0];
        EXPECT_EQ([subelements count], 4U);
        {
            auto subelement = [subelements objectAtIndex: 0];
            EXPECT_TRUE([subelement.renderedText containsString:@"Top box"]);
            EXPECT_EQ(subelement.renderedText.length, 7U);
            EXPECT_EQ(subelement.childFrames.count, 0U);
        }
        {
            auto subelement = [subelements objectAtIndex: 1];
            EXPECT_TRUE([subelement.renderedText containsString:@"the crazy ones"]);
            EXPECT_EQ(subelement.renderedText.length, 64U);
            EXPECT_EQ(subelement.childFrames.count, 0U);
        }
        {
            auto subelement = [subelements objectAtIndex: 2];
            EXPECT_TRUE([subelement.renderedText containsString:@"Lorem ipsum"]);
            EXPECT_EQ(subelement.renderedText.length, 896U);
            EXPECT_EQ(subelement.childFrames.count, 0U);
        }
        {
            auto subelement = [subelements objectAtIndex: 3];
            EXPECT_TRUE([subelement.renderedText containsString:@"Occluded box"]);
            EXPECT_EQ(subelement.renderedText.length, 12U);
            EXPECT_EQ(subelement.childFrames.count, 0U);
        }
    }
    {
        auto subelements = [elements objectAtIndex: 1];
        EXPECT_EQ([subelements count], 3U);
        {
            auto subelement = [subelements objectAtIndex: 0];
            EXPECT_TRUE([subelement.renderedText containsString:@"Occluded box"]);
            EXPECT_EQ(subelement.renderedText.length, 12U);
            EXPECT_EQ(subelement.childFrames.count, 0U);
        }
        {
            auto subelement = [subelements objectAtIndex: 1];
            EXPECT_TRUE([subelement.renderedText containsString:@"the crazy ones"]);
            EXPECT_EQ(subelement.renderedText.length, 64U);
            EXPECT_EQ(subelement.childFrames.count, 0U);
        }
        {
            auto subelement = [subelements objectAtIndex: 2];
            EXPECT_TRUE([subelement.renderedText containsString:@"Lorem ipsum"]);
            EXPECT_EQ(subelement.renderedText.length, 896U);
            EXPECT_EQ(subelement.childFrames.count, 0U);
        }
    }
    {
        auto subelements = [elements objectAtIndex: 2];
        EXPECT_EQ([subelements count], 2U);
        {
            auto subelement = [subelements objectAtIndex: 0];
            EXPECT_TRUE([subelement.renderedText containsString:@"the crazy ones"]);
            EXPECT_EQ(subelement.renderedText.length, 64U);
            EXPECT_EQ(subelement.childFrames.count, 0U);
        }
        {
            auto subelement = [subelements objectAtIndex: 1];
            EXPECT_TRUE([subelement.renderedText containsString:@"Lorem ipsum"]);
            EXPECT_EQ(subelement.renderedText.length, 896U);
            EXPECT_EQ(subelement.childFrames.count, 0U);
        }
    }
    {
        auto subelements = [elements objectAtIndex: 3];
        EXPECT_EQ([subelements count], 1U);
        {
            auto subelement = [subelements objectAtIndex: 0];
            EXPECT_TRUE([subelement.renderedText containsString:@"Lorem ipsum"]);
            EXPECT_EQ(subelement.renderedText.length, 896U);
            EXPECT_EQ(subelement.childFrames.count, 0U);
        }
    }
}
#endif // PLATFORM(VISION)

} // namespace TestWebKitAPI
