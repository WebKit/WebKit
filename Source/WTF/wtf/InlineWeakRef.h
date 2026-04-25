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

#include <algorithm>
#include <utility>
#include <wtf/Assertions.h>
#include <wtf/Compiler.h>
#include <wtf/ForbidHeapAllocation.h>
#include <wtf/SwiftBridging.h>

namespace WTF {

template<typename T>
inline T& weakRef(T& ref)
{
    ref.weakRef();
    return ref;
}

template<typename T>
inline T* weakRefIfNotNull(T* ptr)
{
    if (ptr) [[likely]]
        ptr->weakRef();
    return ptr;
}

template<typename T>
inline void weakDerefIfNotNull(T* ptr)
{
    if (ptr) [[likely]]
        ptr->weakDeref();
}

template<typename T>
class InlineWeakRef {
    WTF_FORBID_HEAP_ALLOCATION_ALLOWING_PLACEMENT_NEW;
public:
    ALWAYS_INLINE InlineWeakRef(T& ptr) : m_ptr(&weakRef(ptr)) { }
    ALWAYS_INLINE InlineWeakRef(const InlineWeakRef& o) : m_ptr(weakRef(*o.m_ptr)) { }
    template<typename X> InlineWeakRef(const InlineWeakRef<X>& o) : m_ptr(weakRef(*o.m_ptr)) { }

    ALWAYS_INLINE InlineWeakRef(InlineWeakRef&& o) : m_ptr(&o.leakWeak()) { }
    template<typename X> InlineWeakRef(InlineWeakRef<X>&& o) : m_ptr(&o.leakWeak()) { }

    ALWAYS_INLINE ~InlineWeakRef() { weakDerefIfNotNull(m_ptr); }

    T& get() const LIFETIME_BOUND;
    T* ptr() const LIFETIME_BOUND;

    ALWAYS_INLINE T* operator->() const LIFETIME_BOUND { return ptr(); }

    InlineWeakRef& operator=(T&);

    template<typename X> void swap(InlineWeakRef<X>&);

private:
    template<typename X> friend class InlineWeakRef;

    [[nodiscard]] T& leakWeak();

    T* m_ptr;
} SWIFT_ESCAPABLE;

template<typename T>
T& InlineWeakRef<T>::get() const LIFETIME_BOUND
{
    RELEASE_ASSERT(m_ptr->refCount());
    return *m_ptr;
}

template<typename T>
T* InlineWeakRef<T>::ptr() const LIFETIME_BOUND
{
    RELEASE_ASSERT(m_ptr->refCount());
    return m_ptr;
}

template<typename T>
inline T& InlineWeakRef<T>::leakWeak()
{
    ASSERT(m_ptr);
    return *std::exchange(m_ptr, nullptr);
}

template<typename T>
inline InlineWeakRef<T>& InlineWeakRef<T>::operator=(T& optr)
{
    InlineWeakRef ptr = optr;
    swap(ptr);
    return *this;
}

template<class T>
template<typename X>
inline void InlineWeakRef<T>::swap(InlineWeakRef<X>& o)
{
    std::swap(m_ptr, o.m_ptr);
}

} // namespace WTF

using WTF::InlineWeakRef;
