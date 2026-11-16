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

// A cell in a PreciseAllocation is only half aligned, which would leave StringEntry no low bits for its key.
static_assert(!JSString::numberOfLowerTierPreciseCells && !JSRopeString::numberOfLowerTierPreciseCells);

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
ALWAYS_INLINE JSString* JSONCache::makeJSString(VM& vm, std::span<const CharacterType> characters, bool isSourceText)
{
    if (characters.empty())
        return jsEmptyString(vm);

    auto firstCharacter = characters.front();
    if (characters.size() == 1) {
        if (firstCharacter <= maxSingleCharacterString)
            return jsSingleCharacterString(vm, firstCharacter);
    } else if (characters.size() > maxAtomizeStringLength)
        return makeLongJSString(vm, characters, isSourceText);

    uint64_t key = atomStringKey(characters);
    unsigned index = primaryAtomStringIndex(key);
    if (atomStringMatches(index, characters)) [[likely]] {
        if (JSString* cached = m_jsStrings[index]) [[likely]]
            return cached;
    }
    return makeJSStringSlow(vm, key, characters);
}

template<typename CharacterType>
ALWAYS_INLINE bool JSONCache::hasUnsafeStringCharacter(std::span<const CharacterType> characters)
{
    using UnsignedType = SameSizeUnsignedInteger<CharacterType>;
    auto vectorMatch = [](auto input) ALWAYS_INLINE_LAMBDA {
        auto quotes = SIMD::equal(input, SIMD::splat<UnsignedType>('"'));
        auto escapes = SIMD::equal(input, SIMD::splat<UnsignedType>('\\'));
        auto controls = SIMD::lessThan(input, SIMD::splat<UnsignedType>(' '));
        return SIMD::findFirstNonZeroIndex(SIMD::bitOr(quotes, escapes, controls));
    };
    auto scalarMatch = [](CharacterType character) ALWAYS_INLINE_LAMBDA {
        return character < ' ' || character == '"' || character == '\\';
    };
    return SIMD::find(characters, vectorMatch, scalarMatch) != std::to_address(characters.end());
}

template<typename CharacterType>
ALWAYS_INLINE bool JSONCache::longStringMatches(JSString* string, std::span<const CharacterType> characters)
{
    // A JSString's characters can move to another StringImpl when it is atomized, so they are read from it.
    SUPPRESS_UNCOUNTED_LOCAL StringImpl* impl = string->getValueImpl();
    ASSERT(impl->length() == characters.size());
    return impl->is8Bit() ? equal(impl->span8().data(), characters) : equal(impl->span16().data(), characters);
}

ALWAYS_INLINE void JSONCache::insertLongString(std::span<StringEntry, longStringWays> entries, JSString* string, uint64_t key)
{
    for (unsigned way = longStringWays - 1; way; --way)
        entries[way] = entries[way - 1];
    entries[0] = { string, key };
}

