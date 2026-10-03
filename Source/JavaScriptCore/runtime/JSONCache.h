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
    static constexpr unsigned primaryStringCapacity = 256;
    static constexpr unsigned secondaryStringCapacity = 64;
    static constexpr unsigned stringCapacity = primaryStringCapacity + secondaryStringCapacity;

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

    static constexpr unsigned stringEntrySize = 64;

    // A short string value with the source text that produced it: the quoted string, without escapes. A
    // repeated value can then be matched with two vector comparisons against the source, without scanning it.
    struct StringEntry {
        JSString* string { nullptr };
        unsigned textLength { 0 };
    };

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
    template<typename CharacterType> ALWAYS_INLINE const NameEntry* findName(StructureID from, unsigned index, std::span<const CharacterType> source) const LIFETIME_BOUND;

    // addName() serves Structures with a single transition. addPrefixedName() serves the others: the first
    // four characters of the key's source text are part of its index, so that a Structure with several transitions
    // can have an entry for each of them.
    void addName(Structure* from, Structure* to, PropertyOffset);
    void addPrefixedName(Structure* from, Structure* to, PropertyOffset);

    // Returns the entry whose text the source starts with. The caller consumes entry->textLength characters
    // of the source.
    ALWAYS_INLINE const StringEntry* findString(std::span<const Latin1Character> source) const LIFETIME_BOUND;
    // Records the first textLength characters of source, a string token without escapes, as the text of string.
    ALWAYS_INLINE void addString(std::span<const Latin1Character> source, unsigned textLength, JSString*);

    ALWAYS_INLINE void clearStrings()
    {
        m_atomStrings.fill({ });
        clearJSStrings();
    }

    ALWAYS_INLINE void clearJSStrings()
    {
        m_jsStrings.fill(nullptr);
        m_longJSStrings.fill(nullptr);
        m_strings.m_entries.fill({ });
    }

    template<typename Visitor> void visitAggregate(Visitor&);

    void reconcileTransitionsAtGCEnd()
    {
        if (m_isParsing)
            return;
        m_primaryTransitions.fill({ });
        m_secondaryTransitions.fill({ });
        m_names.m_entries.fill({ });
        m_prefixedNames.m_entries.fill({ });
    }

