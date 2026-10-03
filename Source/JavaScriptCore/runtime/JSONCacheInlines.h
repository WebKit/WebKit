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
#include <wtf/SIMDHelpers.h>
#include <wtf/UnalignedAccess.h>

namespace JSC {

ALWAYS_INLINE uint64_t JSONCache::atomStringKey(char16_t firstCharacter, char16_t lastCharacter, unsigned length)
{
    return length | (static_cast<uint64_t>(firstCharacter) << 8) | (static_cast<uint64_t>(lastCharacter) << 24);
}

template<typename CharacterType>
ALWAYS_INLINE uint64_t JSONCache::atomStringKey(std::span<const CharacterType> characters)
{
    return atomStringKey(characters.front(), characters.back(), characters.size());
}

ALWAYS_INLINE unsigned JSONCache::primaryAtomStringIndex(uint64_t key)
{
    return (key * 0x9E3779B97F4A7C15ULL) >> (64 - WTF::fastLog2(primaryStringCapacity));
}

ALWAYS_INLINE unsigned JSONCache::secondaryAtomStringIndex(uint64_t key)
{
    return primaryStringCapacity + ((key * 0xC2B2AE3D27D4EB4FULL) >> (64 - WTF::fastLog2(secondaryStringCapacity)));
}
static_assert(hasOneBitSet(JSONCache::primaryStringCapacity) && hasOneBitSet(JSONCache::secondaryStringCapacity));

template<typename CharacterType>
ALWAYS_INLINE bool JSONCache::atomStringMatches(unsigned index, std::span<const CharacterType> characters) const
{
    auto& slot = m_atomStrings[index];
    return slot.m_length == characters.size() && equal(slot.m_buffer, characters);
}

template<typename CharacterType>
inline unsigned JSONCache::findAtomString(uint64_t key, std::span<const CharacterType> characters) const
{
    unsigned index = primaryAtomStringIndex(key);
    if (atomStringMatches(index, characters))
        return index;
    index = secondaryAtomStringIndex(key);
    if (atomStringMatches(index, characters))
        return index;
    return stringCapacity;
}

template<typename CharacterType>
inline unsigned JSONCache::addAtomString(uint64_t key, std::span<const CharacterType> characters, Ref<AtomStringImpl>&& impl)
{
    unsigned index = primaryAtomStringIndex(key);
    auto& slot = m_atomStrings[index];
    if (slot.m_length) {
        unsigned evictedIndex = secondaryAtomStringIndex(atomStringKey(std::span<const char16_t> { slot.m_buffer }.first(slot.m_length)));
        m_atomStrings[evictedIndex] = WTF::move(slot);
        m_jsStrings[evictedIndex] = m_jsStrings[index];
    }
    slot.m_impl = WTF::move(impl);
    slot.m_length = characters.size();
    WTF::copyElements(std::span<char16_t> { slot.m_buffer }, characters);
    m_jsStrings[index] = nullptr;
    return index;
}

template<typename CharacterType>
NEVER_INLINE Ref<AtomStringImpl> JSONCache::makeIdentifierSlow(uint64_t key, std::span<const CharacterType> characters)
{
    unsigned index = findAtomString(key, characters);
    if (index != stringCapacity)
        return *m_atomStrings[index].m_impl;
    Ref result = AtomStringImpl::add(characters).releaseNonNull();
    addAtomString(key, characters, result.copyRef());
    return result;
}

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

    uint64_t key = atomStringKey(characters);
    unsigned index = primaryAtomStringIndex(key);
    if (!atomStringMatches(index, characters)) [[unlikely]]
        return makeIdentifierSlow(key, characters);
    return *m_atomStrings[index].m_impl;
}

template<typename CharacterType>
NEVER_INLINE AtomStringImpl* JSONCache::existingIdentifierSlow(uint64_t key, std::span<const CharacterType> characters)
{
    unsigned index = secondaryAtomStringIndex(key);
    if (!atomStringMatches(index, characters))
        return nullptr;
    return m_atomStrings[index].m_impl.get();
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

    uint64_t key = atomStringKey(characters);
    unsigned index = primaryAtomStringIndex(key);
    if (!atomStringMatches(index, characters)) [[unlikely]]
        return existingIdentifierSlow(key, characters);
    return m_atomStrings[index].m_impl.get();
}

