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
#import <WebKit/WKWebViewPrivate.h>

constexpr CGFloat blackColorComponents[4] = { 0, 0, 0, 1 };
constexpr CGFloat whiteColorComponents[4] = { 1, 1, 1, 1 };

#if HAVE(UISCROLLVIEW_ASYNCHRONOUS_SCROLL_EVENT_HANDLING)
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
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
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

    __block bool done;
    __block bool wasHandled;

    auto synchronouslyHandleScrollEvent = ^(UIScrollPhase phase, CGPoint location, CGVector delta) {
        done = false;
        auto event = adoptNS([[WKUIScrollEvent alloc] initWithPhase:phase location:location delta:delta]);
        [webView _scrollView:[webView scrollView] asynchronouslyHandleScrollEvent:event.get() completion:^(BOOL handled) {
            wasHandled = handled;
            done = true;
        }];
        TestWebKitAPI::Util::run(&done);
    };

    // Don't preventDefault() at all.
    synchronouslyHandleScrollEvent(UIScrollPhaseMayBegin, CGPointMake(100, 100), CGVectorMake(0, 0));
    EXPECT_FALSE(wasHandled);
    synchronouslyHandleScrollEvent(UIScrollPhaseBegan, CGPointMake(100, 100), CGVectorMake(0, 10));
    EXPECT_FALSE(wasHandled);
    EXPECT_TRUE([[webView objectByEvaluatingJavaScript:@"window.lastWheelEvent.cancelable"] intValue]);
    EXPECT_EQ(-10, [[webView objectByEvaluatingJavaScript:@"window.lastWheelEvent.deltaY"] intValue]);
    EXPECT_EQ(30, [[webView objectByEvaluatingJavaScript:@"window.lastWheelEvent.wheelDeltaY"] intValue]);
    synchronouslyHandleScrollEvent(UIScrollPhaseChanged, CGPointMake(100, 100), CGVectorMake(0, 10));
    EXPECT_FALSE(wasHandled);
    synchronouslyHandleScrollEvent(UIScrollPhaseEnded, CGPointMake(100, 100), CGVectorMake(0, 0));
    EXPECT_FALSE(wasHandled);

    // preventDefault() on all events.
    [webView stringByEvaluatingJavaScript:@"window.preventDefaultOnScrollEvents = true;"];
    synchronouslyHandleScrollEvent(UIScrollPhaseMayBegin, CGPointMake(100, 100), CGVectorMake(0, 0));
    EXPECT_FALSE(wasHandled);
    synchronouslyHandleScrollEvent(UIScrollPhaseBegan, CGPointMake(100, 100), CGVectorMake(0, 10));
    EXPECT_TRUE(wasHandled);
    synchronouslyHandleScrollEvent(UIScrollPhaseChanged, CGPointMake(100, 100), CGVectorMake(0, 10));
    EXPECT_TRUE(wasHandled);
    synchronouslyHandleScrollEvent(UIScrollPhaseEnded, CGPointMake(100, 100), CGVectorMake(0, 0));
    EXPECT_FALSE(wasHandled);

    // preventDefault() on all but the begin event; it will be ignored.
    [webView stringByEvaluatingJavaScript:@"window.preventDefaultOnScrollEvents = false;"];
    synchronouslyHandleScrollEvent(UIScrollPhaseMayBegin, CGPointMake(100, 100), CGVectorMake(0, 0));
    EXPECT_FALSE(wasHandled);
    synchronouslyHandleScrollEvent(UIScrollPhaseBegan, CGPointMake(100, 100), CGVectorMake(0, 10));
    EXPECT_TRUE([[webView objectByEvaluatingJavaScript:@"window.lastWheelEvent.cancelable"] intValue]);
    EXPECT_FALSE(wasHandled);
    [webView stringByEvaluatingJavaScript:@"window.preventDefaultOnScrollEvents = true;"];
    synchronouslyHandleScrollEvent(UIScrollPhaseChanged, CGPointMake(100, 100), CGVectorMake(0, 10));
    EXPECT_FALSE(wasHandled);
    EXPECT_FALSE([[webView objectByEvaluatingJavaScript:@"window.lastWheelEvent.cancelable"] intValue]);
    synchronouslyHandleScrollEvent(UIScrollPhaseEnded, CGPointMake(100, 100), CGVectorMake(0, 0));
    EXPECT_FALSE(wasHandled);

    // preventDefault() on the begin event, and some subsequent events.
    [webView stringByEvaluatingJavaScript:@"window.preventDefaultOnScrollEvents = true;"];
    synchronouslyHandleScrollEvent(UIScrollPhaseMayBegin, CGPointMake(100, 100), CGVectorMake(0, 0));
    EXPECT_FALSE(wasHandled);
    synchronouslyHandleScrollEvent(UIScrollPhaseBegan, CGPointMake(100, 100), CGVectorMake(0, 10));
    EXPECT_TRUE(wasHandled);
    synchronouslyHandleScrollEvent(UIScrollPhaseChanged, CGPointMake(100, 100), CGVectorMake(0, 10));
    EXPECT_TRUE(wasHandled);
    [webView stringByEvaluatingJavaScript:@"window.preventDefaultOnScrollEvents = false;"];
    synchronouslyHandleScrollEvent(UIScrollPhaseChanged, CGPointMake(100, 100), CGVectorMake(0, 10));
    EXPECT_FALSE(wasHandled);
    synchronouslyHandleScrollEvent(UIScrollPhaseEnded, CGPointMake(100, 100), CGVectorMake(0, 0));
    EXPECT_FALSE(wasHandled);

    // preventDefault() on the first event with non-zero deltas, and some subsequent events.
    // In this case, the begin event has zero delta, and is not dispatched to the page, so the
    // first non-zero scroll event is actually the first preventable one.
    [webView stringByEvaluatingJavaScript:@"window.preventDefaultOnScrollEvents = true;"];
    synchronouslyHandleScrollEvent(UIScrollPhaseMayBegin, CGPointMake(100, 100), CGVectorMake(0, 0));
    EXPECT_FALSE(wasHandled);
    synchronouslyHandleScrollEvent(UIScrollPhaseBegan, CGPointMake(100, 100), CGVectorMake(0, 0));
    EXPECT_FALSE(wasHandled);
    synchronouslyHandleScrollEvent(UIScrollPhaseChanged, CGPointMake(100, 100), CGVectorMake(0, 10));
    EXPECT_TRUE(wasHandled);
    synchronouslyHandleScrollEvent(UIScrollPhaseChanged, CGPointMake(100, 100), CGVectorMake(0, 10));
    EXPECT_TRUE(wasHandled);
    [webView stringByEvaluatingJavaScript:@"window.preventDefaultOnScrollEvents = false;"];
    synchronouslyHandleScrollEvent(UIScrollPhaseChanged, CGPointMake(100, 100), CGVectorMake(0, 10));
    EXPECT_FALSE(wasHandled);
    synchronouslyHandleScrollEvent(UIScrollPhaseEnded, CGPointMake(100, 100), CGVectorMake(0, 0));
    EXPECT_FALSE(wasHandled);
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

#endif // PLATFORM(IOS_FAMILY)
