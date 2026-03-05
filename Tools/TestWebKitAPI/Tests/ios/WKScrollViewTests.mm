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

#import "InstanceMethodSwizzler.h"
#import "PlatformUtilities.h"
#import "TestCocoa.h"
#import "TestNavigationDelegate.h"
#import "TestWKWebView.h"
#import "UIKitSPIForTesting.h"
#import "UserInterfaceSwizzler.h"
#import "WKBrowserEngineDefinitions.h"
#import <WebCore/ColorCocoa.h>
#import <WebCore/ColorSerialization.h>
#import <WebCore/WebEvent.h>
#import <WebKit/WKPreferencesPrivate.h>
#import <WebKit/WKWebViewPrivate.h>
#import <WebKit/WKWebViewPrivateForTesting.h>
#import <WebKit/WKWebViewPrivateForTestingIOS.h>
#import <WebKit/_WKFeature.h>
#import <wtf/BlockPtr.h>
#import <wtf/darwin/DispatchExtras.h>

constexpr CGFloat blackColorComponents[4] = { 0, 0, 0, 1 };
constexpr CGFloat whiteColorComponents[4] = { 1, 1, 1, 1 };

@interface UIView (TestWebKitAPI)
- (BOOL)_appearsBeforeViewInSubviewOrder:(UIView *)view;
@end

@interface WKWebView (WKScrollViewTestsInternal)
- (void)_scheduleForcedVisibleContentRectUpdate;
@end

@implementation UIView (TestWebKitAPI)

- (BOOL)_appearsBeforeViewInSubviewOrder:(UIView *)view
{
    if (!view || view.superview != self.superview)
        return false;

    RetainPtr subviews = [self.superview subviews];
    return [subviews count] && [subviews indexOfObject:self] < [subviews indexOfObject:view];
}

@end

#if HAVE(LIQUID_GLASS)

@interface ScrollEdgeEffectIsHiddenObserver : NSObject
@end

@implementation ScrollEdgeEffectIsHiddenObserver {
    __weak UIScrollEdgeEffect *_effect;
    BlockPtr<void()> _callback;
}

void *scrollEdgeEffectObservationContext = &scrollEdgeEffectObservationContext;

- (instancetype)initWithScrollEdgeEffect:(UIScrollEdgeEffect *)effect callback:(void(^)())callback
{
    if (!(self = [super init]))
        return nil;

    _effect = effect;
    _callback = makeBlockPtr(callback);
    [effect addObserver:self forKeyPath:NSStringFromSelector(@selector(isHidden)) options:NSKeyValueObservingOptionNew context:scrollEdgeEffectObservationContext];
    return self;
}

- (void)dealloc
{
    [_effect removeObserver:self forKeyPath:NSStringFromSelector(@selector(isHidden))];

    [super dealloc];
}

- (void)observeValueForKeyPath:(NSString *)keyPath ofObject:(id)object change:(NSDictionary *)change context:(void *)context
{
    if (context == scrollEdgeEffectObservationContext)
        _callback();
}

@end

#endif // HAVE(LIQUID_GLASS)

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

namespace TestWebKitAPI {

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

