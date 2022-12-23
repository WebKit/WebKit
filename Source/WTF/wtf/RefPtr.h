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

template<typename T> struct DefaultRefDerefTraits {
    static ALWAYS_INLINE void refIfNotNull(T* ptr)
    {
        if (LIKELY(ptr != nullptr))
            ptr->ref();
    }

    static ALWAYS_INLINE void derefIfNotNull(T* ptr)
    {
        if (LIKELY(ptr != nullptr))
            ptr->deref();
    }
};

template<typename T, typename PtrTraits, typename RefDerefTraits> class RefPtr;
template<typename T, typename PtrTraits = RawPtrTraits<T>, typename RefDerefTraits = DefaultRefDerefTraits<T>> RefPtr<T, PtrTraits, RefDerefTraits> adoptRef(T*);

template<typename T, typename _PtrTraits, typename _RefDerefTraits>
class RefPtr {
    WTF_MAKE_FAST_ALLOCATED;
public:
    using PtrTraits = _PtrTraits;
    using RefDerefTraits = _RefDerefTraits;
    typedef T ValueType;
    typedef ValueType* PtrType;

    static constexpr bool isRefPtr = true;

    ALWAYS_INLINE constexpr RefPtr() : m_ptr(nullptr) { }
    ALWAYS_INLINE constexpr RefPtr(std::nullptr_t) : m_ptr(nullptr) { }
    ALWAYS_INLINE RefPtr(T* ptr) : m_ptr(ptr) { RefDerefTraits::refIfNotNull(ptr); }
    ALWAYS_INLINE RefPtr(const RefPtr& o) : m_ptr(o.m_ptr) { RefDerefTraits::refIfNotNull(PtrTraits::unwrap(m_ptr)); }
    template<typename X, typename Y, typename Z> RefPtr(const RefPtr<X, Y, Z>& o) : m_ptr(o.get()) { RefDerefTraits::refIfNotNull(PtrTraits::unwrap(m_ptr)); }

    ALWAYS_INLINE RefPtr(RefPtr&& o) : m_ptr(o.leakRef()) { }
    template<typename X, typename Y, typename Z> RefPtr(RefPtr<X, Y, Z>&& o) : m_ptr(o.leakRef()) { }
    template<typename X, typename Y> RefPtr(Ref<X, Y>&&);

    // Hash table deleted values, which are only constructed and never copied or destroyed.
    RefPtr(HashTableDeletedValueType) : m_ptr(PtrTraits::hashTableDeletedValue()) { }
    bool isHashTableDeletedValue() const { return PtrTraits::isHashTableDeletedValue(m_ptr); }

    ALWAYS_INLINE ~RefPtr() { RefDerefTraits::derefIfNotNull(PtrTraits::exchange(m_ptr, nullptr)); }

    T* get() const { return PtrTraits::unwrap(m_ptr); }

    Ref<T> releaseNonNull() { ASSERT(m_ptr); Ref<T> tmp(adoptRef(*m_ptr)); m_ptr = nullptr; return tmp; }
    Ref<const T> releaseConstNonNull() { ASSERT(m_ptr); Ref<const T> tmp(adoptRef(*m_ptr)); m_ptr = nullptr; return tmp; }

    T* leakRef() WARN_UNUSED_RETURN;

    T& operator*() const { ASSERT(m_ptr); return *PtrTraits::unwrap(m_ptr); }
    ALWAYS_INLINE T* operator->() const { return PtrTraits::unwrap(m_ptr); }

    bool operator!() const { return !m_ptr; }

    // This conversion operator allows implicit conversion to bool but not to other integer types.
    using UnspecifiedBoolType = void (RefPtr::*)() const;
    operator UnspecifiedBoolType() const { return m_ptr ? &RefPtr::unspecifiedBoolTypeInstance : nullptr; }

    explicit operator bool() const { return !!m_ptr; }
    
    RefPtr& operator=(const RefPtr&);
    RefPtr& operator=(T*);
    RefPtr& operator=(std::nullptr_t);
    template<typename X, typename Y, typename Z> RefPtr& operator=(const RefPtr<X, Y, Z>&);
    RefPtr& operator=(RefPtr&&);
    template<typename X, typename Y, typename Z> RefPtr& operator=(RefPtr<X, Y, Z>&&);
    template<typename X> RefPtr& operator=(Ref<X>&&);

    template<typename X, typename Y, typename Z> void swap(RefPtr<X, Y, Z>&);

    RefPtr copyRef() && = delete;
    RefPtr copyRef() const & WARN_UNUSED_RETURN { return RefPtr(m_ptr); }

private:
    void unspecifiedBoolTypeInstance() const { }

    friend RefPtr adoptRef<T, PtrTraits, RefDerefTraits>(T*);
    template<typename X, typename Y, typename Z> friend class RefPtr;

    template<typename T1, typename U, typename V, typename X, typename Y, typename Z>
    friend bool operator==(const RefPtr<T1, U, V>&, const RefPtr<X, Y, Z>&);
    template<typename T1, typename U, typename V, typename X>
    friend bool operator==(const RefPtr<T1, U, V>&, X*);
    template<typename T1, typename X, typename Y, typename Z>
    friend bool operator==(T1*, const RefPtr<X, Y, Z>&);

    enum AdoptTag { Adopt };
    RefPtr(T* ptr, AdoptTag) : m_ptr(ptr) { }

    typename PtrTraits::StorageType m_ptr;
};

