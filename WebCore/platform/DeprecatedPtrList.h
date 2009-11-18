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

#ifndef DeprecatedPtrList_h
#define DeprecatedPtrList_h

#include "DeprecatedPtrListImpl.h"
#include <wtf/FastAllocBase.h>

namespace WebCore {

template <class T> class DeprecatedPtrListIterator;

template <class T> class DeprecatedPtrList : public FastAllocBase {
public:
    DeprecatedPtrList() : impl(deleteFunc), del_item(false) { }
    ~DeprecatedPtrList() { impl.clear(del_item); }
    
    DeprecatedPtrList(const DeprecatedPtrList& l) : impl(l.impl), del_item(false) { }
    DeprecatedPtrList& operator=(const DeprecatedPtrList &l) { impl.assign(l.impl, del_item); return *this; }

    bool isEmpty() const { return impl.isEmpty(); }
    unsigned count() const { return impl.count(); }
    void clear() { impl.clear(del_item); }

    T *at(unsigned n) { return (T *)impl.at(n); }

    bool insert(unsigned n, const T *item) { return impl.insert(n, item); }
    bool remove() { return impl.remove(del_item); }
    bool remove(unsigned n) { return impl.remove(n, del_item); }
    bool remove(const T *item) { return impl.removeRef(item, del_item); }
    bool removeFirst() { return impl.removeFirst(del_item); }
    bool removeLast() { return impl.removeLast(del_item); }
    bool removeRef(const T *item) { return impl.removeRef(item, del_item); }

    T *getFirst() const { return (T *)impl.getFirst(); }
    T *getLast() const { return (T *)impl.getLast(); }
    T *getNext() const { return (T *)impl.getNext(); }
    T *getPrev() const { return (T *)impl.getPrev(); }
    T *current() const { return (T *)impl.current(); }
    T *first() { return (T *)impl.first(); }
    T *last() { return (T *)impl.last(); }
    T *next() { return (T *)impl.next(); }
    T *prev() { return (T *)impl.prev(); }
    T *take(unsigned n) { return (T *)impl.take(n); }
    T *take() { return (T *)impl.take(); }

    void append(const T *item) { impl.append(item); }
    void prepend(const T *item) { impl.prepend(item); }

    unsigned containsRef(const T *item) const { return impl.containsRef(item); }
    int findRef(const T *item) { return impl.findRef(item); }

    typedef DeprecatedPtrListIterator<T> Iterator;
    typedef DeprecatedPtrListIterator<T> ConstIterator;
    ConstIterator begin() const { return ConstIterator(*this); }
    ConstIterator end() const { ConstIterator itr(*this); itr.toLast(); ++itr; return itr; }

    bool autoDelete() const { return del_item; }
    void setAutoDelete(bool autoDelete) { del_item = autoDelete; }

 private:
    static void deleteFunc(void *item) { delete (T *)item; }

    friend class DeprecatedPtrListIterator<T>;

    DeprecatedPtrListImpl impl;
    bool del_item;
};

template <class T> class DeprecatedPtrListIterator {
public:
    DeprecatedPtrListIterator() { }
    DeprecatedPtrListIterator(const DeprecatedPtrList<T> &l) : impl(l.impl) { }

    unsigned count() const { return impl.count(); }
    T *toFirst() { return (T *)impl.toFirst(); }
    T *toLast() { return (T *)impl.toLast(); }
    T *current() const { return (T *)impl.current(); }

    operator T *() const { return (T *)impl.current(); }
    T *operator*() const { return (T *)impl.current(); }
    T *operator--() { return (T *)--impl; }
    T *operator++()  { return (T *)++impl; }

private:
    DeprecatedPtrListImplIterator impl;
};

}

#endif
