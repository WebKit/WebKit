/*
 * Copyright (C) 2009 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include <array>
#include <wtf/HashFunctions.h>
#include <wtf/text/WTFString.h>

namespace JSC {

class JSString;
class SmallStrings;

class NumericStrings {
public:
    static const size_t cacheSize = 1024;
    static const size_t doubleCacheSize = 4096;

    template<typename T>
    struct CacheEntry {
        T key;
        String value;
    };

    template<typename T>
    struct CacheEntryWithJSString {
        T key;
        String value;
        JSString* jsString { nullptr };
    };

    struct StringWithJSString {
        String value;
        JSString* jsString { nullptr };

        static constexpr ptrdiff_t offsetOfJSString() { return OBJECT_OFFSETOF(StringWithJSString, jsString); }
    };

    struct DoubleCache {
        WTF_MAKE_TZONE_ALLOCATED(DoubleCache);
    public:
        std::array<CacheEntryWithJSString<double>, doubleCacheSize> m_doubleCache { };
    };

    ALWAYS_INLINE const String& add(double d)
    {
        if (!m_doubleCache) [[unlikely]]
            initializeDoubleCache();
        auto& entry = lookup(d);
        if (d == entry.key && !entry.value.isNull())
            return entry.value;
        entry.key = d;
        entry.value = String::number(d);
        entry.jsString = nullptr;
        return entry.value;
    }

    ALWAYS_INLINE const String& add(int i)
    {
        if (static_cast<unsigned>(i) < cacheSize)
            return lookupSmallString(static_cast<unsigned>(i)).value;
        auto& entry = lookup(i);
        if (i == entry.key && !entry.value.isNull())
            return entry.value;
        entry.key = i;
        entry.value = String::number(i);
        entry.jsString = nullptr;
        return entry.value;
    }

    ALWAYS_INLINE const String& add(unsigned i)
    {
        if (i < cacheSize)
            return lookupSmallString(static_cast<unsigned>(i)).value;
        auto& entry = lookup(i);
        if (i == entry.key && !entry.value.isNull())
            return entry.value;
        entry.key = i;
        entry.value = String::number(i);
        return entry.value;
    }

    JSString* addJSString(VM&, int);
    JSString* addJSString(VM&, double);

    void clearOnGarbageCollection()
    {
        for (auto& entry : m_intCache)
            entry.jsString = nullptr;
        if (m_doubleCache) {
            for (auto& entry : m_doubleCache->m_doubleCache)
                entry.jsString = nullptr;
        }
        // 0-9 are managed by SmallStrings. They never die.
        for (unsigned i = 10; i < m_smallIntCache.size(); ++i)
            m_smallIntCache[i].jsString = nullptr;
    }

    template<typename Visitor>
    void visitAggregate(Visitor& visitor)
    {
        for (auto& entry : m_intCache)
            visitor.appendUnbarriered(entry.jsString);
        if (m_doubleCache) {
            for (auto& entry : m_doubleCache->m_doubleCache)
                visitor.appendUnbarriered(entry.jsString);
        }
        // 0-9 are managed by SmallStrings. They never die.
        for (unsigned i = 10; i < m_smallIntCache.size(); ++i)
            visitor.appendUnbarriered(m_smallIntCache[i].jsString);
    }

    const StringWithJSString* smallIntCache() { return m_smallIntCache.data(); }

    void initializeSmallIntCache(VM&);

private:
    CacheEntryWithJSString<double>& lookup(double d)
    {
        ASSERT(m_doubleCache);
        uint64_t bits = std::bit_cast<uint64_t>(d);
        uint32_t hash = static_cast<uint32_t>(bits) ^ static_cast<uint32_t>(bits >> 32);
        return m_doubleCache->m_doubleCache[hash & (doubleCacheSize - 1)];
    }
    CacheEntryWithJSString<int>& lookup(int i) { return m_intCache[WTF::IntHash<int>::hash(i) & (cacheSize - 1)]; }
    CacheEntry<unsigned>& lookup(unsigned i) { return m_unsignedCache[WTF::IntHash<unsigned>::hash(i) & (cacheSize - 1)]; }
    ALWAYS_INLINE StringWithJSString& lookupSmallString(unsigned i)
    {
        ASSERT(i < cacheSize);
        if (m_smallIntCache[i].value.isNull())
            m_smallIntCache[i].value = String::number(i);
        return m_smallIntCache[i];
    }

    void initializeDoubleCache();

    std::array<StringWithJSString, cacheSize> m_smallIntCache { };
    std::array<CacheEntryWithJSString<int>, cacheSize> m_intCache { };
    std::array<CacheEntry<unsigned>, cacheSize> m_unsignedCache { };
    std::unique_ptr<DoubleCache> m_doubleCache;
};

} // namespace JSC
