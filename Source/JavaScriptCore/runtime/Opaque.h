/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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

#include <wtf/HashTraits.h>

namespace JSC {

// The purpose of this class is to simply hide the underlying type T's requiresGCAwareContainer
// property from any container (e.g. Vector) that we wish to store T in. This is OK for cases
// where the T elements to be stored in this container are already protected from GC by some
// other mechanism, and we're only using this container (as opposed to a GC aware container)
// for performance reasons.

template<typename T>
class Opaque {
public:
    static_assert(sizeof(T) == sizeof(int64_t)); // Must match the size of JSValue.

    Opaque() = default;
    Opaque(T value) : m_value(value) { }

    operator const T() const { return m_value; }
    operator T() { return m_value; }

    T operator->() const
    {
        static_assert(std::is_pointer_v<T>);
        return m_value;
    }

    T get() const { return m_value; }

private:
    T m_value;
};

} // namespace JSC

namespace WTF {

template<typename T>
struct DefaultHash<JSC::Opaque<T>> : DefaultHash<T> {
    static_assert(sizeof(T) == sizeof(int64_t)); // Must match the size of JSValue.
};

template<typename T>
struct HashTraits<JSC::Opaque<T>> : HashTraits<T> {
    static_assert(sizeof(T) == sizeof(int64_t)); // Must match the size of JSValue.

    typedef JSC::Opaque<T> TraitType;
    typedef JSC::Opaque<T> EmptyValueType;

    template <typename Traits>
    static void constructEmptyValue(JSC::Opaque<T>& slot)
    {
        new (NotNull, std::addressof(slot)) JSC::Opaque<T>(Traits::emptyValue());
    }

    static JSC::Opaque<T> emptyValue() { return T(); }

    static constexpr bool emptyValueIsZero = true;
    static void constructDeletedValue(JSC::Opaque<T>& slot) { slot = JSC::Opaque { std::bit_cast<T>(static_cast<int64_t>(-1)) }; }
    static bool isDeletedValue(JSC::Opaque<T> value) { return value.get() == std::bit_cast<T>(static_cast<int64_t>(-1)); }
};

} // namespace WTF
