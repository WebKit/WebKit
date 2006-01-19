/*
 * Copyright (C) 2004, 2005 Apple Computer, Inc.  All rights reserved.
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
#include "KWQObject.h"

#include <kxmlcore/Assertions.h>

struct KWQObjectTimer {
    QObject *target;
    int timerId;
    CFRunLoopTimerRef runLoopTimer; // non-0 for running timers
    bool deferred; // true if timer is in the deferredTimers array
    bool deleted;
};

const QObject *QObject::_sender;
bool QObject::_defersTimers;

static CFMutableDictionaryRef timerDictionaries;
static CFMutableArrayRef deferredTimers;
static CFRunLoopTimerRef sendDeferredTimerEventsTimer;
static int lastTimerIdUsed;

QObject::QObject(QObject *parent, const char *name)
    : _signalListHead(0), _signalsBlocked(false)
    , _destroyed(this, SIGNAL(destroyed()))
    , _eventFilterObject(0)
{
    _guardedPtrDummyList.append(this);
}

QObject::~QObject()
{
    _destroyed.call();
    ASSERT(_signalListHead == &_destroyed);
    killTimers();
}

KWQSignal *QObject::findSignal(const char *signalName) const
{
    for (KWQSignal *signal = _signalListHead; signal; signal = signal->_next)
        if (KWQNamesMatch(signalName, signal->_name))
            return signal;
    return 0;
}

void QObject::connect(const QObject *sender, const char *signalName, const QObject *receiver, const char *member)
{
    // FIXME: Assert that sender is not 0 rather than doing the if statement, then fix callers who call with 0.
    if (!sender)
        return;
    
    KWQSignal *signal = sender->findSignal(signalName);
    if (!signal) {
#if !ERROR_DISABLED
        if (1
            && !KWQNamesMatch(member, SIGNAL(setStatusBarText(const QString &)))
            && !KWQNamesMatch(member, SLOT(slotHistoryChanged()))
            && !KWQNamesMatch(member, SLOT(slotJobPercent(KIO::Job *, unsigned long)))
            && !KWQNamesMatch(member, SLOT(slotJobSpeed(KIO::Job *, unsigned long)))
            && !KWQNamesMatch(member, SLOT(slotScrollBarMoved()))
            && !KWQNamesMatch(member, SLOT(slotShowDocument(const QString &, const QString &)))
            && !KWQNamesMatch(member, SLOT(slotViewCleared())) // FIXME: Should implement this one!
            )
        ERROR("connecting member %s to signal %s, but that signal was not found", member, signalName);
#endif
        return;
    }
    signal->connect(KWQSlot(const_cast<QObject *>(receiver), member));
}

void QObject::disconnect(const QObject *sender, const char *signalName, const QObject *receiver, const char *member)
{
    // FIXME: Assert that sender is not 0 rather than doing the if statement, then fix callers who call with 0.
    if (!sender)
        return;
    
    KWQSignal *signal = sender->findSignal(signalName);
    if (!signal) {
        // FIXME: Put a call to ERROR here and clean up callers who do this.
        return;
    }
    signal->disconnect(KWQSlot(const_cast<QObject *>(receiver), member));
}

void QObject::timerEvent(QTimerEvent *)
{
}

bool QObject::event(QEvent *)
{
    return false;
}

static void timerFired(CFRunLoopTimerRef runLoopTimer, void *info)
{
    KWQObjectTimer *timer = static_cast<KWQObjectTimer *>(info);
    if (QObject::defersTimers()) {
        if (!timer->deferred) {
            if (!deferredTimers)
                deferredTimers = CFArrayCreateMutable(0, 0, 0);
            CFArrayAppendValue(deferredTimers, timer);
            timer->deferred = true;
        }
    } else {
        QTimerEvent event(timer->timerId);
        timer->target->timerEvent(&event);
    }
}

int QObject::startTimer(int interval)
{
    int timerId = ++lastTimerIdUsed;
    restartTimer(timerId, interval, interval);
    return timerId;
}

void QObject::restartTimer(int timerId, int nextFireInterval, int repeatInterval)
{
    ASSERT(timerId > 0);
    ASSERT(timerId <= lastTimerIdUsed);

    if (!timerDictionaries)
        timerDictionaries = CFDictionaryCreateMutable(0, 0, 0, &kCFTypeDictionaryValueCallBacks);

    CFMutableDictionaryRef timers = (CFMutableDictionaryRef)CFDictionaryGetValue(timerDictionaries, this);
    if (!timers) {
        timers = CFDictionaryCreateMutable(0, 0, 0, 0);
        CFDictionarySetValue(timerDictionaries, this, timers);
        CFRelease(timers);
    }

    ASSERT(!CFDictionaryGetValue(timers, reinterpret_cast<void *>(timerId)));

    KWQObjectTimer *timer = static_cast<KWQObjectTimer *>(fastMalloc(sizeof(KWQObjectTimer)));
    timer->target = this;
    timer->timerId = timerId;
    CFRunLoopTimerContext context = { 0, timer, 0, 0, 0 };
    CFRunLoopTimerRef runLoopTimer = CFRunLoopTimerCreate(0, CFAbsoluteTimeGetCurrent() + nextFireInterval * 0.001,
        repeatInterval * 0.001, 0, 0, timerFired, &context);
    timer->runLoopTimer = runLoopTimer;
    timer->deferred = false;
    timer->deleted = false;

    CFDictionarySetValue(timers, reinterpret_cast<void *>(timerId), timer);

    CFRunLoopAddTimer(CFRunLoopGetCurrent(), runLoopTimer, kCFRunLoopDefaultMode);
}

void QObject::timerIntervals(int timerId, int& nextFireInterval, int& repeatInterval) const
{
    nextFireInterval = -1;
    repeatInterval = -1;
    if (!timerDictionaries)
        return;
    CFMutableDictionaryRef timers = (CFMutableDictionaryRef)CFDictionaryGetValue(timerDictionaries, this);
    if (!timers)
        return;
    KWQObjectTimer *timer = (KWQObjectTimer *)CFDictionaryGetValue(timers, reinterpret_cast<void *>(timerId));
    if (!timer)
        return;
    nextFireInterval = (int)((CFRunLoopTimerGetNextFireDate(timer->runLoopTimer) - CFAbsoluteTimeGetCurrent()) * 1000);
    repeatInterval = (int)(CFRunLoopTimerGetInterval(timer->runLoopTimer) * 1000);
}

static void deleteTimer(KWQObjectTimer *timer)
{
    CFRunLoopTimerInvalidate(timer->runLoopTimer);
    CFRelease(timer->runLoopTimer);

    if (timer->deferred)
        timer->deleted = true;
    else
        fastFree(timer);
}

void QObject::killTimer(int timerId)
{
    if (!timerDictionaries)
        return;
    CFMutableDictionaryRef timers = (CFMutableDictionaryRef)CFDictionaryGetValue(timerDictionaries, this);
    if (!timers)
        return;
    KWQObjectTimer *timer = (KWQObjectTimer *)CFDictionaryGetValue(timers, reinterpret_cast<void *>(timerId));
    if (!timer)
        return;
    deleteTimer(timer);
    CFDictionaryRemoveValue(timers, reinterpret_cast<void *>(timerId));
}

static void deleteOneTimer(const void *key, const void *value, void *context)
{
    deleteTimer((KWQObjectTimer *)value);
}

void QObject::killTimers()
{
    if (!timerDictionaries)
        return;
    CFMutableDictionaryRef timers = (CFMutableDictionaryRef)CFDictionaryGetValue(timerDictionaries, this);
    if (!timers)
        return;
    CFDictionaryApplyFunction(timers, deleteOneTimer, 0);
    CFDictionaryRemoveValue(timerDictionaries, this);
}

static void sendDeferredTimerEvent(const void *value, void *context)
{
    KWQObjectTimer *timer = (KWQObjectTimer *)value;
    if (!timer)
        return;
    if (timer->deleted) {
        fastFree(timer);
        return;
    }
    QTimerEvent event(timer->timerId);
    timer->target->timerEvent(&event);
    if (timer->deleted) {
        fastFree(timer);
        return;
    }
    timer->deferred = false;
}

static void sendDeferredTimerEvents(CFRunLoopTimerRef, void *)
{
    CFRelease(sendDeferredTimerEventsTimer);
    sendDeferredTimerEventsTimer = 0;

    CFArrayRef timers = deferredTimers;
    deferredTimers = 0;

    if (timers) {
        CFArrayApplyFunction(timers, CFRangeMake(0, CFArrayGetCount(timers)), sendDeferredTimerEvent, 0);
        CFRelease(timers);
    }
}

void QObject::setDefersTimers(bool defers)
{
    if (defers) {
        _defersTimers = true;
        if (sendDeferredTimerEventsTimer) {
            CFRunLoopTimerInvalidate(sendDeferredTimerEventsTimer);
            CFRelease(sendDeferredTimerEventsTimer);
            sendDeferredTimerEventsTimer = 0;
        }
        return;
    }
    
    if (_defersTimers) {
        _defersTimers = false;
        ASSERT(!sendDeferredTimerEventsTimer);
        sendDeferredTimerEventsTimer = CFRunLoopTimerCreate(0, CFAbsoluteTimeGetCurrent(),
            0, 0, 0, sendDeferredTimerEvents, 0);
        CFRunLoopAddTimer(CFRunLoopGetCurrent(), sendDeferredTimerEventsTimer, kCFRunLoopDefaultMode);
    }
}

bool QObject::inherits(const char *className) const
{
    if (strcmp(className, "Frame") == 0) {
        return isFrame();
    }
    if (strcmp(className, "FrameView") == 0) {
        return isFrameView();
    }
    if (strcmp(className, "KParts::Factory") == 0) {
        return false;
    }
    if (strcmp(className, "ObjectContents") == 0) {
        return isObjectContents();
    }
    if (strcmp(className, "QFrame") == 0) {
        return isQFrame();
    }
    if (strcmp(className, "QScrollView") == 0) {
        return isQScrollView();
    }
    ERROR("class name %s not recognized", className);
    return false;
}

bool QObject::isKHTMLLoader() const
{
    return false;
}

bool QObject::isFrame() const
{
    return false;
}

bool QObject::isFrameView() const
{
    return false;
}

bool QObject::isObjectContents() const
{
    return false;
}

bool QObject::isQFrame() const
{
    return false;
}

bool QObject::isQScrollView() const
{
    return false;
}
