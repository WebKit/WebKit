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

#if PLATFORM(IOS_FAMILY)

#import "PlatformUtilities.h"
#import "TestWKWebView.h"
#import "UIKitSPIForTesting.h"
#import "WKBrowserEngineDefinitions.h"
#import <WebCore/WebEvent.h>
#import <WebKit/WKWebViewPrivate.h>

constexpr CGFloat blackColorComponents[4] = { 0, 0, 0, 1 };
constexpr CGFloat whiteColorComponents[4] = { 1, 1, 1, 1 };

#if HAVE(UISCROLLVIEW_ASYNCHRONOUS_SCROLL_EVENT_HANDLING)

#if USE(BROWSERENGINEKIT)

@interface WKTestScrollViewScrollUpdate : NSObject

- (instancetype)initWithScrollEvent:(UIScrollEvent *)scrollEvent phase:(BEScrollViewScrollUpdatePhase)phase;
- (CGPoint)locationInView:(UIView *)view;
- (CGPoint)translationInView:(UIView *)view;

@property (nonatomic, readonly) NSTimeInterval timestamp;
@property (nonatomic, readonly) BEScrollViewScrollUpdatePhase phase;

@end

@implementation WKTestScrollViewScrollUpdate {
    RetainPtr<UIScrollEvent> _scrollEvent;
    BEScrollViewScrollUpdatePhase _phase;
}

- (UIScrollEvent *)_scrollEvent
{
    return _scrollEvent.get();
}

- (instancetype)initWithScrollEvent:(UIScrollEvent *)scrollEvent phase:(BEScrollViewScrollUpdatePhase)phase
{
    if (!(self = [super init]))
        return nil;

    _scrollEvent = scrollEvent;
    _phase = phase;
    return self;
}

- (BEScrollViewScrollUpdatePhase)phase
{
    return _phase;
}

- (NSTimeInterval)timestamp
{
    return [_scrollEvent timestamp];
}

- (CGPoint)locationInView:(UIView *)view
{
    return [_scrollEvent locationInView:view];
}

- (CGPoint)translationInView:(UIView *)view
{
    CGVector adjustedAcceleratedDelta = [_scrollEvent _adjustedAcceleratedDeltaInView:view];
    return CGPointMake(adjustedAcceleratedDelta.dx, adjustedAcceleratedDelta.dy);
}

@end

#endif // USE(BROWSERENGINEKIT)

@interface WKUIScrollEvent : UIScrollEvent

- (instancetype)initWithPhase:(UIScrollPhase)phase location:(CGPoint)location delta:(CGVector)delta;

@end

@implementation WKUIScrollEvent {
    UIScrollPhase _phase;
    CGPoint _location;
    CGVector _delta;
}

- (instancetype)initWithPhase:(UIScrollPhase)phase location:(CGPoint)location delta:(CGVector)delta
{
    self = [super init];
    if (!self)
        return nil;

    _phase = phase;
    _location = location;
    _delta = delta;

    return self;
}

- (UIScrollPhase)phase
{
    return _phase;
}

- (CGPoint)locationInView:(UIView *)view
{
    return _location;
}

- (CGVector)_adjustedAcceleratedDeltaInView:(UIView *)view
{
    return _delta;
}

@end

inline static UIScrollPhase legacyScrollPhase(WKBEScrollViewScrollUpdatePhase phase)
{
#if USE(BROWSERENGINEKIT)
    switch (phase) {
    case WKBEScrollViewScrollUpdatePhaseBegan:
        return UIScrollPhaseBegan;
    case WKBEScrollViewScrollUpdatePhaseChanged:
        return UIScrollPhaseChanged;
    case WKBEScrollViewScrollUpdatePhaseEnded:
        return UIScrollPhaseEnded;
    case WKBEScrollViewScrollUpdatePhaseCancelled:
        return UIScrollPhaseCancelled;
    }
    ASSERT_NOT_REACHED();
    return UIScrollPhaseCancelled;
#else
    return phase;
#endif
}

inline static RetainPtr<WKBEScrollViewScrollUpdate> createScrollUpdate(WKBEScrollViewScrollUpdatePhase phase, CGPoint location, CGVector delta)
{
    auto event = adoptNS([[WKUIScrollEvent alloc] initWithPhase:legacyScrollPhase(phase) location:location delta:delta]);
#if USE(BROWSERENGINEKIT)
    return adoptNS(static_cast<BEScrollViewScrollUpdate *>([[WKTestScrollViewScrollUpdate alloc] initWithScrollEvent:event.get() phase:phase]));
#else
    return event;
#endif
}

