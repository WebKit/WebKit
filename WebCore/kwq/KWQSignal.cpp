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
#import "KWQSignal.h"

#import "KWQObject.h"
#import <kxmlcore/Assertions.h>

using KIO::Job;

using khtml::CachedObject;
using khtml::DocLoader;

KWQSignal::KWQSignal(QObject *object, const char *name)
    : _object(object), _next(object->_signalListHead), _name(name)
{
    object->_signalListHead = this;
}

KWQSignal::~KWQSignal()
{
    KWQSignal **prev = &_object->_signalListHead;
    KWQSignal *signal;
    while ((signal = *prev)) {
        if (signal == this) {
            *prev = _next;
            break;
        }
        prev = &signal->_next;
    }
}

void KWQSignal::connect(const KWQSlot &slot)
{
#if !ERROR_DISABLED
    if (_slots.contains(slot)) {
        ERROR("connecting the same slot to a signal twice, %s", _name);
    }
#endif
    _slots.append(slot);
}

void KWQSignal::disconnect(const KWQSlot &slot)
{
#if !ERROR_DISABLED
    if (!_slots.contains(slot)
            && !KWQNamesMatch(_name, SIGNAL(finishedParsing()))
            && !KWQNamesMatch(_name, SIGNAL(requestDone(khtml::DocLoader *, khtml::CachedObject *)))
            && !KWQNamesMatch(_name, SIGNAL(requestFailed(khtml::DocLoader *, khtml::CachedObject *)))
            && !KWQNamesMatch(_name, SIGNAL(requestStarted(khtml::DocLoader *, khtml::CachedObject *)))
            ) {
        ERROR("disconnecting a signal that wasn't connected, %s", _name);
    }
#endif
    _slots.remove(slot);
}

void KWQSignal::call() const
{
    if (!_object->_signalsBlocked) {
        KWQObjectSenderScope senderScope(_object);
        QValueList<KWQSlot> copiedSlots(_slots);
        QValueListConstIterator<KWQSlot> end = copiedSlots.end();
        for (QValueListConstIterator<KWQSlot> it = copiedSlots.begin(); it != end; ++it) {
            (*it).call();
        }
    }
}

void KWQSignal::call(bool b) const
{
    if (!_object->_signalsBlocked) {
        KWQObjectSenderScope senderScope(_object);
        QValueList<KWQSlot> copiedSlots(_slots);
        QValueListConstIterator<KWQSlot> end = copiedSlots.end();
        for (QValueListConstIterator<KWQSlot> it = copiedSlots.begin(); it != end; ++it) {
            (*it).call(b);
        }
    }
}

void KWQSignal::call(int i) const
{
    if (!_object->_signalsBlocked) {
        KWQObjectSenderScope senderScope(_object);
        QValueList<KWQSlot> copiedSlots(_slots);
        QValueListConstIterator<KWQSlot> end = copiedSlots.end();
        for (QValueListConstIterator<KWQSlot> it = copiedSlots.begin(); it != end; ++it) {
            (*it).call(i);
        }
    }
}

void KWQSignal::call(const DOM::DOMString &s) const
{
    if (!_object->_signalsBlocked) {
        KWQObjectSenderScope senderScope(_object);
        QValueList<KWQSlot> copiedSlots(_slots);
        QValueListConstIterator<KWQSlot> end = copiedSlots.end();
        for (QValueListConstIterator<KWQSlot> it = copiedSlots.begin(); it != end; ++it) {
            (*it).call(s);
        }
    }
}

void KWQSignal::call(Job *j) const
{
    if (!_object->_signalsBlocked) {
        KWQObjectSenderScope senderScope(_object);
        QValueList<KWQSlot> copiedSlots(_slots);
        QValueListConstIterator<KWQSlot> end = copiedSlots.end();
        for (QValueListConstIterator<KWQSlot> it = copiedSlots.begin(); it != end; ++it) {
            (*it).call(j);
        }
    }
}

void KWQSignal::call(DocLoader *l, CachedObject *o) const
{
    if (!_object->_signalsBlocked) {
        KWQObjectSenderScope senderScope(_object);
        QValueList<KWQSlot> copiedSlots(_slots);
        QValueListConstIterator<KWQSlot> end = copiedSlots.end();
        for (QValueListConstIterator<KWQSlot> it = copiedSlots.begin(); it != end; ++it) {
            (*it).call(l, o);
        }
    }
}

void KWQSignal::call(Job *j, const char *d, int s) const
{
    if (!_object->_signalsBlocked) {
        KWQObjectSenderScope senderScope(_object);
        QValueList<KWQSlot> copiedSlots(_slots);
        QValueListConstIterator<KWQSlot> end = copiedSlots.end();
        for (QValueListConstIterator<KWQSlot> it = copiedSlots.begin(); it != end; ++it) {
            (*it).call(j, d, s);
        }
    }
}

void KWQSignal::call(Job *j, const KURL &u) const
{
    if (!_object->_signalsBlocked) {
        KWQObjectSenderScope senderScope(_object);
        QValueList<KWQSlot> copiedSlots(_slots);
        QValueListConstIterator<KWQSlot> end = copiedSlots.end();
        for (QValueListConstIterator<KWQSlot> it = copiedSlots.begin(); it != end; ++it) {
            (*it).call(j, u);
        }
    }
}

void KWQSignal::callWithData(Job *j, NSData *d) const
{
    if (!_object->_signalsBlocked) {
        KWQObjectSenderScope senderScope(_object);
        QValueList<KWQSlot> copiedSlots(_slots);
        QValueListConstIterator<KWQSlot> end = copiedSlots.end();
        for (QValueListConstIterator<KWQSlot> it = copiedSlots.begin(); it != end; ++it) {
            (*it).callWithData(j, d);
        }
    }
}

void KWQSignal::callWithResponse(Job *j, NSURLResponse *r) const
{
    if (!_object->_signalsBlocked) {
        KWQObjectSenderScope senderScope(_object);
        QValueList<KWQSlot> copiedSlots(_slots);
        QValueListConstIterator<KWQSlot> end = copiedSlots.end();
        for (QValueListConstIterator<KWQSlot> it = copiedSlots.begin(); it != end; ++it) {
            (*it).callWithResponse(j, r);
        }
    }
}
