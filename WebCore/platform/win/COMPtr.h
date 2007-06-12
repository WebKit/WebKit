/*
 * Copyright (C) 2007 Apple Inc.  All rights reserved.
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

#ifndef COMPtr_h
#define COMPtr_h

#include <WTF/Assertions.h>

typedef long HRESULT;

enum AdoptCOMTag { AdoptCOM };

template <typename T> class COMPtr {
public:
    COMPtr() : m_ptr(0) { }
    COMPtr(T* ptr) : m_ptr(ptr) { if (m_ptr) m_ptr->AddRef(); }
    COMPtr(AdoptCOMTag, T* ptr) : m_ptr(ptr) { }
    COMPtr(const COMPtr& o) : m_ptr(o.m_ptr) { if (T* ptr = m_ptr) ptr->AddRef(); }

    ~COMPtr() { if (m_ptr) m_ptr->Release(); }

    T *get() const { return m_ptr; }

    T& operator*() const { return *m_ptr; }
    T* operator->() const { return m_ptr; }
        
    T** operator&() { ASSERT(!m_ptr); return &m_ptr; }

    bool operator!() const { return !m_ptr; }
    
    // This conversion operator allows implicit conversion to bool but not to other integer types.
    typedef T * (COMPtr::*UnspecifiedBoolType)() const;
    operator UnspecifiedBoolType() const { return m_ptr ? &COMPtr::get : 0; }

    COMPtr& operator=(const COMPtr&);
    COMPtr& operator=(T*);
    template <typename U> COMPtr& operator=(const COMPtr<U>&);
    
    HRESULT copyRefTo(T**);
    void adoptRef(T*);

private:
    T* m_ptr;
};

template <typename T> inline HRESULT COMPtr<T>::copyRefTo(T** ptr)
{
    if (!ptr)
        return E_POINTER;
    
    *ptr = m_ptr;
    if (m_ptr)
        m_ptr->AddRef();
    return S_OK;
}

template <typename T> inline void COMPtr<T>::adoptRef(T *ptr)
{
    if (m_ptr)
        m_ptr->Release();
    m_ptr = ptr;
}

template <typename T> inline COMPtr<T>& COMPtr<T>::operator=(const COMPtr<T>& o)
{
    T* optr = o.get();
    if (optr)
        optr->AddRef();
    T* ptr = m_ptr;
    m_ptr = optr;
    if (ptr)
        ptr->Release();
    return *this;
}

template <typename T> template <typename U> inline COMPtr<T>& COMPtr<T>::operator=(const COMPtr<U>& o)
{
    T* optr = o.get();
    if (optr)
        optr->AddRef();
    T* ptr = m_ptr;
    m_ptr = optr;
    if (ptr)
        ptr->Release();
    return *this;
}

template <typename T> inline COMPtr<T>& COMPtr<T>::operator=(T* optr)
{
    if (optr)
        optr->AddRef();
    T* ptr = m_ptr;
    m_ptr = optr;
    if (ptr)
        ptr->Release();
    return *this;
}

template <typename T, typename U> inline bool operator==(const COMPtr<T>& a, const COMPtr<U>& b)
{
    return a.get() == b.get();
}

template <typename T, typename U> inline bool operator==(const COMPtr<T>& a, U* b)
{
    return a.get() == b;
}

template <typename T, typename U> inline bool operator==(T* a, const COMPtr<U>& b) 
{
    return a == b.get();
}

template <typename T, typename U> inline bool operator!=(const COMPtr<T>& a, const COMPtr<U>& b)
{
    return a.get() != b.get();
}

template <typename T, typename U> inline bool operator!=(const COMPtr<T>& a, U* b)
{
    return a.get() != b;
}

template <typename T, typename U> inline bool operator!=(T* a, const COMPtr<U>& b)
{
    return a != b.get();
}

#endif
