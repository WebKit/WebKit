/*
 * Copyright (C) 2006 Apple Computer, Inc.  All rights reserved.
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

#include <CoreFoundation/CoreFoundation.h>
#include <wtf/Assertions.h>

namespace WebCore {

static CFRunLoopTimerRef sharedTimer;
static void (*sharedTimerFiredFunction)();

void setSharedTimerFiredFunction(void (*f)())
{
    ASSERT(!sharedTimerFiredFunction || sharedTimerFiredFunction == f);

    sharedTimerFiredFunction = f;
}

static void timerFired(CFRunLoopTimerRef, void*)
{
    sharedTimerFiredFunction();
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
    CFRunLoopAddTimer(CFRunLoopGetCurrent(), sharedTimer, kCFRunLoopDefaultMode);
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