    Util::run(&done);

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

TEST(WKScrollViewTests, WheelEventDispatchedToSubframe)
{
    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 200, 200)]);
    [webView synchronouslyLoadHTMLString:@(R"(
    <!DOCTYPE html>
    <html>
    <body style='margin: 0; padding: 0;'>
        <iframe id='subframe' style='width: 200px; height: 400px;'></iframe>
    </body>
    <script>
        window.addEventListener('load', () => {
            window.subframeHit = 0;
            let subframe = document.getElementById('subframe');
            subframe.srcdoc = `\<script\> document.addEventListener('wheel', (e) => { parent.window.subframeHit = 1; }); \</script\>`;
        });
    </script>
    </html>
    )")];
#if !USE(BROWSERENGINEKIT)
    [webView synchronouslyHandleScrollEventWithPhase:UIScrollPhaseMayBegin location:CGPointMake(100, 100) delta:CGVectorMake(0, 10)];
#else
    [webView synchronouslyHandleScrollEventWithPhase:WKBEScrollViewScrollUpdatePhaseBegan location:CGPointMake(100, 100) delta:CGVectorMake(0, 10)];
#endif
    EXPECT_TRUE([[webView objectByEvaluatingJavaScript:@"window.subframeHit"] intValue]);
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
            dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(0.1 * NSEC_PER_SEC)), mainDispatchQueueSingleton(), ^{
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

    Util::run(&doneWaiting);

    doneWaiting = false;

    [webView scrollView].allowsKeyboardScrolling = YES;

    pressSpacebar(^{
        NSInteger scrollY = [[webView stringByEvaluatingJavaScript:@"window.scrollY"] integerValue];
        EXPECT_GT(scrollY, 0);
        doneWaiting = true;
    });

    Util::run(&doneWaiting);
}
#endif

#if HAVE(LIQUID_GLASS)

TEST(WKScrollViewTests, ClientCanHideScrollEdgeEffects)
{
    IPhoneUserInterfaceSwizzler swizzleUserInterface;

    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 400, 800)]);

    auto insets = UIEdgeInsetsMake(25, 0, 125, 0);
    [webView setObscuredContentInsets:insets];

    RetainPtr scrollView = [webView scrollView];
    [scrollView setContentInsetAdjustmentBehavior:UIScrollViewContentInsetAdjustmentNever];
    [scrollView setContentInset:insets];

    [webView synchronouslyLoadTestPageNamed:@"top-fixed-element"];
    [webView waitForNextPresentationUpdate];

    EXPECT_TRUE([scrollView topEdgeEffect].hidden);
    EXPECT_FALSE([scrollView bottomEdgeEffect].hidden);

    [scrollView topEdgeEffect].hidden = YES;
    [scrollView bottomEdgeEffect].hidden = YES;

    EXPECT_TRUE([scrollView topEdgeEffect].hidden);
    EXPECT_TRUE([scrollView bottomEdgeEffect].hidden);

    [scrollView topEdgeEffect].hidden = NO;
    [scrollView bottomEdgeEffect].hidden = NO;

    EXPECT_TRUE([scrollView topEdgeEffect].hidden); // Remains hidden, due to the top fixed color extension.
    EXPECT_FALSE([scrollView bottomEdgeEffect].hidden);

    [webView stringByEvaluatingJavaScript:@"document.querySelector('header').remove()"];
    [webView waitForNextPresentationUpdate];

    EXPECT_FALSE([scrollView topEdgeEffect].hidden);
    EXPECT_FALSE([scrollView bottomEdgeEffect].hidden);
}

TEST(WKScrollViewTests, ColorExtensionViewsWhenZoomedIn)
{
    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 400, 800)]);

    auto insets = UIEdgeInsetsMake(50, 0, 50, 0);
    [webView setObscuredContentInsets:insets];

    RetainPtr scrollView = [webView scrollView];
    [scrollView setContentInsetAdjustmentBehavior:UIScrollViewContentInsetAdjustmentNever];
    [scrollView setContentInset:insets];

    [webView synchronouslyLoadTestPageNamed:@"top-fixed-element"];
    [webView waitForNextPresentationUpdate];

    RetainPtr topColorExtension = [webView _colorExtensionViewForTesting:UIRectEdgeTop];
    EXPECT_EQ([topColorExtension frame], CGRectMake(0, -50, 400, 50));

    [scrollView setZoomScale:1.5];
    [webView waitForNextVisibleContentRectUpdate];
    [webView waitForNextPresentationUpdate];
    EXPECT_EQ([topColorExtension frame], CGRectMake(0, 125, 600, 50));

    [scrollView setContentOffset:CGPointMake(0, 0)];
    [webView waitForNextVisibleContentRectUpdate];
    [webView waitForNextPresentationUpdate];
    EXPECT_EQ([topColorExtension frame], CGRectMake(0, 0, 600, 50));
}

