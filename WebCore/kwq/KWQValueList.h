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

#ifndef QVALUELIST_H_
#define QVALUELIST_H_

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <KWQDef.h>

// class QValueListIterator ====================================================

template<class T> class QValueListIterator {
public: 

    // typedefs ----------------------------------------------------------------
    // enums -------------------------------------------------------------------
    // constants ---------------------------------------------------------------
    // static member functions -------------------------------------------------
    
    // constructors, copy constructors, and destructors ------------------------
    
    QValueListIterator();
    QValueListIterator(const QValueListIterator<T>&);
     
    ~QValueListIterator();
    
    // member functions --------------------------------------------------------

    // operators ---------------------------------------------------------------

    QValueListIterator &operator=(const QValueListIterator &);
    bool operator==(const QValueListIterator<T>&);
    bool operator!=(const QValueListIterator<T>&);
    T& operator*();
    const T& operator*() const;
    QValueListIterator<T>& operator++();
    QValueListIterator<T>& operator++(int);
    QValueListIterator<T>& operator--();

// protected -------------------------------------------------------------------
// private ---------------------------------------------------------------------

}; // class QValueListIterator =================================================


// class QValueListConstIterator ===============================================

template<class T> class QValueListConstIterator {
public:

    // typedefs ----------------------------------------------------------------
    // enums -------------------------------------------------------------------
    // constants ---------------------------------------------------------------
    // static member functions -------------------------------------------------

    // constructors, copy constructors, and destructors ------------------------

    QValueListConstIterator();
    QValueListConstIterator(const QValueListIterator<T>&);

    ~QValueListConstIterator();

    // member functions --------------------------------------------------------

    // operators ---------------------------------------------------------------

    QValueListConstIterator &operator=(const QValueListConstIterator &);
    bool operator==(const QValueListConstIterator<T>&);
    bool operator!=(const QValueListConstIterator<T>&);
    T& operator*();
    QValueListConstIterator operator++();
    QValueListConstIterator operator++(int);

// protected -------------------------------------------------------------------
// private ---------------------------------------------------------------------

}; // class QValueListConstIterator ============================================


// class QValueList ============================================================

template <class T> class QValueList {
public:

    // typedefs ----------------------------------------------------------------

    typedef QValueListIterator<T> Iterator;
    typedef QValueListConstIterator<T> ConstIterator;

    // enums -------------------------------------------------------------------
    // constants ---------------------------------------------------------------
    // static member functions -------------------------------------------------
    
    // constructors, copy constructors, and destructors ------------------------
    
    QValueList();
    QValueList(const QValueList<T>&);
    
    ~QValueList();
        
    // member functions --------------------------------------------------------

    void clear();
    uint count() const;
    bool isEmpty() const;

    void append(const T&);
    void remove(const T&);

    uint contains(const T&);

    Iterator remove(Iterator);
    Iterator fromLast();

    const T& first() const;
    const T& last() const;

    Iterator begin();
    Iterator end();

    ConstIterator begin() const;
    ConstIterator end() const;

    // operators ---------------------------------------------------------------

    QValueList<T>& operator=(const QValueList<T>&);
    T& operator[] (uint);
    const T& operator[] (uint) const;
    QValueList<T> &operator+=(const T &);

// protected -------------------------------------------------------------------
// private ---------------------------------------------------------------------

}; // class QValueList =========================================================

#endif
