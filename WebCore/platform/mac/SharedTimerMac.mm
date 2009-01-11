/*
 * Copyright (C) 2006 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
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

#import "config.h"
#import "SharedTimer.h"

#import <wtf/Assertions.h>
#import <wtf/UnusedParam.h>

@class WebCorePowerNotifier;

namespace WebCore {

static WebCorePowerNotifier *powerNotifier;
static CFRunLoopTimerRef sharedTimer;
static void (*sharedTimerFiredFunction)();
static void timerFired(CFRunLoopTimerRef, void*);

}

@interface WebCorePowerNotifier : NSObject
@end

@implementation WebCorePowerNotifier

- (id)init
{
    self = [super init];
    
    if (self)
        [[[NSWorkspace sharedWorkspace] notificationCenter] addObserver:self
                                                               selector:@selector(didWake:)
                                                                   name:NSWorkspaceDidWakeNotification 
                                                                 object:nil];
    
    return self;
}

- (void)didWake:(NSNotification *)unusedNotification
{
    UNUSED_PARAM(unusedNotification);

    if (WebCore::sharedTimer) {
        WebCore::stopSharedTimer();
        WebCore::timerFired(0, 0);
    }
}

@end

namespace WebCore {

void setSharedTimerFiredFunction(void (*f)())
{
    ASSERT(!sharedTimerFiredFunction || sharedTimerFiredFunction == f);

    sharedTimerFiredFunction = f;
}

static void timerFired(CFRunLoopTimerRef, void*)
{
    // FIXME: We can remove this global catch-all if we fix <rdar://problem/5299018>.
    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
    sharedTimerFiredFunction();
    [pool drain];
}

void setSharedTimerFireTime(double fireTime)
{
    ASSERT(sharedTimerFiredFunction);

    if (sharedTimer) {
        CFRunLoopTimerInvalidate(sharedTimer);
        CFRelease(sharedTimer);
    }

    CFAbsoluteTime fireDate = fireTime - kCFAbsoluteTimeIntervalSince1970;
    sharedTimer = CFRunLoopTimerCreate(0, fireDate, 0, 0, 0, timerFired, 0);
    CFRunLoopAddTimer(CFRunLoopGetCurrent(), sharedTimer, kCFRunLoopCommonModes);
    
    if (!powerNotifier) {
        powerNotifier = [[WebCorePowerNotifier alloc] init];
        CFRetain(powerNotifier);
        [powerNotifier release];
    }
}

void stopSharedTimer()
{
    if (sharedTimer) {
        CFRunLoopTimerInvalidate(sharedTimer);
        CFRelease(sharedTimer);
        sharedTimer = 0;
    }
}

}