// Long values such as URLs and type names repeat heavily in real JSON, so they are shared without atomizing.
template<typename CharacterType>
inline JSString* JSONCache::makeLongJSString(VM& vm, std::span<const CharacterType> characters, bool isSourceText)
{
    auto create = [&] {
        if constexpr (std::is_same_v<CharacterType, char16_t>)
            return jsNontrivialString(vm, String(StringImpl::create8BitIfPossible(characters)));
        else
            return jsNontrivialString(vm, String(characters));
    };

    if (characters.size() > maxLongStringLength)
        return create();

    auto index = longStringIndex(characters.data(), std::min<size_t>(characters.size(), longStringSearchLength));
    uint64_t key = StringEntry::makeKey(characters.size() + 2, index.hashBits);
    std::span<StringEntry, longStringWays> entries { std::span { m_longStrings }.subspan(index.set, longStringWays) };
    for (unsigned way = 0; way < longStringWays; ++way) {
        JSString* cached = entries[way].string();
        if (!cached)
            break;
        if ((entries[way].key() | sourceTextKeyBit) != key)
            continue;
        if (longStringMatches(cached, characters)) {
            if (way)
                std::swap(entries[0], entries[way]);
            return cached;
        }
    }

    StringEntry* secondary = nullptr;
    uint64_t secondaryKey = 0;
    if (characters.size() >= longStringSearchLength) {
        auto secondaryIndex = secondaryLongStringIndex(characters.data(), characters.size());
        secondary = &m_secondaryLongStrings[secondaryIndex.set];
        secondaryKey = StringEntry::makeKey(characters.size() + 2, secondaryIndex.hashBits);
        if ((secondary->key() | sourceTextKeyBit) == secondaryKey && longStringMatches(secondary->string(), characters)) {
            JSString* cached = secondary->string();
            insertLongString(entries, cached, key & (secondary->key() | ~sourceTextKeyBit));
            return cached;
        }
    }

    JSString* result = create();
    // The source text of a value that needs escapes is not its characters between quotes.
    if (!isSourceText && hasUnsafeStringCharacter(characters)) {
        key &= ~sourceTextKeyBit;
        secondaryKey &= ~sourceTextKeyBit;
    }
    insertLongString(entries, result, key);
    if (secondary)
        *secondary = { result, secondaryKey };
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

template<typename CharacterType>
ALWAYS_INLINE JSONCache::TextWindow JSONCache::loadTextWindow(NameSource<CharacterType> source)
{
    constexpr size_t stride = SIMD::stride<uint8_t>;
    static_assert(std::tuple_size_v<NameText> == 2 * stride);
    if constexpr (sizeof(CharacterType) == 1) {
        auto zero = SIMD::splat<uint8_t>(0);
        return {
            { SIMD::load(std::bit_cast<const uint8_t*>(source.data())), SIMD::load(std::bit_cast<const uint8_t*>(source.template last<stride>().data())) },
            { zero, zero },
        };
    } else {
        auto first = simde_vld2q_u8(std::bit_cast<const uint8_t*>(source.data()));
        auto last = simde_vld2q_u8(std::bit_cast<const uint8_t*>(source.template last<stride>().data()));
        return { { first.val[0], last.val[0] }, { first.val[1], last.val[1] } };
    }
}

ALWAYS_INLINE bool JSONCache::textMatches(const NameText& text, unsigned length, const TextWindow& window)
{
    ASSERT(length <= text.size());
    constexpr size_t stride = SIMD::stride<uint8_t>;
    constexpr simde_uint8x16_t lowIndices { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15 };
    constexpr simde_uint8x16_t highIndices { 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31 };
    auto lengths = SIMD::splat<uint8_t>(length);
    auto expected = std::span { text };
    auto low = SIMD::bitOr(SIMD::bitXor(SIMD::load(std::bit_cast<const uint8_t*>(expected.data())), window.characters[0]), window.nonLatin1[0]);
    auto high = SIMD::bitOr(SIMD::bitXor(SIMD::load(std::bit_cast<const uint8_t*>(expected.template last<stride>().data())), window.characters[1]), window.nonLatin1[1]);
    return !SIMD::isNonZero(SIMD::bitOr(SIMD::bitAnd(low, SIMD::lessThan(lowIndices, lengths)), SIMD::bitAnd(high, SIMD::lessThan(highIndices, lengths))));
}

template<unsigned size>
template<typename CharacterType>
ALWAYS_INLINE const JSONCache::NameEntry* JSONCache::NameTable<size>::match(unsigned index, StructureID from, std::span<const CharacterType> source) const
{
    auto& entry = m_entries[index];
    if (entry.from.value() != from || source.size() < maxNameTextLength)
        return nullptr;
    if (!textMatches(m_texts[index], entry.textLength, source.template first<maxNameTextLength>()))
        return nullptr;
    return &entry;
}

ALWAYS_INLINE uint32_t JSONCache::sourcePrefix(std::span<const Latin1Character> source)
{
    return WTF::unalignedLoad<uint32_t>(source.data());
}

ALWAYS_INLINE uint32_t JSONCache::sourcePrefix(std::span<const char16_t> source)
{
    uint64_t characters = WTF::unalignedLoad<uint64_t>(source.data());
    characters = (characters | (characters >> 8)) & 0x0000FFFF0000FFFFULL;
    return static_cast<uint32_t>(characters | (characters >> 16));
}

template<typename CharacterType>
ALWAYS_INLINE const JSONCache::NameEntry* JSONCache::findName(StructureID from, unsigned index, std::span<const CharacterType> source) const
{
    ASSERT(index == nameIndex(from));
    if (auto* entry = m_names.match(index, from, source))
        return entry;
    if (source.size() < sizeof(uint32_t))
        return nullptr;
    unsigned set = prefixedNameIndex(from, sourcePrefix(source));
    for (unsigned way = 0; way < prefixedNameWays; ++way) {
        if (auto* entry = m_prefixedNames.match(set + way, from, source))
            return entry;
    }
    return nullptr;
}

ALWAYS_INLINE uint64_t JSONCache::stringPrefix(const Latin1Character* characters)
{
    return WTF::unalignedLoad<uint64_t>(characters);
}

ALWAYS_INLINE uint64_t JSONCache::stringPrefix(const char16_t* characters)
{
WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN
    return static_cast<uint64_t>(sourcePrefix(unsafeMakeSpan(characters, 4))) | (static_cast<uint64_t>(sourcePrefix(unsafeMakeSpan(characters + 4, 4))) << 32);
WTF_ALLOW_UNSAFE_BUFFER_USAGE_END
}

ALWAYS_INLINE unsigned JSONCache::stringIndex(std::span<const Latin1Character> source)
{
    static_assert(hasOneBitSet(stringEntrySize) && hasOneBitSet(stringWays));
    ASSERT(source.size() >= sizeof(uint64_t));
    uint64_t prefix = stringPrefix(source.data());
    return ((prefix * 0x9E3779B97F4A7C15ULL) >> (64 - WTF::fastLog2(stringEntrySize))) & ~(stringWays - 1);
}

ALWAYS_INLINE const JSONCache::StringEntry* JSONCache::StringTable::match(unsigned index, const TextWindow& window) const
{
    auto& entry = m_entries[index];
    ASSERT(entry.textLength() <= maxNameTextLength);
    // Both conditions are evaluated without a branch between them, so that the lookup branches only on
    // whether the value matched.
    unsigned matches = static_cast<unsigned>(!!entry.string()) & static_cast<unsigned>(textMatches(m_texts[index], entry.textLength(), window));
    return matches ? &entry : nullptr;
}

ALWAYS_INLINE void JSONCache::StringTable::insert(unsigned set, JSString* string, std::span<const Latin1Character> source, unsigned textLength)
{
    for (unsigned way = stringWays - 1; way; --way) {
        m_entries[set + way] = m_entries[set + way - 1];
        m_texts[set + way] = m_texts[set + way - 1];
    }
    m_entries[set] = { string, StringEntry::makeKey(textLength, 0) };
    WTF::copyElements(std::span<Latin1Character> { m_texts[set] }, source.first(maxNameTextLength));
}

ALWAYS_INLINE const JSONCache::StringEntry* JSONCache::findShortString(std::span<const Latin1Character> source)
{
    static_assert(stringWays == 2);
    if (source.size() < maxNameTextLength)
        return nullptr;
    unsigned set = stringIndex(source);
    auto window = loadTextWindow(source.first<maxNameTextLength>());
    auto* first = m_strings.match(set, window);
    auto* second = m_strings.match(set + 1, window);
    if (first)
        return first;
    if (second) {
        std::swap(m_strings.m_entries[set], m_strings.m_entries[set + 1]);
        // Copying the texts whole is a few vector moves, where std::swap would exchange them a character at a time.
        NameText text = m_strings.m_texts[set];
        m_strings.m_texts[set] = m_strings.m_texts[set + 1];
        m_strings.m_texts[set + 1] = text;
        return &m_strings.m_entries[set];
    }
    return nullptr;
}

ALWAYS_INLINE void JSONCache::addString(std::span<const Latin1Character> source, unsigned textLength, JSString* string)
{
    if (textLength > maxNameTextLength || source.size() < maxNameTextLength)
        return;
    m_strings.insert(stringIndex(source), string, source, textLength);
}

WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN
template<typename CharacterType>
ALWAYS_INLINE uint64_t JSONCache::longStringHash(const CharacterType* characters, size_t length)
{
    static_assert(maxAtomizeStringLength >= sizeof(uint64_t));
    ASSERT(length > maxAtomizeStringLength && length <= maxLongStringLength);
    uint64_t head = stringPrefix(characters);
    uint64_t tail = stringPrefix(characters + length - sizeof(uint64_t));
    uint64_t hash = (head * 0x9E3779B97F4A7C15ULL) ^ (tail * 0xC2B2AE3D27D4EB4FULL) ^ length;
    hash ^= hash >> 32;
    return hash * 0x9E3779B97F4A7C15ULL;
}

// The hash bits below those that choose the set fill the key bits that the text length leaves free: seven
// above it, and three next to sourceTextKeyBit in the low bits of the pointer.
template<unsigned setBits>
ALWAYS_INLINE uint64_t JSONCache::longStringHashBits(uint64_t hash)
{
    constexpr unsigned highBits = 64 - StringEntry::pointerBits - StringEntry::textLengthBits;
    constexpr unsigned lowBits = 3;
    static_assert(sourceTextKeyBit == 1 && !(StringEntry::pointerMask & (((1ULL << lowBits) - 1) << 1)));
    uint64_t bits = (hash >> (64 - setBits - highBits - lowBits)) & ((1ULL << (highBits + lowBits)) - 1);
    return ((bits >> lowBits) << (64 - highBits)) | ((bits & ((1ULL << lowBits) - 1)) << 1) | sourceTextKeyBit;
}

template<typename CharacterType>
ALWAYS_INLINE JSONCache::LongStringIndex JSONCache::longStringIndex(const CharacterType* characters, size_t length)
{
    static_assert(hasOneBitSet(longStringEntrySize) && hasOneBitSet(longStringWays));
    ASSERT(length <= longStringSearchLength);
    uint64_t hash = longStringHash(characters, length);
    constexpr unsigned setBits = WTF::fastLog2(longStringEntrySize);
    return { static_cast<unsigned>(hash >> (64 - setBits)) & ~(longStringWays - 1), longStringHashBits<setBits>(hash) };
}

template<typename CharacterType>
ALWAYS_INLINE JSONCache::LongStringIndex JSONCache::secondaryLongStringIndex(const CharacterType* characters, size_t length)
{
    static_assert(hasOneBitSet(secondaryLongStringEntrySize));
    uint64_t hash = longStringHash(characters, length);
    constexpr unsigned setBits = WTF::fastLog2(secondaryLongStringEntrySize);
    return { static_cast<unsigned>(hash >> (64 - setBits)), longStringHashBits<setBits>(hash) };
}

ALWAYS_INLINE std::optional<size_t> JSONCache::boundedValueLength(std::span<const Latin1Character> source)
{
    constexpr size_t stride = SIMD::stride<uint8_t>;
    static_assert(!(longStringSearchLength % stride));
    ASSERT(!source.empty());
    const Latin1Character* characters = source.data() + 1;
    size_t available = std::min<size_t>(source.size() - 1, longStringSearchLength);
    size_t offset = 0;
    for (; offset + stride <= available; offset += stride) {
        auto quotes = SIMD::equal(SIMD::load(std::bit_cast<const uint8_t*>(characters + offset)), SIMD::splat<uint8_t>('"'));
        if (auto index = SIMD::findFirstNonZeroIndex(quotes))
            return offset + *index;
    }
    for (; offset < available; ++offset) {
        if (characters[offset] == '"')
            return offset;
    }
    if (available == longStringSearchLength)
        return longStringSearchLength;
    return std::nullopt;
}

// Compares 32 characters at a time, starting with the last 32 so that values sharing a common prefix fail on
// the first comparison. Fewer than 32 characters are compared as two overlapping halves.
ALWAYS_INLINE bool JSONCache::longTextMatches(std::span<const Latin1Character> expected, std::span<const Latin1Character> source)
{
    constexpr size_t stride = SIMD::stride<uint8_t>;
    size_t length = expected.size();
    ASSERT(length == source.size());
    ASSERT(length >= stride);
    auto load = [](const Latin1Character* characters) ALWAYS_INLINE_LAMBDA {
        return SIMD::load(std::bit_cast<const uint8_t*>(characters));
    };
    auto differences = [&](size_t first, size_t second) ALWAYS_INLINE_LAMBDA {
        return SIMD::bitOr(SIMD::bitXor(load(expected.data() + first), load(source.data() + first)), SIMD::bitXor(load(expected.data() + second), load(source.data() + second)));
    };
    if (length < 2 * stride)
        return !SIMD::isNonZero(differences(0, length - stride));
    size_t last = length - 2 * stride;
    if (SIMD::isNonZero(differences(last, last + stride)))
        return false;
    for (size_t offset = 0; offset < last; offset += 2 * stride) {
        if (SIMD::isNonZero(differences(offset, offset + stride)))
            return false;
    }
    return true;
}

ALWAYS_INLINE const JSONCache::StringEntry* JSONCache::findLongString(VM& vm, std::span<const Latin1Character> source, StructureID keepAlive)
{
    auto length = boundedValueLength(source);
    if (!length || *length <= maxAtomizeStringLength)
        return nullptr;
    auto index = longStringIndex(source.data() + 1, *length);
    auto entries = std::span { m_longStrings }.subspan(index.set, longStringWays);
    for (unsigned way = 0; way < longStringWays; ++way) {
        JSString* string = entries[way].string();
        if (!string)
            break;
        size_t textLength = entries[way].textLength();
        if (*length < longStringSearchLength) {
            if (entries[way].key() != StringEntry::makeKey(*length + 2, index.hashBits))
                continue;
        } else if ((entries[way].key() & ~StringEntry::textLengthMask) != index.hashBits || source.size() < textLength || source.data()[textLength - 1] != '"')
            continue;
        // A JSString's characters can move to another StringImpl when it is atomized, so they are read from it.
        SUPPRESS_UNCOUNTED_LOCAL StringImpl* impl = string->getValueImpl();
        auto characters = unsafeMakeSpan(source.data() + 1, textLength - 2);
        if (!impl->is8Bit() || !longTextMatches(impl->span8(), characters))
            continue;
        if (way)
            std::swap(entries[0], entries[way]);
        return &entries[0];
    }
    return addLongString(vm, source, *length, index, keepAlive);
}
WTF_ALLOW_UNSAFE_BUFFER_USAGE_END

ALWAYS_INLINE const JSONCache::StringEntry* JSONCache::findOrCreateString(VM& vm, std::span<const Latin1Character> source, StructureID keepAlive)
{
    if (auto* entry = findShortString(source))
        return entry;
    return findLongString(vm, source, keepAlive);
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
