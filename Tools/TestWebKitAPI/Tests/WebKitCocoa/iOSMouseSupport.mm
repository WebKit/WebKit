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

#if PLATFORM(IOS) || PLATFORM(MACCATALYST) || PLATFORM(VISION)

#import "IOSMouseEventTestHarness.h"
#import "InstanceMethodSwizzler.h"
#import "PlatformUtilities.h"
#import "Test.h"
#import "TestNavigationDelegate.h"
#import "TestWKWebView.h"
#import "UIKitSPIForTesting.h"
#import <WebCore/PointerEventTypeNames.h>
#import <WebKit/WKUIDelegatePrivate.h>
#import <WebKit/WKWebViewConfigurationPrivate.h>
#import <WebKit/WKWebViewPrivateForTesting.h>
#import <WebKit/WKWebpagePreferencesPrivate.h>
#import <WebKit/WebKit.h>
#import <pal/spi/cocoa/RevealSPI.h>
#import <wtf/BlockPtr.h>
#import <wtf/MonotonicTime.h>
#import <wtf/RetainPtr.h>
#import <wtf/cocoa/TypeCastsCocoa.h>

#if HAVE(MOUSE_DEVICE_OBSERVATION)

@interface WKMouseDeviceObserver
+ (WKMouseDeviceObserver *)sharedInstance;
- (void)startWithCompletionHandler:(void (^)(void))completionHandler;
- (void)_setHasMouseDeviceForTesting:(BOOL)hasMouseDevice;
@end

#endif

@interface MouseSupportUIDelegate : NSObject <WKUIDelegatePrivate>
@end

@implementation MouseSupportUIDelegate {
    BlockPtr<void(_WKHitTestResult *)> _mouseDidMoveOverElementHandler;
}

- (void)_webView:(WKWebView *)webview mouseDidMoveOverElement:(_WKHitTestResult *)hitTestResult withFlags:(UIKeyModifierFlags)flags userInfo:(id <NSSecureCoding>)userInfo
{
    if (_mouseDidMoveOverElementHandler)
        _mouseDidMoveOverElementHandler(hitTestResult);
}

- (void)setMouseDidMoveOverElementHandler:(void(^)(_WKHitTestResult *))handler
{
    _mouseDidMoveOverElementHandler = handler;
}

@end

#if HAVE(UI_POINTER_INTERACTION)

@interface TestPointerRegionRequest : UIPointerRegionRequest
- (instancetype)initWithLocation:(CGPoint)location;
@end

@implementation TestPointerRegionRequest {
    CGPoint _requestLocation;
}

- (instancetype)initWithLocation:(CGPoint)location
{
    if (!(self = [super init]))
        return nil;

    _requestLocation = location;
    return self;
}

- (CGPoint)location
{
    return _requestLocation;
}

@end

namespace TestWebKitAPI {

struct PointerInfo {
    BOOL isDefault { NO };
    CGRect regionRect { CGRectNull };
};

} // namespace TestWebKitAPI

@interface TestWKWebView (PointerInteractionTests)
- (TestWebKitAPI::PointerInfo)pointerInfoAtLocation:(CGPoint)location;
@end

@implementation TestWKWebView (PointerInteractionTests)

- (UIPointerInteraction *)pointerInteraction
{
    for (id<UIInteraction> interaction in self.textInputContentView.interactions) {
        if (auto result = dynamic_objc_cast<UIPointerInteraction>(interaction))
            return result;
    }
    return nil;
}

- (TestWebKitAPI::PointerInfo)pointerInfoAtLocation:(CGPoint)location
{
    auto contentView = (UIView<UIPointerInteractionDelegate> *)[self textInputContentView];
    auto interaction = [self pointerInteraction];
    auto request = adoptNS([[TestPointerRegionRequest alloc] initWithLocation:location]);

    RetainPtr defaultRegion = [UIPointerRegion regionWithRect:contentView.bounds identifier:nil];
    [contentView pointerInteraction:interaction regionForRequest:request.get() defaultRegion:defaultRegion.get()];
    [self waitForNextPresentationUpdate];

    auto region = [contentView pointerInteraction:interaction regionForRequest:request.get() defaultRegion:defaultRegion.get()];
    auto style = [contentView pointerInteraction:interaction styleForRegion:region];
    return { [style isEqual:UIPointerStyle.systemPointerStyle], region.rect };
}

