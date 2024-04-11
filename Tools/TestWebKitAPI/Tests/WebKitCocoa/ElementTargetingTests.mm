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

@interface WKWebView (ElementTargeting)

- (NSArray<_WKTargetedElementInfo *> *)targetedElementInfoAt:(CGPoint)point;
- (BOOL)adjustVisibilityForTargets:(NSArray<_WKTargetedElementInfo *> *)targets;
- (BOOL)resetVisibilityAdjustmentsForTargets:(NSArray<_WKTargetedElementInfo *> *)elements;

@property (nonatomic, readonly) NSUInteger numberOfVisibilityAdjustmentRects;

@end

@implementation WKWebView (ElementTargeting)

- (NSArray<_WKTargetedElementInfo *> *)targetedElementInfoAt:(CGPoint)point
{
    __block RetainPtr<NSArray<_WKTargetedElementInfo *>> result;
    auto request = adoptNS([_WKTargetedElementRequest new]);
    [request setPoint:point];
    __block bool done = false;
    [self _requestTargetedElementInfo:request.get() completionHandler:^(NSArray<_WKTargetedElementInfo *> *elements) {
        result = elements;
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
    return result.autorelease();
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
        EXPECT_WK_STREQ(".fixed.container", element.selectors.firstObject);
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

TEST(ElementTargeting, NearbyOutOfFlowElements)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 800, 600)]);
    [webView synchronouslyLoadTestPageNamed:@"element-targeting-2"];

    RetainPtr elements = [webView targetedElementInfoAt:CGPointMake(100, 100)];
    EXPECT_EQ([elements count], 5U);
    EXPECT_TRUE([elements objectAtIndex:0].underPoint);
    EXPECT_TRUE([elements objectAtIndex:1].underPoint);
    EXPECT_FALSE([elements objectAtIndex:2].underPoint);
    EXPECT_FALSE([elements objectAtIndex:3].underPoint);
    EXPECT_FALSE([elements objectAtIndex:4].underPoint);
    // The two elements that are directly hit-tested should take precedence over nearby elements.
    EXPECT_WK_STREQ(".fixed.container", [elements firstObject].selectors.firstObject);
    EXPECT_WK_STREQ(".box", [elements objectAtIndex:1].selectors.firstObject);
    __auto_type nextThreeSelectors = [NSSet setWithArray:@[
        [elements objectAtIndex:2].selectors.firstObject,
        [elements objectAtIndex:3].selectors.firstObject,
        [elements objectAtIndex:4].selectors.firstObject,
    ]];
    EXPECT_TRUE([nextThreeSelectors containsObject:@".absolute.top-right"]);
    EXPECT_TRUE([nextThreeSelectors containsObject:@".absolute.bottom-left"]);
    EXPECT_TRUE([nextThreeSelectors containsObject:@".absolute.bottom-right"]);

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

TEST(ElementTargeting, AdjustVisibilityFromPseudoSelectors)
{
    auto webViewFrame = CGRectMake(0, 0, 800, 600);

    auto [webView, window] = setUpWebViewForSnapshotting(webViewFrame);
    RetainPtr preferences = adoptNS([WKWebpagePreferences new]);
    auto selectors = [NSSet setWithObjects:@"main::before", @"HTML::AFTER", nil];
    [preferences _setVisibilityAdjustmentSelectors:selectors];
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

TEST(ElementTargeting, ContentInsideShadowRoot)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 800, 600)]);
    [webView synchronouslyLoadTestPageNamed:@"element-targeting-4"];

    RetainPtr elements = [webView targetedElementInfoAt:CGPointMake(100, 150)];
    EXPECT_EQ([elements count], 1U);
    EXPECT_TRUE([[elements firstObject].selectors containsObject:@"#container"]);
}

} // namespace TestWebKitAPI
