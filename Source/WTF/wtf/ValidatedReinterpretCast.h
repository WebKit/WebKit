/*
 * Copyright (C) 2008-2024 Apple Inc. All rights reserved.
 * Copyright (C) 2024 Samuel Weinig <sam@webkit.org>
 * Copyright (C) 2013 Patrick Gansterer <paroga@paroga.com>
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

#include "Compiler.h"
#include "Platform.h"
#include <bit>
#include <cstdint>
#include <wtf/text/ASCIILiteral.h>

namespace WTF {

// A small mixin to quickly check downcasts in debug builds.
// MAKE_VALIDATED_REINTERPRET_CAST must be the first mixin included in the classs.
// Use VALIDATED_REINTERPRET_CAST to perform the downcast.
// A string (fqn) is provided to identify the class, that way downcasts
// do not need to depend on the definiton of the subclass.
template<typename T, typename U>
constexpr inline T* validatedReinterpretCast(U* v, uint64_t magic)
{
    // We don't want to require the definition of T to do this cast.
    ASSERT_UNUSED(magic, !v || std::bit_cast<uint64_t*>(v)[0] == magic);
    return static_cast<T*>(v);
}

#if ASSERT_ENABLED
#define MAKE_VALIDATED_REINTERPRET_CAST \
    constexpr inline static uint64_t expectedMagicValue(); \
    const uint64_t magicValue = expectedMagicValue();

#define MAKE_VALIDATED_REINTERPRET_CAST_IMPL(fqn, T) \
    constexpr inline uint64_t T::expectedMagicValue() \
    { \
        static_assert(!OBJECT_OFFSETOF(T, magicValue)); \
        return ASCIILiteral(fqn).hash(); \
    }

#else
#define MAKE_VALIDATED_REINTERPRET_CAST
#define MAKE_VALIDATED_REINTERPRET_CAST_IMPL(fqn, T)
#endif

#define VALIDATED_REINTERPRET_CAST(fqn, T, v) \
    WTF::validatedReinterpretCast<T>(v, ASCIILiteral(fqn).hash())

template<typename T, typename U>
ALWAYS_INLINE T* validatedReinterpretCast(U* u)
{
    T* casted = static_cast<T*>(u);
    ASSERT(casted->magicValue == T::MAGIC_VALUE);
    return casted;
}

} // namespace WTF
