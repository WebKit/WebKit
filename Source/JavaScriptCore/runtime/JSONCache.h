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

#include <JavaScriptCore/PropertyOffset.h>
#include <JavaScriptCore/WriteBarrier.h>
#include <array>
#include <span>
#include <wtf/Noncopyable.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/text/AtomStringImpl.h>
#include <wtf/text/Latin1Character.h>

namespace JSC {

class JSString;
class Structure;
class VM;

// Caches for JSON.parse: atoms and JSStrings for short keys and values, a map from (Structure, property
// name) to the Structure's existing property addition transition, so that a key seen before skips both the
// atom lookup and the transition table lookup, and name tables that let such a key be matched directly
// against the source text.
//
// Transition and name entries are stored without write barriers and are not otherwise marked. While a parse is running,
// visitAggregate() marks every entry; otherwise the transitions are cleared at the end of every GC, before
// anything is swept. Both decisions are made with the mutator stopped: the final marking fixpoint runs every
// constraint, and the collector goes from it to the end phase without resuming the mutator, so the two see
// the same m_isParsing. Every entry therefore refers to a live Structure.
class JSONCache {
    WTF_MAKE_NONCOPYABLE(JSONCache);
    WTF_MAKE_TZONE_ALLOCATED(JSONCache);
public:
    static constexpr auto maxStringLengthForCache = 27;
    static constexpr auto atomStringCapacity = 512;
    static constexpr auto stringCapacity = 512;

    struct AtomStringSlot {
        char16_t m_buffer[maxStringLengthForCache] { };
        char16_t m_length { 0 };
        RefPtr<AtomStringImpl> m_impl;
    };
    static_assert(sizeof(AtomStringSlot) <= 64);

    static constexpr unsigned maxLongStringLength = 256;
    static constexpr unsigned longStringCapacityLog2 = 7;
    static constexpr unsigned longStringCapacity = 1U << longStringCapacityLog2;

    static constexpr unsigned primaryTransitionCapacity = 256;
    static constexpr unsigned secondaryTransitionCapacity = 64;

    struct TransitionEntry {
        WriteBarrierStructureID from;
        WriteBarrierStructureID to;
    };
    static_assert(sizeof(TransitionEntry) == 8);

    static constexpr unsigned nameEntrySize = 256;

    // A Structure's property addition transition, with the source text that names it: the quoted
    // key and the colon after it, as JSON.stringify writes them. The next object of the same shape can
    // then match its key with two vector comparisons against the source.
    struct NameEntry {
        WriteBarrierStructureID from;
        WriteBarrierStructureID to;
        PropertyOffset offset;
        uint8_t textLength;
        uint16_t toNameIndex;
    };
    static_assert(sizeof(NameEntry) == 16);
    static_assert(nameEntrySize <= std::numeric_limits<uint16_t>::max());

    class ParsingScope {
        WTF_MAKE_NONCOPYABLE(ParsingScope);
    public:
        explicit ParsingScope(JSONCache& cache)
            : m_cache(cache)
            , m_wasParsing(cache.m_isParsing)
        {
            cache.m_isParsing = true;
        }

        ~ParsingScope()
        {
            m_cache.m_isParsing = m_wasParsing;
        }

    private:
        JSONCache& m_cache;
        bool m_wasParsing;
    };

    JSONCache() = default;

    template<typename CharacterType>
    ALWAYS_INLINE Ref<AtomStringImpl> makeIdentifier(VM&, std::span<const CharacterType> characters);

    template<typename CharacterType>
    ALWAYS_INLINE AtomStringImpl* existingIdentifier(VM&, std::span<const CharacterType> characters);

    template<typename CharacterType>
    ALWAYS_INLINE JSString* makeJSString(VM&, std::span<const CharacterType> characters);

    template<typename CharacterType> ALWAYS_INLINE Structure* getTransition(Structure* from, std::span<const CharacterType> name);
    template<typename CharacterType> ALWAYS_INLINE void addTransition(Structure* from, Structure* to, std::span<const CharacterType> name);

    static ALWAYS_INLINE unsigned nameIndex(StructureID);
    // Returns the entry whose text the source starts with, for a transition from the Structure whose
    // nameIndex() is index. The caller consumes entry->textLength characters of the source.
    ALWAYS_INLINE const NameEntry* findName(StructureID from, unsigned index, std::span<const Latin1Character> source) const LIFETIME_BOUND;