@interface WKWebView (WKBEScrollViewDelegate)
- (void)scrollView:(UIScrollView *)scrollView handleScrollUpdate:(WKBEScrollViewScrollUpdate *)update completion:(void (^)(BOOL handled))completion;
@end

@interface WKWebView (WKScrollViewTests)
- (BOOL)synchronouslyHandleScrollEventWithPhase:(WKBEScrollViewScrollUpdatePhase)phase location:(CGPoint)location delta:(CGVector)delta;
@end

@implementation WKWebView (WKScrollViewTests)

- (BOOL)synchronouslyHandleScrollEventWithPhase:(WKBEScrollViewScrollUpdatePhase)phase location:(CGPoint)location delta:(CGVector)delta
{
    __block bool done = false;
    __block BOOL scrollingPrevented;

    RetainPtr update = createScrollUpdate(phase, location, delta);
    [self scrollView:self.scrollView handleScrollUpdate:update.get() completion:^(BOOL prevented) {
        scrollingPrevented = prevented;
        done = true;
    }];

    TestWebKitAPI::Util::run(&done);
    return scrollingPrevented;
}

@end

#endif // HAVE(UISCROLLVIEW_ASYNCHRONOUS_SCROLL_EVENT_HANDLING)

static void traverseLayerTree(CALayer *layer, void(^block)(CALayer *))
{
    for (CALayer *child in layer.sublayers)
        traverseLayerTree(child, block);
    block(layer);
}

TEST(WKScrollViewTests, PositionFixedLayerAfterScrolling)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    [webView synchronouslyLoadTestPageNamed:@"fixed-nav-bar"];

    __block bool done = false;
    [webView _doAfterNextPresentationUpdate:^() {
        done = true;
    }];

    [CATransaction begin];
    [webView scrollView].contentOffset = CGPointMake(0, 5000);
    [CATransaction commit];

    // Purposefully hang the main thread for a short while to give the remote layer tree transaction an
    // opportunity to arrive in the UI process before dispatching the next visible content rect update.
    usleep(USEC_PER_SEC * 0.25);

    TestWebKitAPI::Util::run(&done);

    bool foundLayerForFixedNavigationBar = false;
    traverseLayerTree([webView layer], [&] (CALayer *layer) {
        if (!CGSizeEqualToSize(layer.bounds.size, CGSizeMake(320, 50)))
            return;

        auto boundsInWebViewCoordinates = [layer convertRect:layer.bounds toLayer:[webView layer]];
        EXPECT_EQ(CGRectGetMinX(boundsInWebViewCoordinates), 0);
        EXPECT_EQ(CGRectGetMinY(boundsInWebViewCoordinates), 0);
        foundLayerForFixedNavigationBar = true;
    });
    EXPECT_TRUE(foundLayerForFixedNavigationBar);
}

#if HAVE(UISCROLLVIEW_ASYNCHRONOUS_SCROLL_EVENT_HANDLING)