TEST(WKScrollViewTests, ColorExtensionViewsDuringAnimatedResize)
{
    RetainPtr configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    RetainPtr preferences = [configuration preferences];
    for (_WKFeature *feature in WKPreferences._features) {
        if ([feature.key isEqualToString:@"AutomaticLiveResizeEnabled"]) {
            [preferences _setEnabled:YES forFeature:feature];
            break;
        }
    }

    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 400, 300) configuration:configuration.get()]);
    RetainPtr window = [webView window];

    static CGSize windowSceneSize = CGSizeZero;
    InstanceMethodSwizzler boundsSwizzler {
        [window windowScene].effectiveGeometry.coordinateSpace.class,
        @selector(bounds),
        imp_implementationWithBlock(^{
            return CGRect { CGPointZero, windowSceneSize };
        })
    };

    auto insets = UIEdgeInsetsMake(50, 0, 0, 0);
    [webView setObscuredContentInsets:insets];

    RetainPtr scrollView = [webView scrollView];
    [scrollView setContentInsetAdjustmentBehavior:UIScrollViewContentInsetAdjustmentNever];
    [scrollView setContentInset:insets];

    [webView synchronouslyLoadTestPageNamed:@"top-fixed-element"];
    [webView waitForNextPresentationUpdate];

    auto targetSizes = std::array {
        CGSizeMake(600, 400),
        CGSizeMake(700, 500),
        CGSizeMake(800, 600),
        CGSizeMake(900, 700),
        CGSizeMake(1000, 800),
    };

    RetainPtr topColorExtension = [webView _colorExtensionViewForTesting:UIRectEdgeTop];
    for (auto size : targetSizes) {
        windowSceneSize = size;
        auto newFrame = CGRectMake(0, 0, size.width, size.height);
        [window setFrame:newFrame];
        [webView setFrame:newFrame];
        [webView waitForNextPresentationUpdate];
        EXPECT_EQ([topColorExtension frame], CGRectMake(0, -50, size.width, 50));
    }
}

TEST(WKScrollViewTests, ShouldSuppressTopColorExtensionView)
{
    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 400, 800)]);

    auto insets = UIEdgeInsetsMake(50, 0, 0, 0);
    [webView setObscuredContentInsets:insets];

    RetainPtr scrollView = [webView scrollView];
    [scrollView setContentInsetAdjustmentBehavior:UIScrollViewContentInsetAdjustmentNever];
    [scrollView setContentInset:insets];

    [webView _setShouldSuppressTopColorExtensionView:YES];
    [webView synchronouslyLoadTestPageNamed:@"top-fixed-element"];
    [webView waitForNextPresentationUpdate];

    EXPECT_NULL([webView _colorExtensionViewForTesting:UIRectEdgeTop]);

    [webView _setShouldSuppressTopColorExtensionView:NO];
    RetainPtr topColorExtension = [webView _colorExtensionViewForTesting:UIRectEdgeTop];
    EXPECT_NOT_NULL(topColorExtension);
    EXPECT_FALSE([topColorExtension isHidden]);

    [webView _setShouldSuppressTopColorExtensionView:YES];
    Util::waitForConditionWithLogging([topColorExtension] {
        return [topColorExtension isHidden];
    }, 5, @"Color extension view failed to hide");
}

