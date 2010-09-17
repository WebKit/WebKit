/*
 *  Copyright (C) 2005, 2006, 2007, 2008 Apple Inc. All rights reserved.
 *  Copyright (C) 2008 Collabora Ltd.
 *  Copyright (C) 2009 Martin Robinson
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public License
 *  along with this library; see the file COPYING.LIB.  If not, write to
 *  the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA 02110-1301, USA.
 *
 */

#ifndef PlatformRefPtr_h
#define PlatformRefPtr_h

#include "AlwaysInline.h"
#include "RefPtr.h"
#include <algorithm>

namespace WTF {

enum PlatformRefPtrAdoptType { PlatformRefPtrAdopt };
template <typename T> inline T* refPlatformPtr(T*);
template <typename T> inline void derefPlatformPtr(T*);
template <typename T> class PlatformRefPtr;
template <typename T> PlatformRefPtr<T> adoptPlatformRef(T*);

template <typename T> class PlatformRefPtr {
public:
    PlatformRefPtr() : m_ptr(0) { }

    PlatformRefPtr(T* ptr)
        : m_ptr(ptr)
    {
        if (ptr)
            refPlatformPtr(ptr);
    }

    PlatformRefPtr(const PlatformRefPtr& o)
        : m_ptr(o.m_ptr)
    {
        if (T* ptr = m_ptr)
            refPlatformPtr(ptr);
    }

    template <typename U> PlatformRefPtr(const PlatformRefPtr<U>& o)
        : m_ptr(o.get())
    {
        if (T* ptr = m_ptr)
            refPlatformPtr(ptr);
    }

    ~PlatformRefPtr()
    {
        if (T* ptr = m_ptr)
            derefPlatformPtr(ptr);
    }

    void clear()
    {
        T* ptr = m_ptr;
        m_ptr = 0;
        if (ptr)
            derefPlatformPtr(ptr);
    }

    // Hash table deleted values, which are only constructed and never copied or destroyed.
    PlatformRefPtr(HashTableDeletedValueType) : m_ptr(hashTableDeletedValue()) { }
    bool isHashTableDeletedValue() const { return m_ptr == hashTableDeletedValue(); }

    T* get() const { return m_ptr; }
    T& operator*() const { return *m_ptr; }
    ALWAYS_INLINE T* operator->() const { return m_ptr; }

    bool operator!() const { return !m_ptr; }

    // This conversion operator allows implicit conversion to bool but not to other integer types.
    typedef T* PlatformRefPtr::*UnspecifiedBoolType;
    operator UnspecifiedBoolType() const { return m_ptr ? &PlatformRefPtr::m_ptr : 0; }

    PlatformRefPtr& operator=(const PlatformRefPtr&);
    PlatformRefPtr& operator=(T*);
    template <typename U> PlatformRefPtr& operator=(const PlatformRefPtr<U>&);

    void swap(PlatformRefPtr&);
    friend PlatformRefPtr adoptPlatformRef<T>(T*);

private:
    static T* hashTableDeletedValue() { return reinterpret_cast<T*>(-1); }
    // Adopting constructor.
    PlatformRefPtr(T* ptr, PlatformRefPtrAdoptType) : m_ptr(ptr) {}

    T* m_ptr;
};

template <typename T> inline PlatformRefPtr<T>& PlatformRefPtr<T>::operator=(const PlatformRefPtr<T>& o)
{
    T* optr = o.get();
    if (optr)
        refPlatformPtr(optr);
    T* ptr = m_ptr;
    m_ptr = optr;
    if (ptr)
        derefPlatformPtr(ptr);
    return *this;
}

template <typename T> inline PlatformRefPtr<T>& PlatformRefPtr<T>::operator=(T* optr)
{
    T* ptr = m_ptr;
    if (optr)
        refPlatformPtr(optr);
    m_ptr = optr;
    if (ptr)
        derefPlatformPtr(ptr);
    return *this;
}

template <class T> inline void PlatformRefPtr<T>::swap(PlatformRefPtr<T>& o)
{
    std::swap(m_ptr, o.m_ptr);
}

template <class T> inline void swap(PlatformRefPtr<T>& a, PlatformRefPtr<T>& b)
{
    a.swap(b);
}

template <typename T, typename U> inline bool operator==(const PlatformRefPtr<T>& a, const PlatformRefPtr<U>& b)
{
    return a.get() == b.get();
}

template <typename T, typename U> inline bool operator==(const PlatformRefPtr<T>& a, U* b)
{
    return a.get() == b;
}

template <typename T, typename U> inline bool operator==(T* a, const PlatformRefPtr<U>& b)
{
    return a == b.get();
}

template <typename T, typename U> inline bool operator!=(const PlatformRefPtr<T>& a, const PlatformRefPtr<U>& b)
{
    return a.get() != b.get();
}

template <typename T, typename U> inline bool operator!=(const PlatformRefPtr<T>& a, U* b)
{
    return a.get() != b;
}

template <typename T, typename U> inline bool operator!=(T* a, const PlatformRefPtr<U>& b)
{
    return a != b.get();
}

template <typename T, typename U> inline PlatformRefPtr<T> static_pointer_cast(const PlatformRefPtr<U>& p)
{
    return PlatformRefPtr<T>(static_cast<T*>(p.get()));
}

template <typename T, typename U> inline PlatformRefPtr<T> const_pointer_cast(const PlatformRefPtr<U>& p)
{
    return PlatformRefPtr<T>(const_cast<T*>(p.get()));
}

template <typename T> inline T* getPtr(const PlatformRefPtr<T>& p)
{
    return p.get();
}

template <typename T> PlatformRefPtr<T> adoptPlatformRef(T* p)
{
    return PlatformRefPtr<T>(p, PlatformRefPtrAdopt);
}

} // namespace WTF

using WTF::PlatformRefPtr;
using WTF::refPlatformPtr;
using WTF::derefPlatformPtr;
using WTF::adoptPlatformRef;
using WTF::static_pointer_cast;
using WTF::const_pointer_cast;

#endif // PlatformRefPtr_h
