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

#ifndef QARRAY_H_
#define QARRAY_H_

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

// USING_BORROWED_QARRAY =======================================================

#ifdef USING_BORROWED_QARRAY
#include <_qarray.h>
#else

#include <KWQDef.h>
#include <KWQArrayImpl.h>

// class QMemArray ================================================================

template <class T> class QMemArray {
public:

    // typedefs ----------------------------------------------------------------
    // enums -------------------------------------------------------------------
    // constants ---------------------------------------------------------------
    // static member functions -------------------------------------------------
    
    // constructors, copy constructors, and destructors ------------------------

    QMemArray() : impl(sizeof(T)) {}
    QMemArray(int i) : impl(sizeof(T),i) {}
    QMemArray(const QMemArray<T> &a) : impl(a.impl) {}
    ~QMemArray() {}
    
    // member functions --------------------------------------------------------

    bool isEmpty() {return impl.size() == 0; }
    T &at(uint u) const {return *(T *)impl.at(u); }
    T *data() const { return (T *)impl.data(); }
    uint size() const { return impl.size(); }
    uint count() const { return size(); }
    bool resize(uint size) { return impl.resize(size); }
    QMemArray<T>& duplicate(const T *data, int size) { impl.duplicate(data, size); return *this; }
    void detach() { duplicate(data(), size()); }
    bool fill(const T &item, int size=-1) { return impl.fill(&item, size); }
    QMemArray<T>& assign(const QMemArray<T> &a) { return *this = a; }


    // operators ---------------------------------------------------------------

    QMemArray<T> &operator=(const QMemArray<T> &a) { impl = a.impl; return *this; }    
    T &operator[](int i) const { return at(i); }
    bool operator==(const QMemArray<T> &a) const { return impl == a.impl; }
    bool operator!=(const QMemArray<T> &a) const { return !(*this == a); }    
    operator const T*() const { return data(); }

// protected -------------------------------------------------------------------
// private ---------------------------------------------------------------------
 private:
    KWQArrayImpl impl;

}; // class QMemArray =============================================================

#ifdef _KWQ_IOSTREAM_

#include <iostream>

template<class T>
inline std::ostream &operator<<(std::ostream &stream, const QMemArray<T>&a)
{
    stream << "QMemArray: [size: " << a.size() << "; items: ";
    for (unsigned i = 0; i < a.size(); i++) {
        stream << a[i];
	if (i < a.size() - 1) {
	    stream << ", ";
	}
    }
    stream << "]";

    return stream;
}

#endif

#endif // USING_BORROWED_QARRAY

#endif