TEST(WKScrollViewTests, AsynchronousWheelEventHandling)
{
    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    [webView synchronouslyLoadHTMLString:@""
        "<style>#handler { width: 200px; height: 200px; }</style>"
        "<div id='handler'></div>"
        "<script>window.preventDefaultOnScrollEvents = false;"
        "document.getElementById('handler').addEventListener('wheel', "
        "function (e) {"
        "   window.lastWheelEvent = e;"
        "   if (window.preventDefaultOnScrollEvents)"
        "       e.preventDefault();"
        "})</script>"];
    [webView waitForNextPresentationUpdate];

    BOOL scrollingPrevented;
    // Don't preventDefault() at all.
#if !USE(BROWSERENGINEKIT)
    scrollingPrevented = [webView synchronouslyHandleScrollEventWithPhase:UIScrollPhaseMayBegin location:CGPointMake(100, 100) delta:CGVectorMake(0, 0)];
    EXPECT_FALSE(scrollingPrevented);
#endif
    scrollingPrevented = [webView synchronouslyHandleScrollEventWithPhase:WKBEScrollViewScrollUpdatePhaseBegan location:CGPointMake(100, 100) delta:CGVectorMake(0, 10)];
    EXPECT_FALSE(scrollingPrevented);
    EXPECT_TRUE([[webView objectByEvaluatingJavaScript:@"window.lastWheelEvent.cancelable"] intValue]);
    EXPECT_EQ(-10, [[webView objectByEvaluatingJavaScript:@"window.lastWheelEvent.deltaY"] intValue]);
    EXPECT_EQ(30, [[webView objectByEvaluatingJavaScript:@"window.lastWheelEvent.wheelDeltaY"] intValue]);
    scrollingPrevented = [webView synchronouslyHandleScrollEventWithPhase:WKBEScrollViewScrollUpdatePhaseChanged location:CGPointMake(100, 100) delta:CGVectorMake(0, 10)];
    EXPECT_FALSE(scrollingPrevented);
    scrollingPrevented = [webView synchronouslyHandleScrollEventWithPhase:WKBEScrollViewScrollUpdatePhaseEnded location:CGPointMake(100, 100) delta:CGVectorMake(0, 0)];
    EXPECT_FALSE(scrollingPrevented);

    // preventDefault() on all events.
    [webView stringByEvaluatingJavaScript:@"window.preventDefaultOnScrollEvents = true;"];
#if !USE(BROWSERENGINEKIT)
    scrollingPrevented = [webView synchronouslyHandleScrollEventWithPhase:UIScrollPhaseMayBegin location:CGPointMake(100, 100) delta:CGVectorMake(0, 0)];
    EXPECT_FALSE(scrollingPrevented);
#endif
    scrollingPrevented = [webView synchronouslyHandleScrollEventWithPhase:WKBEScrollViewScrollUpdatePhaseBegan location:CGPointMake(100, 100) delta:CGVectorMake(0, 10)];
    EXPECT_TRUE(scrollingPrevented);
    scrollingPrevented = [webView synchronouslyHandleScrollEventWithPhase:WKBEScrollViewScrollUpdatePhaseChanged location:CGPointMake(100, 100) delta:CGVectorMake(0, 10)];
    EXPECT_TRUE(scrollingPrevented);
    scrollingPrevented = [webView synchronouslyHandleScrollEventWithPhase:WKBEScrollViewScrollUpdatePhaseEnded location:CGPointMake(100, 100) delta:CGVectorMake(0, 0)];
    EXPECT_FALSE(scrollingPrevented);

    // preventDefault() on all but the begin event; it will be ignored.
    [webView stringByEvaluatingJavaScript:@"window.preventDefaultOnScrollEvents = false;"];
#if !USE(BROWSERENGINEKIT)
    scrollingPrevented = [webView synchronouslyHandleScrollEventWithPhase:UIScrollPhaseMayBegin location:CGPointMake(100, 100) delta:CGVectorMake(0, 0)];
    EXPECT_FALSE(scrollingPrevented);
#endif
    scrollingPrevented = [webView synchronouslyHandleScrollEventWithPhase:WKBEScrollViewScrollUpdatePhaseBegan location:CGPointMake(100, 100) delta:CGVectorMake(0, 10)];
    EXPECT_TRUE([[webView objectByEvaluatingJavaScript:@"window.lastWheelEvent.cancelable"] intValue]);
    EXPECT_FALSE(scrollingPrevented);
    [webView stringByEvaluatingJavaScript:@"window.preventDefaultOnScrollEvents = true;"];
    scrollingPrevented = [webView synchronouslyHandleScrollEventWithPhase:WKBEScrollViewScrollUpdatePhaseChanged location:CGPointMake(100, 100) delta:CGVectorMake(0, 10)];
    EXPECT_FALSE(scrollingPrevented);
    EXPECT_FALSE([[webView objectByEvaluatingJavaScript:@"window.lastWheelEvent.cancelable"] intValue]);
    scrollingPrevented = [webView synchronouslyHandleScrollEventWithPhase:WKBEScrollViewScrollUpdatePhaseEnded location:CGPointMake(100, 100) delta:CGVectorMake(0, 0)];
    EXPECT_FALSE(scrollingPrevented);

    // preventDefault() on the begin event, and some subsequent events.
    [webView stringByEvaluatingJavaScript:@"window.preventDefaultOnScrollEvents = true;"];
#if !USE(BROWSERENGINEKIT)
    scrollingPrevented = [webView synchronouslyHandleScrollEventWithPhase:UIScrollPhaseMayBegin location:CGPointMake(100, 100) delta:CGVectorMake(0, 0)];
    EXPECT_FALSE(scrollingPrevented);
#endif
    scrollingPrevented = [webView synchronouslyHandleScrollEventWithPhase:WKBEScrollViewScrollUpdatePhaseBegan location:CGPointMake(100, 100) delta:CGVectorMake(0, 10)];
    EXPECT_TRUE(scrollingPrevented);
    scrollingPrevented = [webView synchronouslyHandleScrollEventWithPhase:WKBEScrollViewScrollUpdatePhaseChanged location:CGPointMake(100, 100) delta:CGVectorMake(0, 10)];
    EXPECT_TRUE(scrollingPrevented);
    [webView stringByEvaluatingJavaScript:@"window.preventDefaultOnScrollEvents = false;"];
    scrollingPrevented = [webView synchronouslyHandleScrollEventWithPhase:WKBEScrollViewScrollUpdatePhaseChanged location:CGPointMake(100, 100) delta:CGVectorMake(0, 10)];
    EXPECT_FALSE(scrollingPrevented);
    scrollingPrevented = [webView synchronouslyHandleScrollEventWithPhase:WKBEScrollViewScrollUpdatePhaseEnded location:CGPointMake(100, 100) delta:CGVectorMake(0, 0)];
    EXPECT_FALSE(scrollingPrevented);

    // preventDefault() on the first event with non-zero deltas, and some subsequent events.
    // In this case, the begin event has zero delta, and is not dispatched to the page, so the
    // first non-zero scroll event is actually the first preventable one.
    [webView stringByEvaluatingJavaScript:@"window.preventDefaultOnScrollEvents = true;"];
#if !USE(BROWSERENGINEKIT)
    scrollingPrevented = [webView synchronouslyHandleScrollEventWithPhase:UIScrollPhaseMayBegin location:CGPointMake(100, 100) delta:CGVectorMake(0, 0)];
    EXPECT_FALSE(scrollingPrevented);
#endif
    scrollingPrevented = [webView synchronouslyHandleScrollEventWithPhase:WKBEScrollViewScrollUpdatePhaseBegan location:CGPointMake(100, 100) delta:CGVectorMake(0, 0)];
    EXPECT_FALSE(scrollingPrevented);
    scrollingPrevented = [webView synchronouslyHandleScrollEventWithPhase:WKBEScrollViewScrollUpdatePhaseChanged location:CGPointMake(100, 100) delta:CGVectorMake(0, 10)];
    EXPECT_TRUE(scrollingPrevented);
    scrollingPrevented = [webView synchronouslyHandleScrollEventWithPhase:WKBEScrollViewScrollUpdatePhaseChanged location:CGPointMake(100, 100) delta:CGVectorMake(0, 10)];
    EXPECT_TRUE(scrollingPrevented);
    [webView stringByEvaluatingJavaScript:@"window.preventDefaultOnScrollEvents = false;"];
    scrollingPrevented = [webView synchronouslyHandleScrollEventWithPhase:WKBEScrollViewScrollUpdatePhaseChanged location:CGPointMake(100, 100) delta:CGVectorMake(0, 10)];
    EXPECT_FALSE(scrollingPrevented);
    scrollingPrevented = [webView synchronouslyHandleScrollEventWithPhase:WKBEScrollViewScrollUpdatePhaseEnded location:CGPointMake(100, 100) delta:CGVectorMake(0, 0)];
    EXPECT_FALSE(scrollingPrevented);
}

