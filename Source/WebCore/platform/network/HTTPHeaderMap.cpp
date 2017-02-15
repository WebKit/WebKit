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
#include <wtf/text/StringView.h>

namespace WebCore {

HTTPHeaderMap::HTTPHeaderMap()
{
}

HTTPHeaderMap HTTPHeaderMap::isolatedCopy() const
{
    HTTPHeaderMap map;

    for (auto& header : m_commonHeaders)
        map.m_commonHeaders.set(header.key, header.value.isolatedCopy());

    for (auto& header : m_uncommonHeaders)
        map.m_uncommonHeaders.set(header.key.isolatedCopy(), header.value.isolatedCopy());

    return map;
}

String HTTPHeaderMap::get(const String& name) const
{
    HTTPHeaderName headerName;
    if (!findHTTPHeaderName(name, headerName))
        return m_uncommonHeaders.get(name);
    return m_commonHeaders.get(headerName);
}

#if USE(CF)

void HTTPHeaderMap::set(CFStringRef name, const String& value)
{
    // Fast path: avoid constructing a temporary String in the common header case.
    if (auto* nameCharacters = CFStringGetCStringPtr(name, kCFStringEncodingASCII)) {
        unsigned length = CFStringGetLength(name);
        HTTPHeaderName headerName;
        if (findHTTPHeaderName(StringView(reinterpret_cast<const LChar*>(nameCharacters), length), headerName))
            m_commonHeaders.set(headerName, value);
        else
            m_uncommonHeaders.set(String(nameCharacters, length), value);
        return;
    }

    set(String(name), value);
}

#endif // USE(CF)

void HTTPHeaderMap::set(const String& name, const String& value)
{
    HTTPHeaderName headerName;
    if (!findHTTPHeaderName(name, headerName)) {
        m_uncommonHeaders.set(name, value);
        return;
    }
    m_commonHeaders.set(headerName, value);
}

void HTTPHeaderMap::add(const String& name, const String& value)
{
    HTTPHeaderName headerName;
    if (!findHTTPHeaderName(name, headerName)) {
        auto result = m_uncommonHeaders.add(name, value);
        if (!result.isNewEntry)
            result.iterator->value = result.iterator->value + ',' + value;
        return;
    }
    add(headerName, value);
}

bool HTTPHeaderMap::addIfNotPresent(HTTPHeaderName headerName, const String& value)
{
    return m_commonHeaders.add(headerName, value).isNewEntry;
}

bool HTTPHeaderMap::contains(const String& name) const
{
    HTTPHeaderName headerName;
    if (findHTTPHeaderName(name, headerName))
        return contains(headerName);
    return m_uncommonHeaders.contains(name);
}

bool HTTPHeaderMap::remove(const String& name)
{
    HTTPHeaderName headerName;
    if (findHTTPHeaderName(name, headerName))
        return remove(headerName);
    return m_uncommonHeaders.remove(name);
}

String HTTPHeaderMap::get(HTTPHeaderName name) const
{
    return m_commonHeaders.get(name);
}

void HTTPHeaderMap::set(HTTPHeaderName name, const String& value)
{
    m_commonHeaders.set(name, value);
}

bool HTTPHeaderMap::contains(HTTPHeaderName name) const
{
    return m_commonHeaders.contains(name);
}

bool HTTPHeaderMap::remove(HTTPHeaderName name)
{
    return m_commonHeaders.remove(name);
}

void HTTPHeaderMap::add(HTTPHeaderName name, const String& value)
{
    auto result = m_commonHeaders.add(name, value);
    if (!result.isNewEntry)
        result.iterator->value = result.iterator->value + ',' + value;
}

} // namespace WebCore
