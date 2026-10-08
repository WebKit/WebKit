/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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

#if ENABLE(IOS_TOUCH_EVENTS)

#import "Helpers/PlatformUtilities.h"
#import "Helpers/cocoa/TestNavigationDelegate.h"
#import "Helpers/cocoa/TestScriptMessageHandler.h"
#import "Helpers/cocoa/TestWKWebView.h"
#import "InstanceMethodSwizzler.h"
#import "UIKitSPIForTesting.h"
#import "WKTouchEventsGestureRecognizer.h"
#import <wtf/MonotonicTime.h>
#import <wtf/RetainPtr.h>
#import <wtf/WeakObjCPtr.h>
#import <wtf/cocoa/TypeCastsCocoa.h>
#import <wtf/darwin/DispatchExtras.h>

@interface UIView (WKContentView)
- (void)_touchEventsRecognized;
@end

@interface WKTouchEventTestTouch : UITouch
- (instancetype)initWithView:(UIView *)view location:(CGPoint)location;
- (void)setPhase:(UITouchPhase)phase;
@end

@implementation WKTouchEventTestTouch {
    WeakObjCPtr<UIView> _view;
    CGPoint _location;
    UITouchPhase _phase;
}

- (instancetype)initWithView:(UIView *)view location:(CGPoint)location
{
    self = [super init];
    if (!self)
        return nil;

    _view = view;
    _location = location;
    _phase = UITouchPhaseBegan;
    return self;
}

- (UIView *)view
{
    return _view.getAutoreleased();
}

- (NSArray<UIGestureRecognizer *> *)gestureRecognizers
{
    return @[];
}

- (CGPoint)locationInView:(UIView *)view
{
    return _location;
}

- (CGPoint)previousLocationInView:(UIView *)view
{
    return _location;
}

- (void)setPhase:(UITouchPhase)phase
{
    _phase = phase;
}

- (UITouchPhase)phase
{
    return _phase;
}

- (NSTimeInterval)timestamp
{
    return MonotonicTime::now().secondsSinceEpoch().value();
}

- (CGFloat)majorRadius
{
    return 1;
}

- (CGFloat)force
{
    return 0;
}

- (CGFloat)maximumPossibleForce
{
    return 1;
}

- (UITouchType)type
{
    return UITouchTypeDirect;
}

- (BOOL)_isPointerTouch
{
    return NO;
}

@end

@interface WKTouchEventTestUIEvent : UIEvent
- (void)setActiveTouches:(NSArray<UITouch *> *)touches;
@end

@implementation WKTouchEventTestUIEvent {
    RetainPtr<NSSet<UITouch *>> _activeTouches;
}

- (void)setActiveTouches:(NSArray<UITouch *> *)touches
{
    _activeTouches = [NSSet setWithArray:touches];
}

- (NSSet<UITouch *> *)touchesForGestureRecognizer:(UIGestureRecognizer *)gestureRecognizer
{
    return _activeTouches.get();
}

- (NSArray<UITouch *> *)coalescedTouchesForTouch:(UITouch *)touch
{
    return @[];
}

- (NSArray<UITouch *> *)predictedTouchesForTouch:(UITouch *)touch
{
    return @[];
}

@end

static WKWebView *globalWebView = nil;

@interface TouchEventScriptMessageHandler : NSObject<WKScriptMessageHandler>
@end

@implementation TouchEventScriptMessageHandler

- (void)userContentController:(WKUserContentController *)userContentController didReceiveScriptMessage:(WKScriptMessage *)message
{
    if ([message.body isEqualToString:@"touchend"]) {
        @autoreleasepool {
            // This @autoreleasepool ensures that the content view is also deallocated upon releasing the web view.
            [globalWebView removeFromSuperview];
            [globalWebView release];
            globalWebView = nil;
        }
    }
}

@end

static Class touchEventsGestureRecognizerClassSingleton()
{
    static Class result = nil;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        result = NSClassFromString(@"WKTouchEventsGestureRecognizer");
    });
    return result;
}

