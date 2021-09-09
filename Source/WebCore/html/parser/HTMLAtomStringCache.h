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

#include <wtf/NeverDestroyed.h>
#include <wtf/Vector.h>
#include <wtf/text/AtomString.h>

namespace WebCore {

class HTMLAtomStringCache {
public:
    template<size_t inlineCapacity>
    ALWAYS_INLINE static AtomString makeTagOrAttributeName(const Vector<UChar, inlineCapacity>& string)
    {
        return make<Type::TagOrAttributeName>(string);
    }

    template<size_t inlineCapacity>
    ALWAYS_INLINE static AtomString makeAttributeValue(const Vector<UChar, inlineCapacity>& string)
    {
        return make<Type::AttributeValue>(string);
    }

    ALWAYS_INLINE static void clear()
    {
        // FIXME (webkit.org/b/230019): We should try to find more opportunities to clear this cache without hindering this performance optimization.
        cache(Type::TagOrAttributeName).fill({ });
        cache(Type::AttributeValue).fill({ });
    }

private:
    enum class Type : bool { TagOrAttributeName, AttributeValue };

    template<HTMLAtomStringCache::Type type, size_t inlineCapacity>
    ALWAYS_INLINE static AtomString make(const Vector<UChar, inlineCapacity>& string)
    {
        if (string.isEmpty())
            return emptyAtom();

        auto length = string.size();
        if (length > maxStringLengthForCache)
            return AtomString(string);

        auto firstCharacter = string[0];
        auto lastCharacter = string[length - 1];
        auto& slot = cacheSlot(type, firstCharacter, lastCharacter, length);
        if (!equal(slot.impl(), string.data(), length)) {
            AtomString result(string);
            slot = result;
            return result;
        }

        return slot;
    }

    ALWAYS_INLINE static AtomString& cacheSlot(Type type, UChar firstCharacter, UChar lastCharacter, UChar length)
    {
        unsigned hash = (firstCharacter << 6) ^ ((lastCharacter << 14) ^ firstCharacter);
        hash += (hash >> 14) + (length << 14);
        hash ^= hash << 14;
        return cache(type)[(hash + (hash >> 6)) % capacity];
    }

    static constexpr auto maxStringLengthForCache = 36;
    static constexpr auto capacity = 512;
    using Cache = std::array<AtomString, capacity>;
    static Cache& cache(Type);
};

} // namespace WebCore
