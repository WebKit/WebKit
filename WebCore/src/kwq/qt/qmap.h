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

#ifndef QMAP_H_
#define QMAP_H_

#include <KWQDef.h>

// class QMapIterator ==========================================================

template<class K, class T> class QMapIterator {
public:

    // typedefs ----------------------------------------------------------------
    // enums -------------------------------------------------------------------
    // constants ---------------------------------------------------------------
    // static member functions -------------------------------------------------

    // constructors, copy constructors, and destructors ------------------------

    QMapIterator();
    QMapIterator(const QMapIterator<K,T>& it);

    // member functions --------------------------------------------------------

    const K& key() const;
    const T& data() const;

    // operators ---------------------------------------------------------------

    QMapIterator<K,T> &operator=(const QMapIterator<K,T> &);
    bool operator==(const QMapIterator<K,T>&) const;
    bool operator!=(const QMapIterator<K,T>&) const;
    T& operator*();
    const T& operator*() const;
    QMapIterator<K,T>& operator++();

// protected -------------------------------------------------------------------
// private ---------------------------------------------------------------------

}; // class QMapIterator =======================================================


// class QMapConstIterator =====================================================

template<class K, class T> class QMapConstIterator {
public:

    // typedefs ----------------------------------------------------------------
    // enums -------------------------------------------------------------------
    // constants ---------------------------------------------------------------
    // static member functions -------------------------------------------------

    // constructors, copy constructors, and destructors ------------------------

    QMapConstIterator();
    QMapConstIterator(const QMapConstIterator<K,T>&);
    QMapConstIterator(const QMapIterator<K,T>&);

    // member functions --------------------------------------------------------

    const K& key() const;
    const T& data() const;

    // operators ---------------------------------------------------------------

    QMapConstIterator<K,T> &operator=(const QMapConstIterator<K,T> &);
    bool operator==(const QMapConstIterator<K,T>&) const;
    bool operator!=(const QMapConstIterator<K,T>&) const;
    const T &operator*() const;
    QMapConstIterator<K,T>& operator++();

// protected -------------------------------------------------------------------
// private ---------------------------------------------------------------------

}; // class QMapConstIterator ==================================================


// class QMap ==================================================================

template <class K, class T> class QMap {
public:

    // typedefs ----------------------------------------------------------------

    typedef QMapIterator<K, T> Iterator;
    typedef QMapConstIterator< K, T> ConstIterator;

    // enums -------------------------------------------------------------------
    // constants ---------------------------------------------------------------
    // static member functions -------------------------------------------------
    // constructors, copy constructors, and destructors ------------------------
    
    QMap();
    QMap(const QMap<K,T>&);
    
    ~QMap();
    
    // member functions --------------------------------------------------------

    void clear();
    uint count() const;

    Iterator begin();
    Iterator end();
    
    ConstIterator begin() const;
    ConstIterator end() const;

    Iterator insert(const K&, const T&);
    void remove(const K&);

    ConstIterator find (const K &) const;

    // operators ---------------------------------------------------------------

    QMap<K,T>& operator=(const QMap<K,T>&);
    T& operator[](const K& k);

// protected -------------------------------------------------------------------
// private ---------------------------------------------------------------------

}; // class QMap ===============================================================

#endif
