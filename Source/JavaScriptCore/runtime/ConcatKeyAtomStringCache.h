/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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

#include "JSString.h"
#include <wtf/TZoneMalloc.h>
#include <wtf/text/AtomStringImpl.h>

namespace JSC {

class VM;

class ConcatKeyAtomStringCache {
    WTF_MAKE_TZONE_ALLOCATED(ConcatKeyAtomStringCache);
    WTF_MAKE_NONCOPYABLE(ConcatKeyAtomStringCache);
public:
    enum class Mode : uint8_t {
        Variable0,
        Variable1,
        Variable2,
        Megamorphic,
    };

    static constexpr auto maxCapacity = 64;
    static constexpr auto maxStringLengthForCache = 64;

    ConcatKeyAtomStringCache(CodeBlock* owner, Mode mode)
        : m_owner(owner)
        , m_mode(mode)
    { }

    template<typename Func>
    JSString* getOrInsert(VM&, JSString*, JSString*, JSString*, const Func&);

    struct CacheEntry {
        static constexpr ptrdiff_t offsetOfKey()
        {
            return OBJECT_OFFSETOF(CacheEntry, m_key);
        }

        static constexpr ptrdiff_t offsetOfValue()
        {
            return OBJECT_OFFSETOF(CacheEntry, m_value);
        }

        WriteBarrier<JSString> m_key { };
        WriteBarrier<JSString> m_value { };
    };

    static constexpr ptrdiff_t offsetOfQuickCache0()
    {
        return OBJECT_OFFSETOF(ConcatKeyAtomStringCache, m_quickCache);
    }

    static constexpr ptrdiff_t offsetOfQuickCache1()
    {
        return OBJECT_OFFSETOF(ConcatKeyAtomStringCache, m_quickCache) + sizeof(CacheEntry);
    }



    DECLARE_VISIT_AGGREGATE;

    size_t size() const { return m_cache.size(); }

private:
    CodeBlock* m_owner { nullptr };
    std::array<CacheEntry, 2> m_quickCache { };
    UncheckedKeyHashMap<Ref<AtomStringImpl>, JSString*> m_cache;
    Mode m_mode { Mode::Megamorphic };
    Lock m_lock;
};

} // namespace JSC