TEST(WKScrollViewTests, TopScrollEdgeEffectIsHiddenKVO)
{
    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 400, 800)]);

    auto insets = UIEdgeInsetsMake(50, 0, 0, 0);
    [webView setObscuredContentInsets:insets];

    RetainPtr scrollView = [webView scrollView];
    [scrollView setContentInsetAdjustmentBehavior:UIScrollViewContentInsetAdjustmentNever];
    [scrollView setContentInset:insets];

    RetainPtr topScrollEdgeEffect = [scrollView topEdgeEffect];

    __block bool isHiddenChanged = false;
    RetainPtr observer = adoptNS([[ScrollEdgeEffectIsHiddenObserver alloc] initWithScrollEdgeEffect:topScrollEdgeEffect.get() callback:^{
        isHiddenChanged = true;
    }]);

    [webView synchronouslyLoadTestPageNamed:@"top-fixed-element"];
    [webView waitForNextPresentationUpdate];

    Util::run(&isHiddenChanged);
    EXPECT_TRUE([topScrollEdgeEffect isHidden]);
    isHiddenChanged = false;

    [webView synchronouslyLoadTestPageNamed:@"simple"];
    [webView waitForNextPresentationUpdate];

    Util::run(&isHiddenChanged);
    EXPECT_FALSE([topScrollEdgeEffect isHidden]);
}

TEST(WKScrollViewTests, TopColorExtensionViewAfterRemovingRefreshControl)
{
    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 400, 800)]);
    RetainPtr contentView = [webView wkContentView];
    RetainPtr scrollView = [webView scrollView];

    auto insets = UIEdgeInsetsMake(50, 0, 0, 0);
    [webView setObscuredContentInsets:insets];

    [scrollView setContentInsetAdjustmentBehavior:UIScrollViewContentInsetAdjustmentNever];
    [scrollView setContentInset:insets];

    RetainPtr refreshControl = adoptNS([[UIRefreshControl alloc] init]);
    [scrollView setRefreshControl:refreshControl.get()];

    [webView synchronouslyLoadTestPageNamed:@"top-fixed-element"];
    [webView waitForNextPresentationUpdate];

    RetainPtr topColorExtension = [webView _colorExtensionViewForTesting:UIRectEdgeTop];

    [scrollView setContentOffset:CGPointMake(0, -100)];
    [webView waitForNextVisibleContentRectUpdate];
    EXPECT_TRUE([topColorExtension _appearsBeforeViewInSubviewOrder:refreshControl.get()]);

    [scrollView setRefreshControl:nil];
    [scrollView setContentOffset:CGPointMake(0, 100)];
    [webView waitForNextVisibleContentRectUpdate];
    EXPECT_TRUE([contentView _appearsBeforeViewInSubviewOrder:topColorExtension.get()]);
}

TEST(WKScrollViewTests, TopScrollPocketCaptureColorAfterSettingHardStyle)
{
    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 600, 400)]);
    RetainPtr scrollView = [webView scrollView];

    [scrollView topEdgeEffect].style = UIScrollEdgeEffectStyle.softStyle;

    auto insets = UIEdgeInsetsMake(50, 0, 0, 0);
    [webView setObscuredContentInsets:insets];

    [scrollView setContentInsetAdjustmentBehavior:UIScrollViewContentInsetAdjustmentNever];
    [scrollView setContentInset:insets];

    [webView synchronouslyLoadTestPageNamed:@"top-fixed-element"];
    [webView waitForNextPresentationUpdate];

    EXPECT_NULL([scrollView _pocketColorForEdge:UIRectEdgeTop]);
    EXPECT_FALSE([scrollView _prefersSolidColorHardPocketForEdge:UIRectEdgeTop]);

    [scrollView topEdgeEffect].style = UIScrollEdgeEffectStyle.hardStyle;
    [webView waitForNextPresentationUpdate];

    auto topHardPocketColor = WebCore::colorFromCocoaColor([scrollView _pocketColorForEdge:UIRectEdgeTop]);
    EXPECT_WK_STREQ("rgb(255, 99, 71)", WebCore::serializationForCSS(topHardPocketColor));
    EXPECT_TRUE([scrollView _prefersSolidColorHardPocketForEdge:UIRectEdgeTop]);

    [scrollView topEdgeEffect].style = UIScrollEdgeEffectStyle.softStyle;

    // Removing the top fixed element should also remove the top color extension.
    [webView objectByEvaluatingJavaScript:@"document.querySelector('header').remove()"];
    [webView waitForNextPresentationUpdate];

    EXPECT_NULL([scrollView _pocketColorForEdge:UIRectEdgeTop]);
}

