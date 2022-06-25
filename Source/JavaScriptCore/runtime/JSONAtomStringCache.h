/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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

#include <wtf/text/AtomStringImpl.h>

namespace JSC {

class VM;

class JSONAtomStringCache {
public:
    static constexpr auto maxStringLengthForCache = 32;
    static constexpr auto capacity = 512;
    using Cache = std::array<RefPtr<AtomStringImpl>, capacity>;

    enum class Type : bool { Identifier };
    static constexpr unsigned numberOfTypes = 1;

    template<typename CharacterType>
    ALWAYS_INLINE Ref<AtomStringImpl> makeIdentifier(const CharacterType* characters, unsigned length)
    {
        return make(Type::Identifier, characters, length);
    }

    ALWAYS_INLINE void clear()
    {
        for (unsigned i = 0; i < numberOfTypes; ++i)
            cache(static_cast<Type>(i)).fill({ });
    }

    VM& vm() const;

private:
    template<typename CharacterType>
    Ref<AtomStringImpl> make(Type, const CharacterType*, unsigned length);

    ALWAYS_INLINE RefPtr<AtomStringImpl>& cacheSlot(Type type, UChar firstCharacter, UChar lastCharacter, UChar length)
    {
        unsigned hash = (firstCharacter << 6) ^ ((lastCharacter << 14) ^ firstCharacter);
        hash += (hash >> 14) + (length << 14);
        hash ^= hash << 14;
        return cache(type)[(hash + (hash >> 6)) % capacity];
    }

    ALWAYS_INLINE Cache& cache(Type type) { return m_caches[static_cast<size_t>(type)]; }

    Cache m_caches[numberOfTypes] { };
};

} // namespace JSC
