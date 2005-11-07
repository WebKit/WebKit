/*
 * Copyright (C) 2003, 2005 Apple Computer, Inc.  All rights reserved.
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

#include "config.h"
#import "KWQTimer.h"

#import <kxmlcore/Assertions.h>

static void timerFired(CFRunLoopTimerRef, void *info)
{
    ((QTimer *)info)->fire();
}

void QTimer::start(int msec, bool singleShot)
{
    stop();

    CFTimeInterval interval = msec * 0.001;
    CFRunLoopTimerContext context = { 0, this, 0, 0, 0 };
    m_runLoopTimer = CFRunLoopTimerCreate(0, CFAbsoluteTimeGetCurrent() + interval,
        singleShot ? 0 : interval, 0, 0, timerFired, &context);
    CFRunLoopAddTimer(CFRunLoopGetCurrent(), m_runLoopTimer, kCFRunLoopDefaultMode);

    if (m_monitorFunction)
        m_monitorFunction(m_monitorFunctionContext);
}

void QTimer::stop()
{
    if (!m_runLoopTimer)
        return;
    
    CFRunLoopTimerInvalidate(m_runLoopTimer);
    CFRelease(m_runLoopTimer);
    m_runLoopTimer = 0;

    if (m_monitorFunction)
        m_monitorFunction(m_monitorFunctionContext);
}

void QTimer::setMonitor(void (*monitorFunction)(void *context), void *context)
{
    ASSERT(!m_monitorFunction);
    m_monitorFunction = monitorFunction;
    m_monitorFunctionContext = context;
}

void QTimer::fire()
{
    if (m_runLoopTimer && !CFRunLoopTimerIsValid(m_runLoopTimer)) {
        CFRunLoopTimerInvalidate(m_runLoopTimer);
        CFRelease(m_runLoopTimer);
        m_runLoopTimer = 0;
    }

    // Note: This call may destroy the QTimer, so be sure not to touch any fields afterward.
    m_timeoutSignal.call();
}

static void deleteKWQSlot(const void *info)
{
    delete (KWQSlot *)info;
}

static void singleShotTimerFired(CFRunLoopTimerRef, void *info)
{
    ((KWQSlot *)info)->call();
}

void QTimer::singleShot(int msec, QObject *receiver, const char *member)
{
    CFRunLoopTimerContext context = { 0, new KWQSlot(receiver, member), 0, deleteKWQSlot, 0 };
    CFRunLoopTimerRef timer = CFRunLoopTimerCreate(0, CFAbsoluteTimeGetCurrent() + msec * 0.001,
        0, 0, 0, singleShotTimerFired, &context);
    CFRunLoopAddTimer(CFRunLoopGetCurrent(), timer, kCFRunLoopDefaultMode);
    CFRelease(timer);
}
