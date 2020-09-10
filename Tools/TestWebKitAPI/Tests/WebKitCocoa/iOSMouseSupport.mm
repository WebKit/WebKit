/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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

#if PLATFORM(IOS) || PLATFORM(MACCATALYST)

#import "PlatformUtilities.h"
#import "Test.h"
#import "TestNavigationDelegate.h"
#import "TestWKWebView.h"
#import "UIKitSPI.h"
#import <WebKit/WKWebViewConfigurationPrivate.h>
#import <WebKit/WKWebViewPrivateForTesting.h>
#import <WebKit/WKWebpagePreferencesPrivate.h>
#import <WebKit/WebKit.h>
#import <wtf/MonotonicTime.h>
#import <wtf/RetainPtr.h>

@interface WKMouseGestureRecognizer : UIGestureRecognizer
- (void)_hoverEntered:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event;
- (void)_hoverExited:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event;
- (void)touchesBegan:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event;
- (void)touchesEnded:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event;
@end

@interface WKContentView ()
- (void)mouseGestureRecognizerChanged:(WKMouseGestureRecognizer *)gestureRecognizer;
@end

@interface WKTestingEvent : UIEvent
@end

@implementation WKTestingEvent {
    UIEventButtonMask _buttonMask;
}

- (CGPoint)locationInView:(UIView *)view
{
    return CGPointMake(10, 10);
}

- (void)_setButtonMask:(UIEventButtonMask)buttonMask
{
    _buttonMask = buttonMask;
}

- (UIEventButtonMask)_buttonMask
{
    return _buttonMask;
}

@end

@interface WKTestingTouch : UITouch
@end

@implementation WKTestingTouch {
    NSUInteger _tapCount;
}

- (CGPoint)locationInView:(UIView *)view
{
    return CGPointMake(10, 10);
}

- (void)setTapCount:(NSUInteger)tapCount
{
    _tapCount = tapCount;
}

- (NSUInteger)tapCount
{
    return _tapCount;
}

- (NSTimeInterval)timestamp
{
    return MonotonicTime::now().secondsSinceEpoch().value();
}

@end

static WKMouseGestureRecognizer *mouseGesture(UIView *view)
{
    for (UIGestureRecognizer *recognizer in view.gestureRecognizers) {
        if ([recognizer isKindOfClass:NSClassFromString(@"WKMouseGestureRecognizer")])
            return (WKMouseGestureRecognizer *)recognizer;
    }

    return nil;
}

TEST(iOSMouseSupport, DoNotChangeSelectionWithRightClick)
{
    auto webViewConfiguration = adoptNS([[WKWebViewConfiguration alloc] init]);
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:webViewConfiguration.get()]);
    [webView synchronouslyLoadTestPageNamed:@"simple"];
    [webView objectByEvaluatingJavaScript:@"document.body.setAttribute('contenteditable','');"];

    auto contentView = [webView wkContentView];
    auto gesture = mouseGesture(contentView);

    RetainPtr<WKTestingTouch> touch = adoptNS([[WKTestingTouch alloc] init]);
    RetainPtr<NSSet> touchSet = [NSSet setWithObject:touch.get()];

    RetainPtr<WKTestingEvent> event = adoptNS([[WKTestingEvent alloc] init]);

    [gesture _hoverEntered:touchSet.get() withEvent:event.get()];
    [contentView mouseGestureRecognizerChanged:gesture];
    [touch setTapCount:1];
    [event _setButtonMask:UIEventButtonMaskSecondary];
    [gesture touchesBegan:touchSet.get() withEvent:event.get()];
    [contentView mouseGestureRecognizerChanged:gesture];
    [gesture touchesEnded:touchSet.get() withEvent:event.get()];
    [contentView mouseGestureRecognizerChanged:gesture];

    __block bool done = false;

    [webView _doAfterProcessingAllPendingMouseEvents:^{
        NSNumber *result = [webView objectByEvaluatingJavaScript:@"window.getSelection().isCollapsed"];
        EXPECT_TRUE([result boolValue]);
        done = true;
    }];

    TestWebKitAPI::Util::run(&done);
}

TEST(iOSMouseSupport, TrackButtonMaskFromTouchStart)
{
    auto webViewConfiguration = adoptNS([[WKWebViewConfiguration alloc] init]);
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:webViewConfiguration.get()]);
    [webView synchronouslyLoadHTMLString:@"<script>"
    "window.didReleaseRightButton = false;"
    "document.documentElement.addEventListener('mouseup', function (e) {"
    "    if (e.button == 2)"
    "        window.didReleaseRightButton = true;"
    "});"
    "</script>"];

    auto contentView = [webView wkContentView];
    auto gesture = mouseGesture(contentView);

    RetainPtr<WKTestingTouch> touch = adoptNS([[WKTestingTouch alloc] init]);
    RetainPtr<NSSet> touchSet = [NSSet setWithObject:touch.get()];

    RetainPtr<WKTestingEvent> event = adoptNS([[WKTestingEvent alloc] init]);

    [gesture _hoverEntered:touchSet.get() withEvent:event.get()];
    [contentView mouseGestureRecognizerChanged:gesture];
    [touch setTapCount:1];
    [event _setButtonMask:UIEventButtonMaskSecondary];
    [gesture touchesBegan:touchSet.get() withEvent:event.get()];
    [contentView mouseGestureRecognizerChanged:gesture];
    [event _setButtonMask:0];
    [gesture touchesEnded:touchSet.get() withEvent:event.get()];
    [contentView mouseGestureRecognizerChanged:gesture];

    __block bool done = false;

    [webView _doAfterProcessingAllPendingMouseEvents:^{
        NSNumber *result = [webView objectByEvaluatingJavaScript:@"window.didReleaseRightButton"];
        EXPECT_TRUE([result boolValue]);
        done = true;
    }];

    TestWebKitAPI::Util::run(&done);
}