TEST(WKScrollViewTests, OverscrollBehaviorAndOverflowHiddenOnRootShouldNotPreventScrolling)
{
    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    [webView synchronouslyLoadHTMLString:@"<!DOCTYPE html>"
        "<html>"
        "<meta name='viewport' content='width=device-width, initial-scale=1'>"
        "<head>"
        "    <style>"
        "    body, html { width: 100%; height: 100%; margin: 0; overscroll-behavior: contain; overflow: hidden; }"
        "    .scroller { width: 100%; height: 100%; overflow: scroll; }"
        "    .tall { width: 1px; height: 5000px; }"
        "    </style>"
        "</head>"
        "<body>"
        "    <div class='scroller'><div class='tall'></div></div>"
        "    <script>"
        "    window.preventWheelEvents = false;"
        "    document.querySelector('.scroller').addEventListener('wheel', (event) => {"
        "        if (window.preventWheelEvents) event.preventDefault();"
        "    });"
        "    </script>"
        "</body>"
        "</html>"];

    EXPECT_FALSE([webView synchronouslyHandleScrollEventWithPhase:WKBEScrollViewScrollUpdatePhaseBegan location:CGPointMake(100, 100) delta:CGVectorMake(0, 10)]);
    EXPECT_FALSE([webView synchronouslyHandleScrollEventWithPhase:WKBEScrollViewScrollUpdatePhaseChanged location:CGPointMake(100, 100) delta:CGVectorMake(0, 10)]);
    EXPECT_FALSE([webView synchronouslyHandleScrollEventWithPhase:WKBEScrollViewScrollUpdatePhaseEnded location:CGPointMake(100, 100) delta:CGVectorMake(0, 0)]);

    [webView stringByEvaluatingJavaScript:@"window.preventWheelEvents = true;"];

    EXPECT_TRUE([webView synchronouslyHandleScrollEventWithPhase:WKBEScrollViewScrollUpdatePhaseBegan location:CGPointMake(100, 100) delta:CGVectorMake(0, 10)]);
    EXPECT_TRUE([webView synchronouslyHandleScrollEventWithPhase:WKBEScrollViewScrollUpdatePhaseChanged location:CGPointMake(100, 100) delta:CGVectorMake(0, 10)]);
    EXPECT_FALSE([webView synchronouslyHandleScrollEventWithPhase:WKBEScrollViewScrollUpdatePhaseEnded location:CGPointMake(100, 100) delta:CGVectorMake(0, 0)]);
}

