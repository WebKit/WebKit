/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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
#import "WKAppKitGestureController.h"

#if PLATFORM(MAC)

#import "AppKitSPI.h"
#import "NativeWebWheelEvent.h"
#import "ScrollingAccelerationCurve.h"
#import "ViewGestureController.h"
#import "WKWebView.h"
#import "WebEventModifier.h"
#import "WebEventType.h"
#import "WebPageProxy.h"
#import "WebPreferences.h"
#import "WebViewImpl.h"
#import "WebWheelEvent.h"
#import <Carbon/Carbon.h>
#import <WebCore/FloatPoint.h>
#import <WebCore/FloatSize.h>
#import <WebCore/IntPoint.h>
#import <WebCore/PlatformEventFactoryMac.h>
#import <WebCore/Scrollbar.h>
#import <source_location>
#import <wtf/CheckedPtr.h>
#import <wtf/MonotonicTime.h>
#import <wtf/RefPtr.h>
#import <wtf/RetainPtr.h>
#import <wtf/UUID.h>
#import <wtf/WeakPtr.h>

#define WK_APPKIT_GESTURE_CONTROLLER_RELEASE_LOG(pageID, fmt, ...) RELEASE_LOG(ViewGestures, "[pageProxyID=%llu] %s: " fmt, pageID, std::source_location::current().function_name(), ##__VA_ARGS__)

static WebCore::FloatSize translationInView(NSPanGestureRecognizer *gesture, WKWebView *view)
{
    auto translation = WebCore::toFloatSize(WebCore::FloatPoint { [gesture translationInView:view] });
    [gesture setTranslation:NSZeroPoint inView:view];
    return translation;
}

static WebKit::WebWheelEvent::Phase toWheelEventPhase(NSGestureRecognizerState state)
{
    using enum WebKit::WebWheelEvent::Phase;
    switch (state) {
    case NSGestureRecognizerStatePossible:
        return MayBegin;
    case NSGestureRecognizerStateBegan:
        return Began;
    case NSGestureRecognizerStateChanged:
        return Changed;
    case NSGestureRecognizerStateEnded:
        return Ended;
    case NSGestureRecognizerStateCancelled:
    case NSGestureRecognizerStateFailed:
        return Cancelled;
    default:
        ASSERT_NOT_REACHED();
        return None;
    }
}

static WebCore::FloatSize velocityInView(NSPanGestureRecognizer *gesture, WKWebView *view)
{
    return WebCore::toFloatSize(WebCore::FloatPoint { [gesture velocityInView:view] });
}

static WebCore::FloatSize toRawPlatformDelta(WebCore::FloatSize delta)
{
    // rawPlatformDelta uses IOHIDEvent coordinate conventions, which have the opposite
    // sign from WebKit's delta field. This matches WebEventFactory.mm which negates
    // IOHIDEventFieldScrollX/Y values when extracting rawPlatformDelta from real events.
    return -delta;
}

@implementation WKAppKitGestureController {
    WeakPtr<WebKit::WebPageProxy> _page;
    WeakPtr<WebKit::WebViewImpl> _viewImpl;
    RetainPtr<NSPanGestureRecognizer> _panGestureRecognizer;
    RetainPtr<NSClickGestureRecognizer> _singleClickGestureRecognizer;
    RetainPtr<NSClickGestureRecognizer> _doubleClickGestureRecognizer;
    bool _isMomentumActive;
}

#if USE(APPLE_INTERNAL_SDK) && __has_include(<WebKitAdditions/WKAppKitGestureControllerAdditions.mm>)
#import <WebKitAdditions/WKAppKitGestureControllerAdditions.mm>
#else

- (void)configureForScrolling:(NSPanGestureRecognizer *)gesture
{
}

- (void)configureForSingleClick:(NSClickGestureRecognizer *)gesture
{
}

- (void)configureForDoubleClick:(NSClickGestureRecognizer *)gesture
{
}

#endif

- (instancetype)initWithPage:(std::reference_wrapper<WebKit::WebPageProxy>)page viewImpl:(std::reference_wrapper<WebKit::WebViewImpl>)viewImpl
{
    if (!(self = [super init]))
        return nil;

    _page = page.get();
    _viewImpl = viewImpl.get();

    [self setUpPanGestureRecognizer];
    [self setUpSingleClickGestureRecognizer];
    [self setUpDoubleClickGestureRecognizer];
    [self addGesturesToWebView];
    [self enableGesturesIfNeeded];

    return self;
}

- (void)setUpPanGestureRecognizer
{
    _panGestureRecognizer = adoptNS([[NSPanGestureRecognizer alloc] initWithTarget:self action:@selector(panGestureRecognized:)]);
    [self configureForScrolling:_panGestureRecognizer.get()];
    [_panGestureRecognizer setDelegate:self];
#if HAVE(NSGESTURERECOGNIZER_NAME)
    [_panGestureRecognizer setName:@"WKPanGesture"];
#endif
}

