/*
 * Copyright (C) 2004, 2005, 2006 Apple Computer, Inc.  All rights reserved.
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

#include "KWQEvent.h"
#include <kxmlcore/Assertions.h>
#include <kxmlcore/HashMap.h>

const QObject *QObject::_sender;

QObject::QObject()
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
            && !KWQNamesMatch(member, SLOT(slotJobPercent(KIO::Job *, unsigned long)))
            && !KWQNamesMatch(member, SLOT(slotJobSpeed(KIO::Job *, unsigned long)))
            && !KWQNamesMatch(member, SLOT(slotScrollBarMoved()))
            && !KWQNamesMatch(member, SLOT(slotShowDocument(const QString &, const QString &)))
            && !KWQNamesMatch(member, SLOT(slotViewCleared())) // FIXME: Should implement this one!
            )
        LOG_ERROR("connecting member %s to signal %s, but that signal was not found", member, signalName);
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

bool QObject::event(QEvent *)
{
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
