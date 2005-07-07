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

#ifndef QPTRDICT_H_
#define QPTRDICT_H_

#include "KWQDef.h"
#include "KWQCollection.h"

#include "KWQPtrDictImpl.h"

template <class T> class QPtrDictIterator;

template <class T> class QPtrDict : public QPtrCollection {
public:
    QPtrDict(int size=17) : impl(size, deleteFunc) {}
    virtual ~QPtrDict() { impl.clear(del_item); }

    virtual void clear() { impl.clear(del_item); }
    virtual uint count() const { return impl.count(); }

    T *take(void *key) { return (T *)impl.take(key); }
    void insert(void *key, T *value) { impl.insert(key, value); }
    bool remove(void *key) { return impl.remove(key, del_item); }
    void replace(void *key, T *value) {
      if (find(key)) remove(key);
      insert(key, value);
    }
    T* find(void *key) { return (T*)impl.find(key); }

    bool isEmpty() const { return count() == 0; }

    QPtrDict<T> &operator=(const QPtrDict<T> &pd) { impl.assign(pd.impl,del_item); QPtrCollection::operator=(pd); return *this; }
    T *operator[](void *key) const { return (T *)impl.find(key); } 

 private:
    static void deleteFunc(void *item) { delete (T *)item; }

    KWQPtrDictImpl impl;
    
    friend class QPtrDictIterator<T>;
};

template<class T> class QPtrDictIterator {
public:
    QPtrDictIterator(const QPtrDict<T> &pd) : impl(pd.impl) { }
    uint count() { return impl.count(); }
    T *current() const { return (T *)impl.current(); }
    void *currentKey() const { return impl.currentKey(); }

    T* toFirst() { return (T*)(impl.toFirst()); }

    T *operator()() { T *ret = (T *)impl.current(); ++impl; return ret; }
    T *operator++() { return (T *)++impl; }

private:
    KWQPtrDictIteratorImpl impl;

    QPtrDictIterator(const QPtrDictIterator &);
    QPtrDictIterator &operator=(const QPtrDictIterator &);
};

#endif
