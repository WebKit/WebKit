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

#ifndef QVECTOR_H_
#define QVECTOR_H_

#include "KWQDef.h"
#include "KWQCollection.h"

#include "KWQVectorImpl.h"

class QGVector : public QPtrCollection
{
public:
    virtual int compareItems(void *a, void *b) = 0;
};

// class QPtrVector ===============================================================
template<class T> class QPtrVector : public QGVector  {
public:
    QPtrVector() : impl(deleteFunc) {}
    QPtrVector(uint size) : impl(size, deleteFunc) {}
    ~QPtrVector() { if (del_item) { impl.clear(del_item); } }

    void clear() { impl.clear(del_item); }
    bool isEmpty() const { return impl.isEmpty(); }
    uint count() const { return impl.count(); }
    uint size() const { return impl.size(); }
    bool remove(uint n) { return impl.remove(n, del_item); }
    bool resize(uint size) { return impl.resize(size, del_item); }
    bool insert(uint n, T *item) {return impl.insert(n, item, del_item); }
    T *at(int n) const {return (T *)impl.at(n); }
    T **data() {return (T **)impl.data(); }
    int findRef(T *item) {return impl.findRef(item);}

    virtual int compareItems(void *a, void *b) { return a != b; }

    T *operator[](uint n) const {return (T *)impl.at(n); }
    QPtrVector &operator=(const QPtrVector &v) 
    { impl.assign(v.impl,del_item); QPtrCollection::operator=(v); return *this; }

 private:
    static void deleteFunc(void *item) { delete (T *)item; }

    KWQVectorImpl impl;
};

#endif