#endif // HAVE(LIQUID_GLASS)

TEST(WKScrollViewTests, SafeAreaEnvironmentVariablesAccountForObscuredContentInsets)
{
    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 400, 800)]);
    [webView setOverrideSafeAreaInset:UIEdgeInsetsMake(66, 10, 34, 10)];

    RetainPtr scrollView = [webView scrollView];

    auto insets = UIEdgeInsetsMake(100, 0, 20, 0);
    [webView setObscuredContentInsets:insets];

    [scrollView setContentInsetAdjustmentBehavior:UIScrollViewContentInsetAdjustmentNever];
    [scrollView setContentInset:insets];

    [webView synchronouslyLoadTestPageNamed:@"safe-area-env"];
    [webView waitForNextPresentationUpdate];

    auto computedBodyPadding = [&](NSString *side) {
        return [webView stringByEvaluatingJavaScript:[NSString stringWithFormat:@"getComputedStyle(document.body).padding%@", side]];
    };
    EXPECT_WK_STREQ("0px", computedBodyPadding(@"Top"));
    EXPECT_WK_STREQ("10px", computedBodyPadding(@"Left"));
    EXPECT_WK_STREQ("14px", computedBodyPadding(@"Bottom"));
    EXPECT_WK_STREQ("10px", computedBodyPadding(@"Right"));
}

TEST(WKScrollViewTests, ContentInsetAdjustmentBehaviorChangeAfterViewportFitChange)
{
    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 400, 800)]);
    RetainPtr scrollView = [webView scrollView];

    UIEdgeInsets insets = UIEdgeInsetsMake(0, 60, 0, 60);
    [webView setOverrideSafeAreaInset:insets];

    RetainPtr navigationDelegate = adoptNS([TestNavigationDelegate new]);
    [webView setNavigationDelegate:navigationDelegate.get()];

    [webView loadSimulatedRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://domain1.com"]] responseHTMLString:@"<html><head><meta name='viewport' content='width=device-width, initial-scale=1, viewport-fit=cover'></head></html>"];
    [navigationDelegate waitForDidFinishNavigation];
    [webView waitForNextPresentationUpdate];

    EXPECT_EQ([scrollView contentInsetAdjustmentBehavior], UIScrollViewContentInsetAdjustmentNever);
    EXPECT_EQ([scrollView adjustedContentInset], UIEdgeInsetsZero);

    EXPECT_WK_STREQ([webView stringByEvaluatingJavaScript:@"document.body.clientWidth"], "400");

    [webView loadSimulatedRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://domain2.com"]] responseHTMLString:@"<html><head><meta name='viewport' content='width=device-width, initial-scale=1'></head></html>"];
    [navigationDelegate waitForDidFinishNavigation];
    [webView waitForNextPresentationUpdate];

    EXPECT_EQ([scrollView contentInsetAdjustmentBehavior], UIScrollViewContentInsetAdjustmentAlways);
    EXPECT_EQ([scrollView adjustedContentInset], insets);

    EXPECT_WK_STREQ([webView stringByEvaluatingJavaScript:@"document.body.clientWidth"], "280");
}

TEST(WKScrollViewTests, DoNotCrashIfScrollViewOutlivesWebView)
{
    RetainPtr delegate = adoptNS([NSObject<UIScrollViewDelegate> new]);
    RetainPtr<UIScrollView> scrollView;

    @autoreleasepool {
        RetainPtr webView = adoptNS([[WKWebView alloc] initWithFrame:CGRectMake(0, 0, 400, 800)]);
        scrollView = [webView scrollView];
        [scrollView setDelegate:delegate];
    }

    Util::spinRunLoop(); // Make sure the web view has been deallocated.

    RetainPtr window = adoptNS([[UIWindow alloc] initWithFrame:CGRectMake(0, 0, 400, 800)]);
    [window addSubview:scrollView.get()];
}

TEST(WKScrollViewTests, VisibleContentRectUpdatesDuringInteractiveObscuredInsetsChangeAreThrottled)
{
    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 400, 800)]);

    auto insets = UIEdgeInsetsMake(50, 0, 0, 0);
    [webView _setObscuredInsets:insets];

    RetainPtr scrollView = [webView scrollView];
    [scrollView setContentInsetAdjustmentBehavior:UIScrollViewContentInsetAdjustmentNever];
    [scrollView setContentInset:insets];

    [webView synchronouslyLoadTestPageNamed:@"simple-tall"];
    [webView waitForNextPresentationUpdate];

    [scrollView setContentOffset:CGPointMake(0, 500)];
    [webView waitForNextVisibleContentRectUpdate];

    [webView _beginInteractiveObscuredInsetsChange];

    // First scroll within interactive mode triggers a timer.
    [scrollView setContentOffset:CGPointMake(0, 510)];
    [webView waitForNextPresentationUpdate];
    EXPECT_TRUE([webView _hasPendingVisibleContentRectUpdateTimerForTesting]);

    // Second scroll should be throttled too.
    [scrollView setContentOffset:CGPointMake(0, 520)];
    [webView waitForNextPresentationUpdate];
    EXPECT_TRUE([webView _hasPendingVisibleContentRectUpdateTimerForTesting]);
}