@end

#endif // HAVE(UI_POINTER_INTERACTION)

TEST(iOSMouseSupport, DoNotChangeSelectionWithRightClick)
{
    auto webViewConfiguration = adoptNS([[WKWebViewConfiguration alloc] init]);
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:webViewConfiguration.get()]);
    [webView synchronouslyLoadTestPageNamed:@"simple"];
    [webView objectByEvaluatingJavaScript:@"document.body.setAttribute('contenteditable','');"];

    TestWebKitAPI::MouseEventTestHarness testHarness { webView.get() };
    testHarness.mouseMove(10, 10);
    testHarness.mouseDown(UIEventButtonMaskSecondary);
    testHarness.mouseUp();

    __block bool done = false;

    [webView _doAfterProcessingAllPendingMouseEvents:^{
        NSNumber *result = [webView objectByEvaluatingJavaScript:@"window.getSelection().isCollapsed"];
        EXPECT_TRUE([result boolValue]);
        done = true;
    }];

    TestWebKitAPI::Util::run(&done);
}

TEST(iOSMouseSupport, RightClickOutsideOfTextNodeDoesNotSelect)
{
    auto webViewConfiguration = adoptNS([[WKWebViewConfiguration alloc] init]);
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:webViewConfiguration.get()]);
    [webView synchronouslyLoadTestPageNamed:@"emptyTable"];
    [webView stringByEvaluatingJavaScript:@"getSelection().selectAllChildren(document.getElementById('target'))"];

    __block bool done = false;
    [webView selectTextForContextMenuWithLocationInView:CGPointMake(100, 10) completion:^(BOOL) {
        NSNumber *result = [webView objectByEvaluatingJavaScript:@"window.getSelection().isCollapsed"];
        EXPECT_FALSE([result boolValue]);
        done = true;
    }];

    TestWebKitAPI::Util::run(&done);
}

TEST(iOSMouseSupport, RightClickDoesNotShowMenuIfPreventDefault)
{
    auto webViewConfiguration = adoptNS([[WKWebViewConfiguration alloc] init]);
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:webViewConfiguration.get()]);
    [webView synchronouslyLoadTestPageNamed:@"image"];
    [webView stringByEvaluatingJavaScript:@"window.didContextMenu = false; document.addEventListener('contextmenu', (event) => { event.preventDefault(); didContextMenu = true; })"];

    TestWebKitAPI::MouseEventTestHarness testHarness { webView.get() };
    testHarness.mouseMove(10, 10);
    testHarness.mouseDown(UIEventButtonMaskSecondary);
    testHarness.mouseUp();

    __block bool done = false;
    [webView selectTextForContextMenuWithLocationInView:CGPointMake(10, 10) completion:^(BOOL shouldPresentMenu) {
        EXPECT_FALSE(shouldPresentMenu);

        NSNumber *didContextMenu = [webView objectByEvaluatingJavaScript:@"window.didContextMenu"];
        EXPECT_TRUE([didContextMenu boolValue]);

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

    TestWebKitAPI::MouseEventTestHarness testHarness { webView.get() };
    testHarness.mouseMove(10, 10);
    testHarness.mouseDown(UIEventButtonMaskSecondary);
    testHarness.mouseUp();

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

    TestWebKitAPI::MouseEventTestHarness testHarness { webView.get() };
    testHarness.mouseMove(10, 10);
    testHarness.mouseDown();
    testHarness.mouseUp();

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

TEST(iOSMouseSupport, EndedTouchesTriggerClick)
{
    auto webViewConfiguration = adoptNS([[WKWebViewConfiguration alloc] init]);
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:webViewConfiguration.get()]);
    [webView synchronouslyLoadHTMLString:@"<script>"
        "window.wasClicked = false;"
        "document.documentElement.addEventListener('click', function (e) {"
        "    window.wasClicked = true;"
        "});"
        "</script>"];

    TestWebKitAPI::MouseEventTestHarness testHarness { webView.get() };
    testHarness.mouseMove(10, 10);
    testHarness.mouseDown();
    testHarness.mouseUp();

    [webView waitForPendingMouseEvents];

    bool wasClicked = [[webView objectByEvaluatingJavaScript:@"window.wasClicked"] boolValue];
    EXPECT_TRUE(wasClicked);
}

