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

#ifndef QSTACK_H_
#define QSTACK_H_

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

// USING_BORROWED_QSTACK =======================================================

#ifdef USING_BORROWED_QSTACK
#include <_qstack.h>
#else

#include <qlist.h>

// class QStack ================================================================

template<class T> class QStack {
public:

    // typedefs ----------------------------------------------------------------
    // enums -------------------------------------------------------------------
    // constants ---------------------------------------------------------------
    // static member functions -------------------------------------------------
    // constructors, copy constructors, and destructors ------------------------

    QStack() : list() {}
    QStack(const QStack<T> &s) : list(s.list) {}
    ~QStack() {}

    // member functions --------------------------------------------------------

    bool isEmpty() const { return list.isEmpty(); }
    void push(const T *item) { list.append (item); }
    T *pop() { T *tmp = list.getLast(); list.removeLast(); return tmp; }
    uint count() const { return list.count(); }

    // operators ---------------------------------------------------------------

    QStack<T> &operator=(const QStack<T> &s) { list = s.list; return *this; }

// protected -------------------------------------------------------------------
// private ---------------------------------------------------------------------
 private:
    QList<T> list;

}; // class QStack =============================================================

#ifdef _KWQ_IOSTREAM_
template<class T>
inline ostream &operator<<(ostream &stream, const QStack<T>&s)
{
    stream << "QStack: [size: " << s.count() << "; items: ";

    QStack<T> tmp(s);

    while (!tmp.isEmpty()) {
        stream << *tmp.pop();
	if (tmp.count() > 0) {
	    stream << ", ";
	}
    }

    stream << "]";

    return stream;
}


#endif

#endif // USING_BORROWED_QSTACK

#endif