TEST(WKScrollViewTests, ThrottledVisibleContentRectUpdateTimerCancelledWhenEndingInteractiveUpdates)
{
    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 400, 800)]);

    auto insets = UIEdgeInsetsMake(50, 0, 0, 0);
    [webView _setObscuredInsets:insets];

    RetainPtr scrollView = [webView scrollView];
    [scrollView setContentInsetAdjustmentBehavior:UIScrollViewContentInsetAdjustmentNever];
    [scrollView setContentInset:insets];

    [webView synchronouslyLoadTestPageNamed:@"simple-tall"];
    [webView waitForNextPresentationUpdate];

    [scrollView setContentOffset:CGPointMake(0, 500)];
    [webView waitForNextVisibleContentRectUpdate];

    [webView _beginInteractiveObscuredInsetsChange];

    // First scroll within interactive mode triggers a timer.
    [scrollView setContentOffset:CGPointMake(0, 510)];
    [webView waitForNextPresentationUpdate];
    EXPECT_TRUE([webView _hasPendingVisibleContentRectUpdateTimerForTesting]);

    // Second scroll should be throttled too.
    [scrollView setContentOffset:CGPointMake(0, 520)];
    [webView waitForNextPresentationUpdate];
    EXPECT_TRUE([webView _hasPendingVisibleContentRectUpdateTimerForTesting]);

    // Ending interactive mode should cancel the timer.
    [webView _endInteractiveObscuredInsetsChange];
    EXPECT_FALSE([webView _hasPendingVisibleContentRectUpdateTimerForTesting]);

    // Verify we can still do normal updates
    [webView waitForNextVisibleContentRectUpdate];
    [scrollView setContentOffset:CGPointMake(0, 510)];
    [webView waitForNextPresentationUpdate];

    [scrollView setContentOffset:CGPointMake(0, 520)];
    [webView waitForNextPresentationUpdate];
    EXPECT_FALSE([webView _hasPendingVisibleContentRectUpdateTimerForTesting]);
}