#endif // HAVE(UISCROLLVIEW_ASYNCHRONOUS_SCROLL_EVENT_HANDLING)

TEST(WKScrollViewTests, IndicatorStyleSetByClient)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 320, 500)]);

    [webView synchronouslyLoadHTMLStringAndWaitUntilAllImmediateChildFramesPaint:@"<style> body { background-color: black; } </style>"];
    EXPECT_EQ([webView scrollView].indicatorStyle, UIScrollViewIndicatorStyleWhite);

    [webView scrollView].indicatorStyle = UIScrollViewIndicatorStyleBlack;
    EXPECT_EQ([webView scrollView].indicatorStyle, UIScrollViewIndicatorStyleBlack);

    [webView synchronouslyLoadHTMLStringAndWaitUntilAllImmediateChildFramesPaint:@"<style> body { background-color: black; } </style>"];
    EXPECT_EQ([webView scrollView].indicatorStyle, UIScrollViewIndicatorStyleBlack);

    [webView scrollView].indicatorStyle = UIScrollViewIndicatorStyleDefault;
    EXPECT_EQ([webView scrollView].indicatorStyle, UIScrollViewIndicatorStyleWhite);

    [webView synchronouslyLoadHTMLStringAndWaitUntilAllImmediateChildFramesPaint:@"<style> body { background-color: white; } </style>"];
    EXPECT_EQ([webView scrollView].indicatorStyle, UIScrollViewIndicatorStyleBlack);

    [webView scrollView].indicatorStyle = UIScrollViewIndicatorStyleWhite;
    EXPECT_EQ([webView scrollView].indicatorStyle, UIScrollViewIndicatorStyleWhite);

    [webView synchronouslyLoadHTMLStringAndWaitUntilAllImmediateChildFramesPaint:@"<style> body { background-color: white; } </style>"];
    EXPECT_EQ([webView scrollView].indicatorStyle, UIScrollViewIndicatorStyleWhite);

    [webView scrollView].indicatorStyle = UIScrollViewIndicatorStyleDefault;
    EXPECT_EQ([webView scrollView].indicatorStyle, UIScrollViewIndicatorStyleBlack);
}

