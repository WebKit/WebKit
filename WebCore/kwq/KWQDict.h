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

#ifndef QDICT_H_
#define QDICT_H_

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <KWQCollection.h>
#include "qstring.h"

// class QDict =================================================================

template <class T> class QDict : public QCollection {
public:

    // typedefs ----------------------------------------------------------------
    // enums -------------------------------------------------------------------
    // constants ---------------------------------------------------------------
    // static member functions -------------------------------------------------

    // constructors, copy constructors, and destructors ------------------------

    QDict(int size=17, bool caseSensitive=TRUE);
    QDict(const QDict<T> &);
    ~QDict();

    // member functions --------------------------------------------------------

    void insert(const QString &, const T *);
    bool remove(const QString &);
    T *find(const QString &) const;
    uint count() const;

    // operators ---------------------------------------------------------------

    QDict<T> &operator=(const QDict<T> &);

// protected -------------------------------------------------------------------
// private ---------------------------------------------------------------------

}; // class QDict ==============================================================


// class QDictIterator =========================================================

template<class T> class QDictIterator {
public:

    // typedefs ----------------------------------------------------------------
    // enums -------------------------------------------------------------------
    // constants ---------------------------------------------------------------
    // static member functions -------------------------------------------------
    
    // constructors, copy constructors, and destructors ------------------------
    
// add no-arg constructor
#ifdef _KWQ_PEDANTIC_
    QDictIterator() {}
#endif

    QDictIterator(const QDict<T> &);
    ~QDictIterator();

    // member functions --------------------------------------------------------

    uint count() const;
    T *current() const;
    T *toFirst();

    // operators ---------------------------------------------------------------

    T *operator++();

// protected -------------------------------------------------------------------
// private ---------------------------------------------------------------------

private:

// add copy constructor
// this private declaration prevents copying
#ifdef _KWQ_PEDANTIC_
    QDictIterator(const QDictIterator &);
#endif

// add assignment operator 
// this private declaration prevents assignment
#ifdef _KWQ_PEDANTIC_
    QDictIterator &operator=(const QDictIterator &);
#endif

}; // class QDictIterator ======================================================

#endif
