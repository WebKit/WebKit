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

#ifndef QLIST_H_
#define QLIST_H_

#include <kwqdef.h>
#include "qcollection.h"

template <class T> class QList {
public:
    // FIXME: should any of these functions be moved into a base collection
    // class implementation?
    uint count() const;
    T *at(uint);
    void append(const T *);
    void setAutoDelete(bool);
    bool insert(uint i, const T *);
    bool remove();
    bool remove(const T *);
    bool removeFirst();
    bool removeLast();
    T *getLast() const;
    T *current() const;
    T *prev() const;
    void prepend(const T *);
    T *take(uint i);
    T *first();
    T *next();
    T *last();
    void clear();
    bool isEmpty() const;
    bool removeRef(const T *);
    uint containsRef(const T *) const;
    void sort();
};

template <class T> class QListIterator {
public:
    QListIterator(const QList<T> &);
    uint count() const;
    T *toFirst();
    T *toLast();
    T *current() const;
    operator T *() const;
    T *operator--();
    T *operator++();
};

#endif
