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

#ifndef KWQREFPTR_H_
#define KWQREFPTR_H_

template <class T> class KWQRefPtr {
public:
    KWQRefPtr() : pointer(0) { }
    explicit KWQRefPtr(T*);
    ~KWQRefPtr() { unref(); }

    KWQRefPtr(const KWQRefPtr&);
    KWQRefPtr& operator=(const KWQRefPtr&);

    bool isNull() const { return pointer == 0; }
    T& operator*() const { return *pointer; }
    T* operator->() const { return pointer; }

private:
    void unref();
    void ref() const; 

    T* pointer;
};

template <class T> inline KWQRefPtr<T>::KWQRefPtr(T* p)
    : pointer(p)
{
    ref();
}

template <class T> inline KWQRefPtr<T>::KWQRefPtr(const KWQRefPtr& r)
    : pointer(r.pointer)
{
    ref();
}

template <class T> inline KWQRefPtr<T>& KWQRefPtr<T>::operator=(const KWQRefPtr& r)
{
    r.ref();
    unref();
    pointer = r.pointer;
    return *this;
}

template <class T> inline void KWQRefPtr<T>::ref() const
{
    if (pointer) {
        ++pointer->refCount;
    }
}

template <class T> inline void KWQRefPtr<T>::unref()
{
    if (pointer && --pointer->refCount == 0) {
        delete pointer;
    }
}

#endif
