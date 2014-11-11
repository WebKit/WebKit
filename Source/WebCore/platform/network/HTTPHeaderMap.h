/*
 * Copyright (C) 2006 Apple Inc.  All rights reserved.
 * Copyright (C) 2009 Google Inc. All rights reserved.
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

#ifndef HTTPHeaderMap_h
#define HTTPHeaderMap_h

#include "HTTPHeaderNames.h"
#include <utility>
#include <wtf/HashMap.h>
#include <wtf/Optional.h>
#include <wtf/Vector.h>
#include <wtf/text/AtomicString.h>
#include <wtf/text/AtomicStringHash.h>
#include <wtf/text/StringHash.h>
#include <wtf/text/StringView.h>

namespace WebCore {

struct CrossThreadHTTPHeaderMapData {
    Vector<std::pair<HTTPHeaderName, String>> commonHeaders;
    Vector<std::pair<String, String>> uncommonHeaders;
};

// FIXME: Not every header fits into a map. Notably, multiple Set-Cookie header fields are needed to set multiple cookies.

class HTTPHeaderMap {
public:
    typedef HashMap<HTTPHeaderName, String, WTF::IntHash<HTTPHeaderName>, WTF::StrongEnumHashTraits<HTTPHeaderName>> CommonHeadersHashMap;
    typedef HashMap<String, String, CaseFoldingHash> UncommonHeadersHashMap;

    class HTTPHeaderMapConstIterator {
    public:
        HTTPHeaderMapConstIterator(const HTTPHeaderMap& table, CommonHeadersHashMap::const_iterator commonHeadersIt, UncommonHeadersHashMap::const_iterator uncommonHeadersIt)
            : m_table(table)
            , m_commonHeadersIt(commonHeadersIt)
            , m_uncommonHeadersIt(uncommonHeadersIt)
        {
            if (!updateKeyValue(m_commonHeadersIt))
                updateKeyValue(m_uncommonHeadersIt);
        }

        struct KeyValue {
            String key;
            Optional<HTTPHeaderName> keyAsHTTPHeaderName;
            String value;
        };

        const KeyValue* get() const
        {
            ASSERT(*this != m_table.end());
            return &m_keyValue;
        }
        const KeyValue& operator*() const { return *get(); }
        const KeyValue* operator->() const { return get(); }

        HTTPHeaderMapConstIterator& operator++()
        {
            if (m_commonHeadersIt != m_table.m_commonHeaders.end()) {
                if (updateKeyValue(++m_commonHeadersIt))
                    return *this;
            } else
                ++m_uncommonHeadersIt;

            updateKeyValue(m_uncommonHeadersIt);
            return *this;
        }

        bool operator!=(const HTTPHeaderMapConstIterator& other) const { return !(*this == other); }
        bool operator==(const HTTPHeaderMapConstIterator& other) const
        {
            return m_commonHeadersIt == other.m_commonHeadersIt && m_uncommonHeadersIt == other.m_uncommonHeadersIt;
        }

    private:
        bool updateKeyValue(CommonHeadersHashMap::const_iterator it)
        {
            if (it == m_table.commonHeaders().end())
                return false;
            m_keyValue.key = httpHeaderNameString(it->key).toStringWithoutCopying();
            m_keyValue.keyAsHTTPHeaderName = it->key;
            m_keyValue.value = it->value;
            return true;
        }
        bool updateKeyValue(UncommonHeadersHashMap::const_iterator it)
        {
            if (it == m_table.uncommonHeaders().end())
                return false;
            m_keyValue.key = it->key;
            m_keyValue.keyAsHTTPHeaderName = Nullopt;
            m_keyValue.value = it->value;
            return true;
        }

        const HTTPHeaderMap& m_table;
        CommonHeadersHashMap::const_iterator m_commonHeadersIt;
        UncommonHeadersHashMap::const_iterator m_uncommonHeadersIt;
        KeyValue m_keyValue;
    };
    typedef HTTPHeaderMapConstIterator const_iterator;

    WEBCORE_EXPORT HTTPHeaderMap();
    WEBCORE_EXPORT ~HTTPHeaderMap();

    // Gets a copy of the data suitable for passing to another thread.
    std::unique_ptr<CrossThreadHTTPHeaderMapData> copyData() const;
    void adopt(std::unique_ptr<CrossThreadHTTPHeaderMapData>);

    bool isEmpty() const { return m_commonHeaders.isEmpty() && m_uncommonHeaders.isEmpty(); }
    int size() const { return m_commonHeaders.size() + m_uncommonHeaders.size(); }

    void clear()
    {
        m_commonHeaders.clear();
        m_uncommonHeaders.clear();
    }

    WEBCORE_EXPORT String get(const String& name) const;
    WEBCORE_EXPORT void set(const String& name, const String& value);
    void add(const String& name, const String& value);

    WEBCORE_EXPORT String get(HTTPHeaderName) const;
    void set(HTTPHeaderName, const String& value);
    void add(HTTPHeaderName, const String& value);
    bool contains(HTTPHeaderName) const;
    WEBCORE_EXPORT bool remove(HTTPHeaderName);

    // Instead of passing a string literal to any of these functions, just use a HTTPHeaderName instead.
    template<size_t length> String get(const char (&)[length]) const = delete;
    template<size_t length> void set(const char (&)[length], const String&) = delete;
    template<size_t length> bool contains(const char (&)[length]) = delete;
    template<size_t length> bool remove(const char (&)[length]) = delete;

    const CommonHeadersHashMap& commonHeaders() const { return m_commonHeaders; }
    const UncommonHeadersHashMap& uncommonHeaders() const { return m_uncommonHeaders; }
    CommonHeadersHashMap& commonHeaders() { return m_commonHeaders; }
    UncommonHeadersHashMap& uncommonHeaders() { return m_uncommonHeaders; }

    const_iterator begin() const { return const_iterator(*this, m_commonHeaders.begin(), m_uncommonHeaders.begin()); }
    const_iterator end() const { return const_iterator(*this, m_commonHeaders.end(), m_uncommonHeaders.end()); }

    friend bool operator==(const HTTPHeaderMap& a, const HTTPHeaderMap& b)
    {
        return a.m_commonHeaders == b.m_commonHeaders && a.m_uncommonHeaders == b.m_uncommonHeaders;
    }

    friend bool operator!=(const HTTPHeaderMap& a, const HTTPHeaderMap& b)
    {
        return !(a == b);
    }

private:
    CommonHeadersHashMap m_commonHeaders;
    UncommonHeadersHashMap m_uncommonHeaders;
};

} // namespace WebCore

#endif // HTTPHeaderMap_h
