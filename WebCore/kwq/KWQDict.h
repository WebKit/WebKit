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

#ifndef QDICT_H_
#define QDICT_H_

#include "KWQCollection.h"
#include "KWQString.h"

#include "KWQDictImpl.h"

template<class T> class QDictIterator;

template <class T> class QDict : public QPtrCollection {
public:
    QDict(int size=17, bool caseSensitive=true) : impl(size, caseSensitive, QDict::deleteFunc) {}
    virtual ~QDict() { impl.clear(del_item); }

    virtual void clear() { impl.clear(del_item); }
    virtual uint count() const { return impl.count(); }
    void insert(const QString &key, const T *value) { impl.insert(key,(void *)value);}
    bool remove(const QString &key) { return impl.remove(key,del_item); }
    void replace(const QString &key, const T *value) { if (find(key)) remove(key); insert(key, value); }
    
    T *find(const QString &key) const { return (T *)impl.find(key); }

    QDict &operator=(const QDict &d) { impl.assign(d.impl, del_item); QPtrCollection::operator=(d); return *this;}
    T *operator[](const QString &key) const { return find(key); }

 private:
    static void deleteFunc(void *item) { delete (T *)item; }

    KWQDictImpl impl;

    friend class QDictIterator<T>;
};


template<class T> class QDictIterator {
public:
    QDictIterator(const QDict<T> &d) : impl(d.impl) {}

    uint count() const { return impl.count(); }
    T *current() const { return (T *)impl.current(); }
    QString currentKey() const { return impl.currentStringKey(); }
    T *toFirst() { return (T *)impl.toFirst(); }

    T *operator++() { return (T *)++impl; }
    T *operator*() { return (T *)impl.current(); }

private:
    KWQDictIteratorImpl impl;
};

#define Q3Dict QDict
#define Q3DictIterator QDictIterator

#endif
