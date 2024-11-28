/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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
#import "RemoteLayerTreeDrawingAreaProxyIOS.h"

#if PLATFORM(IOS_FAMILY)

#import "CAFrameRateRangeUtilities.h"
#import "RemoteScrollingCoordinatorProxyIOS.h"
#import "WebPageProxy.h"
#import "WebPreferences.h"
#import "WebProcessProxy.h"
#import <QuartzCore/CADisplayLink.h>
#import <WebCore/LocalFrameView.h>
#import <WebCore/ScrollView.h>
#import <pal/spi/cocoa/QuartzCoreSPI.h>
#import <wtf/TZoneMallocInlines.h>

constexpr WebCore::FramesPerSecond DisplayLinkFramesPerSecond = 60;

@interface WKDisplayLinkHandler : NSObject {
    WebKit::RemoteLayerTreeDrawingAreaProxy* _drawingAreaProxy;
    CADisplayLink *_displayLink;
#if ENABLE(TIMER_DRIVEN_DISPLAY_REFRESH_FOR_TESTING)
    RetainPtr<NSTimer> _updateTimer;
    std::optional<WebCore::FramesPerSecond> _overrideFrameRate;
#endif
}

- (id)initWithDrawingAreaProxy:(WebKit::RemoteLayerTreeDrawingAreaProxy*)drawingAreaProxy;
- (void)setPreferredFramesPerSecond:(NSInteger)preferredFramesPerSecond;
- (void)displayLinkFired:(CADisplayLink *)sender;
- (void)invalidate;
- (void)schedule;
- (WebCore::FramesPerSecond)nominalFramesPerSecond;

@end

static void* displayRefreshRateObservationContext = &displayRefreshRateObservationContext;

@implementation WKDisplayLinkHandler

- (id)initWithDrawingAreaProxy:(WebKit::RemoteLayerTreeDrawingAreaProxy*)drawingAreaProxy
{
    if (self = [super init]) {
        _drawingAreaProxy = drawingAreaProxy;
        // Note that CADisplayLink retains its target (self), so a call to -invalidate is needed on teardown.
        bool createDisplayLink = true;
#if ENABLE(TIMER_DRIVEN_DISPLAY_REFRESH_FOR_TESTING)
        NSInteger overrideRefreshRateValue = [NSUserDefaults.standardUserDefaults integerForKey:@"MainScreenRefreshRate"];
        if (overrideRefreshRateValue) {
            _overrideFrameRate = overrideRefreshRateValue;
            createDisplayLink = false;
        }
#endif
        if (createDisplayLink) {
            _displayLink = [CADisplayLink displayLinkWithTarget:self selector:@selector(displayLinkFired:)];
            [_displayLink addToRunLoop:[NSRunLoop mainRunLoop] forMode:NSRunLoopCommonModes];
            [_displayLink.display addObserver:self forKeyPath:@"refreshRate" options:NSKeyValueObservingOptionNew context:displayRefreshRateObservationContext];
            _displayLink.paused = YES;

            if (drawingAreaProxy && !drawingAreaProxy->page().preferences().preferPageRenderingUpdatesNear60FPSEnabled()) {
#if HAVE(CORE_ANIMATION_FRAME_RATE_RANGE)
                [_displayLink setPreferredFrameRateRange:WebKit::highFrameRateRange()];
                [_displayLink setHighFrameRateReason:WebKit::preferPageRenderingUpdatesNear60FPSDisabledHighFrameRateReason];
#else
                _displayLink.preferredFramesPerSecond = (1.0 / _displayLink.maximumRefreshRate);
#endif
            } else
                _displayLink.preferredFramesPerSecond = DisplayLinkFramesPerSecond;
        }
    }
    return self;
}

- (void)dealloc
{
    ASSERT(!_displayLink);
    [super dealloc];
}

- (void)setPreferredFramesPerSecond:(NSInteger)preferredFramesPerSecond
{
    _displayLink.preferredFramesPerSecond = preferredFramesPerSecond;
}

- (void)displayLinkFired:(CADisplayLink *)sender
{
    ASSERT(isUIThread());
    _drawingAreaProxy->didRefreshDisplay();
}

#if ENABLE(TIMER_DRIVEN_DISPLAY_REFRESH_FOR_TESTING)
- (void)timerFired
{
    ASSERT(isUIThread());
    _drawingAreaProxy->didRefreshDisplay();
}
#endif // ENABLE(TIMER_DRIVEN_DISPLAY_REFRESH_FOR_TESTING)

- (void)invalidate
{
    [_displayLink.display removeObserver:self forKeyPath:@"refreshRate" context:displayRefreshRateObservationContext];
    [_displayLink invalidate];
    _displayLink = nullptr;

#if ENABLE(TIMER_DRIVEN_DISPLAY_REFRESH_FOR_TESTING)
    [_updateTimer invalidate];
    _updateTimer = nil;
#endif
}

- (void)schedule
{
    _displayLink.paused = NO;
#if ENABLE(TIMER_DRIVEN_DISPLAY_REFRESH_FOR_TESTING)
    if (!_updateTimer && _overrideFrameRate.has_value())
        _updateTimer = [NSTimer scheduledTimerWithTimeInterval:1.0 / _overrideFrameRate.value() target:self selector:@selector(timerFired) userInfo:nil repeats:YES];
#endif
}

