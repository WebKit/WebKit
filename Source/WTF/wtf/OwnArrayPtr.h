/*
 *  Copyright (C) 2006, 2010, 2013 Apple Inc. All rights reserved.
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

#ifndef WTF_OwnArrayPtr_h
#define WTF_OwnArrayPtr_h

#include <wtf/Assertions.h>
#include <wtf/Noncopyable.h>
#include <wtf/NullPtr.h>
#include <algorithm>

namespace WTF {

template<typename T> inline void deleteOwnedArrayPtr(T*);

template <typename T> class OwnArrayPtr {
public:
    typedef T* PtrType;

    OwnArrayPtr() : m_ptr(nullptr) { }
    OwnArrayPtr(std::nullptr_t) : m_ptr(nullptr) { }
    OwnArrayPtr(OwnArrayPtr&&);
    template<typename U> OwnArrayPtr(OwnArrayPtr<U>&&);

    ~OwnArrayPtr() { deleteOwnedArrayPtr(m_ptr); }

    PtrType get() const { return m_ptr; }

    void clear();
    PtrType leakPtr() WARN_UNUSED_RETURN;

    T& operator*() const { ASSERT(m_ptr); return *m_ptr; }
    PtrType operator->() const { ASSERT(m_ptr); return m_ptr; }

    T& operator[](std::ptrdiff_t i) const { ASSERT(m_ptr); ASSERT(i >= 0); return m_ptr[i]; }

    bool operator!() const { return !m_ptr; }

    // This conversion operator allows implicit conversion to bool but not to other integer types.
    typedef T* OwnArrayPtr::*UnspecifiedBoolType;
    operator UnspecifiedBoolType() const { return m_ptr ? &OwnArrayPtr::m_ptr : 0; }

    OwnArrayPtr& operator=(std::nullptr_t) { clear(); return *this; }
    OwnArrayPtr& operator=(OwnArrayPtr&&);
    template<typename U> OwnArrayPtr& operator=(OwnArrayPtr<U>&&);

    void swap(OwnArrayPtr& o) { std::swap(m_ptr, o.m_ptr); }

private:
    template<typename U> friend OwnArrayPtr<U> adoptArrayPtr(U*);
    explicit OwnArrayPtr(PtrType ptr) : m_ptr(ptr) { }

    PtrType m_ptr;
};

template<typename T>
inline OwnArrayPtr<T>::OwnArrayPtr(OwnArrayPtr&& other)
    : m_ptr(other.leakPtr())
{
}

template<typename T> template<typename U>
inline OwnArrayPtr<T>::OwnArrayPtr(OwnArrayPtr<U>&& other)
    : m_ptr(other.leakPtr())
{
}

template<typename T>
inline void OwnArrayPtr<T>::clear()
{
    PtrType ptr = m_ptr;
    m_ptr = 0;
    deleteOwnedArrayPtr(ptr);
}

template<typename T>
inline typename OwnArrayPtr<T>::PtrType OwnArrayPtr<T>::leakPtr()
{
    PtrType ptr = m_ptr;
    m_ptr = 0;
    return ptr;
}

template<typename T>
inline OwnArrayPtr<T>& OwnArrayPtr<T>::operator=(OwnArrayPtr&& other)
{
    auto ptr = std::move(other);
    swap(ptr);
    return *this;
}

template<typename T> template<typename U>
inline OwnArrayPtr<T>& OwnArrayPtr<T>::operator=(OwnArrayPtr<U>&& other)
{
    auto ptr = std::move(other);
    swap(ptr);
    return *this;
}

template <typename T> inline void swap(OwnArrayPtr<T>& a, OwnArrayPtr<T>& b)
{
    a.swap(b);
}

template<typename T, typename U> inline bool operator==(const OwnArrayPtr<T>& a, U* b)
{
    return a.get() == b; 
}

template<typename T, typename U> inline bool operator==(T* a, const OwnArrayPtr<U>& b) 
{
    return a == b.get(); 
}

template<typename T, typename U> inline bool operator!=(const OwnArrayPtr<T>& a, U* b)
{
    return a.get() != b; 
}

template<typename T, typename U> inline bool operator!=(T* a, const OwnArrayPtr<U>& b)
{
    return a != b.get(); 
}

template <typename T> inline T* getPtr(const OwnArrayPtr<T>& p)
{
    return p.get();
}

template<typename T> inline OwnArrayPtr<T> adoptArrayPtr(T* ptr)
{
    return OwnArrayPtr<T>(ptr);
}

template<typename T> inline void deleteOwnedArrayPtr(T* ptr)
{
    static_assert(sizeof(T) > 0, "deleteOwnedArrayPtr can not delete incomplete types");

    delete[] ptr;
}

} // namespace WTF

using WTF::OwnArrayPtr;
using WTF::adoptArrayPtr;

#endif // WTF_OwnArrayPtr_h
