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

#include "config.h"
#include "JSONCache.h"

#include "JSONCacheInlines.h"
#include <wtf/StdLibExtras.h>

namespace JSC {

std::span<const Latin1Character> JSONCache::recordableName(Structure* from, Structure* to, PropertyOffset offset)
{
    ASSERT_UNUSED(from, !from->isDictionary());
    ASSERT(to->transitionKind() == TransitionKind::PropertyAddition);
    ASSERT(!to->transitionPropertyAttributes());
    ASSERT(to->previousID() == from);
    ASSERT_UNUSED(offset, to->transitionOffset() == offset);
    SUPPRESS_UNCOUNTED_LOCAL UniquedStringImpl* name = to->transitionPropertyName();
    if (!name->is8Bit() || name->length() > maxNameLength)
        return { };
    // Matching the raw source text is only equivalent to lexing the key when the key needs no escaping.
    auto characters = name->span8();
    for (auto character : characters) {
        if (character < ' ' || character == '"' || character == '\\')
            return { };
    }
    return characters;
}

template<unsigned size>
template<unsigned ways>
void JSONCache::NameTable<size>::insert(unsigned set, Structure* from, Structure* to, PropertyOffset offset, const NameText& text, unsigned textLength)
{
    for (unsigned way = ways - 1; way; --way) {
        entries[set + way] = entries[set + way - 1];
        texts[set + way] = texts[set + way - 1];
    }
    auto& entry = entries[set];
    entry.from.setWithoutWriteBarrier(from);
    entry.to.setWithoutWriteBarrier(to);
    entry.offset = offset;
    entry.textLength = textLength;
    entry.toNameIndex = nameIndex(StructureID::encode(to));
    texts[set] = text;
}

unsigned JSONCache::makeText(NameText& text, std::span<const Latin1Character> name)
{
    text.fill(0);
    text[0] = '"';
    WTF::copyElements(std::span { text }.subspan(1, name.size()), name);
    text[name.size() + 1] = '"';
    text[name.size() + 2] = ':';
    return name.size() + 3;
}

uint32_t JSONCache::textPrefix(std::span<const Latin1Character> name)
{
    std::array<Latin1Character, sizeof(uint32_t) + 2> bytes { };
    auto head = name.first(std::min<size_t>(name.size(), sizeof(uint32_t) - 1));
    bytes[0] = '"';
    WTF::copyElements(std::span { bytes }.subspan(1, head.size()), head);
    bytes[head.size() + 1] = '"';
    bytes[head.size() + 2] = ':';
    return WTF::unalignedLoad<uint32_t>(bytes.data());
}

void JSONCache::addPrefixedName(Structure* from, Structure* to, PropertyOffset offset)
{
    SUPPRESS_UNCOUNTED_LOCAL UniquedStringImpl* transitionName = to->transitionPropertyName();
    if (!transitionName->is8Bit() || transitionName->length() > maxNameLength)
        return;
    uint32_t prefix = textPrefix(transitionName->span8());
    StructureID toID = StructureID::encode(to);
    unsigned set = prefixedNameIndex(StructureID::encode(from), prefix);
    for (unsigned way = 0; way < prefixedNameWays; ++way) {
        if (m_prefixedNames.entries[set + way].to.value() == toID)
            return;
    }
    auto name = recordableName(from, to, offset);
    if (name.empty())
        return;
    NameText text;
    unsigned textLength = makeText(text, name);
    ASSERT(WTF::unalignedLoad<uint32_t>(text.data()) == prefix);
    m_prefixedNames.insert<prefixedNameWays>(set, from, to, offset, text, textLength);
}

void JSONCache::addName(Structure* from, Structure* to, PropertyOffset offset)
{
    StructureID fromID = StructureID::encode(from);
    unsigned set = nameIndex(fromID);
    if (m_names.entries[set].from.value() == fromID)
        return;
    auto name = recordableName(from, to, offset);
    if (name.empty())
        return;
    NameText text;
    unsigned textLength = makeText(text, name);
    m_names.insert<1>(set, from, to, offset, text, textLength);
}

} // namespace JSC