namespace TestWebKitAPI {

static WebKit::WKTouchPoint globalTouchPoint {
    .previousLocationInRootViewCoordinates = CGPointZero,
    .locationInRootViewCoordinates = CGPointZero,
    .locationInViewport = CGPointZero,
    .identifier = 100,
    .phase = UITouchPhaseBegan,
    .majorRadiusInWindowCoordinates = 1,
    .twist = 0,
    .force = 0,
    .altitudeAngle = 0,
    .azimuthAngle = 0,
    .touchType = WebKit::WKTouchPointType::Direct,
};
static WebKit::WKTouchEvent globalTouchEvent {
    .type = WebKit::WKTouchEventType::Begin,
    .timestamp = CACurrentMediaTime(),
    .locationInRootViewCoordinates = CGPointZero,
    .scale = 1,
    .rotation = 0,
    .inJavaScriptGesture = false,
    .touchPoints = { globalTouchPoint },
    .coalescedEvents = { },
    .predictedEvents = { },
    .isPotentialTap = true,
};
static void updateSimulatedTouchEvent(CGPoint location, UITouchPhase phase)
{
    globalTouchEvent.locationInRootViewCoordinates = location;
    globalTouchPoint.phase = phase;
    switch (phase) {
    case UITouchPhaseBegan:
        globalTouchEvent.type = WebKit::WKTouchEventType::Begin;
        break;
    case UITouchPhaseMoved:
        globalTouchEvent.type = WebKit::WKTouchEventType::Change;
        break;
    case UITouchPhaseEnded:
        globalTouchEvent.type = WebKit::WKTouchEventType::End;
        break;
    case UITouchPhaseCancelled:
        globalTouchEvent.type = WebKit::WKTouchEventType::Cancel;
        break;
    default:
        break;
    }
}

static const WebKit::WKTouchEvent* simulatedTouchEvent(id, SEL)
{
    return &globalTouchEvent;
}

static WKTouchEventsGestureRecognizer *touchEventsGestureRecognizerForWebView(WKWebView *webView)
{
    Class touchEventsGestureRecognizerClass = touchEventsGestureRecognizerClassSingleton();
    for (UIGestureRecognizer *gestureRecognizer in webView.textInputContentView.gestureRecognizers) {
        if ([gestureRecognizer isKindOfClass:touchEventsGestureRecognizerClass])
            return static_cast<WKTouchEventsGestureRecognizer *>(gestureRecognizer);
    }
    return nil;
}

static NSArray<NSString *> *touchMessageFields(NSString *message)
{
    return [message componentsSeparatedByString:@":"];
}

static NSString *touchMessageType(NSString *message)
{
    return touchMessageFields(message)[0];
}

static NSString *touchMessageTouches(NSString *message)
{
    return touchMessageFields(message)[1];
}

static NSString *touchMessageTargetTouches(NSString *message)
{
    return touchMessageFields(message)[2];
}

static NSString *touchMessageChangedTouches(NSString *message)
{
    return touchMessageFields(message)[3];
}

static NSInteger onlyTouchIdentifier(NSString *identifiers)
{
    EXPECT_FALSE([identifiers containsString:@","]);
    return identifiers.integerValue;
}

static void expectTouchMessage(NSString *message, const char* type, NSString *touches, NSString *targetTouches, NSString *changedTouches)
{
    EXPECT_WK_STREQ(type, touchMessageType(message));
    EXPECT_WK_STREQ(touches, touchMessageTouches(message));
    EXPECT_WK_STREQ(targetTouches, touchMessageTargetTouches(message));
    EXPECT_WK_STREQ(changedTouches, touchMessageChangedTouches(message));
}

static RetainPtr<TestWKWebView> createTouchLoggingWebView(RetainPtr<TestScriptMessageHandler>& messageHandler)
{
    messageHandler = adoptNS([TestScriptMessageHandler new]);

    RetainPtr configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [[configuration userContentController] addScriptMessageHandler:messageHandler.get() name:@"testHandler"];

    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500) configuration:configuration.get() addToWindow:YES]);
    [webView synchronouslyLoadHTMLStringAndWaitUntilAllImmediateChildFramesPaint:@"<!DOCTYPE html>"
        @"<html><meta name='viewport' content='width=device-width, initial-scale=1, user-scalable=no'>"
        @"<body style='margin: 0'><div id='target' style='width: 100vw; height: 100vh'></div>"
        @"<script>"
        @"let target = document.getElementById('target');"
        @"target.addEventListener('touchstart', logTouch);"
        @"target.addEventListener('touchmove', logTouch);"
        @"target.addEventListener('touchend', logTouch);"
        @"target.addEventListener('touchcancel', logTouch);"
        @"function identifiers(touches) {"
        @"  return Array.from(touches).map(t => t.identifier).sort((a, b) => a - b).join(',');"
        @"}"
        @"function logTouch(event) {"
        @"  event.preventDefault();"
        @"  webkit.messageHandlers.testHandler.postMessage(`${event.type}:${identifiers(event.touches)}:${identifiers(event.targetTouches)}:${identifiers(event.changedTouches)}`);"
        @"}"
        @"</script></body></html>"];
    return webView;
}