- (void)setUpSingleClickGestureRecognizer
{
    _singleClickGestureRecognizer = adoptNS([[NSClickGestureRecognizer alloc] initWithTarget:self action:@selector(singleClickGestureRecognized:)]);
    [self configureForSingleClick:_singleClickGestureRecognizer.get()];
    [_singleClickGestureRecognizer setDelegate:self];
#if HAVE(NSGESTURERECOGNIZER_NAME)
    [_singleClickGestureRecognizer setName:@"WKSingleClickGesture"];
#endif
}

- (void)setUpDoubleClickGestureRecognizer
{
    _doubleClickGestureRecognizer = adoptNS([[NSClickGestureRecognizer alloc] initWithTarget:self action:@selector(doubleClickGestureRecognized:)]);
    [self configureForDoubleClick:_doubleClickGestureRecognizer.get()];
    [_doubleClickGestureRecognizer setDelegate:self];
#if HAVE(NSGESTURERECOGNIZER_NAME)
    [_doubleClickGestureRecognizer setName:@"WKDoubleClickGesture"];
#endif
}

- (void)addGesturesToWebView
{
    CheckedPtr viewImpl = _viewImpl.get();
    if (!viewImpl)
        return;

    RetainPtr webView = viewImpl->view();
    if (!webView)
        return;

    [webView addGestureRecognizer:_panGestureRecognizer.get()];
    [webView addGestureRecognizer:_singleClickGestureRecognizer.get()];
    [webView addGestureRecognizer:_doubleClickGestureRecognizer.get()];
}

- (void)enableGesturesIfNeeded
{
    [self enableGestureIfNeeded:_panGestureRecognizer.get()];
    [self enableGestureIfNeeded:_singleClickGestureRecognizer.get()];
    [self enableGestureIfNeeded:_doubleClickGestureRecognizer.get()];
}

- (void)enableGestureIfNeeded:(NSGestureRecognizer *)gesture
{
    RefPtr page = _page.get();
    if (!page)
        return;
    bool gestureEnabled = protect(page->preferences())->useAppKitGestures();
    WK_APPKIT_GESTURE_CONTROLLER_RELEASE_LOG(page->logIdentifier(), "%@ setEnabled:%d", gesture, static_cast<int>(gestureEnabled));
    [gesture setEnabled:gestureEnabled];
}

#pragma mark - Gesture Recognition

- (void)panGestureRecognized:(NSGestureRecognizer *)gesture
{
    CheckedPtr viewImpl = _viewImpl.get();
    if (!viewImpl)
        return;

    RefPtr page = _page.get();
    if (!page)
        return;

    WK_APPKIT_GESTURE_CONTROLLER_RELEASE_LOG(page->logIdentifier(), "%@", gesture);

    RetainPtr panGesture = dynamic_objc_cast<NSPanGestureRecognizer>(gesture);
    if (!panGesture || _panGestureRecognizer != panGesture)
        return;

    if (viewImpl->ignoresAllEvents()) {
        WK_APPKIT_GESTURE_CONTROLLER_RELEASE_LOG(page->logIdentifier(), "Ignored gesture");
        return;
    }

    if ([panGesture state] == NSGestureRecognizerStateBegan)
        viewImpl->dismissContentRelativeChildWindowsWithAnimation(false);

    // FIXME: Need to supply a real event here.
    if (viewImpl->allowsBackForwardNavigationGestures() && viewImpl->ensureProtectedGestureController()->handleScrollWheelEvent(nil)) {
        WK_APPKIT_GESTURE_CONTROLLER_RELEASE_LOG(page->logIdentifier(), "View gesture controller handled gesture");
        return;
    }

    [self sendWheelEventForGesture:panGesture.get()];
    [self startMomentumIfNeededForGesture:panGesture.get()];
}