TEST(WKScrollViewTests, BackgroundColorSetByClient)
{
    auto sRGBColorSpace = adoptCF(CGColorSpaceCreateWithName(kCGColorSpaceSRGB));
    auto blackColor = adoptCF(CGColorCreate(sRGBColorSpace.get(), blackColorComponents));
    auto whiteColor = adoptCF(CGColorCreate(sRGBColorSpace.get(), whiteColorComponents));

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 320, 500)]);

    [webView synchronouslyLoadHTMLStringAndWaitUntilAllImmediateChildFramesPaint:@"<style> body { background-color: black; } </style>"];
    EXPECT_TRUE(CGColorEqualToColor([webView scrollView].backgroundColor.CGColor, blackColor.get()));

    [webView scrollView].backgroundColor = [UIColor colorWithCGColor:whiteColor.get()];
    EXPECT_TRUE(CGColorEqualToColor([webView scrollView].backgroundColor.CGColor, whiteColor.get()));

    [webView synchronouslyLoadHTMLStringAndWaitUntilAllImmediateChildFramesPaint:@"<style> body { background-color: black; } </style>"];
    EXPECT_TRUE(CGColorEqualToColor([webView scrollView].backgroundColor.CGColor, whiteColor.get()));

    [webView scrollView].backgroundColor = nil;
    EXPECT_TRUE(CGColorEqualToColor([webView scrollView].backgroundColor.CGColor, blackColor.get()));

    [webView synchronouslyLoadHTMLStringAndWaitUntilAllImmediateChildFramesPaint:@"<style> body { background-color: white; } </style>"];
    EXPECT_TRUE(CGColorEqualToColor([webView scrollView].backgroundColor.CGColor, whiteColor.get()));

    [webView scrollView].backgroundColor = [UIColor colorWithCGColor:blackColor.get()];
    EXPECT_TRUE(CGColorEqualToColor([webView scrollView].backgroundColor.CGColor, blackColor.get()));

    [webView synchronouslyLoadHTMLStringAndWaitUntilAllImmediateChildFramesPaint:@"<style> body { background-color: white; } </style>"];
    EXPECT_TRUE(CGColorEqualToColor([webView scrollView].backgroundColor.CGColor, blackColor.get()));

    [webView scrollView].backgroundColor = nil;
    EXPECT_TRUE(CGColorEqualToColor([webView scrollView].backgroundColor.CGColor, whiteColor.get()));
}

TEST(WKScrollViewTests, DecelerationSetByClient)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 320, 500)]);

    [webView synchronouslyLoadHTMLStringAndWaitUntilAllImmediateChildFramesPaint:@"first"];
    EXPECT_FLOAT_EQ([webView scrollView].decelerationRate, UIScrollViewDecelerationRateNormal);

    [webView scrollView].decelerationRate = UIScrollViewDecelerationRateFast;
    EXPECT_FLOAT_EQ([webView scrollView].decelerationRate, UIScrollViewDecelerationRateFast);

    [webView synchronouslyLoadHTMLStringAndWaitUntilAllImmediateChildFramesPaint:@"second"];
    EXPECT_FLOAT_EQ([webView scrollView].decelerationRate, UIScrollViewDecelerationRateFast);
}

#if HAVE(UISCROLLVIEW_ALLOWS_KEYBOARD_SCROLLING)
TEST(WKScrollViewTests, AllowsKeyboardScrolling)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 300, 300)]);

    [webView synchronouslyLoadTestPageNamed:@"simple-tall"];
    [webView waitForNextPresentationUpdate];

    auto pressSpacebar = ^(void(^completionHandler)(void)) {
        auto firstWebEvent = adoptNS([[WebEvent alloc] initWithKeyEventType:WebEventKeyDown timeStamp:CFAbsoluteTimeGetCurrent() characters:@" " charactersIgnoringModifiers:@" " modifiers:0 isRepeating:NO withFlags:0 withInputManagerHint:nil keyCode:0 isTabKey:NO]);

        auto secondWebEvent = adoptNS([[WebEvent alloc] initWithKeyEventType:WebEventKeyUp timeStamp:CFAbsoluteTimeGetCurrent() characters:@" " charactersIgnoringModifiers:@" " modifiers:0 isRepeating:NO withFlags:0 withInputManagerHint:nil keyCode:0 isTabKey:NO]);

        [webView handleKeyEvent:firstWebEvent.get() completion:^(WebEvent *theEvent, BOOL wasHandled) {
            dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(0.1 * NSEC_PER_SEC)), dispatch_get_main_queue(), ^{
                [webView handleKeyEvent:secondWebEvent.get() completion:^(WebEvent *theEvent, BOOL wasHandled) {
                    completionHandler();
                }];
            });
        }];
    };

    __block bool doneWaiting = false;

    [webView scrollView].allowsKeyboardScrolling = NO;

    pressSpacebar(^{
        NSInteger scrollY = [[webView stringByEvaluatingJavaScript:@"window.scrollY"] integerValue];
        EXPECT_EQ(scrollY, 0);
        doneWaiting = true;
    });

    TestWebKitAPI::Util::run(&doneWaiting);

    doneWaiting = false;

    [webView scrollView].allowsKeyboardScrolling = YES;

    pressSpacebar(^{
        NSInteger scrollY = [[webView stringByEvaluatingJavaScript:@"window.scrollY"] integerValue];
        EXPECT_GT(scrollY, 0);
        doneWaiting = true;
    });

    TestWebKitAPI::Util::run(&doneWaiting);
}
#endif

#endif // PLATFORM(IOS_FAMILY)
