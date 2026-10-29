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
#import <WebKit/_WKProcessPoolConfiguration.h>
#import <pal/spi/mac/NSTextInputContextSPI.h>
#import <wtf/BlockPtr.h>
#import <wtf/RunLoop.h>
#import <wtf/Vector.h>
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

@interface DeferredMouseTextInputContext : NSTextInputContext {
    Vector<BlockPtr<void(BOOL)>> _mouseEventCompletions;
}
@property (nonatomic, readonly) NSUInteger mouseEventCount;
- (BlockPtr<void(BOOL)>)takeMouseEventCompletionAtIndex:(NSUInteger)index;
@end

@implementation DeferredMouseTextInputContext

- (void)handleEvent:(NSEvent *)event completionHandler:(void(^)(BOOL handled))completionHandler
{
    switch (event.type) {
    case NSEventTypeLeftMouseDown:
    case NSEventTypeLeftMouseUp:
    case NSEventTypeLeftMouseDragged:
        _mouseEventCompletions.append(makeBlockPtr(completionHandler));
        break;
    default:
        completionHandler(NO);
        break;
    }
}

- (NSUInteger)mouseEventCount
{
    return _mouseEventCompletions.size();
}

- (BlockPtr<void(BOOL)>)takeMouseEventCompletionAtIndex:(NSUInteger)index
{
    return std::exchange(_mouseEventCompletions[index], nullptr);
}

@end

@interface DeferredMouseInputWebView : TestWKWebView {
    RetainPtr<DeferredMouseTextInputContext> _deferredMouseInputContext;
}
@property (nonatomic) BOOL suppressInputContext;
@property (nonatomic, readonly) DeferredMouseTextInputContext *deferredMouseInputContext;
@end

@implementation DeferredMouseInputWebView

- (NSTextInputContext *)inputContext
{
    return self.suppressInputContext ? nil : self.deferredMouseInputContext;
}

- (DeferredMouseTextInputContext *)deferredMouseInputContext
{
    if (!_deferredMouseInputContext)
        _deferredMouseInputContext = adoptNS([[DeferredMouseTextInputContext alloc] initWithClient:(id<NSTextInputClient>)self]);
    return _deferredMouseInputContext.get();
}

@end

