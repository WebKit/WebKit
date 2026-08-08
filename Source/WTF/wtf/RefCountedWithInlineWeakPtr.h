/*
 * Copyright (C) 2025-2026 Apple Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#pragma once

#include <cstddef>
#include <type_traits>
#include <wtf/Nonallocatable.h>
#include <wtf/Ref.h>
#include <wtf/RefCountDebugger.h>
#include <wtf/StdLibExtras.h>
#include <wtf/TZoneMalloc.h>

namespace WTF {

// C++ says that accessing T after ~T() is undef. So we store the refcount bits for
// T in a separate header object, like so:
//
//     [ RefCountHeader ] [ T ] <-- header + object
//     ------------------------
//     [ RefCountedStorage<T> ] <-- underlying storage

class RefCountHeader {
public:
    void ref() const
    {
        m_refCountDebugger.willRef(m_strongCount);
        ++m_strongCount;
    }

    template<typename T> void deref(T* object) const;
    template<typename T> void weakDeref(T* object) const;

    bool hasOneRef() const { return m_strongCount == 1; }
    unsigned refCount() const { return m_strongCount; }

    void weakRef() const { ++m_weakCount; }

    RefCountDebugger& refCountDebugger() const { return const_cast<RefCountDebugger&>(m_refCountDebugger); }

private:
    template<typename T> void derefSlowCase(T*) const;
    template<typename T> void weakDerefSlowCase(T*) const;

    mutable unsigned m_strongCount { 1 };
    mutable unsigned m_weakCount { 1 }; // The strong counts collectively share one weak count.
    NO_UNIQUE_ADDRESS RefCountDebugger m_refCountDebugger;
};

template<typename T> struct RefCountedStorage final {
    WTF_MAKE_TZONE_ALLOCATED_TEMPLATE(RefCountedStorage);
public:
    using RefCountedType = typename T::RefCountedType;
    static_assert(alignof(T) == alignof(RefCountedType),
        "Types that inherit from RefCountedWithInlineWeakPtr<T> must not require more alignment than T");

    static constexpr bool allowCompactPointers = []() constexpr {
        if constexpr (requires { T::allowCompactPointers; })
            return T::allowCompactPointers;
        return false;
    }();

    alignas(RefCountHeader) std::byte header[sizeof(RefCountHeader)];
    alignas(RefCountedType) std::byte object[sizeof(T)];
};
WTF_MAKE_TZONE_ALLOCATED_TEMPLATE_IMPL(template<typename T>, RefCountedStorage<T>);

template<typename T>
ALWAYS_INLINE RefCountedStorage<std::remove_cv_t<T>>& refCountedStorage(const T* object)
{
    using U = std::remove_cv_t<T>;
    static_assert(std::is_same_v<U, typename U::RefCountedType>,
        "T must be the concrete RefCountedWithInlineWeakPtr<T> type");
    WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN
    auto* bytes = reinterpret_cast<std::byte*>(const_cast<U*>(object));
    return *reinterpret_cast<RefCountedStorage<U>*>(bytes - offsetof(RefCountedStorage<U>, object));
    WTF_ALLOW_UNSAFE_BUFFER_USAGE_END
}

template<typename T>
ALWAYS_INLINE RefCountHeader& refCountHeader(const T* object)
{
    return *reinterpret_cast<RefCountHeader*>(refCountedStorage(object).header);
}

template<typename T>
ALWAYS_INLINE void RefCountHeader::deref(T* object) const
{
    static_assert(std::is_same_v<T, typename T::RefCountedType>);

    m_refCountDebugger.willDeref(m_strongCount);

    if (m_strongCount != 1) [[likely]] {
        --m_strongCount;
        return;
    }

    m_refCountDebugger.willDelete();
    derefSlowCase(object);
}

template<typename T>
void RefCountHeader::derefSlowCase(T* object) const
{
    object->~T();

    m_refCountDebugger.willDestroy(m_strongCount);
    RELEASE_ASSERT(m_strongCount == 1);

    m_strongCount = 0;
    weakDeref(object);
}

template<typename T>
ALWAYS_INLINE void RefCountHeader::weakDeref(T* object) const
{
    static_assert(std::is_same_v<T, typename T::RefCountedType>);

    if (m_weakCount != 1) [[likely]] {
        --m_weakCount;
        return;
    }

    weakDerefSlowCase(object);
}

template<typename T>
void RefCountHeader::weakDerefSlowCase(T* object) const
{
    auto* storage = &refCountedStorage(object);
    const_cast<RefCountHeader*>(this)->~RefCountHeader();
    RefCountedStorage<T>::freeAfterDestruction(storage);
}

// NOTE: RefCountedWithInlineWeakPtr<T> must be the first base class.
//      class T : public RefCountedWithInlineWeakPtr<T>, public U.... // OK
//      class U : public T, public V... // OK
//
//      class T : public U, public RefCountedWithInlineWeakPtr<T>.... // RELEASE_ASSERT
//      class U : public V, public T.... // RELEASE_ASSERT
template<typename T> class RefCountedWithInlineWeakPtr {
    WTF_MAKE_NONCOPYABLE(RefCountedWithInlineWeakPtr);
    WTF_MAKE_NONALLOCATABLE(RefCountedWithInlineWeakPtr); // Use createRefCountedWithInlineWeakPtr<T>()
public:
    using RefCountedType = T;

    void ref() const { header().ref(); }
    void deref() const { header().deref(object()); }
    bool hasOneRef() const { return header().hasOneRef(); }
    unsigned refCount() const { return header().refCount(); }
    RefCountDebugger& refCountDebugger() const { return header().refCountDebugger(); }

    // C++ says that calling a member function on T after ~T() is undef, so weakRef()
    // and weakDeref() are not member functions. InlineWeakPtr grabs the header and
    // calls weakRef() and weakDeref() on it instead.

    template<typename U = T, typename... Args>
    static U* create(Args&&... args)
    {
        static_assert(std::is_base_of_v<T, U>);
        static_assert(std::is_same_v<T, typename U::RefCountedType>);
        static_assert(std::is_same_v<T, U> || std::has_virtual_destructor_v<T>,
            "Polymorphic RefCountedWithInlineWeakPtr<T> requires T to have a virtual destructor");
        auto* storage = new RefCountedStorage<U>;
        ::new (static_cast<void*>(storage->header)) RefCountHeader;
        auto* result = ::new (static_cast<void*>(storage->object)) U(std::forward<Args>(args)...);
        RELEASE_ASSERT(static_cast<T*>(result) == static_cast<void*>(storage->object));
        return result;
    }

protected:
    RefCountedWithInlineWeakPtr() = default;

private:
    T* object() const { return const_cast<T*>(static_cast<const T*>(this)); }
    RefCountHeader& header() const { return refCountHeader(object()); }
} SWIFT_RETURNED_AS_UNRETAINED_BY_DEFAULT;

template<typename U>
    requires requires { typename U::RefCountedType; }
inline void adopted(U* object)
{
    if (!object)
        return;
    using T = typename U::RefCountedType;
    refCountHeader(static_cast<const T*>(object)).refCountDebugger().adopted();
}

template<typename U, typename... Args>
Ref<U> createRefCountedWithInlineWeakPtr(Args&&... args)
{
    using T = typename U::RefCountedType;
    return adoptRef(*RefCountedWithInlineWeakPtr<T>::template create<U>(std::forward<Args>(args)...));
}

} // namespace WTF

using WTF::RefCountedWithInlineWeakPtr;
using WTF::createRefCountedWithInlineWeakPtr;