// Template deduction guide.
template<typename X, typename Y> RefPtr(Ref<X, Y>&&) -> RefPtr<X, Y, DefaultRefDerefTraits<X>>;

template<typename T, typename U, typename V>
template<typename X, typename Y>
inline RefPtr<T, U, V>::RefPtr(Ref<X, Y>&& reference)
    : m_ptr(&reference.leakRef())
{
}

template<typename T, typename U, typename V>
inline T* RefPtr<T, U, V>::leakRef()
{
    return U::exchange(m_ptr, nullptr);
}

template<typename T, typename U, typename V>
inline RefPtr<T, U, V>& RefPtr<T, U, V>::operator=(const RefPtr& o)
{
    RefPtr ptr = o;
    swap(ptr);
    return *this;
}

template<typename T, typename U, typename V>
template<typename X, typename Y, typename Z>
inline RefPtr<T, U, V>& RefPtr<T, U, V>::operator=(const RefPtr<X, Y, Z>& o)
{
    RefPtr ptr = o;
    swap(ptr);
    return *this;
}

template<typename T, typename U, typename V>
inline RefPtr<T, U, V>& RefPtr<T, U, V>::operator=(T* optr)
{
    RefPtr ptr = optr;
    swap(ptr);
    return *this;
}

template<typename T, typename U, typename V>
inline RefPtr<T, U, V>& RefPtr<T, U, V>::operator=(std::nullptr_t)
{
    V::derefIfNotNull(U::exchange(m_ptr, nullptr));
    return *this;
}

template<typename T, typename U, typename V>
inline RefPtr<T, U, V>& RefPtr<T, U, V>::operator=(RefPtr&& o)
{
    RefPtr ptr = WTFMove(o);
    swap(ptr);
    return *this;
}

template<typename T, typename U, typename V>
template<typename X, typename Y, typename Z>
inline RefPtr<T, U, V>& RefPtr<T, U, V>::operator=(RefPtr<X, Y, Z>&& o)
{
    RefPtr ptr = WTFMove(o);
    swap(ptr);
    return *this;
}

template<typename T, typename V, typename W>
template<typename U>
inline RefPtr<T, V, W>& RefPtr<T, V, W>::operator=(Ref<U>&& reference)
{
    RefPtr ptr = WTFMove(reference);
    swap(ptr);
    return *this;
}

template<class T, typename U, typename V>
template<typename X, typename Y, typename Z>
inline void RefPtr<T, U, V>::swap(RefPtr<X, Y, Z>& o)
{
    U::swap(m_ptr, o.m_ptr);
}

template<typename T, typename U, typename V>
inline void swap(RefPtr<T, U, V>& a, RefPtr<T, U, V>& b)
{
    a.swap(b);
}

template<typename T, typename U, typename V, typename X, typename Y, typename Z>
inline bool operator==(const RefPtr<T, U, V>& a, const RefPtr<X, Y, Z>& b)
{
    return a.m_ptr == b.m_ptr;
}

template<typename T, typename U, typename V, typename X>
inline bool operator==(const RefPtr<T, U, V>& a, X* b)
{
    return a.m_ptr == b;
}

template<typename T, typename X, typename Y, typename Z>
inline bool operator==(T* a, const RefPtr<X, Y, Z>& b)
{
    return a == b.m_ptr;
}

template<typename T, typename U, typename V, typename X, typename Y, typename Z>
inline bool operator!=(const RefPtr<T, U, V>& a, const RefPtr<X, Y, Z>& b)
{
    return !(a == b);
}

template<typename T, typename U, typename V, typename X>
inline bool operator!=(const RefPtr<T, U, V>& a, X* b)
{
    return !(a == b);
}

template<typename T, typename X, typename Y, typename Z>
inline bool operator!=(T* a, const RefPtr<X, Y, Z>& b)
{
    return !(a == b);
}

template<typename T, typename U, typename V>
inline RefPtr<T, U, V> adoptRef(T* p)
{
    adopted(p);
    return RefPtr<T, U, V>(p, RefPtr<T, U, V>::Adopt);
}

template<typename T, typename U = RawPtrTraits<T>, typename V = DefaultRefDerefTraits<T>, typename X, typename Y, typename Z>
inline RefPtr<T, U, V> static_pointer_cast(const RefPtr<X, Y, Z>& p)
{ 
    return RefPtr<T, U, V>(static_cast<T*>(p.get()));
}

template<typename T, typename U = RawPtrTraits<T>, typename V = DefaultRefDerefTraits<T>, typename X, typename Y, typename Z>
inline RefPtr<T, U, V> static_pointer_cast(RefPtr<X, Y, Z>&& p)
{
    return adoptRef(static_cast<T*>(p.leakRef()));
}

template <typename T, typename U, typename V>
struct IsSmartPtr<RefPtr<T, U, V>> {
    static constexpr bool value = true;
};

template<typename ExpectedType, typename ArgType, typename PtrTraits, typename RefDerefTraits>
inline bool is(RefPtr<ArgType, PtrTraits, RefDerefTraits>& source)
{
    return is<ExpectedType>(source.get());
}

template<typename ExpectedType, typename ArgType, typename PtrTraits, typename RefDerefTraits>
inline bool is(const RefPtr<ArgType, PtrTraits, RefDerefTraits>& source)
{
    return is<ExpectedType>(source.get());
}

} // namespace WTF

using WTF::RefPtr;
using WTF::adoptRef;
using WTF::static_pointer_cast;
