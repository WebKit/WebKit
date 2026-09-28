/*
 * Copyright (C) 2021-2026 Apple Inc. All rights reserved.
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

#include "Identifier.h"
#include "JSCJSValueInlines.h"
#include "JSONCache.h"
#include "JSString.h"
#include "SmallStrings.h"
#include "Structure.h"
#include "VM.h"
#include <wtf/MathExtras.h>
#include <wtf/UnalignedAccess.h>

namespace JSC {

template<typename CharacterType>
ALWAYS_INLINE Ref<AtomStringImpl> JSONCache::makeIdentifier(VM& vm, std::span<const CharacterType> characters)
{
    if (characters.empty())
        return *emptyAtom().impl();

    auto firstCharacter = characters.front();
    if (characters.size() == 1) {
        if (firstCharacter <= maxSingleCharacterString)
            return vm.smallStrings.singleCharacterStringRep(firstCharacter);
    } else if (characters.size() > maxStringLengthForCache) [[unlikely]]
        return AtomStringImpl::add(characters).releaseNonNull();

    auto lastCharacter = characters.back();
    unsigned index = stringIndex(firstCharacter, lastCharacter, characters.size());
    auto& slot = m_strings[index];
    if (slot.m_length != characters.size() || !equal(slot.m_buffer, characters)) [[unlikely]] {
        auto result = AtomStringImpl::add(characters);
        slot.m_impl = result;
        slot.m_length = characters.size();
        WTF::copyElements(std::span<char16_t> { slot.m_buffer }, characters);
        m_jsStrings[index] = nullptr;
        return result.releaseNonNull();
    }

    return *slot.m_impl;
}

template<typename CharacterType>
ALWAYS_INLINE AtomStringImpl* JSONCache::existingIdentifier(VM& vm, std::span<const CharacterType> characters)
{
    if (characters.empty())
        return emptyAtom().impl();

    auto firstCharacter = characters.front();
    if (characters.size() == 1) {
        if (firstCharacter <= maxSingleCharacterString)
            return vm.smallStrings.existingSingleCharacterStringRep(firstCharacter);
    } else if (characters.size() > maxStringLengthForCache) [[unlikely]]
        return nullptr;

    auto lastCharacter = characters.back();
    auto& slot = stringSlot(firstCharacter, lastCharacter, characters.size());
    if (slot.m_length != characters.size() || !equal(slot.m_buffer, characters)) [[unlikely]]
        return nullptr;

    return slot.m_impl.get();
}

template<typename CharacterType>
ALWAYS_INLINE JSString* JSONCache::makeJSString(VM& vm, std::span<const CharacterType> characters)
{
    if (characters.empty())
        return jsEmptyString(vm);

    constexpr unsigned maxAtomizeStringLength = 16;
    auto firstCharacter = characters.front();
    if (characters.size() == 1) {
        if (firstCharacter <= maxSingleCharacterString)
            return jsSingleCharacterString(vm, firstCharacter);
    } else if (characters.size() > maxAtomizeStringLength)
        return makeLongJSString(vm, characters);

    auto lastCharacter = characters.back();
    unsigned index = stringIndex(firstCharacter, lastCharacter, characters.size());
    auto& slot = m_strings[index];
    if (slot.m_length == characters.size() && equal(slot.m_buffer, characters)) [[likely]] {
        if (JSString* cached = m_jsStrings[index])
            return cached;
        JSString* result = jsString(vm, String { slot.m_impl.get() });
        m_jsStrings[index] = result;
        return result;
    }

    auto impl = AtomStringImpl::add(characters);
    slot.m_impl = impl;
    slot.m_length = characters.size();
    WTF::copyElements(std::span<char16_t> { slot.m_buffer }, characters);
    JSString* result = jsString(vm, String { WTF::move(impl) });
    m_jsStrings[index] = result;
    return result;
}

// Long values such as URLs and type names repeat heavily in real JSON, so they are shared without
// atomizing. Called only for lengths above maxAtomizeStringLength, so both 8-byte loads are in bounds.
template<typename CharacterType>
inline JSString* JSONCache::makeLongJSString(VM& vm, std::span<const CharacterType> characters)
{
    auto create = [&] {
        if constexpr (std::is_same_v<CharacterType, char16_t>)
            return jsNontrivialString(vm, String(StringImpl::create8BitIfPossible(characters)));
        else
            return jsNontrivialString(vm, String(characters));
    };

    if (characters.size() > maxLongStringLength)
        return create();

    auto bytes = asBytes(characters);
    uint64_t head = WTF::unalignedLoad<uint64_t>(bytes.data());
    uint64_t tail = WTF::unalignedLoad<uint64_t>(bytes.last(sizeof(uint64_t)).data());
    uint64_t hash = (head * 0x9E3779B97F4A7C15ULL) ^ (tail * 0xC2B2AE3D27D4EB4FULL) ^ characters.size();
    hash ^= hash >> 32;
    unsigned index = static_cast<unsigned>(hash * 0x9E3779B97F4A7C15ULL >> (64 - longStringCapacityLog2));

    JSString*& slot = m_longJSStrings[index];
    if (JSString* cached = slot) {
        SUPPRESS_UNCOUNTED_LOCAL StringImpl* impl = cached->getValueImpl();
        if (impl->length() == characters.size()) {
            if (impl->is8Bit()) {
                if (equal(impl->span8().data(), characters))
                    return cached;
            } else if (equal(impl->span16().data(), characters))
                return cached;
        }
    }

    JSString* result = create();
    slot = result;
    return result;
}

// A StructureID is the low 32 bits of a 16-byte aligned Structure's address, so only bits 4 to 31 tell
// Structures apart. The name goes above those, low enough for the multiplies to mix it into the index.
ALWAYS_INLINE uint64_t JSONCache::transitionKey(StructureID structureID, unsigned length, Latin1Character first, Latin1Character last)
{
    uint64_t nameKey = length | (static_cast<uint64_t>(first) << 8) | (static_cast<uint64_t>(last) << 16);
    return (structureID.bits() >> 4) | (nameKey << 28);
}

template<typename CharacterType>
ALWAYS_INLINE uint64_t JSONCache::transitionKey(StructureID structureID, std::span<const CharacterType> name)
{
    if (name.empty())
        return transitionKey(structureID, 0, 0, 0);
    return transitionKey(structureID, name.size(), static_cast<Latin1Character>(name.front()), static_cast<Latin1Character>(name.back()));
}

ALWAYS_INLINE unsigned JSONCache::primaryTransitionIndex(uint64_t key)
{
    return (key * 0x9E3779B97F4A7C15ULL) >> (64 - WTF::fastLog2(primaryTransitionCapacity));
}

ALWAYS_INLINE unsigned JSONCache::secondaryTransitionIndex(uint64_t key)
{
    return (key * 0xC2B2AE3D27D4EB4FULL) >> (64 - WTF::fastLog2(secondaryTransitionCapacity));
}
static_assert(hasOneBitSet(JSONCache::primaryTransitionCapacity) && hasOneBitSet(JSONCache::secondaryTransitionCapacity));

template<typename CharacterType>
ALWAYS_INLINE Structure* JSONCache::transitionIfMatches(const TransitionEntry& entry, StructureID from, std::span<const CharacterType> name)
{
    if (entry.from.value() != from)
        return nullptr;
    Structure* to = entry.to.unvalidatedGet();
    SUPPRESS_UNCOUNTED_LOCAL UniquedStringImpl* transitionName = to->transitionPropertyName();
    ASSERT(transitionName);
    if (transitionName->length() != name.size())
        return nullptr;
    if (transitionName->is8Bit()) [[likely]] {
        if (!WTF::equal(transitionName->span8().data(), name))
            return nullptr;
    } else if (!WTF::equal(transitionName->span16().data(), name))
        return nullptr;
    return to;
}

template<typename CharacterType>
ALWAYS_INLINE Structure* JSONCache::getTransition(Structure* from, std::span<const CharacterType> name)
{
    StructureID fromID = StructureID::encode(from);
    uint64_t key = transitionKey(fromID, name);
    if (Structure* to = transitionIfMatches(m_primaryTransitions[primaryTransitionIndex(key)], fromID, name))
        return to;
    return transitionIfMatches(m_secondaryTransitions[secondaryTransitionIndex(key)], fromID, name);
}

template<typename CharacterType>
ALWAYS_INLINE void JSONCache::addTransition(Structure* from, Structure* to, std::span<const CharacterType> name)
{
    // Pinning a Structure clears its transition property name and previous Structure, but only Structures
    // created by other kinds of transitions are pinned, and only as they are created.
    ASSERT(!from->isDictionary());
    ASSERT(!from->hasBeenDictionary());
    ASSERT(to->transitionKind() == TransitionKind::PropertyAddition);
    ASSERT(!to->transitionPropertyAttributes());
    ASSERT(to->previousID() == from);
    ASSERT(to->transitionPropertyName() && WTF::equal(to->transitionPropertyName(), name));
    auto& entry = m_primaryTransitions[primaryTransitionIndex(transitionKey(StructureID::encode(from), name))];
    if (StructureID evictedFrom = entry.from.value()) {
        // Keys truncate the first and last characters to Latin1Character, however wide the name is.
        auto* evictedName = entry.to.unvalidatedGet()->transitionPropertyName();
        ASSERT(evictedName);
        unsigned length = evictedName->length();
        uint64_t evictedKey = transitionKey(evictedFrom, 0, 0, 0);
        if (length)
            evictedKey = transitionKey(evictedFrom, length, static_cast<Latin1Character>((*evictedName)[0]), static_cast<Latin1Character>((*evictedName)[length - 1]));
        m_secondaryTransitions[secondaryTransitionIndex(evictedKey)] = entry;
    }
    entry.from.setWithoutWriteBarrier(from);
    entry.to.setWithoutWriteBarrier(to);
}

template<typename Visitor>
void JSONCache::visitAggregate(Visitor& visitor)
{
    if (!m_isParsing)
        return;
    for (auto& entry : m_primaryTransitions) {
        visitor.append(entry.from);
        visitor.append(entry.to);
    }
    for (auto& entry : m_secondaryTransitions) {
        visitor.append(entry.from);
        visitor.append(entry.to);
    }
}

} // namespace JSC
