/*
 * Copyright (C) 2010, 2014 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#import "config.h"
#import "DisplayRefreshMonitorIOS.h"

#if USE(REQUEST_ANIMATION_FRAME_DISPLAY_MONITOR) && PLATFORM(IOS_FAMILY)

#import "WebCoreThread.h"
#import <QuartzCore/CADisplayLink.h>
#import <wtf/MainThread.h>

using WebCore::DisplayRefreshMonitorIOS;
@interface WebDisplayLinkHandler : NSObject
{
    DisplayRefreshMonitorIOS* m_monitor;
    CADisplayLink *m_displayLink;
}

- (id)initWithMonitor:(DisplayRefreshMonitorIOS*)monitor;
- (void)handleDisplayLink:(CADisplayLink *)sender;
- (void)invalidate;

@end

@implementation WebDisplayLinkHandler

- (id)initWithMonitor:(DisplayRefreshMonitorIOS*)monitor
{
    if (self = [super init]) {
        m_monitor = monitor;
        // Note that CADisplayLink retains its target (self), so a call to -invalidate is needed on teardown.
        m_displayLink = [CADisplayLink displayLinkWithTarget:self selector:@selector(handleDisplayLink:)];
        [m_displayLink addToRunLoop:WebThreadNSRunLoop() forMode:NSDefaultRunLoopMode];
        m_displayLink.preferredFramesPerSecond = 60;
    }
    return self;
}

- (void)dealloc
{
    ASSERT(!m_displayLink); // -invalidate should have been called already.
    [super dealloc];
}

- (void)handleDisplayLink:(CADisplayLink *)sender
{
    UNUSED_PARAM(sender);
    ASSERT(isMainThread());
    m_monitor->displayLinkFired();
}

- (void)invalidate
{
    [m_displayLink invalidate];
    m_displayLink = nullptr;
}

@end

namespace WebCore {

DisplayRefreshMonitorIOS::DisplayRefreshMonitorIOS(PlatformDisplayID displayID)
    : DisplayRefreshMonitor(displayID)
{
}

DisplayRefreshMonitorIOS::~DisplayRefreshMonitorIOS()
{
    [m_handler invalidate];
}

bool DisplayRefreshMonitorIOS::requestRefreshCallback()
{
    if (!isActive())
        return false;

    if (!m_handler) {
        m_handler = adoptNS([[WebDisplayLinkHandler alloc] initWithMonitor:this]);
        setIsActive(true);
    }

    setIsScheduled(true);
    return true;
}

void DisplayRefreshMonitorIOS::displayLinkFired()
{
    if (!isPreviousFrameDone())
        return;

    setIsPreviousFrameDone(false);
    handleDisplayRefreshedNotificationOnMainThread(this);
}

}

#endif // USE(REQUEST_ANIMATION_FRAME_DISPLAY_MONITOR) && PLATFORM(IOS_FAMILY)