- (void)pause
{
    _displayLink.paused = YES;
#if ENABLE(TIMER_DRIVEN_DISPLAY_REFRESH_FOR_TESTING)
    [_updateTimer invalidate];
    _updateTimer = nil;
#endif
}

- (void)observeValueForKeyPath:(NSString *)keyPath ofObject:(id)object change:(NSDictionary *)change context:(void *)context
{
    if (context != displayRefreshRateObservationContext)
        return;
    [self didChangeNominalFramesPerSecond];
}

- (WebCore::FramesPerSecond)nominalFramesPerSecond
{
    auto& page = _drawingAreaProxy->page();
    if (page.preferences().webAnimationsCustomFrameRateEnabled() || !page.preferences().preferPageRenderingUpdatesNear60FPSEnabled()) {
        auto minimumRefreshInterval = _displayLink.maximumRefreshRate;
        if (minimumRefreshInterval > 0)
            return std::round(1.0 / minimumRefreshInterval);
    }

    return DisplayLinkFramesPerSecond;
}

- (void)didChangeNominalFramesPerSecond
{
    Ref page = _drawingAreaProxy->page();
    if (auto displayID = page->displayID())
        page->windowScreenDidChange(*displayID);
}

@end

namespace WebKit {
using namespace IPC;
using namespace WebCore;

WTF_MAKE_TZONE_ALLOCATED_IMPL(RemoteLayerTreeDrawingAreaProxyIOS);

RemoteLayerTreeDrawingAreaProxyIOS::RemoteLayerTreeDrawingAreaProxyIOS(WebPageProxy& pageProxy, WebProcessProxy& webProcessProxy)
    : RemoteLayerTreeDrawingAreaProxy(pageProxy, webProcessProxy)
{
}

RemoteLayerTreeDrawingAreaProxyIOS::~RemoteLayerTreeDrawingAreaProxyIOS()
{
    [m_displayLinkHandler invalidate];
}

std::unique_ptr<RemoteScrollingCoordinatorProxy> RemoteLayerTreeDrawingAreaProxyIOS::createScrollingCoordinatorProxy() const
{
    return makeUnique<RemoteScrollingCoordinatorProxyIOS>(m_webPageProxy);
}

DelegatedScrollingMode RemoteLayerTreeDrawingAreaProxyIOS::delegatedScrollingMode() const
{
    return DelegatedScrollingMode::DelegatedToNativeScrollView;
}

WKDisplayLinkHandler *RemoteLayerTreeDrawingAreaProxyIOS::displayLinkHandler()
{
    if (!m_displayLinkHandler)
        m_displayLinkHandler = adoptNS([[WKDisplayLinkHandler alloc] initWithDrawingAreaProxy:this]);
    return m_displayLinkHandler.get();
}

void RemoteLayerTreeDrawingAreaProxyIOS::setPreferredFramesPerSecond(IPC::Connection& connection, FramesPerSecond preferredFramesPerSecond)
{
    if (!m_webProcessProxy->hasConnection(connection))
        return;

    [displayLinkHandler() setPreferredFramesPerSecond:preferredFramesPerSecond];
}

void RemoteLayerTreeDrawingAreaProxyIOS::didRefreshDisplay()
{
    if (m_needsDisplayRefreshCallbacksForDrawing)
        RemoteLayerTreeDrawingAreaProxy::didRefreshDisplay();

    if (m_needsDisplayRefreshCallbacksForAnimation) {
        if (auto displayID = m_webPageProxy->displayID())
            m_webPageProxy->scrollingCoordinatorProxy()->displayDidRefresh(*displayID);
    }
}

void RemoteLayerTreeDrawingAreaProxyIOS::scheduleDisplayRefreshCallbacks()
{
    m_needsDisplayRefreshCallbacksForDrawing = true;
    [displayLinkHandler() schedule];
}

void RemoteLayerTreeDrawingAreaProxyIOS::pauseDisplayRefreshCallbacks()
{
    m_needsDisplayRefreshCallbacksForDrawing = false;
    if (!m_needsDisplayRefreshCallbacksForAnimation)
        [displayLinkHandler() pause];
}

void RemoteLayerTreeDrawingAreaProxyIOS::scheduleDisplayRefreshCallbacksForAnimation()
{
    m_needsDisplayRefreshCallbacksForAnimation = true;
    [displayLinkHandler() schedule];
}

void RemoteLayerTreeDrawingAreaProxyIOS::pauseDisplayRefreshCallbacksForAnimation()
{
    m_needsDisplayRefreshCallbacksForAnimation = false;
    if (!m_needsDisplayRefreshCallbacksForDrawing)
        [displayLinkHandler() pause];
}

std::optional<WebCore::FramesPerSecond> RemoteLayerTreeDrawingAreaProxyIOS::displayNominalFramesPerSecond()
{
    return [displayLinkHandler() nominalFramesPerSecond];
}

} // namespace WebKit

#endif // PLATFORM(IOS_FAMILY)
