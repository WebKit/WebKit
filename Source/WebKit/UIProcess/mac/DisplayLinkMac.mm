/*
 * Copyright (C) 2018-2024 Apple Inc. All rights reserved.
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

#include "config.h"
#include "DisplayLink.h"

#if HAVE(CA_DISPLAY_LINK_MAC)

#include <dispatch/dispatch.h>
#include <pal/spi/cocoa/QuartzCoreSPI.h>
#include <wtf/OSObjectPtr.h>
#include <wtf/ProcessPrivilege.h>
#include <wtf/text/TextStream.h>

@interface WKDisplayLinkHandler : NSObject {
    WebKit::DisplayLink* _client;
    RetainPtr<CADisplayLink> _displayLink;
    RetainPtr<CFRunLoopRef> _runLoop;
    OSObjectPtr<dispatch_semaphore_t> _runLoopSemaphore;
    RetainPtr<NSThread> _thread;
}

- (id)initWithScreen:(NSScreen *)screen client:(WebKit::DisplayLink*)client;
- (void)invalidate;
- (WebCore::FramesPerSecond)nominalFramesPerSecond;

@property (getter=isRunning) BOOL running;

@end

@implementation WKDisplayLinkHandler

- (id)initWithScreen:(NSScreen *)screen client:(WebKit::DisplayLink*)client
{
    if (self = [super init]) {
        _client = client;

        _displayLink = [screen displayLinkWithTarget:self selector:@selector(displayLinkFired:)];
        [_displayLink setPaused:YES];

        _runLoopSemaphore = dispatch_semaphore_create(0);

        _thread = adoptNS([[NSThread alloc] initWithTarget:self selector:@selector(threadWasCreated) object:nil]);
        [_thread setQualityOfService:NSQualityOfServiceUserInteractive];
        [_thread start];
    }
    return self;
}

- (void)threadWasCreated
{
    @autoreleasepool {
        [_displayLink addToRunLoop:[NSRunLoop currentRunLoop] forMode:NSDefaultRunLoopMode];
        _runLoop = CFRunLoopGetCurrent();
        dispatch_semaphore_signal(_runLoopSemaphore.get());
    }

    CFRunLoopRunResult result = kCFRunLoopRunHandledSource;
    while (result != kCFRunLoopRunStopped && result != kCFRunLoopRunFinished) {
        @autoreleasepool {
            result = CFRunLoopRunInMode(kCFRunLoopDefaultMode, 1.0e10, true);
        }
    }

    @autoreleasepool {
        [_displayLink invalidate];
    }
}

- (void)displayLinkFired:(CADisplayLink *)sender
{
    ASSERT(!isMainRunLoop());
    _client->displayLinkHandlerCallbackFired();
}

- (void)invalidate
{
    if ([_thread isCancelled])
        return;

    [_thread cancel];

    // Shouldn't be necesssary, but pause this just in case CFRunLoopStop doesn't successfully stop
    // the run loop.
    [_displayLink setPaused:YES];

    dispatch_semaphore_wait(_runLoopSemaphore.get(), DISPATCH_TIME_FOREVER);
    CFRunLoopStop(_runLoop.get());
}

- (BOOL)isRunning
{
    return _displayLink && ![_displayLink isPaused];
}

- (void)setRunning:(BOOL)running
{
    [_displayLink setPaused:!running];
}

- (WebCore::FramesPerSecond)nominalFramesPerSecond
{
    return (1.0 / [_displayLink maximumRefreshRate]);
}

@end

namespace WebKit {

using namespace WebCore;

void DisplayLink::platformInitialize()
{
    ASSERT(hasProcessPrivilege(ProcessPrivilege::CanCommunicateWithWindowServer));
    m_displayLinkHandler = adoptNS([[WKDisplayLinkHandler alloc] initWithScreen:WebCore::screen(displayID()) client:this]);
    m_displayNominalFramesPerSecond = [m_displayLinkHandler nominalFramesPerSecond];
}

void DisplayLink::platformFinalize()
{
    ASSERT(hasProcessPrivilege(ProcessPrivilege::CanCommunicateWithWindowServer));
    ASSERT(m_displayLinkHandler);
    if (!m_displayLinkHandler)
        return;

    [m_displayLinkHandler invalidate];
    m_displayLinkHandler = nil;
}

bool DisplayLink::platformIsRunning() const
{
    return [m_displayLinkHandler isRunning];
}

void DisplayLink::platformStart()
{
    [m_displayLinkHandler setRunning:YES];
}

void DisplayLink::platformStop()
{
    [m_displayLinkHandler setRunning:NO];
}

void DisplayLink::displayLinkHandlerCallbackFired()
{
    notifyObserversDisplayDidRefresh();
}

} // namespace WebKit

#endif // HAVE(CA_DISPLAY_LINK_MAC)
