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

// _KWQ_COMPLETE_ ==============================================================

#ifdef _KWQ_COMPLETE_
#include <_qptrdict.h>
#else

#include <KWQDef.h>
#include <_qcollection.h>

// class QPtrDict ==============================================================

template <class T> class QPtrDict : public QCollection {
public:

    // typedefs ----------------------------------------------------------------
    // enums -------------------------------------------------------------------
    // constants ---------------------------------------------------------------
    // static member functions -------------------------------------------------

    // constructors, copy constructors, and destructors ------------------------

    QPtrDict(int size=17);
    QPtrDict(const QPtrDict<T> &);
    ~QPtrDict();

    // member functions --------------------------------------------------------

    void clear();
    uint count() const;
    T *at(uint);
    T *take(void *);

    void append(const T *);
    void insert(void *, const T *);
    void remove(void *);

    // operators ---------------------------------------------------------------

    QPtrDict<T> &operator=(const QPtrDict<T> &);
    T *operator[](void *) const; 

// protected -------------------------------------------------------------------
// private ---------------------------------------------------------------------

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

    QPtrDictIterator(const QPtrDict<T> &);
    ~QPtrDictIterator() {}

    // member functions --------------------------------------------------------

    T *current() const;
    void *currentKey() const;

    // operators ---------------------------------------------------------------

    T *operator++();

// protected -------------------------------------------------------------------
// private ---------------------------------------------------------------------

private:

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

#endif // _KWQ_COMPLETE_

#endif