private:
    // A new entry replaces the one in its slot, or in the 2-way prefixed table demotes way 0 to way 1,
    // because consecutive objects usually have the same shape, so only recent entries matter. The string
    // table is 2-way for the same reason: it only needs to hold the values seen most recently.
    static constexpr unsigned prefixedNameEntrySize = 128;
    static constexpr unsigned prefixedNameWays = 2;
    static constexpr unsigned stringWays = 2;

    static constexpr unsigned maxNameTextLength = 32;
    static constexpr unsigned maxNameLength = maxNameTextLength - 3;
    using NameText = std::array<Latin1Character, maxNameTextLength>;
    using NameSource = std::span<const Latin1Character, maxNameTextLength>;
    using NameSource16 = std::span<const char16_t, maxNameTextLength>;

    // Slots [0, primaryStringCapacity) are the primary table and the rest the secondary one. A new string
    // takes its primary slot and demotes the previous occupant to that string's secondary slot, so that two
    // strings sharing a primary slot can both stay cached.
    static ALWAYS_INLINE uint64_t atomStringKey(char16_t firstCharacter, char16_t lastCharacter, unsigned length);
    static ALWAYS_INLINE unsigned primaryAtomStringIndex(uint64_t key);
    static ALWAYS_INLINE unsigned secondaryAtomStringIndex(uint64_t key);
    template<typename CharacterType> static ALWAYS_INLINE uint64_t atomStringKey(std::span<const CharacterType>);
    template<typename CharacterType> ALWAYS_INLINE bool atomStringMatches(unsigned index, std::span<const CharacterType>) const;
    // Returns stringCapacity if characters is not cached.
    template<typename CharacterType> unsigned findAtomString(uint64_t key, std::span<const CharacterType>) const;
    template<typename CharacterType> unsigned addAtomString(uint64_t key, std::span<const CharacterType>, Ref<AtomStringImpl>&&);
    // Only the primary probe is inlined into the parser. Inlining the secondary probe too slows down
    // JSON.parse of 8-bit text, where most strings that miss the primary slot are not cached at all.
    template<typename CharacterType> Ref<AtomStringImpl> makeIdentifierSlow(uint64_t key, std::span<const CharacterType>);
    template<typename CharacterType> AtomStringImpl* existingIdentifierSlow(uint64_t key, std::span<const CharacterType>);
    template<typename CharacterType> JSString* makeJSStringSlow(VM&, uint64_t key, std::span<const CharacterType>);

    template<typename CharacterType>
    JSString* makeLongJSString(VM&, std::span<const CharacterType> characters);

    static ALWAYS_INLINE uint64_t transitionKey(StructureID, unsigned length, Latin1Character first, Latin1Character last);
    template<typename CharacterType> static ALWAYS_INLINE uint64_t transitionKey(StructureID, std::span<const CharacterType> name);
    static ALWAYS_INLINE unsigned primaryTransitionIndex(uint64_t key);
    static ALWAYS_INLINE unsigned secondaryTransitionIndex(uint64_t key);
    template<typename CharacterType> static ALWAYS_INLINE Structure* transitionIfMatches(const TransitionEntry&, StructureID from, std::span<const CharacterType> name);
    static ALWAYS_INLINE unsigned prefixedNameIndex(StructureID, uint32_t prefix);
    // Names are indexed by the first characters of their source text truncated to bytes, so that 16-bit
    // source finds the entries recorded from 8-bit source.
    static ALWAYS_INLINE uint32_t sourcePrefix(std::span<const Latin1Character> source);
    static ALWAYS_INLINE uint32_t sourcePrefix(std::span<const char16_t> source);
    static ALWAYS_INLINE unsigned stringIndex(std::span<const Latin1Character> source);
    static std::span<const Latin1Character> recordableName(Structure* from, Structure* to, PropertyOffset);
    static unsigned makeText(NameText&, std::span<const Latin1Character> name);
    static uint32_t textPrefix(std::span<const Latin1Character> name);
    static ALWAYS_INLINE bool textMatches(const NameText&, unsigned length, NameSource);
    static ALWAYS_INLINE bool textMatches(const NameText&, unsigned length, NameSource16);

    // Texts are kept apart from the entries, which a parse reads for every key, so that the entries
    // occupy fewer cache lines. A text is only meaningful while its entry has a source Structure.
    template<unsigned size>
    struct NameTable {
        template<typename CharacterType> ALWAYS_INLINE const NameEntry* match(unsigned index, StructureID from, std::span<const CharacterType> source) const LIFETIME_BOUND;
        template<unsigned ways> void insert(unsigned set, Structure* from, Structure* to, PropertyOffset, const NameText&, unsigned textLength);

        std::array<NameEntry, size> m_entries { };
        std::array<NameText, size> m_texts { };
    };

    // Laid out like NameTable. A text is only meaningful while its entry has a string.
    struct StringTable {
        ALWAYS_INLINE const StringEntry* match(unsigned index, std::span<const Latin1Character> source) const LIFETIME_BOUND;
        ALWAYS_INLINE void insert(unsigned set, JSString*, std::span<const Latin1Character> source, unsigned textLength);

        std::array<StringEntry, stringEntrySize> m_entries { };
        std::array<NameText, stringEntrySize> m_texts { };
    };

    std::array<AtomStringSlot, stringCapacity> m_atomStrings { };
    std::array<JSString*, stringCapacity> m_jsStrings { };
    std::array<JSString*, longStringCapacity> m_longJSStrings { };
    std::array<TransitionEntry, primaryTransitionCapacity> m_primaryTransitions { };
    std::array<TransitionEntry, secondaryTransitionCapacity> m_secondaryTransitions { };
    NameTable<nameEntrySize> m_names { };
    NameTable<prefixedNameEntrySize> m_prefixedNames { };
    StringTable m_strings { };
    bool m_isParsing { false };
};

} // namespace JSC
