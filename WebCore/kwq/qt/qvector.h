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

#ifndef QVECTOR_H_
#define QVECTOR_H_

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <KWQDef.h>
#include <_qcollection.h>

typedef void *Item;

// class QGVector ==============================================================

class QGVector : public QCollection {
public:

    // typedefs ----------------------------------------------------------------
    // enums -------------------------------------------------------------------
    // constants ---------------------------------------------------------------

    // static member functions -------------------------------------------------

    virtual int compareItems(Item, Item);

    // constructors, copy constructors, and destructors ------------------------
    
    QGVector();
    QGVector(const QGVector &);
    ~QGVector();
    
    // member functions --------------------------------------------------------
    
    // operators ---------------------------------------------------------------

    QGVector &operator=(const QGVector &);

// protected -------------------------------------------------------------------
// private ---------------------------------------------------------------------

}; // class QGVector ===========================================================


// class QVector ===============================================================

template<class T> class QVector : public QGVector  {
public:

    // typedefs ----------------------------------------------------------------
    // enums -------------------------------------------------------------------
    // constants ---------------------------------------------------------------
    // static member functions -------------------------------------------------

    // constructors, copy constructors, and destructors ------------------------

    QVector();
    QVector(uint);
    QVector(const QVector &);
    ~QVector();

    // member functions --------------------------------------------------------

    void clear();
    bool isEmpty() const;
    uint count() const;
    uint size() const;
    bool remove(uint);
    bool resize(uint);
    bool insert(uint, const T *);
    T *at(int) const;

    // operators ---------------------------------------------------------------

    T *operator[](int) const;
    QVector &operator=(const QVector &);

// protected -------------------------------------------------------------------
// private ---------------------------------------------------------------------

}; // class QVector ============================================================

#endif
