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

#include "JSONTransitionCache.h"
#include "Structure.h"
#include <wtf/MathExtras.h>

namespace JSC {

// A StructureID is the low 32 bits of a 16-byte aligned Structure's address, so only bits 4 to 31 tell
// Structures apart. The name goes above those, low enough for the multiplies to mix it into the index.
ALWAYS_INLINE uint64_t JSONTransitionCache::key(StructureID structureID, unsigned length, Latin1Character first, Latin1Character last)
{
    uint64_t nameKey = length | (static_cast<uint64_t>(first) << 8) | (static_cast<uint64_t>(last) << 16);
    return (structureID.bits() >> 4) | (nameKey << 28);
}

ALWAYS_INLINE uint64_t JSONTransitionCache::key(StructureID structureID, std::span<const Latin1Character> name)
{
    if (name.empty())
        return key(structureID, 0, 0, 0);
    return key(structureID, name.size(), name.front(), name.back());
}

ALWAYS_INLINE unsigned JSONTransitionCache::primaryIndex(uint64_t key)
{
    return (key * 0x9E3779B97F4A7C15ULL) >> (64 - WTF::fastLog2(primarySize));
}

ALWAYS_INLINE unsigned JSONTransitionCache::secondaryIndex(uint64_t key)
{
    return (key * 0xC2B2AE3D27D4EB4FULL) >> (64 - WTF::fastLog2(secondarySize));
}
static_assert(hasOneBitSet(JSONTransitionCache::primarySize) && hasOneBitSet(JSONTransitionCache::secondarySize));

ALWAYS_INLINE Structure* JSONTransitionCache::transitionIfMatches(const Entry& entry, StructureID from, std::span<const Latin1Character> name)
{
    if (entry.from.value() != from)
        return nullptr;
    Structure* to = entry.to.unvalidatedGet();
    ASSERT(to->transitionPropertyName());
    if (!WTF::equal(to->transitionPropertyName(), name))
        return nullptr;
    return to;
}

ALWAYS_INLINE Structure* JSONTransitionCache::get(Structure* from, std::span<const Latin1Character> name)
{
    StructureID fromID = StructureID::encode(from);
    uint64_t key = this->key(fromID, name);
    if (Structure* to = transitionIfMatches(m_primary[primaryIndex(key)], fromID, name))
        return to;
    return transitionIfMatches(m_secondary[secondaryIndex(key)], fromID, name);
}

ALWAYS_INLINE void JSONTransitionCache::add(Structure* from, Structure* to, std::span<const Latin1Character> name)
{
    // Pinning a Structure clears its transition property name and previous Structure, but only Structures
    // created by other kinds of transitions are pinned, and only as they are created.
    ASSERT(!from->isDictionary());
    ASSERT(!from->hasBeenDictionary());
    ASSERT(to->transitionKind() == TransitionKind::PropertyAddition);
    ASSERT(!to->transitionPropertyAttributes());
    ASSERT(to->previousID() == from);
    ASSERT(to->transitionPropertyName() && WTF::equal(to->transitionPropertyName(), name));
    auto& entry = m_primary[primaryIndex(key(StructureID::encode(from), name))];
    if (StructureID evictedFrom = entry.from.value()) {
        // Keys of cached names were Latin-1 spans, so every character fits in a Latin1Character.
        auto* evictedName = entry.to.unvalidatedGet()->transitionPropertyName();
        ASSERT(evictedName);
        unsigned length = evictedName->length();
        uint64_t evictedKey = key(evictedFrom, 0, 0, 0);
        if (length)
            evictedKey = key(evictedFrom, length, static_cast<Latin1Character>((*evictedName)[0]), static_cast<Latin1Character>((*evictedName)[length - 1]));
        m_secondary[secondaryIndex(evictedKey)] = entry;
    }
    entry.from.setWithoutWriteBarrier(from);
    entry.to.setWithoutWriteBarrier(to);
}

template<typename Visitor>
void JSONTransitionCache::visitAggregate(Visitor& visitor)
{
    if (!m_isParsing)
        return;
    for (auto& entry : m_primary) {
        visitor.append(entry.from);
        visitor.append(entry.to);
    }
    for (auto& entry : m_secondary) {
        visitor.append(entry.from);
        visitor.append(entry.to);
    }
}

} // namespace JSC
