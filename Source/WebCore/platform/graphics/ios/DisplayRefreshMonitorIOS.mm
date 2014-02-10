/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
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
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "config.h"

#if USE(REQUEST_ANIMATION_FRAME_DISPLAY_MONITOR)

#include "DisplayRefreshMonitor.h"

#include <QuartzCore/QuartzCore.h>
#include <wtf/CurrentTime.h>
#include <wtf/MainThread.h>

#import "WebCoreThread.h"

@interface WebDisplayLinkHandler : NSObject
{
    WebCore::DisplayRefreshMonitor* m_monitor;
    CADisplayLink* m_displayLink;
}

- (id)initWithMonitor:(WebCore::DisplayRefreshMonitor*)monitor;
- (void)handleDisplayLink:(CADisplayLink *)sender;
- (void)invalidate;

@end

static double mediaTimeToCurrentTime(CFTimeInterval t)
{
    return monotonicallyIncreasingTime() + t - CACurrentMediaTime();
}

@implementation WebDisplayLinkHandler

- (id)initWithMonitor:(WebCore::DisplayRefreshMonitor*)monitor
{
    if (self = [super init]) {
        m_monitor = monitor;
        // Note that CADisplayLink retains its target (self), so a call to -invalidate is needed on teardown.
        m_displayLink = [CADisplayLink displayLinkWithTarget:self selector:@selector(handleDisplayLink:)];
        [m_displayLink addToRunLoop:WebThreadNSRunLoop() forMode:NSDefaultRunLoopMode];
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
    ASSERT(isMainThread());
    m_monitor->displayLinkFired(sender.timestamp);
}

- (void)invalidate
{
    [m_displayLink invalidate];
    m_displayLink = nullptr;
}

@end

namespace WebCore {
 
DisplayRefreshMonitor::~DisplayRefreshMonitor()
{
    [(WebDisplayLinkHandler*) m_displayLink invalidate];
    [(WebDisplayLinkHandler*) m_displayLink release];
}

bool DisplayRefreshMonitor::requestRefreshCallback()
{
    if (!m_active)
        return false;
        
    if (!m_displayLink) {
        m_displayLink = [[WebDisplayLinkHandler alloc] initWithMonitor:this];
        m_active = true;
    }

    m_scheduled = true;
    return true;
}

void DisplayRefreshMonitor::displayLinkFired(double nowSeconds)
{
    if (!m_previousFrameDone)
        return;

    m_previousFrameDone = false;
    m_monotonicAnimationStartTime = mediaTimeToCurrentTime(nowSeconds);

    handleDisplayRefreshedNotificationOnMainThread(this);
}

}

#endif // USE(REQUEST_ANIMATION_FRAME_DISPLAY_MONITOR)
