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

#import "InstanceMethodSwizzler.h"
#import "PlatformUtilities.h"
#import "TestNavigationDelegate.h"
#import "TestWKWebView.h"
#import "UIKitSPIForTesting.h"
#import "WKTouchEventsGestureRecognizer.h"
#import <wtf/RetainPtr.h>

@interface UIView (WKContentView)
- (void)_touchEventsRecognized:(WKTouchEventsGestureRecognizer *)gestureRecognizer;
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

static Class touchEventsGestureRecognizerClass()
{
    static Class result = nil;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        result = NSClassFromString(@"WKTouchEventsGestureRecognizer");
    });
    return result;
}

@interface WKWebView (TouchEventTests)
@property (nonatomic, readonly) WKTouchEventsGestureRecognizer *touchEventGestureRecognizer;
@end

@implementation WKWebView (TouchEventTests)

- (WKTouchEventsGestureRecognizer *)touchEventGestureRecognizer
{
    for (UIGestureRecognizer *gestureRecognizer in self.textInputContentView.gestureRecognizers) {
        if ([gestureRecognizer isKindOfClass:touchEventsGestureRecognizerClass()])
            return (WKTouchEventsGestureRecognizer *)gestureRecognizer;
    }
    return nil;
}

@end

namespace TestWebKitAPI {

static WebKit::WKTouchPoint globalTouchPoint { CGPointZero, CGPointZero, 100, UITouchPhaseBegan, 1, 0, 0, 0, WebKit::WKTouchPointType::Direct };
static WebKit::WKTouchEvent globalTouchEvent { WebKit::WKTouchEventType::Begin, CACurrentMediaTime(), CGPointZero, CGPointZero, 1, 0, false, { globalTouchPoint }, { }, { }, true };
static void updateSimulatedTouchEvent(CGPoint location, UITouchPhase phase)
{
    globalTouchPoint.locationInScreenCoordinates = location;
    globalTouchPoint.locationInDocumentCoordinates = location;
    globalTouchEvent.locationInScreenCoordinates = location;
    globalTouchEvent.locationInDocumentCoordinates = location;
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

TEST(TouchEventTests, DestroyWebViewWhileHandlingTouchEnd)
{
    InstanceMethodSwizzler lastTouchEventSwizzler { touchEventsGestureRecognizerClass(), @selector(lastTouchEvent), reinterpret_cast<IMP>(simulatedTouchEvent) };
    @autoreleasepool {
        auto messageHandler = adoptNS([TouchEventScriptMessageHandler new]);
        auto controller = adoptNS([[WKUserContentController alloc] init]);
        [controller addScriptMessageHandler:messageHandler.get() name:@"testHandler"];

        auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
        [configuration setUserContentController:controller.get()];

        globalWebView = [[WKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500) configuration:configuration.get()];
        auto hostWindow = adoptNS([[UIWindow alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
        [hostWindow setHidden:NO];
        [hostWindow addSubview:globalWebView];

        [globalWebView loadRequest:[NSURLRequest requestWithURL:[NSBundle.test_resourcesBundle URLForResource:@"active-touch-events" withExtension:@"html"]]];
        [globalWebView _test_waitForDidFinishNavigation];

        updateSimulatedTouchEvent(CGPointMake(100, 100), UITouchPhaseBegan);
        [[globalWebView textInputContentView] _touchEventsRecognized:globalWebView.touchEventGestureRecognizer];

        updateSimulatedTouchEvent(CGPointMake(100, 100), UITouchPhaseEnded);
        [[globalWebView textInputContentView] _touchEventsRecognized:globalWebView.touchEventGestureRecognizer];
    }

    __block bool done = false;
    dispatch_async(dispatch_get_main_queue(), ^{
        done = true;
    });
    TestWebKitAPI::Util::run(&done);
}

} // namespace TestWebKitAPI

#endif // ENABLE(IOS_TOUCH_EVENTS)