static NSString *waitForTouchMessage(TestScriptMessageHandler *messageHandler)
{
    return dynamic_objc_cast<NSString>([messageHandler waitForMessage].body);
}

TEST(TouchEventTests, DestroyWebViewWhileHandlingTouchEnd)
{
    InstanceMethodSwizzler lastTouchEventSwizzler { touchEventsGestureRecognizerClassSingleton(), @selector(lastTouchEvent), reinterpret_cast<IMP>(simulatedTouchEvent) };
    @autoreleasepool {
        RetainPtr messageHandler = adoptNS([TouchEventScriptMessageHandler new]);
        RetainPtr controller = adoptNS([[WKUserContentController alloc] init]);
        [controller addScriptMessageHandler:messageHandler.get() name:@"testHandler"];

        RetainPtr configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
        [configuration setUserContentController:controller.get()];

        globalWebView = [[WKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500) configuration:configuration.get()];
        RetainPtr hostWindow = adoptNS([[UIWindow alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
        [hostWindow setHidden:NO];
        [hostWindow addSubview:globalWebView];

        [globalWebView loadRequest:[NSURLRequest requestWithURL:[NSBundle.test_resourcesBundle URLForResource:@"active-touch-events" withExtension:@"html"]]];
        [globalWebView _test_waitForDidFinishNavigation];

        updateSimulatedTouchEvent(CGPointMake(100, 100), UITouchPhaseBegan);
        [[globalWebView textInputContentView] _touchEventsRecognized];

        updateSimulatedTouchEvent(CGPointMake(100, 100), UITouchPhaseEnded);
        [[globalWebView textInputContentView] _touchEventsRecognized];
    }

    __block bool done = false;
    dispatch_async(mainDispatchQueueSingleton(), ^{
        done = true;
    });
    TestWebKitAPI::Util::run(&done);
}

static void runTouchStartWithConcurrentFinishedTouchTest(UITouchPhase finishedPhase, const char* expectedFinishedEventType)
{
    RetainPtr<TestScriptMessageHandler> messageHandler;
    auto webView = createTouchLoggingWebView(messageHandler);
    RetainPtr contentView = [webView textInputContentView];
    RetainPtr recognizer = touchEventsGestureRecognizerForWebView(webView.get());
    ASSERT_TRUE(recognizer);

    RetainPtr event = adoptNS([WKTouchEventTestUIEvent new]);
    RetainPtr oldTouch = adoptNS([[WKTouchEventTestTouch alloc] initWithView:contentView.get() location:CGPointMake(80, 100)]);
    RetainPtr newTouch = adoptNS([[WKTouchEventTestTouch alloc] initWithView:contentView.get() location:CGPointMake(180, 100)]);

    [event setActiveTouches:@[ oldTouch.get() ]];
    [recognizer touchesBegan:[NSSet setWithObject:oldTouch.get()] withEvent:event.get()];
    RetainPtr firstStart = waitForTouchMessage(messageHandler.get());
    EXPECT_WK_STREQ("touchstart", touchMessageType(firstStart.get()));
    auto oldIdentifier = onlyTouchIdentifier(touchMessageChangedTouches(firstStart.get()));
    auto oldIdentifierString = [NSString stringWithFormat:@"%ld", static_cast<long>(oldIdentifier)];
    expectTouchMessage(firstStart.get(), "touchstart", oldIdentifierString, oldIdentifierString, oldIdentifierString);

    [oldTouch setPhase:finishedPhase];
    [newTouch setPhase:UITouchPhaseBegan];
    [event setActiveTouches:@[ oldTouch.get(), newTouch.get() ]];
    [recognizer touchesBegan:[NSSet setWithObject:newTouch.get()] withEvent:event.get()];
    RetainPtr secondStart = waitForTouchMessage(messageHandler.get());
    EXPECT_WK_STREQ("touchstart", touchMessageType(secondStart.get()));
    auto newIdentifier = onlyTouchIdentifier(touchMessageChangedTouches(secondStart.get()));
    EXPECT_NE(oldIdentifier, newIdentifier);
    auto newIdentifierString = [NSString stringWithFormat:@"%ld", static_cast<long>(newIdentifier)];
    auto bothIdentifiersString = [NSString stringWithFormat:@"%@,%@", oldIdentifierString, newIdentifierString];
    expectTouchMessage(secondStart.get(), "touchstart", bothIdentifiersString, bothIdentifiersString, newIdentifierString);

    if (finishedPhase == UITouchPhaseEnded)
        [recognizer touchesEnded:[NSSet setWithObject:oldTouch.get()] withEvent:event.get()];
    else
        [recognizer touchesCancelled:[NSSet setWithObject:oldTouch.get()] withEvent:event.get()];
    RetainPtr oldFinished = waitForTouchMessage(messageHandler.get());
    expectTouchMessage(oldFinished.get(), expectedFinishedEventType, newIdentifierString, newIdentifierString, oldIdentifierString);

    [newTouch setPhase:UITouchPhaseEnded];
    [event setActiveTouches:@[ newTouch.get() ]];
    [recognizer touchesEnded:[NSSet setWithObject:newTouch.get()] withEvent:event.get()];
    RetainPtr newEnd = waitForTouchMessage(messageHandler.get());
    expectTouchMessage(newEnd.get(), "touchend", @"", @"", newIdentifierString);
}

TEST(TouchEventTests, DispatchesTouchStartWhenTouchBeginsAsAnotherTouchEnds)
{
    runTouchStartWithConcurrentFinishedTouchTest(UITouchPhaseEnded, "touchend");
}

TEST(TouchEventTests, DispatchesTouchStartWhenTouchBeginsAsAnotherTouchCancels)
{
    runTouchStartWithConcurrentFinishedTouchTest(UITouchPhaseCancelled, "touchcancel");
}

TEST(TouchEventTests, CoalescesMovedAndEndedTouchBatchWithoutDuplicateMove)
{
    RetainPtr<TestScriptMessageHandler> messageHandler;
    auto webView = createTouchLoggingWebView(messageHandler);
    RetainPtr contentView = [webView textInputContentView];
    RetainPtr recognizer = touchEventsGestureRecognizerForWebView(webView.get());
    ASSERT_TRUE(recognizer);

    RetainPtr event = adoptNS([WKTouchEventTestUIEvent new]);
    RetainPtr movingTouch = adoptNS([[WKTouchEventTestTouch alloc] initWithView:contentView.get() location:CGPointMake(80, 100)]);
    RetainPtr endingTouch = adoptNS([[WKTouchEventTestTouch alloc] initWithView:contentView.get() location:CGPointMake(180, 100)]);

    [event setActiveTouches:@[ movingTouch.get() ]];
    [recognizer touchesBegan:[NSSet setWithObject:movingTouch.get()] withEvent:event.get()];
    RetainPtr movingStart = waitForTouchMessage(messageHandler.get());
    EXPECT_WK_STREQ("touchstart", touchMessageType(movingStart.get()));
    auto movingIdentifier = onlyTouchIdentifier(touchMessageChangedTouches(movingStart.get()));
    auto movingIdentifierString = [NSString stringWithFormat:@"%ld", static_cast<long>(movingIdentifier)];
    expectTouchMessage(movingStart.get(), "touchstart", movingIdentifierString, movingIdentifierString, movingIdentifierString);

    [movingTouch setPhase:UITouchPhaseStationary];
    [event setActiveTouches:@[ movingTouch.get(), endingTouch.get() ]];
    [recognizer touchesBegan:[NSSet setWithObject:endingTouch.get()] withEvent:event.get()];
    RetainPtr endingStart = waitForTouchMessage(messageHandler.get());
    EXPECT_WK_STREQ("touchstart", touchMessageType(endingStart.get()));
    auto endingIdentifier = onlyTouchIdentifier(touchMessageChangedTouches(endingStart.get()));
    EXPECT_NE(movingIdentifier, endingIdentifier);
    auto endingIdentifierString = [NSString stringWithFormat:@"%ld", static_cast<long>(endingIdentifier)];
    auto bothIdentifiersString = [NSString stringWithFormat:@"%@,%@", movingIdentifierString, endingIdentifierString];
    expectTouchMessage(endingStart.get(), "touchstart", bothIdentifiersString, bothIdentifiersString, endingIdentifierString);

    [movingTouch setPhase:UITouchPhaseMoved];
    [endingTouch setPhase:UITouchPhaseEnded];
    [recognizer touchesMoved:[NSSet setWithObject:movingTouch.get()] withEvent:event.get()];
    [recognizer touchesEnded:[NSSet setWithObject:endingTouch.get()] withEvent:event.get()];

    RetainPtr end = waitForTouchMessage(messageHandler.get());
    expectTouchMessage(end.get(), "touchend", movingIdentifierString, movingIdentifierString, endingIdentifierString);
}

} // namespace TestWebKitAPI

#endif // ENABLE(IOS_TOUCH_EVENTS)
