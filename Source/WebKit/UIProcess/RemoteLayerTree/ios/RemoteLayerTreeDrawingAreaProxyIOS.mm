/*
 * Copyright (C) 2022-2025 Apple Inc. All rights reserved.
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
#import "PageClient.h"
#import "RemoteScrollingCoordinatorProxyIOS.h"
#import "WebPageProxy.h"
#import "WebPreferences.h"
#import "WebProcessProxy.h"
#import <QuartzCore/CADisplayLink.h>
#import <UIKit/UIScreen.h>
#import <WebCore/DisplayUpdate.h>
#import <WebCore/LocalFrameView.h>
#import <WebCore/ScrollView.h>
#import <pal/spi/cocoa/QuartzCoreSPI.h>
#import <wtf/TZoneMallocInlines.h>

constexpr WebCore::FramesPerSecond DisplayLinkFramesPerSecond = 60;

@interface WKDisplayLinkHandler : NSObject {
    WeakPtr<WebKit::RemoteLayerTreeDrawingAreaProxy> _drawingAreaProxy;
    RetainPtr<CADisplayLink> _displayLink;
    __weak UIScreen *_screenForDisplayLink;
    WebCore::FramesPerSecond _preferredFramesPerSecond;
    BOOL _wantsHighFrameRate;
    WebCore::DisplayUpdate _currentUpdate;
#if ENABLE(TIMER_DRIVEN_DISPLAY_REFRESH_FOR_TESTING)
    RetainPtr<NSTimer> _updateTimer;
    std::optional<WebCore::FramesPerSecond> _overrideFrameRate;
#endif
}

- (id)initWithDrawingAreaProxy:(WebKit::RemoteLayerTreeDrawingAreaProxy*)drawingAreaProxy;
- (void)windowScreenDidChange;
- (void)displayLinkFired:(CADisplayLink *)sender;
- (void)invalidate;
- (void)schedule;
- (WebCore::FramesPerSecond)nominalFramesPerSecond;
// The methods setPreferredFramesPerSecond and setWantsHighFrameRate provide the data that
// will let WKDisplayLinkHandler compute the effective frames per second for its managed
// CADisplayLink. The value provided by setPreferredFramesPerSecond identify the frames
// per second wanted by default and dictated by the Web process according to, for instance,
// low-power mode and the "Prefer Page Rendering Updates near 60fps" setting. That value can
// be overridden temporarily by setWantsHighFrameRate to opt into the maximum frames per second
// supported by the display link for things like high-performance animations.
- (void)setPreferredFramesPerSecond:(WebCore::FramesPerSecond)preferredFramesPerSecond;
- (void)setWantsHighFrameRate:(BOOL)wantsHighFrameRate;
- (BOOL)isDisplayRefreshRelevantForPreferredUpdateFrequency;

@end

static void* displayRefreshRateObservationContext = &displayRefreshRateObservationContext;

@implementation WKDisplayLinkHandler

- (id)initWithDrawingAreaProxy:(WebKit::RemoteLayerTreeDrawingAreaProxy*)drawingAreaProxy
{
    if (self = [super init]) {
        _drawingAreaProxy = drawingAreaProxy;
        _wantsHighFrameRate = NO;
        _preferredFramesPerSecond = DisplayLinkFramesPerSecond;
        _currentUpdate = { 0, _preferredFramesPerSecond };
        // Note that CADisplayLink retains its target (self), so a call to -invalidate is needed on teardown.
        bool createDisplayLink = true;
#if ENABLE(TIMER_DRIVEN_DISPLAY_REFRESH_FOR_TESTING)
        NSInteger overrideRefreshRateValue = [NSUserDefaults.standardUserDefaults integerForKey:@"MainScreenRefreshRate"];
        if (overrideRefreshRateValue) {
            _overrideFrameRate = overrideRefreshRateValue;
            createDisplayLink = false;
        }
#endif
        if (createDisplayLink)
            [self _createDisplayLink];
    }
    return self;
}

- (void)windowScreenDidChange
{
    if (!_displayLink)
        return;

    RetainPtr screen = [self _screenForDisplayLink];
    if (_screenForDisplayLink == screen.get())
        return;

ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    if (!_screenForDisplayLink && screen == [UIScreen mainScreen])
        return;
ALLOW_DEPRECATED_DECLARATIONS_END

    BOOL wasPaused = [_displayLink isPaused];

    [self invalidate];

    [self _createDisplayLink];
    [_displayLink setPaused:wasPaused];
}

- (void)dealloc
{
    ASSERT(!_displayLink);
    [super dealloc];
}

- (void)displayLinkFired:(CADisplayLink *)sender
{
    ASSERT(isUIThread());
    if (RefPtr drawingArea = _drawingAreaProxy)
        drawingArea->didRefreshDisplay();
    _currentUpdate = _currentUpdate.nextUpdate();
}

#if ENABLE(TIMER_DRIVEN_DISPLAY_REFRESH_FOR_TESTING)
- (void)timerFired
{
    ASSERT(isUIThread());
    if (RefPtr drawingArea = _drawingAreaProxy)
        drawingArea->didRefreshDisplay();
    _currentUpdate = _currentUpdate.nextUpdate();
}
#endif // ENABLE(TIMER_DRIVEN_DISPLAY_REFRESH_FOR_TESTING)

- (void)invalidate
{
    [_displayLink.get().display removeObserver:self forKeyPath:@"refreshRate" context:displayRefreshRateObservationContext];
    [_displayLink invalidate];
    _displayLink = nullptr;

#if ENABLE(TIMER_DRIVEN_DISPLAY_REFRESH_FOR_TESTING)
    [_updateTimer invalidate];
    _updateTimer = nil;
#endif
}

- (void)schedule
{
    _displayLink.get().paused = NO;
#if ENABLE(TIMER_DRIVEN_DISPLAY_REFRESH_FOR_TESTING)
    if (!_updateTimer && _overrideFrameRate.has_value())
        _updateTimer = [NSTimer scheduledTimerWithTimeInterval:1.0 / _overrideFrameRate.value() target:self selector:@selector(timerFired) userInfo:nil repeats:YES];
#endif
}

- (void)pause
{
    _displayLink.get().paused = YES;
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
    RefPtr page = _drawingAreaProxy ? protect(*_drawingAreaProxy)->page() : nullptr;
    if (page && (page->preferences().webAnimationsCustomFrameRateEnabled() || !page->preferences().preferPageRenderingUpdatesNear60FPSEnabled())) {
        auto minimumRefreshInterval = _displayLink.get().maximumRefreshRate;
        if (minimumRefreshInterval > 0)
            return std::round(1.0 / minimumRefreshInterval);
    }

    return DisplayLinkFramesPerSecond;
}

- (void)didChangeNominalFramesPerSecond
{
    RefPtr page = _drawingAreaProxy ? protect(*_drawingAreaProxy)->page() : nullptr;
    if (!page)
        return;
    if (auto displayID = page->displayID())
        page->windowScreenDidChange(*displayID);
}

- (void)setPreferredFramesPerSecond:(WebCore::FramesPerSecond)preferredFramesPerSecond
{
    if (_preferredFramesPerSecond == preferredFramesPerSecond)
        return;

    _preferredFramesPerSecond = preferredFramesPerSecond;
    [self updateFrameRate];
}

- (void)setWantsHighFrameRate:(BOOL)wantsHighFrameRate
{
    if (_wantsHighFrameRate == wantsHighFrameRate)
        return;

    _wantsHighFrameRate = wantsHighFrameRate;
    [self updateFrameRate];
}

- (void)updateFrameRate
{
    auto effectiveFramesPerSecond = _preferredFramesPerSecond;
    bool canHaveHighFrameRate = _preferredFramesPerSecond >= DisplayLinkFramesPerSecond;

    if (canHaveHighFrameRate && _wantsHighFrameRate) {
#if HAVE(CORE_ANIMATION_FRAME_RATE_RANGE)
        auto frameRateRange = WebKit::highFrameRateRange();
        effectiveFramesPerSecond = frameRateRange.maximum;
        [_displayLink setPreferredFrameRateRange:frameRateRange];

        RefPtr page = _drawingAreaProxy ? protect(*_drawingAreaProxy)->page() : nullptr;
        auto preferPageRenderingUpdatesNear60FPSEnabled = !page || page->preferences().preferPageRenderingUpdatesNear60FPSEnabled();
        auto highFrameRateReason = preferPageRenderingUpdatesNear60FPSEnabled ? WebKit::webAnimationHighFrameRateReason :
            WebKit::preferPageRenderingUpdatesNear60FPSDisabledHighFrameRateReason;
        [_displayLink setHighFrameRateReason:highFrameRateReason];
#else
        effectiveFramesPerSecond = 1.0 / _displayLink.get().maximumRefreshRate;
        _displayLink.get().preferredFramesPerSecond = effectiveFramesPerSecond;
#endif
    } else
        _displayLink.get().preferredFramesPerSecond = effectiveFramesPerSecond;

    if (_currentUpdate.updatesPerSecond != effectiveFramesPerSecond)
        _currentUpdate = { 0, effectiveFramesPerSecond };
}

- (BOOL)isDisplayRefreshRelevantForPreferredUpdateFrequency
{
    return _currentUpdate.relevantForUpdateFrequency(_preferredFramesPerSecond);
}

- (UIScreen *)_screenForDisplayLink
{
    RefPtr page = _drawingAreaProxy->page();
    if (!page)
        return nil;

    RefPtr pageClient = page->pageClient();
    if (!pageClient)
        return nil;

    return pageClient->screen();
}

- (void)_createDisplayLink
{
    ASSERT(!_displayLink);

    _screenForDisplayLink = [self _screenForDisplayLink];

    if (_screenForDisplayLink)
        _displayLink = [_screenForDisplayLink displayLinkWithTarget:self selector:@selector(displayLinkFired:)];
    else {
ALLOW_DEPRECATED_DECLARATIONS_BEGIN
        // FIXME: CoreAnimation version deprecated rdar://164090713
        _displayLink = [CADisplayLink displayLinkWithTarget:self selector:@selector(displayLinkFired:)];
ALLOW_DEPRECATED_DECLARATIONS_END
    }

    [_displayLink addToRunLoop:[NSRunLoop mainRunLoop] forMode:NSRunLoopCommonModes];
    [[_displayLink display] addObserver:self forKeyPath:@"refreshRate" options:NSKeyValueObservingOptionNew context:displayRefreshRateObservationContext];
    [_displayLink setPaused:YES];
}

@end

namespace WebKit {
using namespace IPC;
using namespace WebCore;

WTF_MAKE_TZONE_ALLOCATED_IMPL(RemoteLayerTreeDrawingAreaProxyIOS);

Ref<RemoteLayerTreeDrawingAreaProxyIOS> RemoteLayerTreeDrawingAreaProxyIOS::create(WebPageProxy& page, WebProcessProxy& webProcessProxy)
{
    return adoptRef(*new RemoteLayerTreeDrawingAreaProxyIOS(page, webProcessProxy));
}

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
    return makeUnique<RemoteScrollingCoordinatorProxyIOS>(*page());
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
    if (!webProcessProxy().hasConnection(connection))
        return;

    [displayLinkHandler() setPreferredFramesPerSecond:preferredFramesPerSecond];
}

void RemoteLayerTreeDrawingAreaProxyIOS::didRefreshDisplay()
{
    if (RefPtr page = this->page())
        page->didRefreshDisplay();

    if (m_needsDisplayRefreshCallbacksForDrawing && [displayLinkHandler() isDisplayRefreshRelevantForPreferredUpdateFrequency])
        RemoteLayerTreeDrawingAreaProxy::didRefreshDisplay();

    if (m_needsDisplayRefreshCallbacksForMonotonicAnimations) {
        RefPtr page = this->page();
        if (!page)
            return;
        if (auto displayID = page->displayID())
            page->scrollingCoordinatorProxy()->displayDidRefresh(*displayID);
    }
}

void RemoteLayerTreeDrawingAreaProxyIOS::scheduleDisplayRefreshCallbacks()
{
    m_needsDisplayRefreshCallbacksForDrawing = true;
    scheduleDisplayLinkAndSetFrameRate();
}

void RemoteLayerTreeDrawingAreaProxyIOS::pauseDisplayRefreshCallbacks()
{
    m_needsDisplayRefreshCallbacksForDrawing = false;
    pauseDisplayLinkIfNeeded();
}

void RemoteLayerTreeDrawingAreaProxyIOS::scheduleDisplayRefreshCallbacksForMonotonicAnimations()
{
    m_needsDisplayRefreshCallbacksForMonotonicAnimations = true;
    scheduleDisplayLinkAndSetFrameRate();
}

void RemoteLayerTreeDrawingAreaProxyIOS::pauseDisplayRefreshCallbacksForMonotonicAnimations()
{
    m_needsDisplayRefreshCallbacksForMonotonicAnimations = false;
    pauseDisplayLinkIfNeeded();
}

void RemoteLayerTreeDrawingAreaProxyIOS::scheduleDisplayLinkAndSetFrameRate()
{
    auto preferPageRenderingUpdatesNear60FPSEnabled = [&] {
        return !page() || page()->preferences().preferPageRenderingUpdatesNear60FPSEnabled();
    };

    auto wantsHighFrameRate = m_needsDisplayRefreshCallbacksForMonotonicAnimations
        || (m_needsDisplayRefreshCallbacksForDrawing && !preferPageRenderingUpdatesNear60FPSEnabled());
    [displayLinkHandler() setWantsHighFrameRate:wantsHighFrameRate];
    [displayLinkHandler() schedule];
}

void RemoteLayerTreeDrawingAreaProxyIOS::pauseDisplayLinkIfNeeded()
{
    if (!m_needsDisplayRefreshCallbacksForDrawing && !m_needsDisplayRefreshCallbacksForMonotonicAnimations)
        [displayLinkHandler() pause];
}

std::optional<WebCore::FramesPerSecond> RemoteLayerTreeDrawingAreaProxyIOS::displayNominalFramesPerSecond()
{
    return [displayLinkHandler() nominalFramesPerSecond];
}

void RemoteLayerTreeDrawingAreaProxyIOS::windowScreenDidChange(WebCore::PlatformDisplayID)
{
    [m_displayLinkHandler windowScreenDidChange];
}

UIView *RemoteLayerTreeDrawingAreaProxyIOS::viewWithLayerIDForTesting(WebCore::PlatformLayerIdentifier layerID) const
{
    if (RefPtr node = m_remoteLayerTreeHost->nodeForID(layerID))
        return node->uiView();
    return nil;
}

} // namespace WebKit

#endif // PLATFORM(IOS_FAMILY)
