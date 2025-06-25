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

    template<typename T>
    struct CacheEntry {
        T key { };
        String value;
    };

    template<typename T>
    struct CacheEntryWithJSString {
        T key { };
        String value;
        JSString* jsString { nullptr };
    };

    struct StringWithJSString {
        String value;
        JSString* jsString { nullptr };

        static constexpr ptrdiff_t offsetOfJSString() { return OBJECT_OFFSETOF(StringWithJSString, jsString); }
    };

    ALWAYS_INLINE const String& add(double d)
    {
        uint64_t value = std::bit_cast<uint64_t>(d);
        if (!value)
            return lookupSmallString(0).value;

        auto& entry = lookupDouble(value);
        if (value == entry.key)
            return entry.value;
        entry.key = value;
        entry.value = String::number(d);
        entry.jsString = nullptr;
        return entry.value;
    }

    ALWAYS_INLINE const String& add(int i)
    {
        if (static_cast<unsigned>(i) < m_smallIntCache.size())
            return lookupSmallString(static_cast<unsigned>(i)).value;

        ASSERT(i);
        auto& entry = lookup(i);
        if (i == entry.key)
            return entry.value;
        entry.key = i;
        entry.value = String::number(i);
        entry.jsString = nullptr;
        return entry.value;
    }

    ALWAYS_INLINE const String& add(unsigned i)
    {
        if (i < m_smallIntCache.size())
            return lookupSmallString(static_cast<unsigned>(i)).value;

        ASSERT(i);
        auto& entry = lookup(i);
        if (i == entry.key)
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
        for (auto& entry : m_doubleCache)
            entry.jsString = nullptr;
        // 0-9 are managed by SmallStrings. They never die.
        for (unsigned i = 10; i < m_smallIntCache.size(); ++i)
            m_smallIntCache[i].jsString = nullptr;
    }

    template<typename Visitor>
    void visitAggregate(Visitor& visitor)
    {
        for (auto& entry : m_intCache)
            visitor.appendUnbarriered(entry.jsString);
        for (auto& entry : m_doubleCache)
            visitor.appendUnbarriered(entry.jsString);
        // 0-9 are managed by SmallStrings. They never die.
        for (unsigned i = 10; i < m_smallIntCache.size(); ++i)
            visitor.appendUnbarriered(m_smallIntCache[i].jsString);
    }

    const StringWithJSString* smallIntCache() { return m_smallIntCache.data(); }

    void initializeSmallIntCache(VM&);

private:
    CacheEntryWithJSString<uint64_t>& lookupDouble(uint64_t d) { return m_doubleCache[WTF::IntHash<uint64_t>::hash(d) & (m_doubleCache.size() - 1)]; }
    CacheEntryWithJSString<int>& lookup(int i) { return m_intCache[WTF::IntHash<int>::hash(i) & (m_intCache.size() - 1)]; }
    CacheEntry<unsigned>& lookup(unsigned i) { return m_unsignedCache[WTF::IntHash<unsigned>::hash(i) & (m_unsignedCache.size() - 1)]; }
    ALWAYS_INLINE StringWithJSString& lookupSmallString(unsigned i)
    {
        ASSERT(i < m_smallIntCache.size());
        if (m_smallIntCache[i].value.isNull())
            m_smallIntCache[i].value = String::number(i);
        return m_smallIntCache[i];
    }

    std::array<StringWithJSString, cacheSize> m_smallIntCache { };
    std::array<CacheEntryWithJSString<int>, cacheSize> m_intCache { };
    std::array<CacheEntryWithJSString<uint64_t>, cacheSize> m_doubleCache { };
    std::array<CacheEntry<unsigned>, cacheSize> m_unsignedCache { };
};

} // namespace JSC
