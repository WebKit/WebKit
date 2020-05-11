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
#include <wtf/DumbPtrTraits.h>
#include <wtf/Forward.h>
#include <wtf/GetPtr.h>
#include <wtf/StdLibExtras.h>
#include <wtf/TypeCasts.h>

#if ASAN_ENABLED
extern "C" void __asan_poison_memory_region(void const volatile *addr, size_t size);
extern "C" void __asan_unpoison_memory_region(void const volatile *addr, size_t size);
extern "C" int __asan_address_is_poisoned(void const volatile *addr);
#endif

namespace WTF {

inline void adopted(const void*) { }

template<typename T, typename PtrTraits> class Ref;
template<typename T, typename PtrTraits = DumbPtrTraits<T>> Ref<T, PtrTraits> adoptRef(T&);

template<typename T, typename Traits>
class Ref {
public:
    using PtrTraits = Traits;
    static constexpr bool isRef = true;

    ~Ref()
    {
#if ASAN_ENABLED
        if (__asan_address_is_poisoned(this))
            __asan_unpoison_memory_region(this, sizeof(*this));
#endif
        if (m_ptr)
            PtrTraits::unwrap(m_ptr)->deref();
    }

    Ref(T& object)
        : m_ptr(&object)
    {
        object.ref();
    }

    Ref(const Ref& other)
        : m_ptr(other.ptr())
    {
        m_ptr->ref();
    }

