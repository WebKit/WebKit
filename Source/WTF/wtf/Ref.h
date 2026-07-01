/*
 * Copyright (C) 2013-2020 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include <wtf/Assertions.h>
#include <wtf/ForbidHeapAllocation.h>
#include <wtf/Forward.h>
#include <wtf/GetPtr.h>
#include <wtf/RawPtrTraits.h>
#include <wtf/StdLibExtras.h>
#include <wtf/SwiftBridging.h>
#include <wtf/TypeCasts.h>

#if ASAN_ENABLED
extern "C" void __asan_poison_memory_region(void const volatile *addr, size_t size);
extern "C" void __asan_unpoison_memory_region(void const volatile *addr, size_t size);
extern "C" int __asan_address_is_poisoned(void const volatile *addr);
#endif

namespace WTF {

inline void adopted(const void*) { }

template<typename T> struct DefaultRefDerefTraits {
    static ALWAYS_INLINE T* refIfNotNull(T* ptr)
    {
        if (ptr) [[likely]]
            ptr->ref();
        return ptr;
    }

    static ALWAYS_INLINE T& ref(T& ref)
    {
        ref.ref();
        return ref;
    }

    static ALWAYS_INLINE void derefIfNotNull(T* ptr)
    {
        if (ptr) [[likely]]
            ptr->deref();
    }
};

template<typename T, typename PtrTraits, typename RefDerefTraits> class Ref;
template<typename T, typename PtrTraits = RawPtrTraits<T>, typename RefDerefTraits = DefaultRefDerefTraits<T>> Ref<T, PtrTraits, RefDerefTraits> adoptRef(T&);

template<typename T, typename _PtrTraits, typename RefDerefTraits>
class Ref {
    WTF_FORBID_HEAP_ALLOCATION_ALLOWING_PLACEMENT_NEW;
public:
    using PtrTraits = _PtrTraits;
    static constexpr bool isRef = true;

    ~Ref()
    {
#if ASAN_ENABLED
        if (__asan_address_is_poisoned(this))
            __asan_unpoison_memory_region(this, sizeof(*this));
#endif
        if (auto* ptr = PtrTraits::exchange(m_ptr, nullptr))
            RefDerefTraits::derefIfNotNull(ptr);
    }

    Ref(T& object)
        : m_ptr(&RefDerefTraits::ref(object))
    {
    }

    Ref(const Ref& other)
        : m_ptr(&RefDerefTraits::ref(other.get()))
    {
    }

    template<typename X, typename Y> Ref(const Ref<X, Y>& other)
        : m_ptr(&RefDerefTraits::ref(other.get()))
    {
    }

    Ref(Ref&& other)
        : m_ptr(&other.leakRef())
    {
        ASSERT(m_ptr);
    }

    template<typename X, typename Y>
    Ref(Ref<X, Y>&& other)
        : m_ptr(&other.leakRef())
    {
        ASSERT(m_ptr);
    }

    template<typename X, typename WeakPtrImplType>
    Ref(const WeakRef<X, WeakPtrImplType>& other) requires std::is_convertible_v<X*, T*>
        : m_ptr(&RefDerefTraits::ref(other.get()))
    {
    }

    Ref& operator=(T&);
    Ref& operator=(Ref&&);
    template<typename X, typename Y, typename Z> Ref& operator=(Ref<X, Y, Z>&&);

    Ref& operator=(const Ref&);
    template<typename X, typename Y, typename Z> Ref& operator=(const Ref<X, Y, Z>&);

    template<typename X, typename Y, typename Z> void swap(Ref<X, Y, Z>&);

    // Hash table deleted values, which are only constructed and never copied or destroyed.
    Ref(HashTableDeletedValueType) : m_ptr(PtrTraits::hashTableDeletedValue()) { }
    bool isHashTableDeletedValue() const { return PtrTraits::isHashTableDeletedValue(m_ptr); }

    Ref(HashTableEmptyValueType) : m_ptr(hashTableEmptyValue()) { }
    bool isHashTableEmptyValue() const { return m_ptr == hashTableEmptyValue(); }
    static T* hashTableEmptyValue() { return nullptr; }

    const T* ptrAllowingHashTableEmptyValue() const LIFETIME_BOUND { ASSERT(m_ptr || isHashTableEmptyValue()); return PtrTraits::unwrap(m_ptr); }
    T* ptrAllowingHashTableEmptyValue() LIFETIME_BOUND { ASSERT(m_ptr || isHashTableEmptyValue()); return PtrTraits::unwrap(m_ptr); }

    T* operator->() const LIFETIME_BOUND { ASSERT(m_ptr); return PtrTraits::unwrap(m_ptr); }
    T* ptr() const LIFETIME_BOUND RETURNS_NONNULL { ASSERT(m_ptr); return PtrTraits::unwrap(m_ptr); }
    T* unsafePtr() const RETURNS_NONNULL { ASSERT(m_ptr); return PtrTraits::unwrap(m_ptr); } // FIXME: Replace with ptr() then remove.
    T& get() const LIFETIME_BOUND { ASSERT(m_ptr); return *PtrTraits::unwrap(m_ptr); }
    T& unsafeGet() const { ASSERT(m_ptr); return *PtrTraits::unwrap(m_ptr); } // FIXME: Replace with get() then remove.
    operator T&() const LIFETIME_BOUND { ASSERT(m_ptr); return *PtrTraits::unwrap(m_ptr); }
    bool operator!() const { ASSERT(m_ptr); return !*m_ptr; }

    template<typename X, typename Y, typename Z> [[nodiscard]] Ref<T, PtrTraits, RefDerefTraits> replace(Ref<X, Y, Z>&&);

    // The following function is deprecated.
    Ref copyRef() && = delete;
    [[nodiscard]] Ref copyRef() const & { return Ref(*m_ptr); }

    [[nodiscard]] T& leakRef()
    {
        ASSERT(m_ptr);

        T& result = *PtrTraits::exchange(m_ptr, nullptr);
#if ASAN_ENABLED
        __asan_poison_memory_region(this, sizeof(*this));
#endif
        return result;
    }

private:
    friend Ref adoptRef<T>(T&);
    template<typename X, typename Y, typename Z> friend class Ref;

    template<typename X, typename Y, typename Z, typename U, typename V, typename W>
    friend bool operator==(const Ref<X, Y, Z>&, const Ref<U, V, W>&);

    enum AdoptTag { Adopt };
    Ref(T& object, AdoptTag)
        : m_ptr(&object)
    {
    }

    typename PtrTraits::StorageType m_ptr;
} SWIFT_ESCAPABLE;

// Template deduction guide.
template<typename X, typename Y> Ref(const WeakRef<X, Y>&) -> Ref<X, RawPtrTraits<X>, DefaultRefDerefTraits<X>>;
template<typename X, typename Y> Ref(WeakRef<X, Y>&) -> Ref<X, RawPtrTraits<X>, DefaultRefDerefTraits<X>>;

template<typename T, typename _PtrTraits, typename RefDerefTraits> Ref<T, _PtrTraits, RefDerefTraits> adoptRef(T&);

template<typename T, typename _PtrTraits, typename RefDerefTraits>
inline Ref<T, _PtrTraits, RefDerefTraits>& Ref<T, _PtrTraits, RefDerefTraits>::operator=(T& reference)
{
    Ref copiedReference = reference;
    swap(copiedReference);
    return *this;
}

template<typename T, typename _PtrTraits, typename RefDerefTraits>
inline Ref<T, _PtrTraits, RefDerefTraits>& Ref<T, _PtrTraits, RefDerefTraits>::operator=(Ref&& reference)
{
#if ASAN_ENABLED
    if (__asan_address_is_poisoned(this))
        __asan_unpoison_memory_region(this, sizeof(*this));
#endif
    Ref movedReference = WTF::move(reference);
    swap(movedReference);
    return *this;
}

template<typename T, typename _PtrTraits, typename RefDerefTraits>
template<typename U, typename _OtherPtrTraits, typename OtherRefDerefTraits>
inline Ref<T, _PtrTraits, RefDerefTraits>& Ref<T, _PtrTraits, RefDerefTraits>::operator=(Ref<U, _OtherPtrTraits, OtherRefDerefTraits>&& reference)
{
#if ASAN_ENABLED
    if (__asan_address_is_poisoned(this))
        __asan_unpoison_memory_region(this, sizeof(*this));
#endif
    Ref movedReference = WTF::move(reference);
    swap(movedReference);
    return *this;
}

template<typename T, typename _PtrTraits, typename RefDerefTraits>
inline Ref<T, _PtrTraits, RefDerefTraits>& Ref<T, _PtrTraits, RefDerefTraits>::operator=(const Ref& reference)
{
#if ASAN_ENABLED
    if (__asan_address_is_poisoned(this))
        __asan_unpoison_memory_region(this, sizeof(*this));
#endif
    Ref copiedReference = reference;
    swap(copiedReference);
    return *this;
}

template<typename T, typename _PtrTraits, typename RefDerefTraits>
template<typename U, typename _OtherPtrTraits, typename OtherRefDerefTraits>
inline Ref<T, _PtrTraits, RefDerefTraits>& Ref<T, _PtrTraits, RefDerefTraits>::operator=(const Ref<U, _OtherPtrTraits, OtherRefDerefTraits>& reference)
{
#if ASAN_ENABLED
    if (__asan_address_is_poisoned(this))
        __asan_unpoison_memory_region(this, sizeof(*this));
#endif
    Ref copiedReference = reference;
    swap(copiedReference);
    return *this;
}

template<typename X, typename APtrTraits, typename ARefDerefTraits, typename Y, typename BPtrTraits, typename BRefDerefTraits>
inline bool operator==(const Ref<X, APtrTraits, ARefDerefTraits>& a, const Ref<Y, BPtrTraits, BRefDerefTraits>& b)
{
    return a.m_ptr == b.m_ptr;
}

template<typename X, typename _PtrTraits, typename RefDerefTraits>
template<typename Y, typename _OtherPtrTraits, typename OtherRefDerefTraits>
inline void Ref<X, _PtrTraits, RefDerefTraits>::swap(Ref<Y, _OtherPtrTraits, OtherRefDerefTraits>& other)
{
    _PtrTraits::swap(m_ptr, other.m_ptr);
}

template<typename X, typename APtrTraits, typename ARefDerefTraits, typename Y, typename BPtrTraits, typename BRefDerefTraits>
    requires (!std::same_as<APtrTraits, RawPtrTraits<X>> || !std::same_as<BPtrTraits, RawPtrTraits<Y>>)
inline void swap(Ref<X, APtrTraits, ARefDerefTraits>& a, Ref<Y, BPtrTraits, BRefDerefTraits>& b)
{
    a.swap(b);
}

template<typename X, typename _PtrTraits, typename RefDerefTraits>
template<typename Y, typename _OtherPtrTraits, typename OtherRefDerefTraits>
inline Ref<X, _PtrTraits, RefDerefTraits> Ref<X, _PtrTraits, RefDerefTraits>::replace(Ref<Y, _OtherPtrTraits, OtherRefDerefTraits>&& reference)
{
#if ASAN_ENABLED
    if (__asan_address_is_poisoned(this))
        __asan_unpoison_memory_region(this, sizeof(*this));
#endif
    auto oldReference = adoptRef(*m_ptr);
    m_ptr = &reference.leakRef();
    return oldReference;
}

template<typename X, typename _PtrTraits = RawPtrTraits<X>, typename RefDerefTraits = DefaultRefDerefTraits<X>, typename Y, typename _OtherPtrTraits, typename OtherRefDerefTraits>
inline Ref<X, _PtrTraits, RefDerefTraits> upcast(Ref<Y, _OtherPtrTraits, OtherRefDerefTraits>&& reference)
{
    static_assert(!std::same_as<Y, X>, "Unnecessary cast to same type");
    static_assert(std::derived_from<Y, X>, "Should be an upcast");
    return adoptRef(static_cast<X&>(reference.leakRef()));
}

template<typename X, typename _PtrTraits = RawPtrTraits<X>, typename RefDerefTraits = DefaultRefDerefTraits<X>, typename Y, typename _OtherPtrTraits, typename OtherRefDerefTraits>
ALWAYS_INLINE Ref<X, _PtrTraits, RefDerefTraits> upcast(const Ref<Y, _OtherPtrTraits, OtherRefDerefTraits>& reference)
{
    static_assert(!std::same_as<Y, X>, "Unnecessary cast to same type");
    static_assert(std::derived_from<Y, X>, "Should be an upcast");
    return upcast<X, _PtrTraits, RefDerefTraits>(reference.copyRef());
}

template<typename X, typename _PtrTraits = RawPtrTraits<X>, typename RefDerefTraits = DefaultRefDerefTraits<X>, typename Y, typename _OtherPtrTraits, typename OtherRefDerefTraits>
inline Ref<X, _PtrTraits, RefDerefTraits> unsafeRefDowncast(Ref<Y, _OtherPtrTraits, OtherRefDerefTraits>&& reference)
{
    static_assert(!std::same_as<Y, X>, "Unnecessary cast to same type");
    static_assert(std::derived_from<X, Y>, "Use upcast instead");
    SUPPRESS_MEMORY_UNSAFE_CAST return adoptRef(static_cast<X&>(reference.leakRef()));
}

template<typename X, typename _PtrTraits = RawPtrTraits<X>, typename RefDerefTraits = DefaultRefDerefTraits<X>, typename Y, typename _OtherPtrTraits, typename OtherRefDerefTraits>
ALWAYS_INLINE Ref<X, _PtrTraits, RefDerefTraits> unsafeRefDowncast(const Ref<Y, _OtherPtrTraits, OtherRefDerefTraits>& reference)
{
    static_assert(!std::same_as<Y, X>, "Unnecessary cast to same type");
    static_assert(std::derived_from<X, Y>, "Use upcast instead");
    return unsafeRefDowncast<X, _PtrTraits, RefDerefTraits>(reference.copyRef());
}

template <typename T, typename _PtrTraits, typename RefDerefTraits>
struct GetPtrHelper<Ref<T, _PtrTraits, RefDerefTraits>> {
    using PtrType = T*;
    using UnderlyingType = T;
    static T* getPtr(const Ref<T, _PtrTraits, RefDerefTraits>& p) { return const_cast<T*>(p.ptr()); }
};

template <typename T, typename _PtrTraits, typename RefDerefTraits>
struct IsSmartPtr<Ref<T, _PtrTraits, RefDerefTraits>> {
    static constexpr bool value = true;
    static constexpr bool isNullable = false;
};

template<typename T, typename _PtrTraits, typename RefDerefTraits>
inline Ref<T, _PtrTraits, RefDerefTraits> adoptRef(T& reference)
{
    adopted(&reference);
    return Ref<T, _PtrTraits, RefDerefTraits>(reference, Ref<T, _PtrTraits, RefDerefTraits>::Adopt);
}

template<typename ExpectedType, typename ArgType, typename PtrTraits, typename RefDerefTraits>
inline bool is(const Ref<ArgType, PtrTraits, RefDerefTraits>& source)
{
    return is<ExpectedType>(source.get());
}

template<typename Target, typename Source, typename PtrTraits, typename RefDerefTraits>
inline Ref<match_constness_t<Source, Target>> uncheckedDowncast(Ref<Source, PtrTraits, RefDerefTraits> source)
{
    static_assert(!std::same_as<Source, Target>, "Unnecessary cast to same type");
    static_assert(std::derived_from<Target, Source>, "Should be a downcast");
    ASSERT_WITH_SECURITY_IMPLICATION(is<Target>(source));
    return unsafeRefDowncast<match_constness_t<Source, Target>>(WTF::move(source));
}

template<typename Target, typename Source, typename PtrTraits, typename RefDerefTraits>
inline Ref<match_constness_t<Source, Target>> downcast(Ref<Source, PtrTraits, RefDerefTraits> source)
{
    static_assert(!std::same_as<Source, Target>, "Unnecessary cast to same type");
    static_assert(std::derived_from<Target, Source>, "Should be a downcast");
    RELEASE_ASSERT(is<Target>(source));
    return unsafeRefDowncast<match_constness_t<Source, Target>>(WTF::move(source));
}

template<typename Target, typename Source, typename PtrTraits, typename RefDerefTraits>
inline RefPtr<match_constness_t<Source, Target>> dynamicDowncast(Ref<Source, PtrTraits, RefDerefTraits> source)
{
    static_assert(!std::same_as<Source, Target>, "Unnecessary cast to same type");
    static_assert(std::derived_from<Target, Source>, "Should be a downcast");
    if (!is<Target>(source))
        return nullptr;
    return unsafeRefDowncast<match_constness_t<Source, Target>>(WTF::move(source));
}

template<typename T, typename PtrTraits, typename RefDerefTraits>
inline bool arePointingToEqualData(const Ref<T, PtrTraits, RefDerefTraits>& a, const Ref<T, PtrTraits, RefDerefTraits>& b)
{
    return a.ptr() == b.ptr() || a.get() == b.get();
}

} // namespace WTF

using WTF::Ref;
using WTF::adoptRef;
using WTF::arePointingToEqualData;
using WTF::upcast;
using WTF::unsafeRefDowncast;
