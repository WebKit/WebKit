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
#import "AppKitGestureMomentumEventGenerator.h"

#if HAVE(APPKIT_GESTURES_SUPPORT) && ENABLE(MOMENTUM_EVENT_DISPATCHER)

#import "DrawingAreaProxy.h"
#import "NativeWebWheelEvent.h"
#import "ScrollingAccelerationCurve.h"
#import "WKWebViewInternal.h"
#import "WebPageProxy.h"
#import "WebWheelEvent.h"
#import <QuartzCore/CADisplayLink.h>
#import <wtf/CheckedPtr.h>
#import <wtf/MonotonicTime.h>
#import <wtf/SetForScope.h>

@interface WKAppKitGestureMomentumDisplayLinkTarget : NSObject
- (instancetype)initWithGenerator:(WebKit::AppKitGestureMomentumEventGenerator&)generator;
- (void)invalidate;
- (void)displayLinkFired:(CADisplayLink *)displayLink;
@end

namespace WebKit {

AppKitGestureMomentumEventGenerator::AppKitGestureMomentumEventGenerator(WKAppKitGestureController *controller)
    : m_controller(controller)
    , m_dispatcher(makeUnique<MomentumEventDispatcher>(*this))
{
}

AppKitGestureMomentumEventGenerator::~AppKitGestureMomentumEventGenerator()
{
    // The dispatcher's callbacks reach the controller, which must not destroy the generator while the dispatcher runs.
    RELEASE_ASSERT(!m_isCallingDispatcher);
}

void AppKitGestureMomentumEventGenerator::start(WebPageProxy& page, const NativeWebWheelEvent& event)
{
    ASSERT(event.momentumPhase() == WebWheelEvent::Phase::Began);
    SetForScope isCallingDispatcher { m_isCallingDispatcher, true };

    interrupt();

    m_pageIdentifier = page.webPageIDInMainFrameProcess();
    m_displayID = page.displayID().value_or(0);

    auto curve = ScrollingAccelerationCurve::fromNativeWheelEvent(event).or_else([] {
        return ScrollingAccelerationCurve::fallbackCurve();
    });
    m_dispatcher->setScrollingAccelerationCurve(*m_pageIdentifier, WTF::move(curve));

    RefPtr drawingArea = page.drawingArea();
    m_dispatcher->pageScreenDidChange(*m_pageIdentifier, m_displayID, drawingArea ? drawingArea->displayNominalFramesPerSecond() : std::nullopt);

    m_dispatcher->handleWheelEvent(*m_pageIdentifier, event, { });
}

void AppKitGestureMomentumEventGenerator::interrupt()
{
    if (!m_isActive)
        return;

    SetForScope isCallingDispatcher { m_isCallingDispatcher, true };
    auto timestamp = MonotonicTime::now();
    Ref event = WebWheelEvent::create({ WebEventType::Wheel, { }, timestamp }, {
        .granularity = WebWheelEvent::Granularity::ScrollByPixelWheelEvent,
        .directionInvertedFromDevice = false,
        .phase = WebWheelEvent::Phase::Cancelled,
        .momentumPhase = WebWheelEvent::Phase::None,
        .hasPreciseScrollingDeltas = true,
        .scrollCount = 1,
        .ioHIDEventTimestamp = timestamp,
        .rawPlatformDelta = std::nullopt,
        .momentumEndType = WebWheelEvent::MomentumEndType::Interrupted,
        .inputSource = WebEventInputSource::Touch,
    });
    m_dispatcher->handleWheelEvent(*m_pageIdentifier, event, { });
    ASSERT(!m_isActive);
}

void AppKitGestureMomentumEventGenerator::displayDidRefresh()
{
    SetForScope isCallingDispatcher { m_isCallingDispatcher, true };
    m_dispatcher->displayDidRefresh(m_displayID);
}

void AppKitGestureMomentumEventGenerator::handleSyntheticWheelEvent(WebCore::PageIdentifier, const WebWheelEvent& event, WebCore::RectEdges<WebCore::RubberBandingBehavior>)
{
    // The controller sends the momentum Began event itself, since the scrolling thread needs its
    // rawPlatformDelta to start its own curve.
    if (event.momentumPhase() == WebWheelEvent::Phase::Began)
        return;

    RetainPtr webView = [m_controller.get() webView];
    if (!webView)
        return;

    [webView _protectedPage]->handleNativeWheelEvent(NativeWebWheelEvent::create(event));
}

void AppKitGestureMomentumEventGenerator::startDisplayDidRefreshCallbacks(WebCore::PlatformDisplayID)
{
    m_isActive = true;

    RetainPtr webView = [m_controller.get() webView];
    if (!webView || m_displayLink)
        return;

    m_displayLinkTarget = adoptNS([[WKAppKitGestureMomentumDisplayLinkTarget alloc] initWithGenerator:*this]);
    m_displayLink = [webView displayLinkWithTarget:m_displayLinkTarget.get() selector:@selector(displayLinkFired:)];
    [m_displayLink addToRunLoop:NSRunLoop.mainRunLoop forMode:NSRunLoopCommonModes];
}

void AppKitGestureMomentumEventGenerator::stopDisplayDidRefreshCallbacks(WebCore::PlatformDisplayID)
{
    [std::exchange(m_displayLink, nil) invalidate];
    [std::exchange(m_displayLinkTarget, nil) invalidate];
}

void AppKitGestureMomentumEventGenerator::didEndSyntheticMomentumScrolling()
{
    m_isActive = false;
    RetainPtr controller = m_controller.get();
    [controller generatedMomentumDidEnd];
}

} // namespace WebKit

// The display link retains its target, so this keeps it from retaining the gesture controller.
@implementation WKAppKitGestureMomentumDisplayLinkTarget {
    CheckedPtr<WebKit::AppKitGestureMomentumEventGenerator> _generator;
}

- (instancetype)initWithGenerator:(WebKit::AppKitGestureMomentumEventGenerator&)generator
{
    if (!(self = [super init]))
        return nil;

    _generator = &generator;
    return self;
}

- (void)invalidate
{
    _generator = nullptr;
}

- (void)displayLinkFired:(CADisplayLink *)displayLink
{
    if (CheckedPtr generator = _generator)
        generator->displayDidRefresh();
}

@end

#endif // HAVE(APPKIT_GESTURES_SUPPORT) && ENABLE(MOMENTUM_EVENT_DISPATCHER)
