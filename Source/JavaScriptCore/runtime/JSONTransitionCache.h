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

#include <JavaScriptCore/WriteBarrier.h>
#include <array>
#include <span>
#include <wtf/Noncopyable.h>
#include <wtf/text/Latin1Character.h>

namespace JSC {

class Structure;

// Maps (Structure, property name) to the Structure's existing property addition transition, so that
// JSON.parse can skip both the atom lookup and the transition table lookup for a key it has seen before.
//
// Entries are stored without write barriers and are not otherwise marked. While a parse is running,
// visitAggregate() marks every entry; otherwise the table is cleared at the end of every GC, before anything
// is swept. Both decisions are made with the mutator stopped: the final marking fixpoint runs every
// constraint, and the collector goes from it to the end phase without resuming the mutator, so the two see
// the same m_isParsing. Every entry therefore refers to a live Structure.
class JSONTransitionCache {
    WTF_MAKE_NONCOPYABLE(JSONTransitionCache);
public:
    static constexpr unsigned primarySize = 256;
    static constexpr unsigned secondarySize = 64;

    struct Entry {
        WriteBarrierStructureID from;
        WriteBarrierStructureID to;
    };
    static_assert(sizeof(Entry) == 8);

    class ParsingScope {
        WTF_MAKE_NONCOPYABLE(ParsingScope);
    public:
        explicit ParsingScope(JSONTransitionCache& cache)
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
        JSONTransitionCache& m_cache;
        bool m_wasParsing;
    };

    JSONTransitionCache() = default;

    ALWAYS_INLINE Structure* get(Structure* from, std::span<const Latin1Character> name);
    ALWAYS_INLINE void add(Structure* from, Structure* to, std::span<const Latin1Character> name);

    template<typename Visitor> void visitAggregate(Visitor&);

    void reconcileAtGCEnd()
    {
        if (m_isParsing)
            return;
        m_primary.fill({ });
        m_secondary.fill({ });
    }

private:
    static ALWAYS_INLINE uint64_t key(StructureID, unsigned length, Latin1Character first, Latin1Character last);
    static ALWAYS_INLINE uint64_t key(StructureID, std::span<const Latin1Character> name);
    static ALWAYS_INLINE unsigned primaryIndex(uint64_t key);
    static ALWAYS_INLINE unsigned secondaryIndex(uint64_t key);
    static ALWAYS_INLINE Structure* transitionIfMatches(const Entry&, StructureID from, std::span<const Latin1Character> name);

    std::array<Entry, primarySize> m_primary { };
    std::array<Entry, secondarySize> m_secondary { };
    bool m_isParsing { false };
};

} // namespace JSC