TEST(iOSMouseSupport, CancelledTouchesDoNotTriggerClick)
{
    auto webViewConfiguration = adoptNS([[WKWebViewConfiguration alloc] init]);
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:webViewConfiguration.get()]);
    [webView synchronouslyLoadHTMLString:@"<script>"
        "window.wasClicked = false;"
        "document.documentElement.addEventListener('click', function (e) {"
        "    window.wasClicked = true;"
        "});"
        "</script>"];

    TestWebKitAPI::MouseEventTestHarness testHarness { webView.get() };
    testHarness.mouseMove(10, 10);
    testHarness.mouseDown();
    testHarness.mouseCancel();

    [webView waitForPendingMouseEvents];

    bool wasClicked = [[webView objectByEvaluatingJavaScript:@"window.wasClicked"] boolValue];
    EXPECT_FALSE(wasClicked);
}

TEST(iOSMouseSupport, MouseDidMoveOverElement)
{
    auto webViewConfiguration = adoptNS([[WKWebViewConfiguration alloc] init]);
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:webViewConfiguration.get()]);
    auto delegate = adoptNS([MouseSupportUIDelegate new]);

    __block bool mouseDidMoveOverElement = false;
    __block RetainPtr<_WKHitTestResult> hitTestResult;
    [delegate setMouseDidMoveOverElementHandler:^(_WKHitTestResult *result) {
        hitTestResult = result;
        mouseDidMoveOverElement = true;
    }];

    [webView synchronouslyLoadTestPageNamed:@"simple"];
    [webView setUIDelegate:delegate.get()];

    TestWebKitAPI::MouseEventTestHarness { webView.get() }.mouseMove(10, 10);
    TestWebKitAPI::Util::run(&mouseDidMoveOverElement);

    EXPECT_TRUE(mouseDidMoveOverElement);
    EXPECT_NOT_NULL(hitTestResult);
}

static bool selectionUpdated = false;
static void handleUpdatedSelection(id, SEL)
{
    selectionUpdated = true;
}

TEST(iOSMouseSupport, SelectionUpdatesBeforeContextMenuAppears)
{
    InstanceMethodSwizzler swizzler { UIWKTextInteractionAssistant.class, @selector(selectionChanged), reinterpret_cast<IMP>(handleUpdatedSelection) };

    auto webViewConfiguration = adoptNS([[WKWebViewConfiguration alloc] init]);
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:webViewConfiguration.get()]);
    [webView synchronouslyLoadTestPageNamed:@"simple"];
    [webView objectByEvaluatingJavaScript:@"document.body.setAttribute('contenteditable','');"];

    [webView _simulateSelectionStart];
    __block bool done = false;
    [webView selectTextForContextMenuWithLocationInView:CGPointZero completion:^(BOOL) {
        EXPECT_TRUE(selectionUpdated);
        done = true;
    }];

    TestWebKitAPI::Util::run(&done);
}

constexpr auto largeResponsiveHelloMarkup = "<meta name='viewport' content='width=device-width'><span style='font-size: 400px;'>Hello</span>";

TEST(iOSMouseSupport, DisablingTextIteractionPreventsSelectionWhenShowingContextMenu)
{
    auto configuration = adoptNS([WKWebViewConfiguration new]);
    [configuration preferences].textInteractionEnabled = NO;

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 800, 600) configuration:configuration.get()]);
    [webView synchronouslyLoadHTMLString:@(largeResponsiveHelloMarkup)];

    __block bool done = false;
    [webView selectTextForContextMenuWithLocationInView:CGPointMake(100, 100) completion:^(BOOL) {
        done = true;
    }];

    TestWebKitAPI::Util::run(&done);
    EXPECT_WK_STREQ("", [webView stringByEvaluatingJavaScript:@"getSelection().toString()"]);
}

