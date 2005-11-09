/*
 * Copyright (C) 2004 Apple Computer, Inc.  All rights reserved.
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
#import "KWQObject.h"

#import "KWQVariant.h"
#import <kxmlcore/Assertions.h>

// The Foundation-level Cocoa calls here (NSTimer, NSDate, NSArray,
// NSDictionary) should be exception-free, so no need to block
// exceptions.

const QObject *QObject::_sender;
bool QObject::_defersTimers;

static CFMutableDictionaryRef timerDictionaries;
static CFMutableDictionaryRef allPausedTimers;
static NSMutableArray *deferredTimers;
static bool deferringTimers;

@interface KWQObjectTimerTarget : NSObject
{
@public
    QObject *target;
    int timerId;
    NSTimeInterval remainingTime;
}

- initWithQObject:(QObject *)object timerId:(int)timerId;
- (void)timerFired;

@end

KWQSignal *QObject::findSignal(const char *signalName) const
{
    for (KWQSignal *signal = _signalListHead; signal; signal = signal->_next) {
        if (KWQNamesMatch(signalName, signal->_name)) {
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
    : _savedSender(QObject::_sender)
{
    QObject::_sender = o;
}

KWQObjectSenderScope::~KWQObjectSenderScope()
{
    QObject::_sender = _savedSender;
}

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

void QObject::timerEvent(QTimerEvent *te)
{
}

bool QObject::event(QEvent *)
{
    return false;
}

void QObject::pauseTimer (int _timerId, const void *key)
{
    NSMutableDictionary *timers = (NSMutableDictionary *)CFDictionaryGetValue(timerDictionaries, this);
    NSNumber *timerId = [NSNumber numberWithInt:_timerId];
    NSTimer *timer = (NSTimer *)[timers objectForKey:timerId];

    if ([timer isValid]){
        KWQObjectTimerTarget *target = (KWQObjectTimerTarget *)[timer userInfo];

        if (target){
            NSDate *fireDate = [timer fireDate];
            NSTimeInterval remainingTime = [fireDate timeIntervalSinceDate: [NSDate date]];
        
            if (remainingTime < 0)
                remainingTime = DBL_EPSILON;
            
            if (allPausedTimers == NULL) {
                // The global targets dictionary itself leaks, but the contents are removed
                // when each timer fires or is killed.
                allPausedTimers = CFDictionaryCreateMutable(NULL, 0, NULL, &kCFTypeDictionaryValueCallBacks);
            }
    
            NSMutableArray *pausedTimers = (NSMutableArray *)CFDictionaryGetValue(allPausedTimers, key);
            if (pausedTimers == nil) {
                pausedTimers = [[NSMutableArray alloc] init];
                CFDictionarySetValue(allPausedTimers, key, pausedTimers);
                [pausedTimers release];
            }
            
            target->remainingTime = remainingTime;
            [pausedTimers addObject:target];
                    
            [timer invalidate];
            [timers removeObjectForKey:timerId];
        }
    }
}

void QObject::_addTimer(NSTimer *timer, int _timerId)
{
    NSMutableDictionary *timers = (NSMutableDictionary *)CFDictionaryGetValue(timerDictionaries, this);
    if (timers == nil) {
        timers = [[NSMutableDictionary alloc] init];
        CFDictionarySetValue(timerDictionaries, this, timers);
        [timers release];
    }
    [timers setObject:timer forKey:[NSNumber numberWithInt:_timerId]];
}

static int nextTimerID = 1;

void QObject::clearPausedTimers (const void *key)
{
    if (allPausedTimers)
        CFDictionaryRemoveValue(allPausedTimers, key);
}

void QObject::resumeTimers (const void *key, QObject *_target)
{
    if (allPausedTimers == NULL) {
        return;
    }
    
    int maxId = MAX(0, nextTimerID);
        
    NSMutableArray *pausedTimers = (NSMutableArray *)CFDictionaryGetValue(allPausedTimers, key);
    if (pausedTimers == nil)
        return;

    int count = [pausedTimers count];
    while (count--){
        KWQObjectTimerTarget *target = [pausedTimers objectAtIndex: count];
        target->target = _target;
        NSTimer *timer = [NSTimer scheduledTimerWithTimeInterval:target->remainingTime
            target:target
            selector:@selector(timerFired)
            userInfo:target
            repeats:YES];
        [pausedTimers removeLastObject];

        maxId = MAX (maxId, target->timerId);
                        
        _addTimer (timer, target->timerId);
    }
    nextTimerID = maxId+1;
    
    CFDictionaryRemoveValue(allPausedTimers, key);
}

int QObject::startTimer(int milliseconds)
{
    if (timerDictionaries == NULL) {
        // The global timers dictionary itself lives forever, but the contents are removed
        // when each timer fires or is killed.
        timerDictionaries = CFDictionaryCreateMutable(NULL, 0, NULL, &kCFTypeDictionaryValueCallBacks);
    }
    
    NSMutableDictionary *timers = (NSMutableDictionary *)CFDictionaryGetValue(timerDictionaries, this);
    if (timers == nil) {
        timers = [[NSMutableDictionary alloc] init];
        CFDictionarySetValue(timerDictionaries, this, timers);
        [timers release];
    }

    KWQObjectTimerTarget *target = [[KWQObjectTimerTarget alloc] initWithQObject:this timerId:nextTimerID];
    NSTimer *timer = [NSTimer scheduledTimerWithTimeInterval:milliseconds / 1000.0
          target:target
        selector:@selector(timerFired)
        userInfo:target
         repeats:YES];
    [target release];
    
    _addTimer (timer, nextTimerID);
    
    return nextTimerID++;    
}

void QObject::killTimer(int _timerId)
{
    if (_timerId == 0) {
        return;
    }
    if (timerDictionaries == NULL) {
        return;
    }
    NSMutableDictionary *timers = (NSMutableDictionary *)CFDictionaryGetValue(timerDictionaries, this);
    NSNumber *timerId = [NSNumber numberWithInt:_timerId];
    NSTimer *timer = (NSTimer *)[timers objectForKey:timerId];
    // Only try to remove the timer is it hasn't fired (and is therefore valid).  It is NOT
    // permissible to reference a timer's userInfo if it is invalid.
    if ([timer isValid]){
        [deferredTimers removeObject:(KWQObjectTimerTarget *)[timer userInfo]];
        [timer invalidate];
    }
    [timers removeObjectForKey:timerId];
}

void QObject::killTimers()
{
    if (timerDictionaries == NULL) {
        return;
    }
    NSMutableDictionary *timers = (NSMutableDictionary *)CFDictionaryGetValue(timerDictionaries, this);
    if (timers == nil) {
        return;
    }
    NSEnumerator *e = [timers keyEnumerator];
    NSNumber *timerId;
    while ((timerId = [e nextObject]) != nil) {
	killTimer([timerId intValue]);
    }

    CFDictionaryRemoveValue(timerDictionaries, this);
}

void QObject::setDefersTimers(bool defers)
{
    if (defers) {
        _defersTimers = true;
        deferringTimers = true;
        [NSObject cancelPreviousPerformRequestsWithTarget:[KWQObjectTimerTarget class]];
        return;
    }
    
    if (_defersTimers) {
        _defersTimers = false;
        if (deferringTimers) {
            [KWQObjectTimerTarget performSelector:@selector(stopDeferringTimers) withObject:nil afterDelay:0];
        }
    }
}

@implementation KWQObjectTimerTarget

- initWithQObject:(QObject *)qo timerId:(int)t
{
    [super init];
    target = qo;
    timerId = t;
    return self;
}

- (void)sendTimerEvent
{
    QTimerEvent event(timerId);
    target->timerEvent(&event);
}

- (void)timerFired
{
    if (deferringTimers) {
        if (deferredTimers == nil) {
            deferredTimers = [[NSMutableArray alloc] init];
        }
	if (![deferredTimers containsObject:self]) {
	    [deferredTimers addObject:self];
	}
    } else {
        [self sendTimerEvent];
    }
}

+ (void)stopDeferringTimers
{
    ASSERT(deferringTimers);
    while ([deferredTimers count] != 0) {
	// remove before sending the timer event, in case the timer
	// callback cancels the timer - we don't want to remove too
	// much in that case.
	KWQObjectTimerTarget *timerTarget = [deferredTimers objectAtIndex:0];
	[timerTarget retain];
	[deferredTimers removeObjectAtIndex:0];
        [timerTarget sendTimerEvent];
	[timerTarget release];
    }

    deferringTimers = false;
}

@end

bool QObject::inherits(const char *className) const
{
    if (strcmp(className, "KHTMLPart") == 0) {
        return isKHTMLPart();
    }
    if (strcmp(className, "KHTMLView") == 0) {
        return isKHTMLView();
    }
    if (strcmp(className, "KParts::Factory") == 0) {
        return false;
    }
    if (strcmp(className, "KParts::ReadOnlyPart") == 0) {
        return isKPartsReadOnlyPart();
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

bool QObject::isKHTMLPart() const
{
    return false;
}

bool QObject::isKHTMLView() const
{
    return false;
}

bool QObject::isKPartsReadOnlyPart() const
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