namespace TestWebKitAPI {

static void loadDeferredMouseInputPage(DeferredMouseInputWebView *webView)
{
    [webView synchronouslyLoadHTMLString:@"<!DOCTYPE html>"
        "<style>body { margin: 0; } #editor { height: 50px; }"
        "input, a { position: absolute; left: 100px; top: 100px; width: 100px; height: 100px; }"
        "a { left: 250px; }</style>"
        "<div id='editor' contenteditable>Editable content</div><input id='target'>"
        "<a id='link' href='#destination'>Navigate</a>"
        "<script>"
        "let events = [];"
        "for (let type of ['pointerdown', 'mousedown', 'blur', 'focusout', 'pointerup', 'mouseup', 'click'])"
        "    document.addEventListener(type, event => events.push(event.type), true);"
        "editor.focus();"
        "</script>"];
    [webView focus];
    [webView waitForNextPresentationUpdate];
    EXPECT_WK_STREQ(@"editor", [webView stringByEvaluatingJavaScript:@"document.activeElement.id"]);
    [webView stringByEvaluatingJavaScript:@"events = []; ''"];
}

static RetainPtr<DeferredMouseInputWebView> createDeferredMouseInputWebView()
{
    RetainPtr webView = adoptNS([[DeferredMouseInputWebView alloc] initWithFrame:NSMakeRect(0, 0, 400, 400)]);
    loadDeferredMouseInputPage(webView.get());
    return webView;
}

static void expectOrderedMouseClick(TestWKWebView *webView)
{
    [webView waitForPendingMouseEvents];
    EXPECT_WK_STREQ(@"pointerdown,mousedown,blur,focusout,pointerup,mouseup,click", [webView stringByEvaluatingJavaScript:@"events.join(',')"]);
    EXPECT_WK_STREQ(@"target", [webView stringByEvaluatingJavaScript:@"document.activeElement.id"]);
}

TEST(MouseEventTests, InputMethodMouseUpCompletesBeforeMouseDown)
{
    RetainPtr webView = createDeferredMouseInputWebView();
    RetainPtr context = [webView deferredMouseInputContext];
    [webView mouseDownAtPoint:NSMakePoint(150, 250) simulatePressure:NO];
    [webView mouseUpAtPoint:NSMakePoint(150, 250)];
    ASSERT_EQ(context.get().mouseEventCount, 2U);

    [context takeMouseEventCompletionAtIndex:1](false);
    [webView waitForPendingMouseEvents];
    EXPECT_WK_STREQ(@"", [webView stringByEvaluatingJavaScript:@"events.join(',')"]);
    [context takeMouseEventCompletionAtIndex:0](false);
    expectOrderedMouseClick(webView.get());
}

TEST(MouseEventTests, InputContextDisappearsDuringPendingMouseDown)
{
    RetainPtr webView = createDeferredMouseInputWebView();
    RetainPtr context = [webView deferredMouseInputContext];
    [webView mouseDownAtPoint:NSMakePoint(150, 250) simulatePressure:NO];
    [webView setSuppressInputContext:YES];
    [webView mouseUpAtPoint:NSMakePoint(150, 250)];
    ASSERT_EQ(context.get().mouseEventCount, 1U);

    [webView waitForPendingMouseEvents];
    EXPECT_WK_STREQ(@"", [webView stringByEvaluatingJavaScript:@"events.join(',')"]);
    [context takeMouseEventCompletionAtIndex:0](false);
    expectOrderedMouseClick(webView.get());
}

TEST(MouseEventTests, InputMethodMouseCompletionsAcrossSuccessiveClicks)
{
    RetainPtr webView = createDeferredMouseInputWebView();
    RetainPtr context = [webView deferredMouseInputContext];
    for (unsigned i = 0; i < 2; ++i) {
        [webView mouseDownAtPoint:NSMakePoint(150, 250) simulatePressure:NO];
        [webView mouseUpAtPoint:NSMakePoint(150, 250)];
    }
    ASSERT_EQ(context.get().mouseEventCount, 4U);

    [context takeMouseEventCompletionAtIndex:3](false);
    [context takeMouseEventCompletionAtIndex:2](false);
    [context takeMouseEventCompletionAtIndex:0](false);
    [webView waitForPendingMouseEvents];
    EXPECT_WK_STREQ(@"pointerdown,mousedown,blur,focusout", [webView stringByEvaluatingJavaScript:@"events.join(',')"]);

    [context takeMouseEventCompletionAtIndex:1](false);
    [webView waitForPendingMouseEvents];
    EXPECT_WK_STREQ(@"pointerdown,mousedown,blur,focusout,pointerup,mouseup,click,pointerdown,mousedown,pointerup,mouseup,click", [webView stringByEvaluatingJavaScript:@"events.join(',')"]);
}

TEST(MouseEventTests, InputMethodMouseUpCompletesBeforeMouseDownOnLink)
{
    RetainPtr webView = createDeferredMouseInputWebView();
    RetainPtr context = [webView deferredMouseInputContext];
    RetainPtr navigationDelegate = adoptNS([[TestNavigationDelegate alloc] init]);
    __block bool navigated = false;
    [navigationDelegate setDidSameDocumentNavigation:^(WKWebView *, WKNavigation *) {
        navigated = true;
    }];
    [webView setNavigationDelegate:navigationDelegate.get()];
    [webView mouseDownAtPoint:NSMakePoint(300, 250) simulatePressure:NO];
    [webView mouseUpAtPoint:NSMakePoint(300, 250)];
    ASSERT_EQ(context.get().mouseEventCount, 2U);

    [context takeMouseEventCompletionAtIndex:1](false);
    [webView waitForPendingMouseEvents];
    EXPECT_WK_STREQ(@"", [webView stringByEvaluatingJavaScript:@"location.hash"]);

    [context takeMouseEventCompletionAtIndex:0](false);
    [webView waitForPendingMouseEvents];
    EXPECT_WK_STREQ(@"pointerdown,mousedown,pointerup,mouseup,click", [webView stringByEvaluatingJavaScript:@"events.filter(type => !['blur', 'focusout'].includes(type)).join(',')"]);
    ASSERT_EQ(1, [[webView objectByEvaluatingJavaScript:@"events.filter(type => type === 'click').length"] intValue]);
    Util::run(&navigated);
    EXPECT_WK_STREQ(@"#destination", [webView stringByEvaluatingJavaScript:@"location.hash"]);
}

TEST(MouseEventTests, InputMethodConsumesPendingMouseDown)
{
    RetainPtr webView = createDeferredMouseInputWebView();
    RetainPtr context = [webView deferredMouseInputContext];
    [webView mouseDownAtPoint:NSMakePoint(150, 250) simulatePressure:NO];
    [webView mouseUpAtPoint:NSMakePoint(150, 250)];
    ASSERT_EQ(context.get().mouseEventCount, 2U);

    [context takeMouseEventCompletionAtIndex:1](false);
    [context takeMouseEventCompletionAtIndex:0](true);
    [webView waitForPendingMouseEvents];
    EXPECT_WK_STREQ(@"pointerup,mouseup", [webView stringByEvaluatingJavaScript:@"events.join(',')"]);

    [webView mouseDownAtPoint:NSMakePoint(150, 250) simulatePressure:NO];
    [webView mouseUpAtPoint:NSMakePoint(150, 250)];
    ASSERT_EQ(context.get().mouseEventCount, 4U);
    [context takeMouseEventCompletionAtIndex:2](false);
    [context takeMouseEventCompletionAtIndex:3](false);
    [webView waitForPendingMouseEvents];
    EXPECT_EQ(1, [[webView objectByEvaluatingJavaScript:@"events.filter(type => type === 'click').length"] intValue]);
}

TEST(MouseEventTests, InputMethodMouseCompletionAfterViewDestruction)
{
    BlockPtr<void(BOOL)> completion;
    @autoreleasepool {
        RetainPtr webView = createDeferredMouseInputWebView();
        [webView mouseDownAtPoint:NSMakePoint(150, 250) simulatePressure:NO];
        RetainPtr context = [webView deferredMouseInputContext];
        ASSERT_EQ(context.get().mouseEventCount, 1U);
        completion = [context takeMouseEventCompletionAtIndex:0];
        [webView removeFromSuperview];
    }
    completion(false);
}

TEST(MouseEventTests, InputMethodMouseCompletionAfterProcessTermination)
{
    RetainPtr webView = createDeferredMouseInputWebView();
    RetainPtr context = [webView deferredMouseInputContext];
    [webView mouseDownAtPoint:NSMakePoint(150, 250) simulatePressure:NO];
    [webView mouseUpAtPoint:NSMakePoint(150, 250)];
    ASSERT_EQ(context.get().mouseEventCount, 2U);
    [context takeMouseEventCompletionAtIndex:1](false);

    [webView _killWebContentProcessAndResetState];
    loadDeferredMouseInputPage(webView.get());
    [webView mouseDownAtPoint:NSMakePoint(150, 250) simulatePressure:NO];
    [webView mouseUpAtPoint:NSMakePoint(150, 250)];
    ASSERT_EQ(context.get().mouseEventCount, 4U);
    [context takeMouseEventCompletionAtIndex:3](false);
    [context takeMouseEventCompletionAtIndex:2](false);
    expectOrderedMouseClick(webView.get());

    // Completing an event for the terminated process must not dispatch it to the new page.
    [context takeMouseEventCompletionAtIndex:0](false);
    expectOrderedMouseClick(webView.get());
}

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

} // namespace TestWebKitAPI

#endif // PLATFORM(MAC)
