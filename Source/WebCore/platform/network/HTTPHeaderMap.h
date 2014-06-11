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

#include <utility>
#include <wtf/HashMap.h>
#include <wtf/Vector.h>
#include <wtf/text/AtomicString.h>
#include <wtf/text/AtomicStringHash.h>
#include <wtf/text/StringHash.h>

namespace WebCore {

typedef Vector<std::pair<String, String>> CrossThreadHTTPHeaderMapData;

// FIXME: Not every header fits into a map. Notably, multiple Set-Cookie header fields are needed to set multiple cookies.

class HTTPHeaderMap {
    typedef HashMap<AtomicString, String, CaseFoldingHash> HashMapType;
public:
    typedef HashMapType::const_iterator const_iterator;
    typedef HashMapType::iterator iterator;
    typedef HashMapType::AddResult AddResult;

    HTTPHeaderMap();
    ~HTTPHeaderMap();

    // Gets a copy of the data suitable for passing to another thread.
    std::unique_ptr<CrossThreadHTTPHeaderMapData> copyData() const;
    void adopt(std::unique_ptr<CrossThreadHTTPHeaderMapData>);

    bool isEmpty() const { return m_headers.isEmpty(); }
    int size() const { return m_headers.size(); }

    void clear() { m_headers.clear(); }

    String get(const AtomicString& name) const;

    AddResult set(const AtomicString& name, const String& value);
    AddResult add(const AtomicString& name, const String& value);

    // Alternate accessors that are faster than converting the char* to AtomicString first.
    bool contains(const char*) const;
    String get(const char*) const;
    const_iterator find(const char*) const;
    iterator find(const char*);
    AddResult add(const char* name, const String& value);
    bool remove(const char*);

    const_iterator begin() const { return m_headers.begin(); }
    const_iterator end() const { return m_headers.end(); }

    WTF::IteratorRange<const_iterator::Keys> keys() const;

    friend bool operator==(const HTTPHeaderMap& a, const HTTPHeaderMap& b)
    {
        return a.m_headers == b.m_headers;
    }

    friend bool operator!=(const HTTPHeaderMap& a, const HTTPHeaderMap& b)
    {
        return !(a == b);
    }

private:
    HashMap<AtomicString, String, CaseFoldingHash> m_headers;
};

} // namespace WebCore

#endif // HTTPHeaderMap_h
