/*
 * Copyright (C) 2017-2018 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include <cstddef>
#include <utility>
#include <wtf/Assertions.h>

#define ENABLE_POISON_ASSERTS 0

#if !ENABLE(POISON)
#undef ENABLE_POISON_ASSERTS
#define ENABLE_POISON_ASSERTS 0
#endif

namespace WTF {

using PoisonedBits = uintptr_t;

namespace PoisonedImplHelper {

template<typename T>
struct isFunctionPointer : std::integral_constant<bool, std::is_function<typename std::remove_pointer<T>::type>::value> { };

template<typename T>
struct isVoidPointer : std::integral_constant<bool, std::is_void<typename std::remove_pointer<T>::type>::value> { };

template<typename T>
struct isConvertibleToReference : std::integral_constant<bool, !isFunctionPointer<T>::value && !isVoidPointer<T>::value> { };

template<typename T>
typename std::enable_if_t<!isConvertibleToReference<T>::value, int>&
asReference(T) { RELEASE_ASSERT_NOT_REACHED(); }

template<typename T>
typename std::enable_if_t<isConvertibleToReference<T>::value, typename std::remove_pointer<T>::type>&
asReference(T ptr) { return *ptr; }

} // namespace PoisonedImplHelper

enum AlreadyPoisonedTag { AlreadyPoisoned };

template<uintptr_t& poisonKey>
class Poison {
public:
    template<typename PoisonedType = void>
    static uintptr_t key(const PoisonedType* = nullptr) { return poisonKey; }
};

template<typename Poison, typename T, typename = std::enable_if_t<std::is_pointer<T>::value>>
class Poisoned {
public:
    static constexpr bool isPoisonedType = true;

    Poisoned() { }

    Poisoned(std::nullptr_t) { }

    Poisoned(T ptr)
        : m_poisonedBits(poison(ptr))
    { }

    Poisoned(const Poisoned&) = default;

    template<typename Other, typename = std::enable_if_t<Other::isPoisonedType>>
    Poisoned(const Other& other)
        : m_poisonedBits(poison<T>(other.unpoisoned()))
    { }

    Poisoned(Poisoned&&) = default;

    explicit Poisoned(AlreadyPoisonedTag, PoisonedBits poisonedBits)
        : m_poisonedBits(poisonedBits)
    { }

#if ENABLE(POISON_ASSERTS)
    template<typename U = void*>
    static bool isPoisoned(U value) { return !value || (reinterpret_cast<uintptr_t>(value) & 0xffff000000000000); }
    template<typename U = void*>
    static void assertIsPoisoned(U value) { RELEASE_ASSERT(isPoisoned(value)); }
    template<typename U = void*>
    static void assertIsNotPoisoned(U value) { RELEASE_ASSERT(!isPoisoned(value)); }
#else
    template<typename U = void*> static void assertIsPoisoned(U) { }
    template<typename U = void*> static void assertIsNotPoisoned(U) { }
#endif
    void assertIsPoisoned() const { assertIsPoisoned(m_poisonedBits); }
    void assertIsNotPoisoned() const { assertIsNotPoisoned(m_poisonedBits); }

    template<typename U = T>
    U unpoisoned() const { return unpoison<U>(m_poisonedBits); }

    void clear() { m_poisonedBits = 0; }

    auto& operator*() const { ASSERT(m_poisonedBits); return PoisonedImplHelper::asReference(unpoison(m_poisonedBits)); }
    ALWAYS_INLINE T operator->() const { return unpoison(m_poisonedBits); }

    template<typename U = PoisonedBits>
    U bits() const { return bitwise_cast<U>(m_poisonedBits); }

    bool operator!() const { return !m_poisonedBits; }
    explicit operator bool() const { return !!m_poisonedBits; }

    bool operator==(const Poisoned& b) const { return m_poisonedBits == b.m_poisonedBits; }
    bool operator!=(const Poisoned& b) const { return m_poisonedBits != b.m_poisonedBits; }

    bool operator==(T b) const { return unpoisoned() == b; }
    bool operator!=(T b) const { return unpoisoned() != b; }
    bool operator<(T b) const { return unpoisoned() < b; }
    bool operator<=(T b) const { return unpoisoned() <= b; }
    bool operator>(T b) const { return unpoisoned() > b; }
    bool operator>=(T b) const { return unpoisoned() >= b; }

    Poisoned& operator=(T ptr)
    {
        m_poisonedBits = poison(ptr);
        return *this;
    }
    Poisoned& operator=(const Poisoned&) = default;

    Poisoned& operator=(std::nullptr_t)
    {
        clear();
        return *this;
    }

    template<typename Other, typename = std::enable_if_t<Other::isPoisonedType>>
    Poisoned& operator=(const Other& other)
    {
        m_poisonedBits = poison<T>(other.unpoisoned());
        return *this;
    }

    void swap(Poisoned& other)
    {
        std::swap(m_poisonedBits, other.m_poisonedBits);
    }

    void swap(std::nullptr_t) { clear(); }

    template<typename Other, typename = std::enable_if_t<Other::isPoisonedType>>
    void swap(Other& other)
    {
        T t1 = this->unpoisoned();
        T t2 = other.unpoisoned();
        std::swap(t1, t2);
        m_poisonedBits = poison(t1);
        other.m_poisonedBits = other.poison(t2);
    }

    void swap(T& t2)
    {
        T t1 = this->unpoisoned();
        std::swap(t1, t2);
        m_poisonedBits = poison(t1);
    }

    template<class U>
    T exchange(U&& newValue)
    {
        T oldValue = unpoisoned();
        m_poisonedBits = poison(std::forward<U>(newValue));
        return oldValue;
    }

private:
    template<typename U>
    ALWAYS_INLINE PoisonedBits poison(U ptr) const { return poison<PoisonedBits>(this, bitwise_cast<uintptr_t>(ptr)); }
    template<typename U = T>
    ALWAYS_INLINE U unpoison(PoisonedBits poisonedBits) const { return unpoison<U>(this, poisonedBits); }

    constexpr static PoisonedBits poison(const Poisoned*, std::nullptr_t) { return 0; }
#if ENABLE(POISON)
    template<typename U>
    ALWAYS_INLINE static PoisonedBits poison(const Poisoned* thisPoisoned, U ptr) { return ptr ? bitwise_cast<PoisonedBits>(ptr) ^ Poison::key(thisPoisoned) : 0; }
    template<typename U = T>
    ALWAYS_INLINE static U unpoison(const Poisoned* thisPoisoned, PoisonedBits poisonedBits) { return poisonedBits ? bitwise_cast<U>(poisonedBits ^ Poison::key(thisPoisoned)) : bitwise_cast<U>(0ll); }
#else
    template<typename U>
    ALWAYS_INLINE static PoisonedBits poison(const Poisoned*, U ptr) { return bitwise_cast<PoisonedBits>(ptr); }
    template<typename U = T>
    ALWAYS_INLINE static U unpoison(const Poisoned*, PoisonedBits poisonedBits) { return bitwise_cast<U>(poisonedBits); }
#endif

    PoisonedBits m_poisonedBits { 0 };

    template<typename, typename, typename> friend class Poisoned;
};

template<typename T, typename U, typename = std::enable_if_t<T::isPoisonedType && U::isPoisonedType && !std::is_same<T, U>::value>>
inline bool operator==(const T& a, const U& b) { return a.unpoisoned() == b.unpoisoned(); }

template<typename T, typename U, typename = std::enable_if_t<T::isPoisonedType && U::isPoisonedType && !std::is_same<T, U>::value>>
inline bool operator!=(const T& a, const U& b) { return a.unpoisoned() != b.unpoisoned(); }

template<typename T, typename U, typename = std::enable_if_t<T::isPoisonedType && U::isPoisonedType>>
inline bool operator<(const T& a, const U& b) { return a.unpoisoned() < b.unpoisoned(); }

template<typename T, typename U, typename = std::enable_if_t<T::isPoisonedType && U::isPoisonedType>>
inline bool operator<=(const T& a, const U& b) { return a.unpoisoned() <= b.unpoisoned(); }

template<typename T, typename U, typename = std::enable_if_t<T::isPoisonedType && U::isPoisonedType>>
inline bool operator>(const T& a, const U& b) { return a.unpoisoned() > b.unpoisoned(); }

template<typename T, typename U, typename = std::enable_if_t<T::isPoisonedType && U::isPoisonedType>>
inline bool operator>=(const T& a, const U& b) { return a.unpoisoned() >= b.unpoisoned(); }

template<typename T, typename U, typename = std::enable_if_t<T::isPoisonedType>>
inline void swap(T& a, U& b)
{
    a.swap(b);
}

WTF_EXPORT_PRIVATE uintptr_t makePoison();

template<typename Poison, typename T>
struct PoisonedPtrTraits {
    using StorageType = Poisoned<Poison, T*>;

    template<class U> static ALWAYS_INLINE T* exchange(StorageType& ptr, U&& newValue) { return ptr.exchange(newValue); }

    template<typename Other>
    static ALWAYS_INLINE void swap(Poisoned<Poison, T*>& a, Other& b) { a.swap(b); }

    static ALWAYS_INLINE T* unwrap(const StorageType& ptr) { return ptr.unpoisoned(); }
};

template<typename Poison, typename T>
struct PoisonedValueTraits {
    using StorageType = Poisoned<Poison, T>;

    template<class U> static ALWAYS_INLINE T exchange(StorageType& val, U&& newValue) { return val.exchange(newValue); }

    template<typename Other>
    static ALWAYS_INLINE void swap(Poisoned<Poison, T>& a, Other& b) { a.swap(b); }

    static ALWAYS_INLINE T unwrap(const StorageType& val) { return val.unpoisoned(); }
};

} // namespace WTF

using WTF::AlreadyPoisoned;
using WTF::Poison;
using WTF::Poisoned;
using WTF::PoisonedBits;
using WTF::PoisonedPtrTraits;
using WTF::PoisonedValueTraits;
using WTF::makePoison;
