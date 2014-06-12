/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "HTTPHeaderMap.h"

#include <utility>

namespace WebCore {

HTTPHeaderMap::HTTPHeaderMap()
{
}

HTTPHeaderMap::~HTTPHeaderMap()
{
}

std::unique_ptr<CrossThreadHTTPHeaderMapData> HTTPHeaderMap::copyData() const
{
    auto data = std::make_unique<CrossThreadHTTPHeaderMapData>();
    data->reserveInitialCapacity(m_headers.size());

    for (const auto& header : *this)
        data->uncheckedAppend(std::make_pair(header.key.string().isolatedCopy(), header.value.isolatedCopy()));

    return data;
}

void HTTPHeaderMap::adopt(std::unique_ptr<CrossThreadHTTPHeaderMapData> data)
{
    m_headers.clear();

    for (auto& header : *data)
        m_headers.add(std::move(header.first), std::move(header.second));
}

String HTTPHeaderMap::get(const AtomicString& name) const
{
    return m_headers.get(name);
}

void HTTPHeaderMap::set(const AtomicString& name, const String& value)
{
    m_headers.set(name, value);
}

void HTTPHeaderMap::add(const AtomicString& name, const String& value)
{
    auto result = m_headers.add(name, value);
    if (!result.isNewEntry)
        result.iterator->value = result.iterator->value + ", " + value;
}

// Adapter that allows the HashMap to take C strings as keys.
struct CaseFoldingCStringTranslator {
    static unsigned hash(const char* cString)
    {
        return CaseFoldingHash::hash(cString, strlen(cString));
    }
    
    static bool equal(const AtomicString& key, const char* cString)
    {
        return equalIgnoringCase(key, cString);
    }
    
    static void translate(AtomicString& location, const char* cString, unsigned /*hash*/)
    {
        location = AtomicString(cString);
    }
};

String HTTPHeaderMap::get(const char* name) const
{
    auto it = find(name);
    if (it == end())
        return String();
    return it->value;
}
    
bool HTTPHeaderMap::contains(const char* name) const
{
    return find(name) != end();
}

HTTPHeaderMap::const_iterator HTTPHeaderMap::find(const char* name) const
{
    return m_headers.find<CaseFoldingCStringTranslator>(name);
}

bool HTTPHeaderMap::remove(const char* name)
{
    return m_headers.remove(m_headers.find<CaseFoldingCStringTranslator>(name));
}

} // namespace WebCore
