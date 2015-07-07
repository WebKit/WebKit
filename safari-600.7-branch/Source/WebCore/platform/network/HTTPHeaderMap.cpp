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

#include "HTTPHeaderNames.h"
#include <utility>
#include <wtf/text/StringView.h>

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
        data->uncheckedAppend(std::make_pair(header.key.isolatedCopy(), header.value.isolatedCopy()));

    return data;
}

void HTTPHeaderMap::adopt(std::unique_ptr<CrossThreadHTTPHeaderMapData> data)
{
    m_headers.clear();

    for (auto& header : *data)
        m_headers.add(WTF::move(header.first), WTF::move(header.second));
}

static String internHTTPHeaderNameString(const String& nameString)
{
    HTTPHeaderName headerName;
    if (!findHTTPHeaderName(nameString, headerName))
        return nameString;

    return httpHeaderNameString(headerName).toStringWithoutCopying();
}

String HTTPHeaderMap::get(const String& name) const
{
    return m_headers.get(internHTTPHeaderNameString(name));
}

void HTTPHeaderMap::set(const String& name, const String& value)
{
    m_headers.set(internHTTPHeaderNameString(name), value);
}

void HTTPHeaderMap::add(const String& name, const String& value)
{
    auto result = m_headers.add(internHTTPHeaderNameString(name), value);
    if (!result.isNewEntry)
        result.iterator->value = result.iterator->value + ", " + value;
}

String HTTPHeaderMap::get(HTTPHeaderName name) const
{
    auto it = find(name);
    if (it == end())
        return String();

    return it->value;
}

void HTTPHeaderMap::set(HTTPHeaderName name, const String& value)
{
    m_headers.set(httpHeaderNameString(name).toStringWithoutCopying(), value);
}

bool HTTPHeaderMap::contains(HTTPHeaderName name) const
{
    return m_headers.contains(httpHeaderNameString(name).toStringWithoutCopying());
}

HTTPHeaderMap::const_iterator HTTPHeaderMap::find(HTTPHeaderName name) const
{
    return m_headers.find(httpHeaderNameString(name).toStringWithoutCopying());
}

bool HTTPHeaderMap::remove(HTTPHeaderName name)
{
    return m_headers.remove(httpHeaderNameString(name).toStringWithoutCopying());
}

} // namespace WebCore