TEST(iOSMouseSupport, MouseTimestampTimebase)
{
    auto webViewConfiguration = adoptNS([[WKWebViewConfiguration alloc] init]);
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:webViewConfiguration.get()]);
    [webView synchronouslyLoadHTMLString:@"<script>"
    "window.mouseDownTimestamp = -1;"
    "document.documentElement.addEventListener('mousedown', function (e) {"
    "    window.mouseDownTimestamp = e.timeStamp;"
    "});"
    "</script>"];

    auto contentView = [webView wkContentView];
    auto gesture = mouseGesture(contentView);

    RetainPtr<WKTestingTouch> touch = adoptNS([[WKTestingTouch alloc] init]);
    RetainPtr<NSSet> touchSet = [NSSet setWithObject:touch.get()];

    RetainPtr<WKTestingEvent> hoverEvent = adoptNS([[NSClassFromString(@"UIHoverEvent") alloc] init]);
    RetainPtr<WKTestingEvent> event = adoptNS([[WKTestingEvent alloc] init]);

    [gesture _hoverEntered:touchSet.get() withEvent:hoverEvent.get()];
    [contentView mouseGestureRecognizerChanged:gesture];
    [touch setTapCount:1];
    [event _setButtonMask:UIEventButtonMaskPrimary];
    [gesture touchesBegan:touchSet.get() withEvent:event.get()];
    [contentView mouseGestureRecognizerChanged:gesture];
    [gesture touchesEnded:touchSet.get() withEvent:event.get()];
    [contentView mouseGestureRecognizerChanged:gesture];

    __block bool done = false;

    [webView _doAfterProcessingAllPendingMouseEvents:^{
        double mouseDownTimestamp = [[webView objectByEvaluatingJavaScript:@"window.mouseDownTimestamp"] doubleValue];
        // Ensure that the timestamp is not clamped to 0.
        EXPECT_GT(mouseDownTimestamp, 0);

        // The test should always complete in 10 seconds, so ensure that
        // the timestamp is also not overly large.
        EXPECT_LE(mouseDownTimestamp, 10000);
        done = true;
    }];

    TestWebKitAPI::Util::run(&done);
}

#if ENABLE(IOS_TOUCH_EVENTS)

TEST(iOSMouseSupport, WebsiteMouseEventPolicies)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600)]);

    auto contentView = [webView wkContentView];
    auto gesture = mouseGesture(contentView);

    void (^tapAndWait)(void) = ^{
        RetainPtr<WKTestingTouch> touch = adoptNS([[WKTestingTouch alloc] init]);
        RetainPtr<NSSet> touchSet = [NSSet setWithObject:touch.get()];

        RetainPtr<WKTestingEvent> event = adoptNS([[WKTestingEvent alloc] init]);

        [gesture _hoverEntered:touchSet.get() withEvent:event.get()];
        [contentView mouseGestureRecognizerChanged:gesture];
        [touch setTapCount:1];
        [event _setButtonMask:UIEventButtonMaskPrimary];
        [gesture touchesBegan:touchSet.get() withEvent:event.get()];
        [contentView mouseGestureRecognizerChanged:gesture];
        [event _setButtonMask:0];
        [gesture touchesEnded:touchSet.get() withEvent:event.get()];
        [contentView mouseGestureRecognizerChanged:gesture];

        __block bool done = false;

        [webView _doAfterProcessingAllPendingMouseEvents:^{
            done = true;
        }];

        TestWebKitAPI::Util::run(&done);
    };

    // By default, a mouse should generate mouse events.

    [webView synchronouslyLoadHTMLString:@"<script>"
    "window.lastPointerEventType = undefined;"
    "document.documentElement.addEventListener('pointerdown', function (e) {"
    "    window.lastPointerEventType = e.pointerType;"
    "});"
    "</script>"];

    tapAndWait();

    NSString *result = [webView objectByEvaluatingJavaScript:@"window.lastPointerEventType"];
    EXPECT_WK_STREQ("mouse", result);

    EXPECT_TRUE([gesture isEnabled]);

    // If loaded with _WKWebsiteMouseEventPolicySynthesizeTouchEvents, it should send touch events instead.

    WKWebpagePreferences *preferences = [[[WKWebpagePreferences alloc] init] autorelease];
    preferences._mouseEventPolicy = _WKWebsiteMouseEventPolicySynthesizeTouchEvents;

    [webView synchronouslyLoadHTMLString:@"two" preferences:preferences];

    // FIXME: Because we're directly calling mouseGestureRecognizerChanged: to emulate the gesture recognizer,
    // we can't just tapAndWait() again and expect the fact that it's disabled to stop the mouse events.
    // Instead, we'll just ensure that the gesture is disabled.

    EXPECT_FALSE([gesture isEnabled]);
}

#endif // ENABLE(IOS_TOUCH_EVENTS)

#endif // PLATFORM(IOS) || PLATFORM(MACCATALYST)
