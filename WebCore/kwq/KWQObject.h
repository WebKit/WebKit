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

#ifndef QOBJECT_H_
#define QOBJECT_H_

#include "KWQNamespace.h"
#include "KWQPtrList.h"
#include "KWQSignal.h"

#define slots : public
#define SLOT(x) "SLOT:" #x
#define signals protected
#define SIGNAL(x) "SIGNAL:" #x
#define emit
#define Q_OBJECT

class KWQGuardedPtrBase;

class QObject : public Qt {
public:
    QObject();
    virtual ~QObject();

    static void connect(const QObject *sender, const char *signal, const QObject *receiver, const char *member);
    static void disconnect(const QObject *sender, const char *signal, const QObject *receiver, const char *member);
    void connect(const QObject *sender, const char *signal, const char *member) const
        { connect(sender, signal, this, member); }

    void installEventFilter(const QObject* o) { _eventFilterObject = o; }
    void removeEventFilter() { _eventFilterObject = 0; }
    const QObject* eventFilterObject() const { return _eventFilterObject; }

    virtual void eventFilterFocusIn() const { }
    virtual void eventFilterFocusOut() const { }

    virtual bool isFrame() const;
    virtual bool isFrameView() const;

private:
    // no copying or assignment
    QObject(const QObject &);
    QObject &operator=(const QObject &);
    
    KWQSignal *findSignal(const char *signalName) const;
    
    QPtrList<QObject> _guardedPtrDummyList;
    
    mutable KWQSignal *_signalListHead;

    KWQSignal _destroyed;
    
    const QObject *_eventFilterObject;
    
    friend class KWQGuardedPtrBase;
    friend class KWQSignal;
};

#endif
