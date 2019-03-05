/*
 *  Copyright (C) 2005-2019 Apple Inc. All rights reserved.
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

// RefPtr is documented at http://webkit.org/coding/RefPtr.html

#pragma once

#include <algorithm>
#include <utility>
#include <wtf/FastMalloc.h>
#include <wtf/Ref.h>

namespace WTF {

template<typename T, typename PtrTraits> class RefPtr;
template<typename T, typename PtrTraits = DumbPtrTraits<T>> RefPtr<T, PtrTraits> adoptRef(T*);

template<typename T> ALWAYS_INLINE void refIfNotNull(T* ptr)
{
    if (LIKELY(ptr != nullptr))
        ptr->ref();
}

template<typename T> ALWAYS_INLINE void derefIfNotNull(T* ptr)
{
    if (LIKELY(ptr != nullptr))
        ptr->deref();
}

template<typename T, typename PtrTraits>
class RefPtr {
    WTF_MAKE_FAST_ALLOCATED;
public:
    typedef T ValueType;
    typedef ValueType* PtrType;

    static constexpr bool isRefPtr = true;

    ALWAYS_INLINE constexpr RefPtr() : m_ptr(nullptr) { }
    ALWAYS_INLINE RefPtr(T* ptr) : m_ptr(ptr) { refIfNotNull(ptr); }
    ALWAYS_INLINE RefPtr(const RefPtr& o) : m_ptr(o.m_ptr) { refIfNotNull(PtrTraits::unwrap(m_ptr)); }
    template<typename X, typename Y> RefPtr(const RefPtr<X, Y>& o) : m_ptr(o.get()) { refIfNotNull(PtrTraits::unwrap(m_ptr)); }

    ALWAYS_INLINE RefPtr(RefPtr&& o) : m_ptr(o.leakRef()) { }
    template<typename X, typename Y> RefPtr(RefPtr<X, Y>&& o) : m_ptr(o.leakRef()) { }
    template<typename X, typename Y> RefPtr(Ref<X, Y>&&);

    // Hash table deleted values, which are only constructed and never copied or destroyed.
    RefPtr(HashTableDeletedValueType) : m_ptr(hashTableDeletedValue()) { }
    bool isHashTableDeletedValue() const { return m_ptr == hashTableDeletedValue(); }

    ALWAYS_INLINE ~RefPtr() { derefIfNotNull(PtrTraits::exchange(m_ptr, nullptr)); }

    T* get() const { return PtrTraits::unwrap(m_ptr); }

    Ref<T> releaseNonNull() { ASSERT(m_ptr); Ref<T> tmp(adoptRef(*m_ptr)); m_ptr = nullptr; return tmp; }
    Ref<const T> releaseConstNonNull() { ASSERT(m_ptr); Ref<const T> tmp(adoptRef(*m_ptr)); m_ptr = nullptr; return tmp; }

    T* leakRef() WARN_UNUSED_RETURN;

    T& operator*() const { ASSERT(m_ptr); return *PtrTraits::unwrap(m_ptr); }
    ALWAYS_INLINE T* operator->() const { return PtrTraits::unwrap(m_ptr); }

    bool operator!() const { return !m_ptr; }

    // This conversion operator allows implicit conversion to bool but not to other integer types.
    typedef T* (RefPtr::*UnspecifiedBoolType);
    operator UnspecifiedBoolType() const { return m_ptr ? &RefPtr::m_ptr : nullptr; }

    explicit operator bool() const { return !!m_ptr; }
    
    RefPtr& operator=(const RefPtr&);
    RefPtr& operator=(T*);
    RefPtr& operator=(std::nullptr_t);
    template<typename X, typename Y> RefPtr& operator=(const RefPtr<X, Y>&);
    RefPtr& operator=(RefPtr&&);
    template<typename X, typename Y> RefPtr& operator=(RefPtr<X, Y>&&);
    template<typename X> RefPtr& operator=(Ref<X>&&);

    template<typename X, typename Y> void swap(RefPtr<X, Y>&);

    static T* hashTableDeletedValue() { return reinterpret_cast<T*>(-1); }

    RefPtr copyRef() && = delete;
    RefPtr copyRef() const & WARN_UNUSED_RETURN { return RefPtr(m_ptr); }

private:
    friend RefPtr adoptRef<T, PtrTraits>(T*);
    template<typename X, typename Y> friend class RefPtr;

    enum AdoptTag { Adopt };
    RefPtr(T* ptr, AdoptTag) : m_ptr(ptr) { }

    typename PtrTraits::StorageType m_ptr;
};

template<typename T, typename U>
template<typename X, typename Y>
inline RefPtr<T, U>::RefPtr(Ref<X, Y>&& reference)
    : m_ptr(&reference.leakRef())
{
}

template<typename T, typename U>
inline T* RefPtr<T, U>::leakRef()
{
    return U::exchange(m_ptr, nullptr);
}

template<typename T, typename U>
inline RefPtr<T, U>& RefPtr<T, U>::operator=(const RefPtr& o)
{
    RefPtr ptr = o;
    swap(ptr);
    return *this;
}

template<typename T, typename U>
template<typename X, typename Y>
inline RefPtr<T, U>& RefPtr<T, U>::operator=(const RefPtr<X, Y>& o)
{
    RefPtr ptr = o;
    swap(ptr);
    return *this;
}

template<typename T, typename U>
inline RefPtr<T, U>& RefPtr<T, U>::operator=(T* optr)
{
    RefPtr ptr = optr;
    swap(ptr);
    return *this;
}

template<typename T, typename U>
inline RefPtr<T, U>& RefPtr<T, U>::operator=(std::nullptr_t)
{
    derefIfNotNull(U::exchange(m_ptr, nullptr));
    return *this;
}

template<typename T, typename U>
inline RefPtr<T, U>& RefPtr<T, U>::operator=(RefPtr&& o)
{
    RefPtr ptr = WTFMove(o);
    swap(ptr);
    return *this;
}

template<typename T, typename U>
template<typename X, typename Y>
inline RefPtr<T, U>& RefPtr<T, U>::operator=(RefPtr<X, Y>&& o)
{
    RefPtr ptr = WTFMove(o);
    swap(ptr);
    return *this;
}

template<typename T, typename V>
template<typename U>
inline RefPtr<T, V>& RefPtr<T, V>::operator=(Ref<U>&& reference)
{
    RefPtr ptr = WTFMove(reference);
    swap(ptr);
    return *this;
}

template<class T, typename U>
template<typename X, typename Y>
inline void RefPtr<T, U>::swap(RefPtr<X, Y>& o)
{
    U::swap(m_ptr, o.m_ptr);
}

template<typename T, typename U, typename X, typename Y, typename = std::enable_if_t<!std::is_same<U, DumbPtrTraits<T>>::value || !std::is_same<Y, DumbPtrTraits<X>>::value>>
inline void swap(RefPtr<T, U>& a, RefPtr<X, Y>& b)
{
    a.swap(b);
}

template<typename T, typename U, typename X, typename Y>
inline bool operator==(const RefPtr<T, U>& a, const RefPtr<X, Y>& b)
{ 
    return a.get() == b.get();
}

template<typename T, typename U, typename X>
inline bool operator==(const RefPtr<T, U>& a, X* b)
{ 
    return a.get() == b; 
}

template<typename T, typename X, typename Y>
inline bool operator==(T* a, const RefPtr<X, Y>& b)
{
    return a == b.get(); 
}

template<typename T, typename U, typename X, typename Y>
inline bool operator!=(const RefPtr<T, U>& a, const RefPtr<X, Y>& b)
{ 
    return a.get() != b.get(); 
}

template<typename T, typename U, typename X>
inline bool operator!=(const RefPtr<T, U>& a, X* b)
{
    return a.get() != b; 
}

template<typename T, typename X, typename Y>
inline bool operator!=(T* a, const RefPtr<X, Y>& b)
{ 
    return a != b.get(); 
}

template<typename T, typename U = DumbPtrTraits<T>, typename X, typename Y>
inline RefPtr<T, U> static_pointer_cast(const RefPtr<X, Y>& p)
{ 
    return RefPtr<T, U>(static_cast<T*>(p.get()));
}

template <typename T, typename U>
struct IsSmartPtr<RefPtr<T, U>> {
    static const bool value = true;
};

template<typename T, typename U>
inline RefPtr<T, U> adoptRef(T* p)
{
    adopted(p);
    return RefPtr<T, U>(p, RefPtr<T, U>::Adopt);
}

template<typename T> inline RefPtr<T> makeRefPtr(T* pointer)
{
    return pointer;
}

template<typename T> inline RefPtr<T> makeRefPtr(T& reference)
{
    return &reference;
}

template<typename ExpectedType, typename ArgType, typename PtrTraits>
inline bool is(RefPtr<ArgType, PtrTraits>& source)
{
    return is<ExpectedType>(source.get());
}

template<typename ExpectedType, typename ArgType, typename PtrTraits>
inline bool is(const RefPtr<ArgType, PtrTraits>& source)
{
    return is<ExpectedType>(source.get());
}

} // namespace WTF

using WTF::RefPtr;
using WTF::adoptRef;
using WTF::makeRefPtr;
using WTF::static_pointer_cast;
