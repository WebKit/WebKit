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

// USING_BORROWED_QDICT ========================================================

#ifdef USING_BORROWED_QDICT

#include <_qdict.h>

#else


#include <qcollection.h>
#include <qstring.h>

#include <KWQDictImpl.h>

template<class T> class QDictIterator;

// class QDict =================================================================

template <class T> class QDict : public QCollection {
public:

    // typedefs ----------------------------------------------------------------
    // enums -------------------------------------------------------------------
    // constants ---------------------------------------------------------------
    // static member functions -------------------------------------------------

    // constructors, copy constructors, and destructors ------------------------

    QDict(int size=17, bool caseSensitive=TRUE) : impl(size, caseSensitive, QDict::deleteFunc) {}
    QDict(const QDict<T> &d) : impl(d.impl) {}
    virtual ~QDict() { impl.clear(del_item); }

    // member functions --------------------------------------------------------

    virtual void clear() { impl.clear(del_item); }
    virtual uint count() const { return impl.count(); }
    void insert(const QString &key, const T *value) { impl.insert(key,(void *)value);}
    bool remove(const QString &key) { return impl.remove(key,del_item); }
    T *find(const QString &key) const { return (T *)impl.find(key); }

    // operators ---------------------------------------------------------------

    QDict<T> &operator=(const QDict<T> &d) { impl.assign(d.impl,del_item); QCollection::operator=(d); return *this;}

// protected -------------------------------------------------------------------
// private ---------------------------------------------------------------------
 private:
    static void deleteFunc(void *item) {
	delete (T *)item;
    }

    KWQDictImpl impl;

    friend class QDictIterator<T>;
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

    QDictIterator(const QDict<T> &d) : impl(d.impl) {}
    ~QDictIterator() {}

    // member functions --------------------------------------------------------

    uint count() const { return impl.count(); }
    T *current() const { return (T *) impl.current(); }
    QString currentKey() const { return impl.currentStringKey(); }
    T *toFirst() { return (T *)impl.toFirst(); }

    // operators ---------------------------------------------------------------

    T *operator++() { return (T *)(++impl); }

// protected -------------------------------------------------------------------
// private ---------------------------------------------------------------------

private:
    KWQDictIteratorImpl impl;

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

#endif // USING_BORROWED_QDICT

#endif
