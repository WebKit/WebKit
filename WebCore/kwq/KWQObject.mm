/*
 * Copyright (C) 2001, 2002 Apple Computer, Inc.  All rights reserved.
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

#import <qobject.h>

#import <qvariant.h>
#import <qguardedptr.h>
#import <kwqdebug.h>
#import <KWQSignal.h>
#import <KWQSlot.h>

const QObject *QObject::m_sender;

KWQSignal *QObject::findSignal(const char *signalName) const
{
    for (KWQSignal *signal = m_signalListHead; signal; signal = signal->m_next) {
        if (KWQNamesMatch(signalName, signal->m_name)) {
            return signal;
        }
    }
    return 0;
}

void QObject::connect(const QObject *sender, const char *signalName, const QObject *receiver, const char *member)
{
    // FIXME: Assert that sender is not NULL rather than doing the if statement.
    if (!sender) {
        return;
    }
    
    KWQSignal *signal = sender->findSignal(signalName);
    if (!signal) {
        // FIXME: ERROR
        return;
    }
    signal->connect(KWQSlot(const_cast<QObject *>(receiver), member));
}

void QObject::disconnect(const QObject *sender, const char *signalName, const QObject *receiver, const char *member)
{
    // FIXME: Assert that sender is not NULL rather than doing the if statement.
    if (!sender)
        return;
    
    KWQSignal *signal = sender->findSignal(signalName);
    if (!signal) {
        // FIXME: ERROR
        return;
    }
    signal->disconnect(KWQSlot(const_cast<QObject *>(receiver), member));
}

KWQObjectSenderScope::KWQObjectSenderScope(const QObject *o)
    : m_savedSender(QObject::m_sender)
{
    QObject::m_sender = o;
}

KWQObjectSenderScope::~KWQObjectSenderScope()
{
    QObject::m_sender = m_savedSender;
}

QObject::QObject(QObject *parent, const char *name)
    : m_signalListHead(0), m_signalsBlocked(false), m_eventFilterObject(0)
{
    guardedPtrDummyList.append(this);
}

QObject::~QObject()
{
    KWQ_ASSERT(m_signalListHead == 0);
}

@interface KWQTimerCallback : NSObject
{
    QObject *target;
    int timerId;
}
- initWithQObject: (QObject *)object timerId: (int)timerId;
- (void)timerFired: (id)context;
@end

@implementation KWQTimerCallback
- initWithQObject: (QObject *)qo timerId: (int)t
{
    [super init];
    timerId = t;
    target = qo;
    return self;
}

- (void)timerFired: (id)context
{
    QTimerEvent te(timerId);
    target->timerEvent (&te);
}
@end

static NSMutableDictionary *timers;

void QObject::timerEvent(QTimerEvent *te)
{
}

int QObject::startTimer(int milliseconds)
{
    static int timerCount = 1;

    NSNumber *timerId = [NSNumber numberWithInt: timerCount];
    
    if (timers == nil) {
        // The global timers dictionary itself leaks, but the contents are removed
        // when a timer expires or is killed.
        timers = [[NSMutableDictionary alloc] init];
    }
    NSTimer *timer = [NSTimer scheduledTimerWithTimeInterval: ((NSTimeInterval)milliseconds)/1000
                target: [[[KWQTimerCallback alloc] initWithQObject: this timerId: timerCount] autorelease]
                selector: @selector(timerFired:)
                userInfo: timerId
                repeats: NO];
    [timers setObject: timer forKey: timerId];
        
    return timerCount++;    
}

void QObject::killTimer(int _timerId)
{
    NSNumber *timerId = [NSNumber numberWithInt: _timerId];
    NSTimer *timer;
    
    timer = (NSTimer *)[timers objectForKey: timerId];
    [timer invalidate];
    [timers removeObjectForKey: timerId];
}

void QObject::killTimers()
{
    NSArray *contexts;
    NSNumber *key;
    NSTimer *timer;
    int i, count;
    
    contexts = [timers allKeys];
    count = [contexts count];
    for (i = 0; i < count; i++){
        key = (NSNumber *)[contexts objectAtIndex: i];
        timer = (NSTimer *)[timers objectForKey: key];
        [timer invalidate];
        [timers removeObjectForKey: key];
    }
}

bool QObject::event(QEvent *)
{
    return false;
}

// special includes only for inherits

#import <khtml_part.h>
#import <khtmlview.h>

bool QObject::inherits(const char *className) const
{
    if (strcmp(className, "KHTMLPart") == 0) {
        return dynamic_cast<const KHTMLPart *>(this);
    }
    if (strcmp(className, "KHTMLView") == 0) {
        return dynamic_cast<const KHTMLView *>(this);
    }
    if (strcmp(className, "KParts::Factory") == 0) {
        return false;
    }
    if (strcmp(className, "KParts::ReadOnlyPart") == 0) {
        return dynamic_cast<const KParts::ReadOnlyPart *>(this);
    }
    if (strcmp(className, "QFrame") == 0) {
        return dynamic_cast<const QFrame *>(this);
    }
    if (strcmp(className, "QScrollView") == 0) {
        return dynamic_cast<const QScrollView *>(this);
    }
    // FIXME: ERROR here because we don't know the class name.
    return false;
}
