/*
 * Copyright (C) 2003 Apple Computer, Inc.  All rights reserved.
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

#import "KWQTimer.h"

#import "KWQAssertions.h"

// We know the Cocoa calls in this file are safe because they are all
// to the simple ObjC class defined here, or simple NSTimer calls that
// can't throw.

@interface KWQTimerTarget : NSObject
{
    QTimer *timer;
}
+ (KWQTimerTarget *)targetWithQTimer:(QTimer *)timer;
- (void)timerFired:(id)userInfo;
@end

@interface KWQSingleShotTimerTarget : NSObject
{
    KWQSlot *slot;
}
+ (KWQSingleShotTimerTarget *)targetWithQObject:(QObject *)object member:(const char *)member;
- (void)timerFired:(id)userInfo;
@end

@implementation KWQTimerTarget

+ (KWQTimerTarget *)targetWithQTimer:(QTimer *)t
{
    KWQTimerTarget *target = [[[self alloc] init] autorelease];
    target->timer = t;
    return target;
}

- (void)timerFired:(id)userInfo
{
    timer->fire();
}

@end

@implementation KWQSingleShotTimerTarget

+ (KWQSingleShotTimerTarget *)targetWithQObject:(QObject *)object member:(const char *)member
{
    KWQSingleShotTimerTarget *target = [[[self alloc] init] autorelease];
    target->slot = new KWQSlot(object, member);
    return target;
}

- (void)dealloc
{
    delete slot;
    [super dealloc];
}

- (void)timerFired:(id)userInfo
{
    slot->call();
}

@end

QTimer::QTimer()
    : m_timer(nil), m_monitorFunction(0), m_timeoutSignal(this, SIGNAL(timeout()))
{
}

bool QTimer::isActive() const
{
    return m_timer;
}

void QTimer::start(int msec, bool singleShot)
{
    stop();
    m_timer = [[NSTimer scheduledTimerWithTimeInterval:(msec / 1000.0)
                                                target:[KWQTimerTarget targetWithQTimer:this]
                                              selector:@selector(timerFired:)
                                              userInfo:nil
                                               repeats:!singleShot] retain];

    if (m_monitorFunction) {
        m_monitorFunction(m_monitorFunctionContext);
    }
}

void QTimer::stop()
{
    if (m_timer == nil) {
        return;
    }
    
    [m_timer invalidate];
    [m_timer release];
    m_timer = nil;

    if (m_monitorFunction) {
        m_monitorFunction(m_monitorFunctionContext);
    }
}

void QTimer::setMonitor(void (*monitorFunction)(void *context), void *context)
{
    ASSERT(!m_monitorFunction);
    m_monitorFunction = monitorFunction;
    m_monitorFunctionContext = context;
}

void QTimer::fire()
{
    m_timeoutSignal.call();

    if (![m_timer isValid]) {
        [m_timer release];
        m_timer = nil;
    }
}

void QTimer::singleShot(int msec, QObject *receiver, const char *member)
{
    [NSTimer scheduledTimerWithTimeInterval:(msec / 1000.0)
                                     target:[KWQSingleShotTimerTarget targetWithQObject:receiver member:member]
                                   selector:@selector(timerFired:)
                                   userInfo:nil
                                    repeats:NO];
}