    template<typename X, typename Y> Ref(const Ref<X, Y>& other)
        : m_ptr(other.ptr())
    {
        m_ptr->ref();
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

    Ref& operator=(T&);
    Ref& operator=(Ref&&);
    template<typename X, typename Y> Ref& operator=(Ref<X, Y>&&);

    Ref& operator=(const Ref&);
    template<typename X, typename Y> Ref& operator=(const Ref<X, Y>&);

    template<typename X, typename Y> void swap(Ref<X, Y>&);

    // Hash table deleted values, which are only constructed and never copied or destroyed.
    Ref(HashTableDeletedValueType) : m_ptr(PtrTraits::hashTableDeletedValue()) { }
    bool isHashTableDeletedValue() const { return PtrTraits::isHashTableDeletedValue(m_ptr); }

    Ref(HashTableEmptyValueType) : m_ptr(hashTableEmptyValue()) { }
    bool isHashTableEmptyValue() const { return m_ptr == hashTableEmptyValue(); }
    static T* hashTableEmptyValue() { return nullptr; }

    const T* ptrAllowingHashTableEmptyValue() const { ASSERT(m_ptr || isHashTableEmptyValue()); return PtrTraits::unwrap(m_ptr); }
    T* ptrAllowingHashTableEmptyValue() { ASSERT(m_ptr || isHashTableEmptyValue()); return PtrTraits::unwrap(m_ptr); }

    void assignToHashTableEmptyValue(Ref&& reference)
    {
#if ASAN_ENABLED
        if (__asan_address_is_poisoned(this))
            __asan_unpoison_memory_region(this, sizeof(*this));
#endif
        ASSERT(m_ptr == hashTableEmptyValue());
        m_ptr = &reference.leakRef();
        ASSERT(m_ptr);
    }

    T* operator->() const { ASSERT(m_ptr); return PtrTraits::unwrap(m_ptr); }
    T* ptr() const RETURNS_NONNULL { ASSERT(m_ptr); return PtrTraits::unwrap(m_ptr); }
    T& get() const { ASSERT(m_ptr); return *PtrTraits::unwrap(m_ptr); }
    operator T&() const { ASSERT(m_ptr); return *PtrTraits::unwrap(m_ptr); }
    bool operator!() const { ASSERT(m_ptr); return !*m_ptr; }

    template<typename X, typename Y> Ref<T, PtrTraits> replace(Ref<X, Y>&&) WARN_UNUSED_RETURN;

    // The following function is deprecated.
    Ref copyRef() && = delete;
    Ref copyRef() const & WARN_UNUSED_RETURN { return Ref(*m_ptr); }

    T& leakRef() WARN_UNUSED_RETURN
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
    template<typename X, typename Y> friend class Ref;

    enum AdoptTag { Adopt };
    Ref(T& object, AdoptTag)
        : m_ptr(&object)
    {
    }

    typename PtrTraits::StorageType m_ptr;
};

template<typename T, typename U> Ref<T, U> adoptRef(T&);
template<typename T> Ref<T> makeRef(T&);

template<typename T, typename U>
inline Ref<T, U>& Ref<T, U>::operator=(T& reference)
{
    Ref copiedReference = reference;
    swap(copiedReference);
    return *this;
}

template<typename T, typename U>
inline Ref<T, U>& Ref<T, U>::operator=(Ref&& reference)
{
#if ASAN_ENABLED
    if (__asan_address_is_poisoned(this))
        __asan_unpoison_memory_region(this, sizeof(*this));
#endif
    Ref movedReference = WTFMove(reference);
    swap(movedReference);
    return *this;
}

template<typename T, typename U>
template<typename X, typename Y>
inline Ref<T, U>& Ref<T, U>::operator=(Ref<X, Y>&& reference)
{
#if ASAN_ENABLED
    if (__asan_address_is_poisoned(this))
        __asan_unpoison_memory_region(this, sizeof(*this));
#endif
    Ref movedReference = WTFMove(reference);
    swap(movedReference);
    return *this;
}

template<typename T, typename U>
inline Ref<T, U>& Ref<T, U>::operator=(const Ref& reference)
{
#if ASAN_ENABLED
    if (__asan_address_is_poisoned(this))
        __asan_unpoison_memory_region(this, sizeof(*this));
#endif
    Ref copiedReference = reference;
    swap(copiedReference);
    return *this;
}

template<typename T, typename U>
template<typename X, typename Y>
inline Ref<T, U>& Ref<T, U>::operator=(const Ref<X, Y>& reference)
{
#if ASAN_ENABLED
    if (__asan_address_is_poisoned(this))
        __asan_unpoison_memory_region(this, sizeof(*this));
#endif
    Ref copiedReference = reference;
    swap(copiedReference);
    return *this;
}

template<typename T, typename U>
template<typename X, typename Y>
inline void Ref<T, U>::swap(Ref<X, Y>& other)
{
    U::swap(m_ptr, other.m_ptr);
}

template<typename T, typename U, typename X, typename Y, typename = std::enable_if_t<!std::is_same<U, DumbPtrTraits<T>>::value || !std::is_same<Y, DumbPtrTraits<X>>::value>>
inline void swap(Ref<T, U>& a, Ref<X, Y>& b)
{
    a.swap(b);
}

template<typename T, typename U>
template<typename X, typename Y>
inline Ref<T, U> Ref<T, U>::replace(Ref<X, Y>&& reference)
{
#if ASAN_ENABLED
    if (__asan_address_is_poisoned(this))
        __asan_unpoison_memory_region(this, sizeof(*this));
#endif
    auto oldReference = adoptRef(*m_ptr);
    m_ptr = &reference.leakRef();
    return oldReference;
}

template<typename T, typename U = DumbPtrTraits<T>, typename X, typename Y>
inline Ref<T, U> static_reference_cast(Ref<X, Y>& reference)
{
    return Ref<T, U>(static_cast<T&>(reference.get()));
}

template<typename T, typename U = DumbPtrTraits<T>, typename X, typename Y>
inline Ref<T, U> static_reference_cast(Ref<X, Y>&& reference)
{
    return adoptRef(static_cast<T&>(reference.leakRef()));
}

template<typename T, typename U = DumbPtrTraits<T>, typename X, typename Y>
inline Ref<T, U> static_reference_cast(const Ref<X, Y>& reference)
{
    return Ref<T, U>(static_cast<T&>(reference.copyRef().get()));
}

template <typename T, typename U>
struct GetPtrHelper<Ref<T, U>> {
    typedef T* PtrType;
    static T* getPtr(const Ref<T, U>& p) { return const_cast<T*>(p.ptr()); }
};

template <typename T, typename U>
struct IsSmartPtr<Ref<T, U>> {
    static constexpr bool value = true;
};

template<typename T, typename U>
inline Ref<T, U> adoptRef(T& reference)
{
    adopted(&reference);
    return Ref<T, U>(reference, Ref<T, U>::Adopt);
}

template<typename T>
inline Ref<T> makeRef(T& reference)
{
    return Ref<T>(reference);
}

template<typename ExpectedType, typename ArgType, typename PtrTraits>
inline bool is(Ref<ArgType, PtrTraits>& source)
{
    return is<ExpectedType>(source.get());
}

template<typename ExpectedType, typename ArgType, typename PtrTraits>
inline bool is(const Ref<ArgType, PtrTraits>& source)
{
    return is<ExpectedType>(source.get());
}

} // namespace WTF

using WTF::Ref;
using WTF::adoptRef;
using WTF::makeRef;
using WTF::static_reference_cast;
