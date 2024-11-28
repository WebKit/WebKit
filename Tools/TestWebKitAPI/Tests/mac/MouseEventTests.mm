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

#import "InstanceMethodSwizzler.h"
#import "PlatformUtilities.h"
#import "TestNavigationDelegate.h"
#import "TestURLSchemeHandler.h"
#import "TestWKWebView.h"
#import <WebKit/WKProcessPoolPrivate.h>
#import <WebKit/WKWebViewConfigurationPrivate.h>
#import <WebKit/_WKProcessPoolConfiguration.h>
#import <wtf/RunLoop.h>
#import <wtf/cocoa/TypeCastsCocoa.h>
#import <wtf/text/MakeString.h>

namespace TestWebKitAPI {

TEST(MouseEventTests, SendMouseMoveEventStream)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600)]);
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
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600)]);
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
    auto processPoolConfiguration = adoptNS([[_WKProcessPoolConfiguration alloc] init]);
    [processPoolConfiguration setProcessSwapsOnNavigation:YES];
    [processPoolConfiguration setUsesWebProcessCache:YES];
    [processPoolConfiguration setPrewarmsProcessesAutomatically:YES];
    [processPoolConfiguration setProcessSwapsOnNavigationWithinSameNonHTTPFamilyProtocol:YES];

    auto processPool = adoptNS([[WKProcessPool alloc] _initWithConfiguration:processPoolConfiguration.get()]);
    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [configuration setProcessPool:processPool.get()];

    auto handler = adoptNS([[TestURLSchemeHandler alloc] init]);
    [handler setStartURLSchemeTaskHandler:^(WKWebView *, id<WKURLSchemeTask> task) {
        auto host = task.request.URL.host;
        if ([host isEqualToString:@"www.apple.com"])
            return respond(task, "<body>Hello world</body>");

        if ([host isEqualToString:@"webkit.org"]) {
            return respond(task, makeString("<body style='width: 100%; height: 100%;'>"_s,
                "<script>"_s,
                "    document.body.addEventListener('mousemove', () => {"_s,
                "        location.href = 'pson://www.apple.com/index.html';"_s,
                "    });"_s,
                "</script>"_s,
                "</body>"_s).utf8().data());
        }
    }];
    [configuration setURLSchemeHandler:handler.get() forURLScheme:@"PSON"];

    auto navigationDelegate = adoptNS([[TestNavigationDelegate alloc] init]);
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
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

    RunLoop::main().dispatchAfter(5_ms, [&] {
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

} // namespace TestWebKitAPI

#endif // PLATFORM(MAC)
