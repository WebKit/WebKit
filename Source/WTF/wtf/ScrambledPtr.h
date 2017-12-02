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

#define ENABLE_SCRAMBLED_PTR_ASSERTS 0

// Not currently supported for 32-bit or OS(WINDOWS) builds (because of missing llint support).
// Make sure it's disabled.
#if USE(JSVALUE32_64) || OS(WINDOWS)
#undef ENABLE_SCRAMBLED_PTR_ASSERTS
#define ENABLE_SCRAMBLED_PTR_ASSERTS 0
#endif

namespace WTF {

using ScrambledPtrBits = uintptr_t;

template<typename T, uintptr_t& key, typename = std::enable_if_t<std::is_pointer<T>::value>>
class ScrambledPtr {
public:
    ScrambledPtr() { }

    explicit ScrambledPtr(T ptr)
        : m_scrambledBits(scramble(ptr))
    {
        ASSERT(ptr && m_scrambledBits);
    }

    ScrambledPtr(const ScrambledPtr&) = default;

    explicit ScrambledPtr(ScrambledPtrBits scrambledBits)
        : m_scrambledBits(scrambledBits)
    {
        ASSERT(m_scrambledBits);
    }

#if ENABLE(SCRAMBLED_PTR_ASSERTS)
    template<typename U = void*>
    static bool isScrambled(U value) { return !value || (reinterpret_cast<uintptr_t>(value) & 0xffff000000000000); }
    template<typename U = void*>
    static void assertIsScrambled(U value) { RELEASE_ASSERT(isScrambled(value)); }
    template<typename U = void*>
    static void assertIsNotScrambled(U value) { RELEASE_ASSERT(!isScrambled(value)); }
#else
    template<typename U = void*> static void assertIsScrambled(U) { }
    template<typename U = void*> static void assertIsNotScrambled(U) { }
#endif
    void assertIsScrambled() const { assertIsScrambled(m_scrambledBits); }
    void assertIsNotScrambled() const { assertIsNotScrambled(m_scrambledBits); }

    template<typename U = T>
    U descrambled() const { return descramble<U>(m_scrambledBits); }

    ALWAYS_INLINE T operator->() const { return descramble<T>(m_scrambledBits); }

    template<typename U = ScrambledPtrBits>
    U bits() const { return bitwise_cast<U>(m_scrambledBits); }

    bool operator!() const { return !m_scrambledBits; }
    explicit operator bool() const { return !!m_scrambledBits; }

    bool operator==(const ScrambledPtr& b) const
    {
        return m_scrambledBits == b.m_scrambledBits;
    }

    template<typename PtrType = void*, typename = typename std::enable_if<std::is_pointer<PtrType>::value>::type>
    bool operator==(const PtrType b)
    {
        return descrambled<PtrType>() == b;
    }

    ScrambledPtr& operator=(T ptr)
    {
        m_scrambledBits = ptr ? scramble(ptr) : 0;
        return *this;
    }
    ScrambledPtr& operator=(const ScrambledPtr&) = default;

private:
#if USE(JSVALUE64)
    template<typename U>
    ALWAYS_INLINE static ScrambledPtrBits scramble(U ptr) { return bitwise_cast<ScrambledPtrBits>(ptr) ^ key; }
    template<typename U>
    ALWAYS_INLINE static U descramble(ScrambledPtrBits scrambledBits) { return bitwise_cast<U>(scrambledBits ^ key); }
#else
    template<typename U>
    ALWAYS_INLINE static ScrambledPtrBits scramble(U ptr) { return bitwise_cast<ScrambledPtrBits>(ptr); }
    template<typename U>
    ALWAYS_INLINE static U descramble(ScrambledPtrBits scrambledBits) { return bitwise_cast<U>(scrambledBits); }
#endif

    ScrambledPtrBits m_scrambledBits { 0 };
};

void initializeScrambledPtr();
WTF_EXPORT_PRIVATE uintptr_t makeScrambledPtrKey();

} // namespace WTF

using WTF::ScrambledPtr;
using WTF::ScrambledPtrBits;
using WTF::makeScrambledPtrKey;