TEST(iOSMouseSupport, ShowingContextMenuSelectsEditableText)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 800, 600)]);
    [webView _setEditable:YES];
    [webView synchronouslyLoadHTMLString:@(largeResponsiveHelloMarkup)];

    __block bool done = false;
    [webView selectTextForContextMenuWithLocationInView:CGPointMake(100, 100) completion:^(BOOL) {
        done = true;
    }];

    TestWebKitAPI::Util::run(&done);
    EXPECT_WK_STREQ("Hello", [webView stringByEvaluatingJavaScript:@"getSelection().toString()"]);
    EXPECT_FALSE(CGRectIsEmpty([webView selectionViewRectsInContentCoordinates].firstObject.CGRectValue));
}

TEST(iOSMouseSupport, ShowingContextMenuSelectsNonEditableText)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 800, 600)]);
    [webView synchronouslyLoadHTMLString:@(largeResponsiveHelloMarkup)];

    __block bool done = false;
    [webView selectTextForContextMenuWithLocationInView:CGPointMake(100, 100) completion:^(BOOL) {
        done = true;
    }];

    TestWebKitAPI::Util::run(&done);
    EXPECT_WK_STREQ("Hello", [webView stringByEvaluatingJavaScript:@"getSelection().toString()"]);
    EXPECT_FALSE(CGRectIsEmpty([webView selectionViewRectsInContentCoordinates].firstObject.CGRectValue));
}

static void simulateEditContextMenuAppearance(TestWKWebView *webView, CGPoint location)
{
    __block bool done = false;
    [webView selectTextForContextMenuWithLocationInView:location completion:^(BOOL) {
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
}

TEST(iOSMouseSupport, ContextClickAtEndOfSelection)
{
    RetainPtr<TestWKWebView> webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 1000, 400)]);
    [webView synchronouslyLoadTestPageNamed:@"try-text-select-with-disabled-text-interaction"];

    __auto_type setupSelection = ^(TestWKWebView* webView) {
        // Creates a selection of the characters "Hell"
        NSString *modifySelectionJavascript = @""
        "(() => {"
        "  const node = document.getElementById('editable').firstChild;"
        "  const range = document.createRange();"
        "  range.setStart(node, 0);"
        "  range.setEnd(node, 4);"
        "  "
        "  var selection = window.getSelection();"
        "  selection.removeAllRanges();"
        "  selection.addRange(range);"
        "})();";

        __block bool done = false;
        [webView evaluateJavaScript:modifySelectionJavascript completionHandler:^(id, NSError *error) {
            EXPECT_NULL(error);
            done = true;
        }];
        TestWebKitAPI::Util::run(&done);

        [webView stringByEvaluatingJavaScript:modifySelectionJavascript];
        [webView waitForNextPresentationUpdate];

        EXPECT_WK_STREQ("Hell", [webView selectedText]);
    };

    const CGFloat centerOfLetterO = 795;

    setupSelection(webView.get());

    // Right click a bit to the left of the center of the "o". This should not change the selection.
    simulateEditContextMenuAppearance(webView.get(), NSMakePoint(centerOfLetterO - 5, 200));
    EXPECT_WK_STREQ("Hell", [webView selectedText]);

    setupSelection(webView.get());

    // Right click a bit to the right of the center of the "o". This should change the selection.
    simulateEditContextMenuAppearance(webView.get(), NSMakePoint(centerOfLetterO + 5, 200));
    EXPECT_WK_STREQ("Hello", [webView selectedText]);
}

#if ENABLE(IOS_TOUCH_EVENTS)

TEST(iOSMouseSupport, WebsiteMouseEventPolicies)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600)]);
    TestWebKitAPI::MouseEventTestHarness testHarness { webView.get() };

    auto tapAndWait = [&] {
        testHarness.mouseMove(10, 10);
        testHarness.mouseDown();
        testHarness.mouseUp();
        [webView waitForPendingMouseEvents];
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
    EXPECT_WK_STREQ(WebCore::mousePointerEventType(), result);

    EXPECT_TRUE(testHarness.mouseInteraction().enabled);

    // If loaded with _WKWebsiteMouseEventPolicySynthesizeTouchEvents, it should send touch events instead.

    auto preferences = adoptNS([[WKWebpagePreferences alloc] init]);
    [preferences _setMouseEventPolicy:_WKWebsiteMouseEventPolicySynthesizeTouchEvents];

    [webView synchronouslyLoadHTMLString:@"two" preferences:preferences.get()];

    // FIXME: Because we're directly calling mouseGestureRecognizerChanged: to emulate the gesture recognizer,
    // we can't just tapAndWait() again and expect the fact that it's disabled to stop the mouse events.
    // Instead, we'll just ensure that the gesture is disabled.

    EXPECT_FALSE(testHarness.mouseInteraction().enabled);
}

