/*
 *  Copyright (C) 2005-2025 Apple Inc. All rights reserved.
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

#pragma once

#include <wtf/HashTraits.h>
#include <wtf/InlineWeakRef.h>

namespace WTF {

template<typename T>
class InlineWeakPtr {
    WTF_FORBID_HEAP_ALLOCATION_ALLOWING_PLACEMENT_NEW;
public:
    ALWAYS_INLINE constexpr InlineWeakPtr() : m_ptr(nullptr) { }
    ALWAYS_INLINE constexpr InlineWeakPtr(std::nullptr_t) : m_ptr(nullptr) { }
    ALWAYS_INLINE InlineWeakPtr(T* ptr) : m_ptr(weakRefIfNotNull(ptr)) { }
    ALWAYS_INLINE InlineWeakPtr(T& ptr) : m_ptr(&weakRef(ptr)) { }
    ALWAYS_INLINE InlineWeakPtr(const InlineWeakPtr& o) : m_ptr(weakRefIfNotNull(o.m_ptr)) { }
    template<typename X> InlineWeakPtr(const InlineWeakPtr<X>& o) : m_ptr(weakRefIfNotNull(o.m_ptr)) { }

    ALWAYS_INLINE InlineWeakPtr(InlineWeakPtr&& o) : m_ptr(o.leakWeak()) { }
    template<typename X> InlineWeakPtr(InlineWeakPtr<X>&& o) : m_ptr(o.leakWeak()) { }

    static T* hashTableDeletedValue() { return std::bit_cast<T*>(static_cast<uintptr_t>(-1)); }
    InlineWeakPtr(HashTableDeletedValueType) : m_ptr(hashTableDeletedValue()) { }
    InlineWeakPtr(HashTableEmptyValueType) : m_ptr(nullptr) { }

    ALWAYS_INLINE ~InlineWeakPtr() { weakDerefIfNotNull(m_ptr); }

    bool isHashTableDeletedValue() const { return m_ptr == hashTableDeletedValue(); }
    bool isHashTableEmptyValue() const { return !m_ptr; }
    bool isWeakNullValue() const { return !m_ptr->refCount(); }

    T* get() const LIFETIME_BOUND;

    WARN_UNUSED_RETURN T* leakWeak();

    T& operator*() const LIFETIME_BOUND { ASSERT(get()); return *get(); }
    ALWAYS_INLINE T* operator->() const LIFETIME_BOUND { return get(); }

    bool operator!() const { return !m_ptr || !m_ptr->refCount(); }

    explicit operator bool() const { return m_ptr && m_ptr->refCount(); }

    InlineWeakPtr& operator=(T*);
    InlineWeakPtr& operator=(std::nullptr_t);
    InlineWeakPtr& operator=(const InlineWeakPtr&);
    InlineWeakPtr& operator=(InlineWeakPtr&&);

    template<typename X> void swap(InlineWeakPtr<X>&);

private:
    template<typename X> friend class InlineWeakPtr;

    T* m_ptr;
} SWIFT_ESCAPABLE;

template<typename T>
T* InlineWeakPtr<T>::get() const LIFETIME_BOUND
{
    if (!m_ptr)
        return nullptr;
    if (!m_ptr->refCount())
        return nullptr;
    return m_ptr;
}

template<typename T>
inline T* InlineWeakPtr<T>::leakWeak()
{
    return std::exchange(m_ptr, nullptr);
}

template<typename T>
inline InlineWeakPtr<T>& InlineWeakPtr<T>::operator=(T* optr)
{
    InlineWeakPtr ptr = optr;
    swap(ptr);
    return *this;
}

template<typename T>
inline InlineWeakPtr<T>& InlineWeakPtr<T>::operator=(std::nullptr_t)
{
    weakDerefIfNotNull(std::exchange(m_ptr, nullptr));
    return *this;
}

template<typename T>
inline InlineWeakPtr<T>& InlineWeakPtr<T>::operator=(const InlineWeakPtr& o)
{
    InlineWeakPtr ptr = o;
    swap(ptr);
    return *this;
}

template<typename T>
inline InlineWeakPtr<T>& InlineWeakPtr<T>::operator=(InlineWeakPtr&& o)
{
    InlineWeakPtr ptr = WTF::move(o);
    swap(ptr);
    return *this;
}

template<class T>
template<typename X>
inline void InlineWeakPtr<T>::swap(InlineWeakPtr<X>& o)
{
    std::swap(m_ptr, o.m_ptr);
}

template<typename T, typename U>
inline bool operator==(const InlineWeakPtr<T>& a, const InlineWeakPtr<U>& b)
{
    return a.get() == b.get();
}

template<typename T, typename U>
inline bool operator==(const InlineWeakPtr<T>& a, U* b)
{
    return a.get() == b;
}

template <typename T>
struct GetPtrHelper<InlineWeakPtr<T>> {
    using PtrType = T*;
    using UnderlyingType = T;
    static T* getPtr(const InlineWeakPtr<T>& p) { return const_cast<T*>(p.get()); }
};

template <typename T>
struct IsSmartPtr<InlineWeakPtr<T>> {
    static constexpr bool value = true;
    static constexpr bool isNullable = true;
};

template<typename P> struct InlineWeakPtrHashTraits : public SimpleClassHashTraits<InlineWeakPtr<P>> {
    static constexpr bool emptyValueIsZero = true;
    static P* emptyValue() { return nullptr; }

    template <typename>
    static void constructEmptyValue(InlineWeakPtr<P>& slot)
    {
        new (NotNull, std::addressof(slot)) InlineWeakPtr<P>();
    }

    static constexpr bool hasIsEmptyValueFunction = true;
    static bool isEmptyValue(const InlineWeakPtr<P>& value) { return value.isHashTableEmptyValue(); }

    static constexpr bool hasIsWeakNullValueFunction = true;
    static bool isWeakNullValue(const InlineWeakPtr<P>& value) { return value.isWeakNullValue(); }

    using PeekType = P*;
    static PeekType peek(const InlineWeakPtr<P>& value) { return const_cast<PeekType>(value.get()); }
    static PeekType peek(P* value) { return value; }

    using TakeType = InlineWeakPtr<P>;
    static TakeType take(InlineWeakPtr<P>&& value) { return isEmptyValue(value) ? nullptr : InlineWeakPtr<P>(WTF::move(value)); }
};

template<typename P> struct HashTraits<InlineWeakPtr<P>> : InlineWeakPtrHashTraits<P> { };

template<typename P> struct PtrHash<InlineWeakPtr<P>> : PtrHashBase<InlineWeakPtr<P>, IsSmartPtr<InlineWeakPtr<P>>::value> {
    static constexpr bool safeToCompareToEmptyOrDeleted = false;
};

template<typename P> struct DefaultHash<InlineWeakPtr<P>> : PtrHash<InlineWeakPtr<P>> { };

} // namespace WTF

using WTF::InlineWeakPtr;