TEST(WKScrollViewTests, ForcedVisibleContentRectUpdateCancelsPendingThrottleTimer)
{
    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 400, 800)]);

    auto insets = UIEdgeInsetsMake(50, 0, 0, 0);
    [webView _setObscuredInsets:insets];

    RetainPtr scrollView = [webView scrollView];
    [scrollView setContentInsetAdjustmentBehavior:UIScrollViewContentInsetAdjustmentNever];
    [scrollView setContentInset:insets];

    [webView synchronouslyLoadTestPageNamed:@"simple-tall"];
    [webView waitForNextPresentationUpdate];

    [scrollView setContentOffset:CGPointMake(0, 500)];
    [webView waitForNextVisibleContentRectUpdate];

    [webView _beginInteractiveObscuredInsetsChange];

    // First scroll within interactive mode triggers a timer.
    [scrollView setContentOffset:CGPointMake(0, 510)];
    [webView waitForNextPresentationUpdate];

    // Second scroll should be throttled too.
    [scrollView setContentOffset:CGPointMake(0, 520)];
    [webView waitForNextPresentationUpdate];
    EXPECT_TRUE([webView _hasPendingVisibleContentRectUpdateTimerForTesting]);

    // Force an update which should cancel pending throttle timer.
    [webView _scheduleForcedVisibleContentRectUpdate];
    EXPECT_FALSE([webView _hasPendingVisibleContentRectUpdateTimerForTesting]);

    [webView _endInteractiveObscuredInsetsChange];
}

TEST(WKScrollViewTests, FixedLayerPositionDoesNotFreezeDuringInteractiveObscuredInsetsChange)
{
    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 800)]);
    [webView synchronouslyLoadHTMLString:@"<meta name='viewport' content='width=device-width, initial-scale=1'>"
        "<style>"
        "body { margin: 0; height: 10000px; }"
        "#footer { position: fixed; bottom: 0; left: 0; width: 320px; height: 47px; background: blue; }"
        "</style>"
        "<div id='footer'></div>"];

    auto insets = UIEdgeInsetsMake(50, 0, 100, 0);
    [webView _setObscuredInsets:insets];
    RetainPtr scrollView = [webView scrollView];
    [scrollView setContentInsetAdjustmentBehavior:UIScrollViewContentInsetAdjustmentNever];
    [scrollView setContentInset:insets];
    [webView waitForNextPresentationUpdate];

    [scrollView setContentOffset:CGPointMake(0, 500)];
    [webView waitForNextVisibleContentRectUpdate];

    [webView _beginInteractiveObscuredInsetsChange];

    CGFloat maxDeviation = 0;
    constexpr int steps = 30;
    constexpr CGFloat webViewHeight = 800;
    constexpr CGFloat footerHeight = 47;

    for (int i = 0; i <= steps; i++) {
        CGFloat bottomInset = 100.0 - (i * 80.0 / steps);
        CGFloat scrollY = 500.0 + i * 8.0;

        [webView _setObscuredInsets:UIEdgeInsetsMake(50, 0, bottomInset, 0)];
        [scrollView setContentOffset:CGPointMake(0, scrollY)];

        [CATransaction begin];
        [CATransaction commit];
        [CATransaction flush];

        CGFloat expectedY = webViewHeight - bottomInset - footerHeight;

        __block CGFloat actualY = NAN;
        traverseLayerTree([webView layer], ^(CALayer *layer) {
            if (CGSizeEqualToSize(layer.bounds.size, CGSizeMake(320, footerHeight))) {
                auto rect = [layer convertRect:layer.bounds toLayer:[webView layer]];
                actualY = CGRectGetMinY(rect);
            }
        });

        if (!isnan(actualY))
            maxDeviation = std::max(maxDeviation, std::abs(actualY - expectedY));
    }

    [webView _endInteractiveObscuredInsetsChange];
    [webView waitForNextPresentationUpdate];

    // The footer's position should not freeze in the view between throttled visible content rect updates.
    EXPECT_LE(maxDeviation, 2);
}

} // namespace TestWebKitAPI

#endif // PLATFORM(IOS_FAMILY)