- (void)singleClickGestureRecognized:(NSGestureRecognizer *)gesture
{
    CheckedPtr viewImpl = _viewImpl.get();
    if (!viewImpl)
        return;

    RetainPtr webView = viewImpl->view();
    if (!webView)
        return;

    RefPtr page = _page.get();
    if (!page)
        return;

    WK_APPKIT_GESTURE_CONTROLLER_RELEASE_LOG(page->logIdentifier(), "%@", gesture);

    RetainPtr clickGesture = dynamic_objc_cast<NSClickGestureRecognizer>(gesture);
    if (!clickGesture || _singleClickGestureRecognizer != clickGesture)
        return;

    auto timestamp = GetCurrentEventTime();
ALLOW_NEW_API_WITHOUT_GUARDS_BEGIN
    auto modifierFlags = [gesture modifierFlags];
ALLOW_NEW_API_WITHOUT_GUARDS_END
    auto location = [gesture locationInView:nil];
    auto windowNumber = viewImpl->windowNumber();
    RetainPtr mouseDown = [NSEvent mouseEventWithType:NSEventTypeLeftMouseDown location:location modifierFlags:modifierFlags timestamp:timestamp windowNumber:windowNumber context:NULL eventNumber:0 clickCount:1 pressure:1.0];
    RetainPtr mouseUp = [NSEvent mouseEventWithType:NSEventTypeLeftMouseUp location:location modifierFlags:modifierFlags timestamp:timestamp windowNumber:windowNumber context:NULL eventNumber:0 clickCount:1 pressure:0.0];

    viewImpl->mouseDown(mouseDown.get());
    viewImpl->mouseUp(mouseUp.get());
}

- (void)doubleClickGestureRecognized:(NSGestureRecognizer *)gesture
{
    CheckedPtr viewImpl = _viewImpl.get();
    if (!viewImpl)
        return;

    RetainPtr webView = viewImpl->view();
    if (!webView)
        return;

    RefPtr page = _page.get();
    if (!page)
        return;

    WK_APPKIT_GESTURE_CONTROLLER_RELEASE_LOG(page->logIdentifier(), "%@", gesture);

    RetainPtr clickGesture = dynamic_objc_cast<NSClickGestureRecognizer>(gesture);
    if (!clickGesture || _doubleClickGestureRecognizer != clickGesture)
        return;

    viewImpl->dismissContentRelativeChildWindowsWithAnimation(false);

    auto magnificationOrigin = [webView convertPoint:[gesture locationInView:nil] fromView:nil];
    viewImpl->ensureProtectedGestureController()->handleSmartMagnificationGesture(magnificationOrigin);
}

#pragma mark - Wheel Event Handling

- (void)sendWheelEventForGesture:(NSPanGestureRecognizer *)gesture
{
    CheckedPtr viewImpl = _viewImpl.get();
    if (!viewImpl)
        return;

    RetainPtr webView = viewImpl->view();
    if (!webView)
        return;

    RefPtr page = _page.get();
    if (!page)
        return;

    auto timestamp = MonotonicTime::fromRawSeconds([gesture timestamp]);
    WebCore::IntPoint position { [gesture locationInView:webView.get()] };
    auto globalPosition { WebCore::globalPoint([gesture locationInView:nil], [webView window]) };
    auto gestureDelta { translationInView(gesture, webView.get()) };
    auto wheelTicks { gestureDelta.scaled(1. / static_cast<float>(WebCore::Scrollbar::pixelsPerLineStep())) };
    auto granularity = WebKit::WebWheelEvent::Granularity::ScrollByPixelWheelEvent;
    bool directionInvertedFromDevice = false;
    auto phase = toWheelEventPhase(gesture.state);
    auto momentumPhase = WebKit::WebWheelEvent::Phase::None;
    bool hasPreciseScrollingDeltas = true;
    uint32_t scrollCount = 1;
    auto unacceleratedScrollingDelta = gestureDelta;
    auto ioHIDEventTimestamp = timestamp;
    std::optional<WebCore::FloatSize> rawPlatformDelta;
    auto momentumEndType = WebKit::WebWheelEvent::MomentumEndType::Unknown;

    WebKit::WebWheelEvent wheelEvent {
        { WebKit::WebEventType::Wheel, { }, timestamp, WTF::UUID::createVersion4() },
        WebCore::IntPoint { position },
        WebCore::IntPoint { globalPosition },
        gestureDelta,
        wheelTicks,
        granularity,
        directionInvertedFromDevice,
        phase,
        momentumPhase,
        hasPreciseScrollingDeltas,
        scrollCount,
        unacceleratedScrollingDelta,
        ioHIDEventTimestamp,
        rawPlatformDelta,
        momentumEndType
    };

    page->handleNativeWheelEvent(WebKit::NativeWebWheelEvent { wheelEvent });
}

#pragma mark - Momentum Handling

