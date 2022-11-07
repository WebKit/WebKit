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

#import "RemoteScrollingCoordinatorProxyIOS.h"
#import "WebPageProxy.h"

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

@end

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
            _displayLink.paused = YES;

            if (drawingAreaProxy && !drawingAreaProxy->page().preferences().preferPageRenderingUpdatesNear60FPSEnabled())
                _displayLink.preferredFramesPerSecond = (1.0 / _displayLink.maximumRefreshRate);
            else
                _displayLink.preferredFramesPerSecond = 60;
        }

        if (drawingAreaProxy) {
            auto& page = drawingAreaProxy->page();
            if (page.preferences().webAnimationsCustomFrameRateEnabled() || !page.preferences().preferPageRenderingUpdatesNear60FPSEnabled()) {
                auto minimumRefreshInterval = _displayLink.maximumRefreshRate;
                if (minimumRefreshInterval > 0) {
                    if (auto displayID = page.displayID()) {
                        WebCore::FramesPerSecond frameRate = std::round(1.0 / minimumRefreshInterval);
                        page.windowScreenDidChange(*displayID, frameRate);
                    }
                }
            }
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

@end

namespace WebKit {
using namespace IPC;
using namespace WebCore;

RemoteLayerTreeDrawingAreaProxyIOS::RemoteLayerTreeDrawingAreaProxyIOS(WebPageProxy& pageProxy, WebProcessProxy& processProxy)
    : RemoteLayerTreeDrawingAreaProxy(pageProxy, processProxy)
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

void RemoteLayerTreeDrawingAreaProxyIOS::setPreferredFramesPerSecond(FramesPerSecond preferredFramesPerSecond)
{
    [displayLinkHandler() setPreferredFramesPerSecond:preferredFramesPerSecond];
}

void RemoteLayerTreeDrawingAreaProxyIOS::scheduleDisplayRefreshCallbacks()
{
    [displayLinkHandler() schedule];
}

void RemoteLayerTreeDrawingAreaProxyIOS::pauseDisplayRefreshCallbacks()
{
    [displayLinkHandler() pause];
}

} // namespace WebKit

#endif // PLATFORM(IOS_FAMILY)