#endif // ENABLE(IOS_TOUCH_EVENTS)

#if HAVE(MOUSE_DEVICE_OBSERVATION)

TEST(iOSMouseSupport, MouseInitiallyDisconnected)
{
    WKMouseDeviceObserver *mouseDeviceObserver = [NSClassFromString(@"WKMouseDeviceObserver") sharedInstance];

    __block bool didStartMouseDeviceObserver = false;
    [mouseDeviceObserver startWithCompletionHandler:^{
        didStartMouseDeviceObserver = true;
    }];
    TestWebKitAPI::Util::run(&didStartMouseDeviceObserver);

    [mouseDeviceObserver _setHasMouseDeviceForTesting:NO];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600)]);

    [webView synchronouslyLoadHTMLString:@""];

    EXPECT_FALSE([webView evaluateMediaQuery:@"hover"]);
    EXPECT_TRUE([webView evaluateMediaQuery:@"hover: none"]);
    EXPECT_FALSE([webView evaluateMediaQuery:@"hover: hover"]);

    EXPECT_FALSE([webView evaluateMediaQuery:@"any-hover"]);
    EXPECT_TRUE([webView evaluateMediaQuery:@"any-hover: none"]);
    EXPECT_FALSE([webView evaluateMediaQuery:@"any-hover: hover"]);

    EXPECT_TRUE([webView evaluateMediaQuery:@"pointer"]);
    EXPECT_FALSE([webView evaluateMediaQuery:@"pointer: none"]);
    EXPECT_TRUE([webView evaluateMediaQuery:@"pointer: coarse"]);
    EXPECT_FALSE([webView evaluateMediaQuery:@"pointer: fine"]);

    EXPECT_TRUE([webView evaluateMediaQuery:@"any-pointer"]);
    EXPECT_FALSE([webView evaluateMediaQuery:@"any-pointer: none"]);
    EXPECT_TRUE([webView evaluateMediaQuery:@"any-pointer: coarse"]);
    EXPECT_FALSE([webView evaluateMediaQuery:@"any-pointer: fine"]);
}

TEST(iOSMouseSupport, MouseInitiallyConnected)
{
    WKMouseDeviceObserver *mouseDeviceObserver = [NSClassFromString(@"WKMouseDeviceObserver") sharedInstance];

    __block bool didStartMouseDeviceObserver = false;
    [mouseDeviceObserver startWithCompletionHandler:^{
        didStartMouseDeviceObserver = true;
    }];
    TestWebKitAPI::Util::run(&didStartMouseDeviceObserver);

    [mouseDeviceObserver _setHasMouseDeviceForTesting:YES];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600)]);

    [webView synchronouslyLoadHTMLString:@""];

    EXPECT_FALSE([webView evaluateMediaQuery:@"hover"]);
    EXPECT_TRUE([webView evaluateMediaQuery:@"hover: none"]);
    EXPECT_FALSE([webView evaluateMediaQuery:@"hover: hover"]);

    EXPECT_TRUE([webView evaluateMediaQuery:@"any-hover"]);
    EXPECT_FALSE([webView evaluateMediaQuery:@"any-hover: none"]);
    EXPECT_TRUE([webView evaluateMediaQuery:@"any-hover: hover"]);

    EXPECT_TRUE([webView evaluateMediaQuery:@"pointer"]);
    EXPECT_FALSE([webView evaluateMediaQuery:@"pointer: none"]);
    EXPECT_TRUE([webView evaluateMediaQuery:@"pointer: coarse"]);
    EXPECT_FALSE([webView evaluateMediaQuery:@"pointer: fine"]);

    EXPECT_TRUE([webView evaluateMediaQuery:@"any-pointer"]);
    EXPECT_FALSE([webView evaluateMediaQuery:@"any-pointer: none"]);
    EXPECT_TRUE([webView evaluateMediaQuery:@"any-pointer: coarse"]);
    EXPECT_TRUE([webView evaluateMediaQuery:@"any-pointer: fine"]);
}