    // addName() serves Structures with a single transition. addPrefixedName() serves the others: the first
    // four bytes of the key's source text are part of its index, so that a Structure with several transitions
    // can have an entry for each of them.
    void addName(Structure* from, Structure* to, PropertyOffset);
    void addPrefixedName(Structure* from, Structure* to, PropertyOffset);

    ALWAYS_INLINE void clearStrings()
    {
        m_atomStrings.fill({ });
        clearJSStrings();
    }

    ALWAYS_INLINE void clearJSStrings()
    {
        m_jsStrings.fill(nullptr);
        m_longJSStrings.fill(nullptr);
    }

    template<typename Visitor> void visitAggregate(Visitor&);

    void reconcileTransitionsAtGCEnd()
    {
        if (m_isParsing)
            return;
        m_primaryTransitions.fill({ });
        m_secondaryTransitions.fill({ });
        m_names.entries.fill({ });
        m_prefixedNames.entries.fill({ });
    }

private:
    // A new entry replaces the one in its slot, or in the 2-way prefixed table demotes way 0 to way 1,
    // because consecutive objects usually have the same shape, so only recent entries matter.
    static constexpr unsigned prefixedNameEntrySize = 128;
    static constexpr unsigned prefixedNameWays = 2;

    static constexpr unsigned maxNameTextLength = 32;
    static constexpr unsigned maxNameLength = maxNameTextLength - 3;
    using NameText = std::array<Latin1Character, maxNameTextLength>;
    using NameSource = std::span<const Latin1Character, maxNameTextLength>;

    ALWAYS_INLINE unsigned atomStringIndex(char16_t firstCharacter, char16_t lastCharacter, char16_t length)
    {
        unsigned hash = (firstCharacter << 6) ^ ((lastCharacter << 14) ^ firstCharacter);
        hash += (hash >> 14) + (length << 14);
        hash ^= hash << 14;
        return (hash + (hash >> 6)) % atomStringCapacity;
    }

    ALWAYS_INLINE AtomStringSlot& atomStringSlot(char16_t firstCharacter, char16_t lastCharacter, char16_t length)
    {
        return m_atomStrings[atomStringIndex(firstCharacter, lastCharacter, length)];
    }

    template<typename CharacterType>
    JSString* makeLongJSString(VM&, std::span<const CharacterType> characters);

    static ALWAYS_INLINE uint64_t transitionKey(StructureID, unsigned length, Latin1Character first, Latin1Character last);
    template<typename CharacterType> static ALWAYS_INLINE uint64_t transitionKey(StructureID, std::span<const CharacterType> name);
    static ALWAYS_INLINE unsigned primaryTransitionIndex(uint64_t key);
    static ALWAYS_INLINE unsigned secondaryTransitionIndex(uint64_t key);
    template<typename CharacterType> static ALWAYS_INLINE Structure* transitionIfMatches(const TransitionEntry&, StructureID from, std::span<const CharacterType> name);
    static ALWAYS_INLINE unsigned prefixedNameIndex(StructureID, uint32_t prefix);
    static std::span<const Latin1Character> recordableName(Structure* from, Structure* to, PropertyOffset);
    static unsigned makeText(NameText&, std::span<const Latin1Character> name);
    static uint32_t textPrefix(std::span<const Latin1Character> name);
    static ALWAYS_INLINE bool textMatches(const NameText&, unsigned length, NameSource);

    // Texts are kept apart from the entries, which a parse reads for every key, so that the entries
    // occupy fewer cache lines. A text is only meaningful while its entry has a source Structure.
    template<unsigned size>
    struct NameTable {
        ALWAYS_INLINE const NameEntry* match(unsigned index, StructureID from, std::span<const Latin1Character> source) const LIFETIME_BOUND;
        template<unsigned ways> void insert(unsigned set, Structure* from, Structure* to, PropertyOffset, const NameText&, unsigned textLength);

        std::array<NameEntry, size> entries { };
        std::array<NameText, size> texts { };
    };

    std::array<AtomStringSlot, stringCapacity> m_atomStrings { };
    std::array<JSString*, stringCapacity> m_jsStrings { };
    std::array<JSString*, longStringCapacity> m_longJSStrings { };
    std::array<TransitionEntry, primaryTransitionCapacity> m_primaryTransitions { };
    std::array<TransitionEntry, secondaryTransitionCapacity> m_secondaryTransitions { };
    NameTable<nameEntrySize> m_names { };
    NameTable<prefixedNameEntrySize> m_prefixedNames { };
    bool m_isParsing { false };
};

} // namespace JSC
