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

#include "QualifiedName.h"
#include <wtf/NeverDestroyed.h>
#include <wtf/Vector.h>
#include <wtf/text/AtomString.h>

namespace WebCore {

class HTMLNameCache {
public:
    ALWAYS_INLINE static AtomString makeTagName(Span<const UChar> string)
    {
        return makeAtomString<AtomStringType::TagName>(string);
    }

    ALWAYS_INLINE static QualifiedName makeAttributeQualifiedName(Span<const UChar> string)
    {
        return makeQualifiedName(string);
    }

    ALWAYS_INLINE static AtomString makeAttributeValue(Span<const UChar> string)
    {
        return makeAtomString<AtomStringType::AttributeValue>(string);
    }

    ALWAYS_INLINE static void clear()
    {
        // FIXME (webkit.org/b/230019): We should try to find more opportunities to clear this cache without hindering this performance optimization.
        atomStringCache(AtomStringType::TagName).fill({ });
        atomStringCache(AtomStringType::AttributeValue).fill({ });
        qualifiedNameCache().fill({ });
    }

private:
    enum class AtomStringType : bool { TagName, AttributeValue };

    template<HTMLNameCache::AtomStringType type>
    ALWAYS_INLINE static AtomString makeAtomString(Span<const UChar> string)
    {
        if (string.empty())
            return emptyAtom();

        auto length = string.size();
        if (length > maxStringLengthForCache)
            return AtomString(string.data(), length);

        auto firstCharacter = string[0];
        auto lastCharacter = string[length - 1];
        auto& slot = atomStringCacheSlot(type, firstCharacter, lastCharacter, length);
        if (!equal(slot.impl(), string.data(), length)) {
            AtomString result(string.data(), length);
            slot = result;
            return result;
        }

        return slot;
    }

    ALWAYS_INLINE static QualifiedName makeQualifiedName(Span<const UChar> string)
    {
        if (string.empty())
            return nullQName();

        auto length = string.size();
        if (length > maxStringLengthForCache)
            return QualifiedName(nullAtom(), AtomString(string.data(), length), nullAtom());

        auto firstCharacter = string[0];
        auto lastCharacter = string[length - 1];
        auto& slot = qualifiedNameCacheSlot(firstCharacter, lastCharacter, length);
        if (!slot || !equal(slot->m_localName.impl(), string.data(), length)) {
            QualifiedName result(nullAtom(), AtomString(string.data(), length), nullAtom());
            slot = result.impl();
            return result;
        }

        return *slot;
    }

    ALWAYS_INLINE static size_t slotIndex(UChar firstCharacter, UChar lastCharacter, UChar length)
    {
        unsigned hash = (firstCharacter << 6) ^ ((lastCharacter << 14) ^ firstCharacter);
        hash += (hash >> 14) + (length << 14);
        hash ^= hash << 14;
        return (hash + (hash >> 6)) % capacity;
    }

    ALWAYS_INLINE static AtomString& atomStringCacheSlot(AtomStringType type, UChar firstCharacter, UChar lastCharacter, UChar length)
    {
        auto index = slotIndex(firstCharacter, lastCharacter, length);
        return atomStringCache(type)[index];
    }

    ALWAYS_INLINE static RefPtr<QualifiedName::QualifiedNameImpl>& qualifiedNameCacheSlot(UChar firstCharacter, UChar lastCharacter, UChar length)
    {
        auto index = slotIndex(firstCharacter, lastCharacter, length);
        return qualifiedNameCache()[index];
    }

    static constexpr auto maxStringLengthForCache = 36;
    static constexpr auto capacity = 512;

    using AtomStringCache = std::array<AtomString, capacity>;
    using QualifiedNameCache = std::array<RefPtr<QualifiedName::QualifiedNameImpl>, capacity>;

    static AtomStringCache& atomStringCache(AtomStringType);
    static QualifiedNameCache& qualifiedNameCache();
};

} // namespace WebCore