TEST(iOSMouseSupport, MouseLaterDisconnected)
{
    WKMouseDeviceObserver *mouseDeviceObserver = [NSClassFromString(@"WKMouseDeviceObserver") sharedInstance];

    __block bool didStartMouseDeviceObserver = false;
    [mouseDeviceObserver startWithCompletionHandler:^{
        didStartMouseDeviceObserver = true;
    }];
    TestWebKitAPI::Util::run(&didStartMouseDeviceObserver);

    [mouseDeviceObserver _setHasMouseDeviceForTesting:YES];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600)]);

    [webView synchronouslyLoadHTMLString:@""];

    [mouseDeviceObserver _setHasMouseDeviceForTesting:NO];

    EXPECT_FALSE([webView evaluateMediaQuery:@"hover"]);
    EXPECT_TRUE([webView evaluateMediaQuery:@"hover: none"]);
    EXPECT_FALSE([webView evaluateMediaQuery:@"hover: hover"]);

    EXPECT_FALSE([webView evaluateMediaQuery:@"any-hover"]);
    EXPECT_TRUE([webView evaluateMediaQuery:@"any-hover: none"]);
    EXPECT_FALSE([webView evaluateMediaQuery:@"any-hover: hover"]);

    EXPECT_TRUE([webView evaluateMediaQuery:@"pointer"]);
    EXPECT_FALSE([webView evaluateMediaQuery:@"pointer: none"]);
    EXPECT_TRUE([webView evaluateMediaQuery:@"pointer: coarse"]);
    EXPECT_FALSE([webView evaluateMediaQuery:@"pointer: fine"]);

    EXPECT_TRUE([webView evaluateMediaQuery:@"any-pointer"]);
    EXPECT_FALSE([webView evaluateMediaQuery:@"any-pointer: none"]);
    EXPECT_TRUE([webView evaluateMediaQuery:@"any-pointer: coarse"]);
    EXPECT_FALSE([webView evaluateMediaQuery:@"any-pointer: fine"]);
}

TEST(iOSMouseSupport, MouseLaterConnected)
{
    WKMouseDeviceObserver *mouseDeviceObserver = [NSClassFromString(@"WKMouseDeviceObserver") sharedInstance];

    __block bool didStartMouseDeviceObserver = false;
    [mouseDeviceObserver startWithCompletionHandler:^{
        didStartMouseDeviceObserver = true;
    }];
    TestWebKitAPI::Util::run(&didStartMouseDeviceObserver);

    [mouseDeviceObserver _setHasMouseDeviceForTesting:NO];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600)]);

    [webView synchronouslyLoadHTMLString:@""];

    [mouseDeviceObserver _setHasMouseDeviceForTesting:YES];

    EXPECT_FALSE([webView evaluateMediaQuery:@"hover"]);
    EXPECT_TRUE([webView evaluateMediaQuery:@"hover: none"]);
    EXPECT_FALSE([webView evaluateMediaQuery:@"hover: hover"]);

    EXPECT_TRUE([webView evaluateMediaQuery:@"any-hover"]);
    EXPECT_FALSE([webView evaluateMediaQuery:@"any-hover: none"]);
    EXPECT_TRUE([webView evaluateMediaQuery:@"any-hover: hover"]);

    EXPECT_TRUE([webView evaluateMediaQuery:@"pointer"]);
    EXPECT_FALSE([webView evaluateMediaQuery:@"pointer: none"]);
    EXPECT_TRUE([webView evaluateMediaQuery:@"pointer: coarse"]);
    EXPECT_FALSE([webView evaluateMediaQuery:@"pointer: fine"]);

    EXPECT_TRUE([webView evaluateMediaQuery:@"any-pointer"]);
    EXPECT_FALSE([webView evaluateMediaQuery:@"any-pointer: none"]);
    EXPECT_TRUE([webView evaluateMediaQuery:@"any-pointer: coarse"]);
    EXPECT_TRUE([webView evaluateMediaQuery:@"any-pointer: fine"]);
}