- (void)startMomentumIfNeededForGesture:(NSPanGestureRecognizer *)gesture
{
    CheckedPtr viewImpl = _viewImpl.get();
    if (!viewImpl)
        return;

    RetainPtr webView = viewImpl->view();
    if (!webView)
        return;

    RefPtr page = _page.get();
    if (!page)
        return;

    if (gesture.state != NSGestureRecognizerStateEnded)
        return;

    auto velocity = velocityInView(gesture, webView.get());
    auto velocityMagnitude = std::max(std::abs(velocity.width()), std::abs(velocity.height()));
    static constexpr float minimumVelocityForMomentum = 20;
    if (velocityMagnitude < minimumVelocityForMomentum)
        return;

    auto timestamp = MonotonicTime::fromRawSeconds([gesture timestamp]);
    WebCore::IntPoint position { [gesture locationInView:webView.get()] };
    auto globalPosition = WebCore::globalPoint([gesture locationInView:nil], [webView window]);

    WebKit::WebWheelEvent momentumEvent {
        { WebKit::WebEventType::Wheel, { }, timestamp, WTF::UUID::createVersion4() },
        position,
        WebCore::IntPoint { globalPosition },
        WebCore::FloatSize { },
        WebCore::FloatSize { },
        WebKit::WebWheelEvent::Granularity::ScrollByPixelWheelEvent,
        false,
        WebKit::WebWheelEvent::Phase::None,
        WebKit::WebWheelEvent::Phase::Began,
        true,
        1,
        WebCore::FloatSize { },
        timestamp,
        std::nullopt,
        WebKit::WebWheelEvent::MomentumEndType::Unknown
    };
    WebKit::NativeWebWheelEvent nativeMomentumEvent { momentumEvent };

    nativeMomentumEvent.setRawPlatformDelta([&nativeMomentumEvent, velocity] {
        static constexpr WebCore::FramesPerSecond fallbackMomentumFrameRate { 60 };
        auto momentumFrameRate = WebKit::ScrollingAccelerationCurve::fromNativeWheelEvent(nativeMomentumEvent)
            .or_else([] {
                return WebKit::ScrollingAccelerationCurve::fallbackCurve();
            }).transform([](const auto& curve) {
                return curve.frameRate();
            }).value_or(fallbackMomentumFrameRate);
        auto initialMomentumDelta = velocity / momentumFrameRate;
        return toRawPlatformDelta(initialMomentumDelta);
    }());

    page->handleNativeWheelEvent(nativeMomentumEvent);
    _isMomentumActive = true;

    WK_APPKIT_GESTURE_CONTROLLER_RELEASE_LOG(page->logIdentifier(), "Started momentum scrolling with velocity %.2f pts/s", velocityMagnitude);
}

- (void)interruptMomentumIfNeeded
{
    if (!std::exchange(_isMomentumActive, false))
        return;

    RefPtr page = _page.get();
    if (!page)
        return;

    auto timestamp = MonotonicTime::now();

    WebKit::WebWheelEvent cancelEvent {
        { WebKit::WebEventType::Wheel, { }, timestamp, WTF::UUID::createVersion4() },
        WebCore::IntPoint { },
        WebCore::IntPoint { },
        WebCore::FloatSize { },
        WebCore::FloatSize { },
        WebKit::WebWheelEvent::Granularity::ScrollByPixelWheelEvent,
        false,
        WebKit::WebWheelEvent::Phase::Cancelled,
        WebKit::WebWheelEvent::Phase::None,
        true,
        1,
        WebCore::FloatSize { },
        timestamp,
        std::nullopt,
        WebKit::WebWheelEvent::MomentumEndType::Interrupted
    };

    page->handleNativeWheelEvent(WebKit::NativeWebWheelEvent { cancelEvent });
    WK_APPKIT_GESTURE_CONTROLLER_RELEASE_LOG(page->logIdentifier(), "Interrupted momentum scrolling");
}

#pragma mark - NSGestureRecognizerDelegate

static inline bool isSamePair(NSGestureRecognizer *a, NSGestureRecognizer *b, NSGestureRecognizer *x, NSGestureRecognizer *y)
{
    return (a == x && b == y) || (b == x && a == y);
}

- (BOOL)gestureRecognizer:(NSGestureRecognizer *)gestureRecognizer shouldRecognizeSimultaneouslyWithGestureRecognizer:(NSGestureRecognizer *)otherGestureRecognizer
{
    WK_APPKIT_GESTURE_CONTROLLER_RELEASE_LOG(RefPtr { _page.get() }->logIdentifier(), "Gesture: %@, Other gesture: %@", gestureRecognizer, otherGestureRecognizer);
    if (isSamePair(gestureRecognizer, otherGestureRecognizer, _singleClickGestureRecognizer.get(), _panGestureRecognizer.get()))
        return YES;
    return NO;
}

- (BOOL)gestureRecognizerShouldBegin:(NSGestureRecognizer *)gestureRecognizer
{
    WK_APPKIT_GESTURE_CONTROLLER_RELEASE_LOG(RefPtr { _page.get() }->logIdentifier(), "Gesture: %@", gestureRecognizer);

    if (gestureRecognizer == _doubleClickGestureRecognizer.get()) {
        CheckedPtr viewImpl = _viewImpl.get();
        if (!viewImpl || !viewImpl->allowsMagnification())
            return NO;
    }

    return YES;
}

@end

#undef WK_APPKIT_GESTURE_CONTROLLER_RELEASE_LOG

#endif
