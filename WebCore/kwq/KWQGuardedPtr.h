/*
 * Copyright (C) 2001 Apple Computer, Inc.  All rights reserved.
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

#ifndef QGUARDEDPTR_H_
#define QGUARDEDPTR_H_

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "qobject.h"

// class QGuardedPtrPrivate ====================================================

class QGuardedPtrPrivate : public QObject, public QShared {
public:
    QGuardedPtrPrivate(QObject *o);
    ~QGuardedPtrPrivate();
    
    QObject *object() const;
    
private:
    QObject *p;
};

// class QGuardedPtr ===========================================================

template <class T> class QGuardedPtr : public QObject {
public:

    // typedefs ----------------------------------------------------------------
    // enums -------------------------------------------------------------------
    // constants ---------------------------------------------------------------
    // static member functions -------------------------------------------------

    // constructors, copy constructors, and destructors ------------------------

    QGuardedPtr() {
        d = new QGuardedPtrPrivate(0L);
    }
    QGuardedPtr(T *o) {
        d = new QGuardedPtrPrivate(o);
    }
    
    QGuardedPtr(const QGuardedPtr<T> &p) {
        d = p.d;
        ref();
    }

    ~QGuardedPtr() {
        deref();
    }

    // member functions --------------------------------------------------------

    bool isNull() const {
        return !d->object();
    }

    // operators ---------------------------------------------------------------

    QGuardedPtr &operator=(const QGuardedPtr &p) {
        if (d != p.d) {
            deref();
            d = p.d;
            ref();
        } 
        return *this;
    }
    
    operator T *() const {
        return (T*)d->object();
    }
    
    T *operator->() const {
        return (T*)d->object();
    }

// protected -------------------------------------------------------------------
// private ---------------------------------------------------------------------
private:
    QGuardedPtrPrivate *d;

    void ref()
    {
        d->ref();
    }
    
    void deref()
    {
        if (d->deref())
            delete d;
    }


}; // class QGuardedPtr ========================================================


inline QObject *QGuardedPtrPrivate::object() const
{
    return p;
}

#endif
