/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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

#include <wtf/Assertions.h>

#define ENABLE_POISON_ASSERTS 0

// Not currently supported for 32-bit or OS(WINDOWS) builds (because of missing llint support).
// Make sure it's disabled.
#if USE(JSVALUE32_64) || OS(WINDOWS)
#undef ENABLE_POISON_ASSERTS
#define ENABLE_POISON_ASSERTS 0
#endif

namespace WTF {

using PoisonedBits = uintptr_t;

template<typename KeyType, KeyType key, typename T, typename = std::enable_if_t<std::is_pointer<T>::value>>
class PoisonedImpl {
public:
    PoisonedImpl() { }

    explicit PoisonedImpl(T ptr)
        : m_poisonedBits(poison(ptr))
    {
        ASSERT(ptr && m_poisonedBits);
    }

    PoisonedImpl(const PoisonedImpl&) = default;

    explicit PoisonedImpl(PoisonedBits poisonedBits)
        : m_poisonedBits(poisonedBits)
    {
        ASSERT(m_poisonedBits);
    }

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

    ALWAYS_INLINE T operator->() const { return unpoison<T>(m_poisonedBits); }

    template<typename U = PoisonedBits>
    U bits() const { return bitwise_cast<U>(m_poisonedBits); }

    bool operator!() const { return !m_poisonedBits; }
    explicit operator bool() const { return !!m_poisonedBits; }

    bool operator==(const PoisonedImpl& b) const
    {
        return m_poisonedBits == b.m_poisonedBits;
    }

    template<typename PtrType = void*, typename = typename std::enable_if<std::is_pointer<PtrType>::value>::type>
    bool operator==(const PtrType b)
    {
        return unpoisoned<PtrType>() == b;
    }

    PoisonedImpl& operator=(T ptr)
    {
        m_poisonedBits = poison(ptr);
        return *this;
    }
    PoisonedImpl& operator=(const PoisonedImpl&) = default;

private:
#if USE(JSVALUE64)
    template<typename U>
    ALWAYS_INLINE static PoisonedBits poison(U ptr) { return ptr ? bitwise_cast<PoisonedBits>(ptr) ^ key : 0; }
    template<typename U>
    ALWAYS_INLINE static U unpoison(PoisonedBits poisonedBits) { return poisonedBits ? bitwise_cast<U>(poisonedBits ^ key) : bitwise_cast<U>(0ll); }
#else
    template<typename U>
    ALWAYS_INLINE static PoisonedBits poison(U ptr) { return bitwise_cast<PoisonedBits>(ptr); }
    template<typename U>
    ALWAYS_INLINE static U unpoison(PoisonedBits poisonedBits) { return bitwise_cast<U>(poisonedBits); }
#endif

    PoisonedBits m_poisonedBits { 0 };
};

template<uintptr_t& key, typename T>
using Poisoned = PoisonedImpl<const uintptr_t&, key, T>;

#if USE(JSVALUE64)
template<uint32_t key, typename T>
using Int32Poisoned = PoisonedImpl<uintptr_t, static_cast<uintptr_t>(key) << 32, T>;
#else
template<uint32_t, typename T>
using Int32Poisoned = PoisonedImpl<uintptr_t, 0, T>;
#endif

WTF_EXPORT_PRIVATE uintptr_t makePoison();

} // namespace WTF

using WTF::Int32Poisoned;
using WTF::Poisoned;
using WTF::PoisonedBits;
using WTF::makePoison;

