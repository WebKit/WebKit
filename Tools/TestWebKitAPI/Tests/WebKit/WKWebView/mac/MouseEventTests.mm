/*
 * Copyright (C) 2023 Apple Inc. All rights reserved.
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

#if PLATFORM(MAC)

#import "ClassMethodSwizzler.h"
#import "InstanceMethodSwizzler.h"
#import "Helpers/PlatformUtilities.h"
#import "Helpers/cocoa/TestNavigationDelegate.h"
#import "TestURLSchemeHandler.h"
#import "Helpers/cocoa/TestWKWebView.h"
#import <WebKit/WKProcessPoolPrivate.h>
#import <WebKit/WKWebViewConfigurationPrivate.h>
#import <WebKit/WKWebViewPrivate.h>
#import <WebKit/WKWebViewPrivateForTesting.h>
#import <WebKit/_WKProcessPoolConfiguration.h>
#import <wtf/RunLoop.h>
#import <wtf/Scope.h>
#import <wtf/cocoa/TypeCastsCocoa.h>

@interface OverrideMouseLocation : NSObject
+ (NSPoint)overrideMouseLocation;
+ (void)setOverrideMouseLocation:(NSPoint)location;
@end

@implementation OverrideMouseLocation
static NSPoint overrideMouseLocation = { 0, 0 };
+ (NSPoint)overrideMouseLocation {
    return overrideMouseLocation;
}

+ (void)setOverrideMouseLocation:(NSPoint)location {
    overrideMouseLocation = location;
}
@end

@interface NSWindow (MouseEventTestsWindowSwizzler)
- (NSPoint)_mouseEventTests_mouseLocationOutsideOfEventStream;
@end

@implementation NSWindow (MouseEventTestsWindowSwizzler)
- (NSPoint)_mouseEventTests_mouseLocationOutsideOfEventStream
{
    return [self convertPointFromScreen:[OverrideMouseLocation overrideMouseLocation]];
}
@end

static IMP s_originalScreenFrameIMP;
static std::optional<NSRect> s_overrideScreenFrame;

@interface NSScreen (MouseEventTestsSwizzler)
- (NSRect)_mouseEventTests_frame;
@end

@implementation NSScreen (MouseEventTestsSwizzler)
- (NSRect)_mouseEventTests_frame
{
    if (s_overrideScreenFrame)
        return *s_overrideScreenFrame;
    return reinterpret_cast<NSRect (*)(id, SEL)>(s_originalScreenFrameIMP)(self, @selector(frame));
}
@end

namespace TestWebKitAPI {

TEST(MouseEventTests, SendMouseMoveEventStream)
{
    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600)]);
    [webView synchronouslyLoadHTMLString:@"<!DOCTYPE html>"
        "<html>"
        "<head>"
        "<style>"
        "    body, html { margin: 0; width: 100%; height: 100%; }"
        "</style>"
        "</head>"
        "<body>"
        "<script>"
        "    let eventData = [];"
        "    addEventListener('mousemove', event => eventData.push({ x: event.clientX, y: event.clientY }));"
        "</script>"
        "</body>"
        "</html>"];

    for (unsigned i = 0; i <= 300; ++i) {
        [webView mouseMoveToPoint:NSMakePoint(100 + i, 300) withFlags:0];
        Util::runFor(8_ms);
    }

    [webView waitForPendingMouseEvents];

    NSArray<NSDictionary *> *mouseEvents = [webView objectByEvaluatingJavaScript:@"eventData"];
    EXPECT_GT(mouseEvents.count, 2U);
    EXPECT_EQ([mouseEvents.firstObject[@"x"] doubleValue], 100.0);
    EXPECT_EQ([mouseEvents.firstObject[@"y"] doubleValue], 300.0);
    EXPECT_EQ([mouseEvents.lastObject[@"x"] doubleValue], 400.0);
    EXPECT_EQ([mouseEvents.lastObject[@"y"] doubleValue], 300.0);
}

TEST(MouseEventTests, CoalesceMouseMoveEvents)
{
    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600)]);
    [webView synchronouslyLoadHTMLString:@"<!DOCTYPE html>"
        "<html>"
        "<head>"
        "<style>"
        "body, html { margin: 0; width: 100%; height: 100%; }"
        "</style>"
        "</head>"
        "<body>"
        "<script>"
        "let allMouseEvents = [];"
        "function pushMouseEvent(event) {"
        "    allMouseEvents.push({"
        "        x: event.clientX,"
        "        y: event.clientY,"
        "        type: event.type"
        "    });"
        "}"
        "addEventListener('mousemove', pushMouseEvent);"
        "addEventListener('mousedown', pushMouseEvent);"
        "addEventListener('mouseup', pushMouseEvent);"
        "</script>"
        "</body>"
        "</html>"];

    [webView mouseEnterAtPoint:NSMakePoint(100, 300)];
    for (unsigned i = 1; i <= 200; ++i) {
        [webView mouseMoveToPoint:NSMakePoint(100 + i, 300) withFlags:0];
        Util::runFor(100_us);
    }
    [webView mouseDownAtPoint:NSMakePoint(300, 300) simulatePressure:NO];
    [webView mouseUpAtPoint:NSMakePoint(300, 300)];
    [webView waitForPendingMouseEvents];

    NSArray<NSDictionary *> *mouseEvents = [webView objectByEvaluatingJavaScript:@"allMouseEvents"];
    auto checkEventAtIndex = [&](NSString *type, NSPoint location, NSUInteger index) {
        auto info = mouseEvents[index];
        EXPECT_WK_STREQ(type, dynamic_objc_cast<NSString>(info[@"type"]));
        EXPECT_EQ([info[@"x"] floatValue], location.x);
        EXPECT_EQ([info[@"y"] floatValue], location.y);
    };

    auto numberOfMouseEvents = mouseEvents.count;
    EXPECT_GE(numberOfMouseEvents, 4U);
    EXPECT_LT(numberOfMouseEvents, 200U);

    checkEventAtIndex(@"mousemove", NSMakePoint(101, 300), 0);
    checkEventAtIndex(@"mousemove", NSMakePoint(300, 300), numberOfMouseEvents - 3);
    checkEventAtIndex(@"mousedown", NSMakePoint(300, 300), numberOfMouseEvents - 2);
    checkEventAtIndex(@"mouseup", NSMakePoint(300, 300), numberOfMouseEvents - 1);
}

TEST(MouseEventTests, ProcessSwapWithDeferredMouseMoveEventCompletion)
{
    RetainPtr processPoolConfiguration = adoptNS([[_WKProcessPoolConfiguration alloc] init]);
    [processPoolConfiguration setProcessSwapsOnNavigation:YES];
    [processPoolConfiguration setUsesWebProcessCache:YES];
    [processPoolConfiguration setPrewarmsProcessesAutomatically:YES];
    [processPoolConfiguration setProcessSwapsOnNavigationWithinSameNonHTTPFamilyProtocol:YES];

    RetainPtr processPool = adoptNS([[WKProcessPool alloc] _initWithConfiguration:processPoolConfiguration.get()]);
    RetainPtr configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [configuration setProcessPool:processPool.get()];

    RetainPtr handler = adoptNS([[TestURLSchemeHandler alloc] init]);
    [handler setStartURLSchemeTaskHandler:^(WKWebView *, id<WKURLSchemeTask> task) {
        auto host = task.request.URL.host;
        if ([host isEqualToString:@"www.apple.com"])
            return respond(task, "<body>Hello world</body>");

        if ([host isEqualToString:@"webkit.org"]) {
            return respond(task, "<body style='width: 100%; height: 100%;'>"
                "<script>"
                "    document.body.addEventListener('mousemove', () => {"
                "        location.href = 'pson://www.apple.com/index.html';"
                "    });"
                "</script>"
                "</body>");
        }
    }];
    [configuration setURLSchemeHandler:handler.get() forURLScheme:@"PSON"];

    RetainPtr navigationDelegate = adoptNS([[TestNavigationDelegate alloc] init]);
    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
    [webView setNavigationDelegate:navigationDelegate.get()];

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"pson://webkit.org/index.html"]]];
    [navigationDelegate waitForDidFinishNavigation];

    [webView mouseEnterAtPoint:NSMakePoint(400, 300)];

    for (int iteration = 0; iteration < 3; ++iteration) {
        [webView mouseMoveToPoint:NSMakePoint(401, 301) withFlags:0];
        [webView mouseMoveToPoint:NSMakePoint(402, 302) withFlags:0];
        [navigationDelegate waitForDidFinishNavigation];

        [webView goBack];
        [navigationDelegate waitForDidFinishNavigation];
    }
}

TEST(MouseEventTests, TerminateWebContentProcessDuringMouseEventHandling)
{
    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600)]);
    [webView synchronouslyLoadHTMLString:@""];

    RunLoop::mainSingleton().dispatchAfter(5_ms, [&] {
        [webView _killWebContentProcessAndResetState];
    });
    for (unsigned i = 0; i < 10; ++i) {
        [webView mouseMoveToPoint:NSMakePoint(100 + i, 300) withFlags:0];
        Util::runFor(1_ms);
    }
    [webView waitForPendingMouseEvents];
}

TEST(MouseEventTests, MouseEnterDoesNotDispatchMultipleMouseMoveEvents)
{
    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600)]);
    [webView removeFromSuperview];
    [webView addToTestWindow];

    RetainPtr trackingAreas = [webView trackingAreas];
    EXPECT_EQ([trackingAreas count], 3U); // The first two are created by WebKit, and the last created by AppKit.

    [webView synchronouslyLoadHTMLString:@"<!DOCTYPE html>"
        "<html>"
        "<head>"
        "<style>"
        "    body, html { margin: 0; width: 100%; height: 100%; }"
        "</style>"
        "</head>"
        "<body>"
        "<script>"
        "    let eventData = [];"
        "    addEventListener('mousemove', event => eventData.push({ x: event.clientX, y: event.clientY }));"
        "</script>"
        "</body>"
        "</html>"];

    RetainPtr firstEvent = [NSEvent enterExitEventWithType:NSEventTypeMouseEntered location:NSMakePoint(100, 100) modifierFlags:0 timestamp:[webView eventTimestamp] windowNumber:[[webView window] windowNumber] context:[NSGraphicsContext currentContext] eventNumber:1 trackingNumber:1 userData:nil];

    RetainPtr secondEvent = [NSEvent enterExitEventWithType:NSEventTypeMouseEntered location:NSMakePoint(100, 100) modifierFlags:0 timestamp:[webView eventTimestamp] windowNumber:[[webView window] windowNumber] context:[NSGraphicsContext currentContext] eventNumber:2 trackingNumber:1 userData:nil];

    InstanceMethodSwizzler trackingAreaSwizzler(NSEvent.class, @selector(trackingArea), imp_implementationWithBlock(^(NSEvent *event) {
        if (event != firstEvent.get() && event != secondEvent.get())
            return (NSTrackingArea *)nil;

        NSUInteger index = event == firstEvent.get() ? 0 : 1;
        return [trackingAreas objectAtIndex:index];
    }));

    [webView _simulateMouseEnter:firstEvent.get()];
    [webView _simulateMouseEnter:secondEvent.get()];

    [webView waitForPendingMouseEvents];

    RetainPtr mouseEvents = [webView objectByEvaluatingJavaScript:@"eventData"];
    EXPECT_EQ([mouseEvents count], 1U);
}

TEST(MouseEventTests, ShouldDelayWindowOrderingForEvent)
{
    RetainPtr processPoolConfiguration = adoptNS([[_WKProcessPoolConfiguration alloc] init]);
    [processPoolConfiguration setIgnoreSynchronousMessagingTimeoutsForTesting:YES];

    RetainPtr configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 800, 600) configuration:configuration.get() processPoolConfiguration:processPoolConfiguration.get()]);
    [[webView window] resignKeyWindow];
    [webView synchronouslyLoadTestPageNamed:@"lots-of-text"];
    [webView objectByEvaluatingJavaScript:@"const t = document.body.childNodes[0]; getSelection().setBaseAndExtent(t, 0, t, 400);"];
    [webView waitForNextPresentationUpdate];

    auto makeMouseEventAt = [webView](float x, float y) {
        auto windowHeight = NSHeight([[webView window] frame]);
        return [NSEvent mouseEventWithType:NSEventTypeLeftMouseDown location:NSMakePoint(x, windowHeight - y) modifierFlags:0 timestamp:0 windowNumber:[webView window].windowNumber context:[NSGraphicsContext currentContext] eventNumber:1 clickCount:1 pressure:NO];
    };

    EXPECT_TRUE([webView shouldDelayWindowOrderingForEvent:makeMouseEventAt(16, 16)]);

    [webView evaluateJavaScript:@"while (1);" completionHandler:nil];

    EXPECT_FALSE([webView shouldDelayWindowOrderingForEvent:makeMouseEventAt(16, 500)]);
}

static void runModifierIsKeptWhenJSInterceptsClickTest(NSEventModifierFlags modifiers)
{
    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600)]);
    [webView synchronouslyLoadHTMLString:@"<!DOCTYPE html>"
        "<html>"
        "<head>"
        "<style>"
        "body, html, div { margin: 0; width: 100%; height: 100%; }"
        "</style>"
        "</head>"
        "<body>"
        "<a href='https://www.apple.com' id='testLink'><div>Link</div></a>"
        "<script>"
        "function handleClickEvent(e) {"
        "    e.stopPropagation();"
        "    e.stopImmediatePropagation();"
        "    e.preventDefault();"
        "    testLink.removeEventListener('click', handleClickEvent);"
        "    let newMouseEvent1 = document.createEvent('MouseEvents');"
        "    newMouseEvent1.initMouseEvent('click', e.bubbles, e.cancelable, e.view, e.detail, e.screenX, e.screenY, e.clientX, e.clientY, e.ctrlKey, e.altKey, e.shiftKey, e.metaKey, e.button, e.relatedTarget);"
        "    let newMouseEvent2 = document.createEvent('MouseEvents');"
        "    newMouseEvent2.initMouseEvent('click', e.bubbles, e.cancelable, e.view, e.detail, e.screenX, e.screenY, e.clientX, e.clientY, e.ctrlKey, e.altKey, e.shiftKey, e.metaKey, e.button, e.relatedTarget);"
        "    setTimeout(() => {"
        "        testLink.dispatchEvent(newMouseEvent1);"
        "    }, 0);"
        "    setTimeout(() => {"
        "        testLink.dispatchEvent(newMouseEvent2);"
        "    }, 0);"
        "}"
        "testLink.addEventListener('click', handleClickEvent);"
        "</script>"
        "</body>"
        "</html>"];

    __block unsigned navigationCount = 0;
    RetainPtr navigationDelegate = adoptNS([[TestNavigationDelegate alloc] init]);
    navigationDelegate.get().decidePolicyForNavigationAction = ^(WKNavigationAction *action, void (^completionHandler)(WKNavigationActionPolicy)) {
        if (!navigationCount) {
            // Modifiers should be kept the first time.
            EXPECT_EQ(action.modifierFlags, modifiers);
        } else {
            // Any further attempts to simulate events with the same modifiers should lose
            // the modifiers since the user click was consumed the first time around.
            EXPECT_EQ(action.modifierFlags, 0U);
        }
        ++navigationCount;
        completionHandler(WKNavigationActionPolicyCancel);
    };
    webView.get().navigationDelegate = navigationDelegate.get();

    [webView mouseEnterAtPoint:NSMakePoint(100, 300)];
    [webView mouseDownAtPoint:NSMakePoint(300, 300) simulatePressure:NO withFlags:modifiers eventType:NSEventTypeLeftMouseDown];
    [webView mouseUpAtPoint:NSMakePoint(300, 300) withFlags:modifiers eventType:NSEventTypeLeftMouseUp];

    while (navigationCount != 2)
        Util::spinRunLoop(10);
}

TEST(MouseEventTests, CmdModifierIsKeptWhenJSInterceptsClick)
{
    runModifierIsKeptWhenJSInterceptsClickTest(NSEventModifierFlagCommand);
}

TEST(MouseEventTests, ShiftModifierIsKeptWhenJSInterceptsClick)
{
    runModifierIsKeptWhenJSInterceptsClickTest(NSEventModifierFlagShift);
}

TEST(MouseEventTests, AltModifierIsKeptWhenJSInterceptsClick)
{
    runModifierIsKeptWhenJSInterceptsClickTest(NSEventModifierFlagOption);
}

TEST(MouseEventTests, CmdShiftModifierIsKeptWhenJSInterceptsClick)
{
    runModifierIsKeptWhenJSInterceptsClickTest(NSEventModifierFlagCommand | NSEventModifierFlagShift);
}

static void runDispatchMouseLeaveEventOnWindowMoveTest(NSPoint initialMouseLocationInWindow, NSRect initialFrame, NSRect finalFrame, int expectedNumberOfMouseLeaveEvents)
{
    ClassMethodSwizzler swizzler([NSEvent class], @selector(mouseLocation), [OverrideMouseLocation methodForSelector:@selector(overrideMouseLocation)]);

    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:initialFrame]);
    [webView removeFromSuperview];
    [webView addToTestWindow];

    [webView synchronouslyLoadHTMLString:@"<!DOCTYPE html>"
        "<html>"
        "<head>"
        "<style>"
        "    body, html { margin: 0; width: 100%; height: 100%; }"
        "    #target {width: 100px; height: 100px; background-color: red; position: absolute; bottom: 0; right: 0;}"
        "</style>"
        "</head>"
        "<body>"
        "<div id='target'></div>"
        "<script>"
        "    let mouseLeaveCount = 0;"
        "    let target = document.getElementById('target');"
        "    target.addEventListener('mouseleave', function() { mouseLeaveCount++ });"
        "</script>"
        "</body>"
        "</html>"];

    [webView mouseMoveToPoint:initialMouseLocationInWindow withFlags:0];
    [OverrideMouseLocation setOverrideMouseLocation:initialMouseLocationInWindow];

    [[webView window] setFrame:finalFrame display:YES];

    [webView waitForPendingMouseEvents];

    EXPECT_EQ([[webView objectByEvaluatingJavaScript:@"mouseLeaveCount"] intValue], expectedNumberOfMouseLeaveEvents);
}

TEST(MouseEventTests, WindowChangeShouldCauseMouseLeaveEvent)
{
    runDispatchMouseLeaveEventOnWindowMoveTest(NSMakePoint(350, 50), NSMakeRect(0, 0, 400, 400), NSMakeRect(100, 0, 400, 400), 1);
}
TEST(MouseEventTests, WindowChangeShouldNotCauseMouseLeaveEvent)
{
    runDispatchMouseLeaveEventOnWindowMoveTest(NSMakePoint(250, 50), NSMakeRect(0, 0, 400, 400), NSMakeRect(100, 0, 400, 400), 0);
}

TEST(MouseEventTests, WindowChangeWithNonZeroWebViewOffsetAndObscuredInsets)
{
    [OverrideMouseLocation setOverrideMouseLocation:NSZeroPoint];
    auto resetOverride = makeScopeExit([] {
        [OverrideMouseLocation setOverrideMouseLocation:NSZeroPoint];
    });
    InstanceMethodSwizzler windowSwizzler([NSWindow class], @selector(mouseLocationOutsideOfEventStream), [NSWindow instanceMethodForSelector:@selector(_mouseEventTests_mouseLocationOutsideOfEventStream)]);
    ClassMethodSwizzler swizzler([NSEvent class], @selector(mouseLocation), [OverrideMouseLocation methodForSelector:@selector(overrideMouseLocation)]);

    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 400, 300)]);
    [webView removeFromSuperview];
    [webView addToTestWindow];
    [[webView window] setFrame:NSMakeRect(100, 100, 600, 500) display:YES];
    [webView setFrame:NSMakeRect(80, 60, 400, 300)];
    [webView _setObscuredContentInsets:NSEdgeInsetsMake(50, 0, 0, 0) immediate:YES];

    bool didEnter = false;
    bool didLeave = false;
    [webView performAfterReceivingMessage:@"mouseenter" action:[&] { didEnter = true; }];
    [webView performAfterReceivingMessage:@"mouseleave" action:[&] { didLeave = true; }];
    [webView synchronouslyLoadHTMLString:@"<!DOCTYPE html>"
        "<html>"
        "<head>"
        "<style>"
        "    body, html { margin: 0; width: 100%; height: 100%; }"
        "    #target { position: absolute; left: 20px; top: 30px; width: 60px; height: 40px; background: red; }"
        "</style>"
        "</head>"
        "<body>"
        "<div id='target'></div>"
        "<script>"
        "    let mouseLeaveCount = 0;"
        "    let pointerLeaveCount = 0;"
        "    let mouseMoveCount = 0;"
        "    let pointerMoveCount = 0;"
        "    let lastLeaveClientX = -1;"
        "    let lastLeaveClientY = -1;"
        "    let target = document.getElementById('target');"
        "    target.addEventListener('mouseenter', () => { window.webkit.messageHandlers.testHandler.postMessage('mouseenter'); });"
        "    target.addEventListener('mouseleave', event => {"
        "        mouseLeaveCount++;"
        "        lastLeaveClientX = event.clientX;"
        "        lastLeaveClientY = event.clientY;"
        "        window.webkit.messageHandlers.testHandler.postMessage('mouseleave');"
        "    });"
        "    target.addEventListener('pointerleave', () => { pointerLeaveCount++; });"
        "    document.addEventListener('mousemove', () => { mouseMoveCount++; });"
        "    document.addEventListener('pointermove', () => { pointerMoveCount++; });"
        "</script>"
        "</body>"
        "</html>"];

    // Center of #target is client (50, 50) -> flipped view (50, 100) with 50px top obscured inset.
    NSPoint initialWindowPoint = [webView convertPoint:NSMakePoint(50, 100) toView:nil];
    [OverrideMouseLocation setOverrideMouseLocation:[[webView window] convertPointToScreen:initialWindowPoint]];
    [webView mouseMoveToPoint:initialWindowPoint withFlags:0];
    [webView waitForPendingMouseEvents];
    EXPECT_TRUE(Util::runFor(&didEnter, 5_s));
    EXPECT_TRUE([[webView objectByEvaluatingJavaScript:@"target.matches(':hover')"] boolValue]);

    [webView objectByEvaluatingJavaScript:@"mouseMoveCount = 0; pointerMoveCount = 0;"];

    // Shift window by (-15, +10): stationary cursor maps to client (65, 60), still inside #target ([20, 80) x [30, 70)).
    [[webView window] setFrame:NSMakeRect(85, 110, 600, 500) display:YES];
    Util::spinRunLoop(5);
    [webView objectByEvaluatingJavaScript:@"0"];
    Util::spinRunLoop(5);
    [webView objectByEvaluatingJavaScript:@"0"];

    EXPECT_EQ([[webView objectByEvaluatingJavaScript:@"mouseLeaveCount"] intValue], 0);
    EXPECT_EQ([[webView objectByEvaluatingJavaScript:@"pointerLeaveCount"] intValue], 0);
    EXPECT_EQ([[webView objectByEvaluatingJavaScript:@"mouseMoveCount"] intValue], 0);
    EXPECT_EQ([[webView objectByEvaluatingJavaScript:@"pointerMoveCount"] intValue], 0);
    EXPECT_TRUE([[webView objectByEvaluatingJavaScript:@"target.matches(':hover')"] boolValue]);

    // Shift window further left to x=20: stationary cursor maps to client (130, 60), outside #target.
    [[webView window] setFrame:NSMakeRect(20, 110, 600, 500) display:YES];
    EXPECT_TRUE(Util::runFor(&didLeave, 5_s));

    EXPECT_EQ([[webView objectByEvaluatingJavaScript:@"mouseLeaveCount"] intValue], 1);
    EXPECT_EQ([[webView objectByEvaluatingJavaScript:@"pointerLeaveCount"] intValue], 1);
    EXPECT_EQ([[webView objectByEvaluatingJavaScript:@"mouseMoveCount"] intValue], 0);
    EXPECT_EQ([[webView objectByEvaluatingJavaScript:@"pointerMoveCount"] intValue], 0);
    EXPECT_EQ([[webView objectByEvaluatingJavaScript:@"lastLeaveClientX"] intValue], 130);
    EXPECT_EQ([[webView objectByEvaluatingJavaScript:@"lastLeaveClientY"] intValue], 60);
    EXPECT_FALSE([[webView objectByEvaluatingJavaScript:@"target.matches(':hover')"] boolValue]);
}

TEST(MouseEventTests, WindowChangeOnExternalMonitorWithOffsetAndNegativeCoordinates)
{
    s_overrideScreenFrame = std::nullopt;
    [OverrideMouseLocation setOverrideMouseLocation:NSZeroPoint];
    auto resetOverrides = makeScopeExit([] {
        s_overrideScreenFrame = std::nullopt;
        [OverrideMouseLocation setOverrideMouseLocation:NSZeroPoint];
    });
    s_originalScreenFrameIMP = [NSScreen instanceMethodForSelector:@selector(frame)];
    InstanceMethodSwizzler screenFrameSwizzler([NSScreen class], @selector(frame), [NSScreen instanceMethodForSelector:@selector(_mouseEventTests_frame)]);
    InstanceMethodSwizzler windowSwizzler([NSWindow class], @selector(mouseLocationOutsideOfEventStream), [NSWindow instanceMethodForSelector:@selector(_mouseEventTests_mouseLocationOutsideOfEventStream)]);
    ClassMethodSwizzler mouseLocationSwizzler([NSEvent class], @selector(mouseLocation), [OverrideMouseLocation methodForSelector:@selector(overrideMouseLocation)]);

    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 400, 400)]);
    [webView removeFromSuperview];
    [webView addToTestWindow];

    bool didEnter = false;
    bool didLeave = false;
    [webView performAfterReceivingMessage:@"mouseenter" action:[&] { didEnter = true; }];
    [webView performAfterReceivingMessage:@"mouseleave" action:[&] { didLeave = true; }];
    [webView synchronouslyLoadHTMLString:@"<!DOCTYPE html>"
        "<html>"
        "<head>"
        "<style>"
        "    body, html { margin: 0; width: 100%; height: 100%; }"
        "    #target { position: absolute; left: 50px; top: 50px; width: 100px; height: 100px; background: blue; }"
        "</style>"
        "</head>"
        "<body>"
        "<div id='target'></div>"
        "<script>"
        "    let enterCoords = null;"
        "    let leaveCoords = null;"
        "    let mouseMoveCount = 0;"
        "    let target = document.getElementById('target');"
        "    target.addEventListener('mouseenter', event => {"
        "        enterCoords = { clientX: event.clientX, clientY: event.clientY, screenX: event.screenX, screenY: event.screenY };"
        "        window.webkit.messageHandlers.testHandler.postMessage('mouseenter');"
        "    });"
        "    target.addEventListener('mouseleave', event => {"
        "        leaveCoords = { clientX: event.clientX, clientY: event.clientY, screenX: event.screenX, screenY: event.screenY };"
        "        window.webkit.messageHandlers.testHandler.postMessage('mouseleave');"
        "    });"
        "    document.addEventListener('mousemove', () => { mouseMoveCount++; });"
        "</script>"
        "</body>"
        "</html>"];

    // Simulate a 3008x1692 external display positioned above a 1728x1117 primary display (NSMaxY = 2809, negative X origin).
    s_overrideScreenFrame = NSMakeRect(-640, 1117, 3008, 1692);
    [[webView window] setFrame:NSMakeRect(-500, 2200, 400, 400) display:NO];

    // Window point (100, 300) -> flipped client (100, 100), Cocoa screen (-400, 2500) -> flipped WebKit global (-400, 2809 - 2500 = 309).
    NSPoint initialWindowPoint = NSMakePoint(100, 300);
    [OverrideMouseLocation setOverrideMouseLocation:NSMakePoint(-400, 2500)];
    [webView mouseMoveToPoint:initialWindowPoint withFlags:0];
    [webView waitForPendingMouseEvents];
    EXPECT_TRUE(Util::runFor(&didEnter, 5_s));

    NSDictionary *enterCoords = [webView objectByEvaluatingJavaScript:@"enterCoords"];
    EXPECT_EQ([enterCoords[@"clientX"] intValue], 100);
    EXPECT_EQ([enterCoords[@"clientY"] intValue], 100);
    EXPECT_EQ([enterCoords[@"screenX"] intValue], -400);
    EXPECT_EQ([enterCoords[@"screenY"] intValue], 309);

    [webView objectByEvaluatingJavaScript:@"mouseMoveCount = 0;"];

    // Move window to (-650, 2200): stationary screen cursor (-400, 2500) is now at window (250, 300) -> client (250, 100), outside #target.
    [[webView window] setFrame:NSMakeRect(-650, 2200, 400, 400) display:NO];
    EXPECT_TRUE(Util::runFor(&didLeave, 5_s));

    NSDictionary *leaveCoords = [webView objectByEvaluatingJavaScript:@"leaveCoords"];
    EXPECT_EQ([leaveCoords[@"clientX"] intValue], 250);
    EXPECT_EQ([leaveCoords[@"clientY"] intValue], 100);
    EXPECT_EQ([leaveCoords[@"screenX"] intValue], -400);
    EXPECT_EQ([leaveCoords[@"screenY"] intValue], 309);
    EXPECT_EQ([[webView objectByEvaluatingJavaScript:@"mouseMoveCount"] intValue], 0);
    EXPECT_FALSE([[webView objectByEvaluatingJavaScript:@"target.matches(':hover')"] boolValue]);
}

TEST(MouseEventTests, WindowChangeToOutsideViewShouldCauseMouseLeaveAndClearHover)
{
    [OverrideMouseLocation setOverrideMouseLocation:NSZeroPoint];
    auto resetOverride = makeScopeExit([] {
        [OverrideMouseLocation setOverrideMouseLocation:NSZeroPoint];
    });
    InstanceMethodSwizzler windowSwizzler([NSWindow class], @selector(mouseLocationOutsideOfEventStream), [NSWindow instanceMethodForSelector:@selector(_mouseEventTests_mouseLocationOutsideOfEventStream)]);
    ClassMethodSwizzler eventSwizzler([NSEvent class], @selector(mouseLocation), [OverrideMouseLocation methodForSelector:@selector(overrideMouseLocation)]);

    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 400, 400)]);
    [webView removeFromSuperview];
    [webView addToTestWindow];
    [[webView window] setFrame:NSMakeRect(100, 100, 400, 400) display:YES];

    bool pointerLeaveFired = false;
    bool mouseLeaveFired = false;
    [webView performAfterReceivingMessage:@"pointerleave" action:[&] { pointerLeaveFired = true; }];
    [webView performAfterReceivingMessage:@"mouseleave" action:[&] { mouseLeaveFired = true; }];

    [webView synchronouslyLoadHTMLString:@"<!DOCTYPE html>"
        "<html>"
        "<head>"
        "<style>"
        "    body, html { margin: 0; width: 100%; height: 100%; }"
        "    #target { width: 100px; height: 100px; position: absolute; bottom: 0; right: 0; }"
        "</style>"
        "</head>"
        "<body>"
        "<div id='target'></div>"
        "<script>"
        "    let counts = { pointerout: 0, mouseout: 0, pointerleave: 0, mouseleave: 0, mousemove: 0, pointermove: 0 };"
        "    const target = document.getElementById('target');"
        "    for (const type of Object.keys(counts)) {"
        "        target.addEventListener(type, () => {"
        "            counts[type]++;"
        "            if (type === 'pointerleave' || type === 'mouseleave')"
        "                window.webkit.messageHandlers.testHandler.postMessage(type);"
        "        });"
        "    }"
        "</script>"
        "</body>"
        "</html>"];

    NSPoint pointInWindow = NSMakePoint(350, 50);
    [OverrideMouseLocation setOverrideMouseLocation:[[webView window] convertPointToScreen:pointInWindow]];

    [webView mouseMoveToPoint:pointInWindow withFlags:0];
    [webView waitForPendingMouseEvents];
    EXPECT_TRUE([[webView objectByEvaluatingJavaScript:@"target.matches(':hover')"] boolValue]);
    [webView objectByEvaluatingJavaScript:@"counts.mousemove = 0; counts.pointermove = 0;"];

    // Move the window completely away from the stationary screen cursor so the cursor is outside the WKWebView bounds (-150, 50).
    [[webView window] setFrame:NSMakeRect(600, 100, 400, 400) display:YES];

    EXPECT_TRUE(Util::runFor(&pointerLeaveFired, 5_s));
    EXPECT_TRUE(Util::runFor(&mouseLeaveFired, 5_s));
    EXPECT_FALSE([[webView objectByEvaluatingJavaScript:@"target.matches(':hover')"] boolValue]);
    EXPECT_EQ([[webView objectByEvaluatingJavaScript:@"counts.pointerout"] intValue], 1);
    EXPECT_EQ([[webView objectByEvaluatingJavaScript:@"counts.mouseout"] intValue], 1);
    EXPECT_EQ([[webView objectByEvaluatingJavaScript:@"counts.pointerleave"] intValue], 1);
    EXPECT_EQ([[webView objectByEvaluatingJavaScript:@"counts.mouseleave"] intValue], 1);
    EXPECT_EQ([[webView objectByEvaluatingJavaScript:@"counts.mousemove"] intValue], 0);
    EXPECT_EQ([[webView objectByEvaluatingJavaScript:@"counts.pointermove"] intValue], 0);
}

TEST(MouseEventTests, WindowChangeAfterHoveredChildRemovedShouldCauseAncestorMouseLeave)
{
    [OverrideMouseLocation setOverrideMouseLocation:NSZeroPoint];
    auto resetOverride = makeScopeExit([] {
        [OverrideMouseLocation setOverrideMouseLocation:NSZeroPoint];
    });
    InstanceMethodSwizzler windowSwizzler([NSWindow class], @selector(mouseLocationOutsideOfEventStream), [NSWindow instanceMethodForSelector:@selector(_mouseEventTests_mouseLocationOutsideOfEventStream)]);
    ClassMethodSwizzler eventSwizzler([NSEvent class], @selector(mouseLocation), [OverrideMouseLocation methodForSelector:@selector(overrideMouseLocation)]);

    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 400, 400)]);
    [webView removeFromSuperview];
    [webView addToTestWindow];
    [[webView window] setFrame:NSMakeRect(100, 100, 400, 400) display:YES];

    bool buttonMouseLeaveFired = false;
    bool buttonPointerLeaveFired = false;
    [webView performAfterReceivingMessage:@"pointerleave" action:[&] { buttonPointerLeaveFired = true; }];
    [webView performAfterReceivingMessage:@"mouseleave" action:[&] { buttonMouseLeaveFired = true; }];

    [webView synchronouslyLoadHTMLString:@"<!DOCTYPE html>"
        "<html>"
        "<head>"
        "<style>"
        "    body, html { margin: 0; width: 100%; height: 100%; }"
        "    #fsBtn { position: absolute; bottom: 10px; right: 10px; width: 80px; height: 80px; padding: 0; }"
        "    #iconDiv { width: 80px; height: 80px; }"
        "</style>"
        "</head>"
        "<body>"
        "<button id='fsBtn'><span id='iconShape'><div id='iconDiv'></div></span></button>"
        "<script>"
        "    let counts = { pointerout: 0, mouseout: 0, pointerleave: 0, mouseleave: 0, mousemove: 0 };"
        "    const fsBtn = document.getElementById('fsBtn');"
        "    const iconShape = document.getElementById('iconShape');"
        "    for (const type of Object.keys(counts)) {"
        "        fsBtn.addEventListener(type, e => {"
        "            if (e.target === fsBtn)"
        "                counts[type]++;"
        "            if (e.target === fsBtn && (type === 'pointerleave' || type === 'mouseleave'))"
        "                window.webkit.messageHandlers.testHandler.postMessage(type);"
        "        });"
        "    }"
        "    function swapIcon() {"
        "        while (iconShape.firstChild)"
        "            iconShape.removeChild(iconShape.firstChild);"
        "        const next = document.createElement('div');"
        "        next.style.cssText = 'width: 80px; height: 80px;';"
        "        iconShape.appendChild(next);"
        "    }"
        "</script>"
        "</body>"
        "</html>"];

    NSPoint pointInWindow = NSMakePoint(350, 50);
    [OverrideMouseLocation setOverrideMouseLocation:[[webView window] convertPointToScreen:pointInWindow]];

    [webView mouseMoveToPoint:pointInWindow withFlags:0];
    [webView waitForPendingMouseEvents];
    [webView objectByEvaluatingJavaScript:@"swapIcon()"];

    // Move window so cursor is outside the view (-150, 50), exercising both m_lastElementUnderMouse == nullptr and targetElement == nullptr.
    [[webView window] setFrame:NSMakeRect(600, 100, 400, 400) display:YES];

    EXPECT_TRUE(Util::runFor(&buttonPointerLeaveFired, 5_s));
    EXPECT_TRUE(Util::runFor(&buttonMouseLeaveFired, 5_s));
    EXPECT_EQ([[webView objectByEvaluatingJavaScript:@"counts.pointerout"] intValue], 0);
    EXPECT_EQ([[webView objectByEvaluatingJavaScript:@"counts.mouseout"] intValue], 0);
    EXPECT_EQ([[webView objectByEvaluatingJavaScript:@"counts.pointerleave"] intValue], 1);
    EXPECT_EQ([[webView objectByEvaluatingJavaScript:@"counts.mouseleave"] intValue], 1);
    EXPECT_EQ([[webView objectByEvaluatingJavaScript:@"counts.mousemove"] intValue], 0);
}

static NSString *tallDocumentLines = @"Line of text. Line of text. Line of text. Line of text. "
    "Line of text. Line of text. Line of text. Line of text. Line of text. "
    "Line of text. Line of text. Line of text. Line of text. Line of text. "
    "Line of text. Line of text. Line of text. Line of text. Line of text. "
    "Line of text. Line of text. Line of text. Line of text. Line of text. "
    "Line of text. Line of text. Line of text. Line of text. Line of text. "
    "Line of text. Line of text. Line of text. Line of text. Line of text. "
    "Line of text. Line of text. Line of text. Line of text. Line of text. "
    "Line of text. Line of text. Line of text. Line of text. Line of text. "
    "Line of text. Line of text. Line of text. Line of text. Line of text. ";

static int scrollOffsetAfterDraggingSelectionBelowWindow(TestWKWebView *webView)
{
    EXPECT_EQ([[webView stringByEvaluatingJavaScript:@"window.scrollY"] intValue], 0);

    ClassMethodSwizzler swizzler([NSEvent class], @selector(pressedMouseButtons), imp_implementationWithBlock(^NSUInteger(id) {
        return 1 << 0; // Left button
    }));

    [webView mouseEnterAtPoint:NSMakePoint(200, 380)];
    [webView mouseDownAtPoint:NSMakePoint(200, 380) simulatePressure:NO];
    [webView waitForPendingMouseEvents];
    [webView mouseDragToPoint:NSMakePoint(200, 200)];
    [webView waitForPendingMouseEvents];

    RetainPtr exitEvent = [NSEvent enterExitEventWithType:NSEventTypeMouseExited location:NSMakePoint(200, -50) modifierFlags:0 timestamp:[webView eventTimestamp] windowNumber:[[webView window] windowNumber] context:nil eventNumber:0 trackingNumber:1 userData:nil];
    [webView _simulateMouseExit:exitEvent.get()];

    Util::runFor(100_ms);

    return [[webView stringByEvaluatingJavaScript:@"window.scrollY"] intValue];
}

TEST(MouseEventTests, AutoscrollOnMouseDragBelowWindow)
{
    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 400, 400)]);
    [webView synchronouslyLoadHTMLString:[NSString stringWithFormat:@"<!DOCTYPE html>"
        "<html>"
        "<head>"
        "<style>"
        "    body, html { margin: 0; width: 100%%; height: 100%%; }"
        "    .tall { height: 5000px; line-height: 1.5; }"
        "</style>"
        "</head>"
        "<body>"
        "<div class='tall'>%@</div>"
        "</body>"
        "</html>", tallDocumentLines]];

    EXPECT_GT(scrollOffsetAfterDraggingSelectionBelowWindow(webView.get()), 0);
}

TEST(MouseEventTests, AutoscrollOnMouseDragBelowWindowFromFixedContainer)
{
    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 400, 400)]);
    [webView synchronouslyLoadHTMLString:[NSString stringWithFormat:@"<!DOCTYPE html>"
        "<html>"
        "<head>"
        "<style>"
        "    body, html { margin: 0; width: 100%%; height: 100%%; }"
        "    .fixed { position: fixed; top: 0; left: 0; width: 100%%; height: 100%%; line-height: 1.5; }"
        "    .spacer { height: 5000px; }"
        "</style>"
        "</head>"
        "<body>"
        "<div class='fixed'>%@</div>"
        "<div class='spacer'></div>"
        "</body>"
        "</html>", tallDocumentLines]];

    EXPECT_GT(scrollOffsetAfterDraggingSelectionBelowWindow(webView.get()), 0);
}

static NSString *overlappingWebViewsHTML = @"<!DOCTYPE html>"
    "<html><head><style>body, html { margin: 0; width: 100%; height: 100%; } #target { width: 100%; height: 100%; }</style></head>"
    "<body><div id='target'></div><script>"
    "    let moveCount = 0;"
    "    let enterCount = 0;"
    "    let leaveCount = 0;"
    "    let target = document.getElementById('target');"
    "    target.addEventListener('mousemove', () => moveCount++);"
    "    target.addEventListener('mouseenter', () => enterCount++);"
    "    target.addEventListener('mouseleave', () => leaveCount++);"
    "</script></body></html>";

static auto setupOverlappingWebViews()
{
    RetainPtr bottomWebView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 400, 400)]);
    [bottomWebView synchronouslyLoadHTMLString:overlappingWebViewsHTML];
    [bottomWebView evaluateJavaScript:@"document.body.style.backgroundColor = 'blue'" completionHandler:nil];

    RetainPtr topWebView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(200, 0, 200, 200)]);
    [topWebView synchronouslyLoadHTMLString:overlappingWebViewsHTML];
    [topWebView evaluateJavaScript:@"document.body.style.backgroundColor = 'red'" completionHandler:nil];
    [[bottomWebView window].contentView addSubview:topWebView.get()];

    auto findTrackingAreaObserver = [](TestWKWebView *webView) -> id {
        for (NSTrackingArea *area in [webView trackingAreas]) {
            if (area.options & NSTrackingMouseMoved)
                return area.owner;
        }
        return nil;
    };

    RetainPtr<id> bottomObserver = findTrackingAreaObserver(bottomWebView.get());
    RetainPtr<id> topObserver = findTrackingAreaObserver(topWebView.get());

    return std::make_tuple(bottomWebView, topWebView, bottomObserver, topObserver);
}

static void checkMouseEventCounts(TestWKWebView *view, int expectedMouseMoveCount, int expectedMouseEnterCount, int expectedMouseLeaveCount)
{
    [view waitForPendingMouseEvents];

    EXPECT_EQ([[view objectByEvaluatingJavaScript:@"moveCount"] intValue], expectedMouseMoveCount);
    EXPECT_EQ([[view objectByEvaluatingJavaScript:@"enterCount"] intValue], expectedMouseEnterCount);
    EXPECT_EQ([[view objectByEvaluatingJavaScript:@"leaveCount"] intValue], expectedMouseLeaveCount);
}

TEST(MouseEventTests, OverlappingWebViewsTopViewReceivesMouseMove)
{
    auto [bottomWebView, topWebView, bottomObserver, topObserver] = setupOverlappingWebViews();

    RetainPtr event = [bottomWebView _mouseEventWithType:NSEventTypeMouseMoved atLocation:NSMakePoint(300, 100)];
    [bottomObserver mouseMoved:event.get()];
    [topObserver mouseMoved:event.get()];

    checkMouseEventCounts(bottomWebView, 0, 0, 0);
    checkMouseEventCounts(topWebView, 1, 1, 0);
}

TEST(MouseEventTests, OverlappingWebViewsBottomViewReceivesMouseMove)
{
    auto [bottomWebView, topWebView, bottomObserver, topObserver] = setupOverlappingWebViews();

    RetainPtr event = [bottomWebView _mouseEventWithType:NSEventTypeMouseMoved atLocation:NSMakePoint(100, 200)];
    [bottomObserver mouseMoved:event.get()];
    [topObserver mouseMoved:event.get()];

    checkMouseEventCounts(bottomWebView, 1, 1, 0);
    checkMouseEventCounts(topWebView, 0, 0, 0);
}

TEST(MouseEventTests, OverlappingWebViewsTopViewReceivesMouseEnterAndMouseExit)
{
    auto [bottomWebView, topWebView, bottomObserver, topObserver] = setupOverlappingWebViews();

    RetainPtr enterTopViewEvent = [bottomWebView _mouseEventWithType:NSEventTypeMouseEntered atLocation:NSMakePoint(300, 100)];

    [bottomObserver mouseEntered:enterTopViewEvent.get()];
    [topObserver mouseEntered:enterTopViewEvent.get()];

    checkMouseEventCounts(bottomWebView, 0, 0, 0);
    checkMouseEventCounts(topWebView, 1, 1, 0);

    RetainPtr exitTopViewEvent = [bottomWebView _mouseEventWithType:NSEventTypeMouseExited atLocation:NSMakePoint(500, 100)];

    [bottomObserver mouseExited:exitTopViewEvent.get()];
    [topObserver mouseExited:exitTopViewEvent.get()];

    checkMouseEventCounts(bottomWebView, 0, 0, 0);
    checkMouseEventCounts(topWebView, 1, 1, 1);
}

TEST(MouseEventTests, OverlappingWebViewsBottomViewReceivesMouseEnterAndMouseExit)
{
    auto [bottomWebView, topWebView, bottomObserver, topObserver] = setupOverlappingWebViews();

    RetainPtr enterBottomViewEvent = [bottomWebView _mouseEventWithType:NSEventTypeMouseEntered atLocation:NSMakePoint(300, 300)];

    [bottomObserver mouseEntered:enterBottomViewEvent.get()];
    [topObserver mouseEntered:enterBottomViewEvent.get()];

    checkMouseEventCounts(bottomWebView, 1, 1, 0);
    checkMouseEventCounts(topWebView, 0, 0, 0);

    RetainPtr exitBottomViewEvent = [bottomWebView _mouseEventWithType:NSEventTypeMouseExited atLocation:NSMakePoint(300, 500)];

    [bottomObserver mouseExited:exitBottomViewEvent.get()];
    [topObserver mouseExited:exitBottomViewEvent.get()];

    checkMouseEventCounts(bottomWebView, 1, 1, 1);
    checkMouseEventCounts(topWebView, 0, 0, 0);
}

TEST(MouseEventTests, OverlappingWebViewsTopAndBottomViewsReceiveMouseEnterAndMouseExit)
{
    auto [bottomWebView, topWebView, bottomObserver, topObserver] = setupOverlappingWebViews();

    RetainPtr enterBottomViewEvent = [bottomWebView _mouseEventWithType:NSEventTypeMouseEntered atLocation:NSMakePoint(300, 300)];

    [bottomObserver mouseEntered:enterBottomViewEvent.get()];
    [topObserver mouseEntered:enterBottomViewEvent.get()];

    checkMouseEventCounts(bottomWebView, 1, 1, 0);
    checkMouseEventCounts(topWebView, 0, 0, 0);

    RetainPtr enterTopViewEvent = [bottomWebView _mouseEventWithType:NSEventTypeMouseEntered atLocation:NSMakePoint(300, 100)];

    [bottomObserver mouseEntered:enterTopViewEvent.get()];
    [topObserver mouseEntered:enterTopViewEvent.get()];

    checkMouseEventCounts(bottomWebView, 1, 1, 0);
    checkMouseEventCounts(topWebView, 1, 1, 0);

    RetainPtr moveOnBottomViewEvent = [bottomWebView _mouseEventWithType:NSEventTypeMouseMoved atLocation:NSMakePoint(300, 300)];

    [bottomObserver mouseMoved:moveOnBottomViewEvent.get()];

    RetainPtr exitTopViewEvent = [bottomWebView _mouseEventWithType:NSEventTypeMouseExited atLocation:NSMakePoint(300, 300)];

    [topObserver mouseExited:exitTopViewEvent.get()];

    checkMouseEventCounts(bottomWebView, 2, 1, 0);
    checkMouseEventCounts(topWebView, 1, 1, 1);

    RetainPtr exitBottomViewEvent = [bottomWebView _mouseEventWithType:NSEventTypeMouseExited atLocation:NSMakePoint(300, 500)];

    [bottomObserver mouseExited:exitBottomViewEvent.get()];
    [topObserver mouseExited:exitBottomViewEvent.get()];

    checkMouseEventCounts(bottomWebView, 2, 1, 1);
    checkMouseEventCounts(topWebView, 1, 1, 1);
}

TEST(MouseEventTests, MouseMoveNotForwardedWhenCoveredByOverlay)
{
    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 400, 400)]);
    [webView synchronouslyLoadHTMLString:@"<body><script>"
        "let moveCount = 0;"
        "addEventListener('mousemove', () => moveCount++);"
        "</script></body>"];

    RetainPtr overlay = adoptNS([[NSView alloc] initWithFrame:NSMakeRect(200, 0, 200, 400)]);
    [[webView superview] addSubview:overlay.get() positioned:NSWindowAbove relativeTo:webView.get()];

    auto findTrackingAreaObserver = [](TestWKWebView *view) -> id {
        for (NSTrackingArea *area in [view trackingAreas]) {
            if (area.options & NSTrackingMouseMoved)
                return area.owner;
        }
        return nil;
    };

    RetainPtr<id> observer = findTrackingAreaObserver(webView.get());

    RetainPtr eventOnOverlay = [webView _mouseEventWithType:NSEventTypeMouseMoved atLocation:NSMakePoint(300, 200)];
    [observer mouseMoved:eventOnOverlay.get()];
    [webView waitForPendingMouseEvents];
    EXPECT_EQ([[webView objectByEvaluatingJavaScript:@"moveCount"] intValue], 0);

    RetainPtr eventOnWebView = [webView _mouseEventWithType:NSEventTypeMouseMoved atLocation:NSMakePoint(100, 200)];
    [observer mouseMoved:eventOnWebView.get()];
    [webView waitForPendingMouseEvents];
    EXPECT_GE([[webView objectByEvaluatingJavaScript:@"moveCount"] intValue], 1);
}

TEST(MouseEventTests, PendingMouseEventCallbacksRunOnClose)
{
    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 400, 400)]);
    [webView synchronouslyLoadHTMLString:@"<body></body>"];

    __block bool didProcessMouseEvents = false;
    [webView mouseMoveToPoint:NSMakePoint(100, 100) withFlags:0];
    [webView _doAfterProcessingAllPendingMouseEvents:^{
        didProcessMouseEvents = true;
    }];
    EXPECT_FALSE(didProcessMouseEvents);

    [webView _close];
    EXPECT_TRUE(didProcessMouseEvents);
}

TEST(MouseEventTests, PendingMouseEventCallbacksRunOnWebProcessTermination)
{
    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 400, 400)]);
    [webView synchronouslyLoadHTMLString:@"<body></body>"];

    __block bool didProcessMouseEvents = false;
    // The second event stays queued behind the first, which is already sent.
    [webView mouseMoveToPoint:NSMakePoint(100, 100) withFlags:0];
    [webView mouseMoveToPoint:NSMakePoint(200, 200) withFlags:0];
    [webView _doAfterProcessingAllPendingMouseEvents:^{
        didProcessMouseEvents = true;
    }];
    EXPECT_FALSE(didProcessMouseEvents);

    [webView _killWebContentProcessAndResetState];
    EXPECT_TRUE(didProcessMouseEvents);
}

} // namespace TestWebKitAPI

#endif // PLATFORM(MAC)
