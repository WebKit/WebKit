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

#ifndef QPTRDICT_H_
#define QPTRDICT_H_

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

// USING_BORROWED_QPTRDICT =====================================================

#ifdef USING_BORROWED_QPTRDICT
#include <_qptrdict.h>
#else

#include <KWQDef.h>
#include <qcollection.h>

#include <KWQPtrDictImpl.h>

template <class T> class QPtrDictIterator;

// class QPtrDict ==============================================================

template <class T> class QPtrDict : public QCollection {
public:

    // typedefs ----------------------------------------------------------------
    // enums -------------------------------------------------------------------
    // constants ---------------------------------------------------------------
    // static member functions -------------------------------------------------

    // constructors, copy constructors, and destructors ------------------------

    QPtrDict(int size=17) : impl(size, deleteFunc) {}
    QPtrDict(const QPtrDict<T> &pd) : impl(pd.impl) {}
    virtual ~QPtrDict() { impl.clear(del_item); }

    // member functions --------------------------------------------------------

    virtual void clear() { impl.clear(del_item); }
    virtual uint count() const { return impl.count(); }

    T *take(void *key) { return (T *)impl.take(key); }
    void insert(void *key, const T *value) { impl.insert(key, (void *)value); }
    bool remove(void *key) { return impl.remove(key, del_item); }

    // operators ---------------------------------------------------------------

    QPtrDict<T> &operator=(const QPtrDict<T> &pd) { impl.assign(pd.impl,del_item); QCollection::operator=(pd); return *this; }
    T *operator[](void *key) const { return (T *)impl.find(key); } 

// protected -------------------------------------------------------------------
// private ---------------------------------------------------------------------
 private:
    static void deleteFunc(void *item) {
	delete (T *)item;
    }

    KWQPtrDictImpl impl;

    friend class QPtrDictIterator<T>;
}; // class QPtrDict ===========================================================


// class QPtrDictIterator ======================================================

template<class T> class QPtrDictIterator {
public:

    // typedefs ----------------------------------------------------------------
    // enums -------------------------------------------------------------------
    // constants ---------------------------------------------------------------
    // static member functions -------------------------------------------------

    // constructors, copy constructors, and destructors ------------------------

// add no-arg constructor
#ifdef _KWQ_PEDANTIC_
    QPtrDictIterator() {}
#endif

    QPtrDictIterator(const QPtrDict<T> &pd) : impl(pd.impl) {}
    ~QPtrDictIterator() {}

    // member functions --------------------------------------------------------

    uint count() { return impl.count(); }
    T *current() const { return (T *)impl.current(); }
    void *currentKey() const { return impl.currentKey(); }

    // operators ---------------------------------------------------------------

    T *operator++() { return (T *)(++impl); }

// protected -------------------------------------------------------------------
// private ---------------------------------------------------------------------

private:
    KWQPtrDictIteratorImpl impl;

// add copy constructor
// this private declaration prevents copying
#ifdef _KWQ_PEDANTIC_
    QPtrDictIterator(const QPtrDictIterator &);
#endif

// add assignment operator 
// this private declaration prevents assignment
#ifdef _KWQ_PEDANTIC_
    QPtrDictIterator &operator=(const QPtrDictIterator &);
#endif

}; // class QPtrDictIterator ===================================================

#endif // USING_BORROWED_QPTRDICT

#endif
