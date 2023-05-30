/*
 * Copyright (C) 2012 Apple Inc. All rights reserved.
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
#import "WebProcessProxy.h"

#if PLATFORM(IOS_FAMILY)

#import "AccessibilitySupportSPI.h"
#import "EventDispatcherMessages.h"
#import "WKFullKeyboardAccessWatcher.h"
#import "WKMouseDeviceObserver.h"
#import "WKStylusDeviceObserver.h"
#import "WebProcessMessages.h"
#import "WebProcessPool.h"
#import <QuartzCore/CADisplayLink.h>
#import <pal/system/cocoa/SleepDisablerCocoa.h>

@interface WKProcessProxyDisplayLinkHandler : NSObject {
    WebKit::WebProcessProxy* _webProcessProxy;
    CADisplayLink *_displayLink;
}

- (id)initWithWebProcessProxy:(WebKit::WebProcessProxy*)webProcessProxy;
- (void)setPreferredFramesPerSecond:(NSInteger)preferredFramesPerSecond;
- (NSInteger)preferredFramesPerSecond;
- (void)displayLinkFired:(CADisplayLink *)sender;
- (void)invalidate;
- (void)schedule;
- (void)pause;

@end

@implementation WKProcessProxyDisplayLinkHandler

- (id)initWithWebProcessProxy:(WebKit::WebProcessProxy*)webProcessProxy
{
    if (self = [super init]) {
        _webProcessProxy = webProcessProxy;
        // Note that CADisplayLink retains its target (self), so a call to -invalidate is needed on teardown.
        _displayLink = [CADisplayLink displayLinkWithTarget:self selector:@selector(displayLinkFired:)];
        [_displayLink addToRunLoop:[NSRunLoop mainRunLoop] forMode:NSRunLoopCommonModes];
        _displayLink.paused = YES;
        _displayLink.preferredFramesPerSecond = 60;
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

- (NSInteger)preferredFramesPerSecond
{
    return _displayLink.preferredFramesPerSecond;
}

- (void)displayLinkFired:(CADisplayLink *)sender
{
    ASSERT(isUIThread());
    _webProcessProxy->displayLinkFired();
}

- (void)invalidate
{
    [_displayLink invalidate];
    _displayLink = nullptr;
}

- (void)schedule
{
    _displayLink.paused = NO;
}

- (void)pause
{
    _displayLink.paused = YES;
}

@end

namespace WebKit {

void WebProcessProxy::platformInitialize()
{
#if HAVE(MOUSE_DEVICE_OBSERVATION)
    [[WKMouseDeviceObserver sharedInstance] start];
#endif
#if HAVE(STYLUS_DEVICE_OBSERVATION)
    [[WKStylusDeviceObserver sharedInstance] start];
#endif

    static bool didSetScreenWakeLockHandler = false;
    if (!didSetScreenWakeLockHandler) {
        didSetScreenWakeLockHandler = true;
        PAL::SleepDisablerCocoa::setScreenWakeLockHandler([](bool shouldKeepScreenAwake) {
            RefPtr<WebPageProxy> visiblePage;
            for (auto& page : globalPageMap().values()) {
                if (!visiblePage)
                    visiblePage = page.get();
                else if (page->isViewVisible()) {
                    visiblePage = page.get();
                    break;
                }
            }
            if (!visiblePage) {
                ASSERT_NOT_REACHED();
                return false;
            }
            return visiblePage->uiClient().setShouldKeepScreenAwake(shouldKeepScreenAwake);
        });
    }

    m_throttler.setAllowsActivities(!m_processPool->processesShouldSuspend());
}

void WebProcessProxy::platformDestroy()
{
#if HAVE(MOUSE_DEVICE_OBSERVATION)
    [[WKMouseDeviceObserver sharedInstance] stop];
#endif
#if HAVE(STYLUS_DEVICE_OBSERVATION)
    [[WKStylusDeviceObserver sharedInstance] stop];
#endif

    if (m_displayLinkHandler)
        [m_displayLinkHandler invalidate];
}

bool WebProcessProxy::fullKeyboardAccessEnabled()
{
#if ENABLE(FULL_KEYBOARD_ACCESS)
    return [WKFullKeyboardAccessWatcher fullKeyboardAccessEnabled];
#else
    return NO;
#endif
}

WKProcessProxyDisplayLinkHandler *WebProcessProxy::displayLinkHandler()
{
    if (!m_displayLinkHandler) {
        m_displayLinkHandler = adoptNS([[WKProcessProxyDisplayLinkHandler alloc] initWithWebProcessProxy:this]);
        m_nextDisplayUpdate = { 0, 60 };
    }
    return m_displayLinkHandler.get();
}

void WebProcessProxy::startDisplayLink(DisplayLinkObserverID observerID, WebCore::PlatformDisplayID displayID, WebCore::FramesPerSecond framesPerSecond, bool)
{
    auto addResult = m_displayLinkObservers.set(observerID, DisplayLinkObserverData { displayID, framesPerSecond });
    ASSERT_UNUSED(addResult, addResult.isNewEntry);
    WebCore::FramesPerSecond maxFramesPerSecond = 0;
    for (auto& data : m_displayLinkObservers)
        maxFramesPerSecond = std::max(data.value.framesPerSecond, maxFramesPerSecond);

    if (maxFramesPerSecond != [displayLinkHandler() preferredFramesPerSecond]) {
        [displayLinkHandler() setPreferredFramesPerSecond:maxFramesPerSecond];
        m_nextDisplayUpdate = { 0, maxFramesPerSecond };
    }
    [displayLinkHandler() schedule];
}

void WebProcessProxy::stopDisplayLink(DisplayLinkObserverID observerID, WebCore::PlatformDisplayID, bool)
{
    m_displayLinkObservers.remove(observerID);
    if (m_displayLinkObservers.isEmpty())
        [displayLinkHandler() pause];
}

void WebProcessProxy::setDisplayLinkPreferredFramesPerSecond(DisplayLinkObserverID observerID, WebCore::PlatformDisplayID, WebCore::FramesPerSecond preferredFramesPerSecond)
{
    auto it = m_displayLinkObservers.find(observerID);
    if (it == m_displayLinkObservers.end())
        return;
    it->value.framesPerSecond = preferredFramesPerSecond;

    WebCore::FramesPerSecond maxFramesPerSecond = 0;
    for (auto& data : m_displayLinkObservers)
        maxFramesPerSecond = std::max(data.value.framesPerSecond, maxFramesPerSecond);

    if (maxFramesPerSecond != [displayLinkHandler() preferredFramesPerSecond]) {
        [displayLinkHandler() setPreferredFramesPerSecond:maxFramesPerSecond];
        m_nextDisplayUpdate = { 0, maxFramesPerSecond };
    }
}

void WebProcessProxy::displayLinkFired()
{
    if (!connection())
        return;

    HashSet<WebCore::PlatformDisplayID> visitedDisplayIDs;
    for (auto& data : m_displayLinkObservers) {
        auto addResult = visitedDisplayIDs.add(data.value.displayID);
        if (addResult.isNewEntry)
            connection()->send(Messages::EventDispatcher::DisplayDidRefresh(data.value.displayID, m_nextDisplayUpdate, false, true), 0, { }, Thread::QOS::UserInteractive);
    }
    m_nextDisplayUpdate = m_nextDisplayUpdate.nextUpdate();
}

} // namespace WebKit

#endif // PLATFORM(IOS_FAMILY)