#endif // HAVE(MOUSE_DEVICE_OBSERVATION)

#if PLATFORM(MACCATALYST)

TEST(iOSMouseSupport, MouseAlwaysConnected)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600)]);

    [webView synchronouslyLoadHTMLString:@""];

    EXPECT_TRUE([webView evaluateMediaQuery:@"hover"]);
    EXPECT_FALSE([webView evaluateMediaQuery:@"hover: none"]);
    EXPECT_TRUE([webView evaluateMediaQuery:@"hover: hover"]);

    EXPECT_TRUE([webView evaluateMediaQuery:@"any-hover"]);
    EXPECT_FALSE([webView evaluateMediaQuery:@"any-hover: none"]);
    EXPECT_TRUE([webView evaluateMediaQuery:@"any-hover: hover"]);

    EXPECT_TRUE([webView evaluateMediaQuery:@"pointer"]);
    EXPECT_FALSE([webView evaluateMediaQuery:@"pointer: none"]);
    EXPECT_FALSE([webView evaluateMediaQuery:@"pointer: coarse"]);
    EXPECT_TRUE([webView evaluateMediaQuery:@"pointer: fine"]);

    EXPECT_TRUE([webView evaluateMediaQuery:@"any-pointer"]);
    EXPECT_FALSE([webView evaluateMediaQuery:@"any-pointer: none"]);
    EXPECT_FALSE([webView evaluateMediaQuery:@"any-pointer: coarse"]);
    EXPECT_TRUE([webView evaluateMediaQuery:@"any-pointer: fine"]);
}

#endif // PLATFORM(MACCATALYST)

#if HAVE(UI_POINTER_INTERACTION)

TEST(iOSMouseSupport, BasicPointerInteractionRegions)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 800, 600)]);
    [webView synchronouslyLoadTestPageNamed:@"cursor-styles"];

    {
        auto info = [webView pointerInfoAtLocation:[webView elementMidpointFromSelector:@"#container"]];
        EXPECT_TRUE(info.isDefault);
        EXPECT_TRUE(CGRectEqualToRect(info.regionRect, [webView bounds]));
    }
    {
        auto elementRect = [webView elementRectFromSelector:@"#editable-container"];
        auto info = [webView pointerInfoAtLocation:[webView elementMidpointFromSelector:@"#editable-container"]];
        EXPECT_FALSE(info.isDefault);
        EXPECT_TRUE(CGRectContainsRect(elementRect, info.regionRect));
    }
    {
        auto elementRect = [webView elementRectFromSelector:@"#container-with-cursor-pointer"];
        auto info = [webView pointerInfoAtLocation:[webView elementMidpointFromSelector:@"#container-with-cursor-pointer"]];
        EXPECT_TRUE(info.isDefault);
        EXPECT_TRUE(CGRectContainsRect(elementRect, info.regionRect));
    }
    {
        auto elementRect = [webView elementRectFromSelector:@"#container-with-text"];
        auto info = [webView pointerInfoAtLocation:[webView elementMidpointFromSelector:@"#container-with-text"]];
        EXPECT_FALSE(info.isDefault);
        EXPECT_TRUE(CGRectContainsRect(elementRect, info.regionRect));
    }
    {
        auto elementRect = [webView elementRectFromSelector:@"#container-with-link"];
        auto info = [webView pointerInfoAtLocation:[webView elementMidpointFromSelector:@"#container-with-link"]];
        EXPECT_TRUE(info.isDefault);
        EXPECT_TRUE(CGRectContainsRect(elementRect, info.regionRect));
    }
    {
        auto elementRect = [webView elementRectFromSelector:@"#container-with-cursor-text"];
        auto info = [webView pointerInfoAtLocation:[webView elementMidpointFromSelector:@"#container-with-cursor-text"]];
        EXPECT_FALSE(info.isDefault);
        EXPECT_TRUE(CGRectContainsRect(elementRect, info.regionRect));
    }
}

#endif // HAVE(UI_POINTER_INTERACTION)

#endif // PLATFORM(IOS) || PLATFORM(MACCATALYST) || PLATFORM(VISION)
