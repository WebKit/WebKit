/*
 * Copyright (C) 2004 Apple Computer, Inc.  All rights reserved.
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

#ifndef ARRAY_H_
#define ARRAY_H_

#include "ArrayImpl.h"

namespace WebCore {

template <class T> class DeprecatedArray {
public:
    DeprecatedArray() : impl(sizeof(T)) { }
    DeprecatedArray(int i) : impl(sizeof(T), i) { }
    
    bool isEmpty() { return impl.size() == 0; }
    T &at(unsigned u) { return *(T *)impl.at(u); }
    const T &at(unsigned u) const { return *(T *)impl.at(u); }
    T *data() { return (T *)impl.data(); }
    const T *data() const { return (T *)impl.data(); }
    unsigned size() const { return impl.size(); }
    unsigned count() const { return impl.size(); }
    bool resize(unsigned size) { return impl.resize(size); }
    DeprecatedArray<T>& duplicate(const DeprecatedArray<T> &a) { impl.duplicate(a.data(), a.size()); return *this; }
    DeprecatedArray<T>& duplicate(const T *data, int size) { impl.duplicate(data, size); return *this; }
    void detach() { impl.detach(); }
    bool fill(const T &item, int size=-1) { return impl.fill(&item, size); }
    DeprecatedArray<T>& assign(const DeprecatedArray<T> &a) { return *this = a; }

    T &operator[](int i) { return *(T *)impl.at(i); }
    const T &operator[](int i) const { return *(T *)impl.at(i); }
#if WIN32
    // FIXME: Look into this strange compile error on Win32.
    T &operator[](unsigned i) { return *(T *)impl.at(i); }
    const T &operator[](unsigned i) const { return *(T *)impl.at(i); }
#endif
    bool operator==(const DeprecatedArray<T> &a) const { return impl == a.impl; }
    bool operator!=(const DeprecatedArray<T> &a) const { return !(*this == a); }    
    operator const T*() const { return data(); }

 private:
    ArrayImpl impl;
};

typedef DeprecatedArray<char> DeprecatedByteArray;

}

#endif