template<typename CharacterType>
NEVER_INLINE JSString* JSONCache::makeJSStringSlow(VM& vm, uint64_t key, std::span<const CharacterType> characters)
{
    unsigned index = findAtomString(key, characters);
    if (index != stringCapacity) {
        if (JSString* cached = m_jsStrings[index])
            return cached;
        JSString* result = jsString(vm, String { m_atomStrings[index].m_impl.get() });
        m_jsStrings[index] = result;
        return result;
    }

    Ref impl = AtomStringImpl::add(characters).releaseNonNull();
    JSString* result = jsString(vm, String { impl.copyRef() });
    m_jsStrings[addAtomString(key, characters, WTF::move(impl))] = result;
    return result;
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

    uint64_t key = atomStringKey(characters);
    unsigned index = primaryAtomStringIndex(key);
    if (atomStringMatches(index, characters)) [[likely]] {
        if (JSString* cached = m_jsStrings[index]) [[likely]]
            return cached;
    }
    return makeJSStringSlow(vm, key, characters);
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

ALWAYS_INLINE unsigned JSONCache::nameIndex(StructureID structureID)
{
    static_assert(hasOneBitSet(nameEntrySize));
    return (static_cast<uint32_t>(structureID.bits() >> 4) * 0x9E3779B9U) >> (32 - WTF::fastLog2(nameEntrySize));
}

ALWAYS_INLINE unsigned JSONCache::prefixedNameIndex(StructureID structureID, uint32_t prefix)
{
    static_assert(hasOneBitSet(prefixedNameEntrySize) && hasOneBitSet(prefixedNameWays));
    uint64_t key = (structureID.bits() >> 4) | (static_cast<uint64_t>(prefix) << 28);
    return ((key * 0x9E3779B97F4A7C15ULL) >> (64 - WTF::fastLog2(prefixedNameEntrySize))) & ~(prefixedNameWays - 1);
}

ALWAYS_INLINE bool JSONCache::textMatches(const NameText& text, unsigned length, NameSource source)
{
    ASSERT(length <= text.size());
    constexpr size_t stride = SIMD::stride<uint8_t>;
    static_assert(std::tuple_size_v<NameText> == 2 * stride);
    constexpr simde_uint8x16_t lowIndices { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15 };
    constexpr simde_uint8x16_t highIndices { 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31 };
    auto load = [](std::span<const Latin1Character, stride> characters) ALWAYS_INLINE_LAMBDA {
        return SIMD::load(std::bit_cast<const uint8_t*>(characters.data()));
    };
    NameSource expected { text };
    auto lengths = SIMD::splat<uint8_t>(length);
    auto low = SIMD::bitAnd(SIMD::bitXor(load(source.first<stride>()), load(expected.first<stride>())), SIMD::lessThan(lowIndices, lengths));
    auto high = SIMD::bitAnd(SIMD::bitXor(load(source.last<stride>()), load(expected.last<stride>())), SIMD::lessThan(highIndices, lengths));
    return !SIMD::isNonZero(SIMD::bitOr(low, high));
}

template<unsigned size>
ALWAYS_INLINE const JSONCache::NameEntry* JSONCache::NameTable<size>::match(unsigned index, StructureID from, std::span<const Latin1Character> source) const
{
    auto& entry = m_entries[index];
    if (entry.from.value() != from || source.size() < maxNameTextLength)
        return nullptr;
    if (!textMatches(m_texts[index], entry.textLength, source.first<maxNameTextLength>()))
        return nullptr;
    return &entry;
}

ALWAYS_INLINE const JSONCache::NameEntry* JSONCache::findName(StructureID from, unsigned index, std::span<const Latin1Character> source) const
{
    ASSERT(index == nameIndex(from));
    if (auto* entry = m_names.match(index, from, source))
        return entry;
    if (source.size() < sizeof(uint32_t))
        return nullptr;
    unsigned set = prefixedNameIndex(from, WTF::unalignedLoad<uint32_t>(source.data()));
    for (unsigned way = 0; way < prefixedNameWays; ++way) {
        if (auto* entry = m_prefixedNames.match(set + way, from, source))
            return entry;
    }
    return nullptr;
}

ALWAYS_INLINE unsigned JSONCache::stringIndex(std::span<const Latin1Character> source)
{
    static_assert(hasOneBitSet(stringEntrySize) && hasOneBitSet(stringWays));
    uint64_t prefix = WTF::unalignedLoad<uint64_t>(source.data());
    return ((prefix * 0x9E3779B97F4A7C15ULL) >> (64 - WTF::fastLog2(stringEntrySize))) & ~(stringWays - 1);
}

ALWAYS_INLINE const JSONCache::StringEntry* JSONCache::StringTable::match(unsigned index, std::span<const Latin1Character> source) const
{
    auto& entry = m_entries[index];
    if (!entry.string || source.size() < maxNameTextLength)
        return nullptr;
    if (!textMatches(m_texts[index], entry.textLength, source.first<maxNameTextLength>()))
        return nullptr;
    return &entry;
}

ALWAYS_INLINE void JSONCache::StringTable::insert(unsigned set, JSString* string, std::span<const Latin1Character> source, unsigned textLength)
{
    for (unsigned way = stringWays - 1; way; --way) {
        m_entries[set + way] = m_entries[set + way - 1];
        m_texts[set + way] = m_texts[set + way - 1];
    }
    m_entries[set] = { string, textLength };
    WTF::copyElements(std::span<Latin1Character> { m_texts[set] }, source.first(maxNameTextLength));
}

ALWAYS_INLINE const JSONCache::StringEntry* JSONCache::findString(std::span<const Latin1Character> source) const
{
    if (source.size() < sizeof(uint64_t))
        return nullptr;
    unsigned set = stringIndex(source);
    for (unsigned way = 0; way < stringWays; ++way) {
        if (auto* entry = m_strings.match(set + way, source))
            return entry;
    }
    return nullptr;
}

ALWAYS_INLINE void JSONCache::addString(std::span<const Latin1Character> source, unsigned textLength, JSString* string)
{
    if (textLength > maxNameTextLength || source.size() < maxNameTextLength)
        return;
    m_strings.insert(stringIndex(source), string, source, textLength);
}

template<typename Visitor>
void JSONCache::visitAggregate(Visitor& visitor)
{
    if (!m_isParsing)
        return;
    for (auto& entry : m_names.m_entries) {
        visitor.append(entry.from);
        visitor.append(entry.to);
    }
    for (auto& entry : m_prefixedNames.m_entries) {
        visitor.append(entry.from);
        visitor.append(entry.to);
    }
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
