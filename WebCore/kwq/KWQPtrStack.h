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

#ifndef QSTACK_H_
#define QSTACK_H_

#include "KWQPtrList.h"

template<class T> class QPtrStack {
public:
    QPtrStack() { }

    bool isEmpty() const { return list.isEmpty(); }
    void push(const T *item) { list.append (item); }
    T *pop() { T *tmp = list.getLast(); list.removeLast(); return tmp; }
    uint count() const { return list.count(); }
    T* current() const { return list.getLast(); }
    
    void setAutoDelete(bool b) { list.setAutoDelete(b); }
    
 private:
    QPtrList<T> list;

};

#define Q3PtrStack QPtrStack

#ifdef _KWQ_IOSTREAM_

template<class T>
inline std::ostream &operator<<(std::ostream &stream, const QPtrStack<T>&s)
{
    stream << "QPtrStack: [size: " << s.count() << "; items: ";

    QPtrStack<T> tmp(s);

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

#endif
