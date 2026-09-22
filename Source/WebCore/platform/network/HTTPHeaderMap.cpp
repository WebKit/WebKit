/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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
#include <wtf/CrossThreadCopier.h>
#include <wtf/text/MakeString.h>
#include <wtf/text/StringView.h>

#if USE(CF)
#include <wtf/cf/VectorCF.h>
#endif

namespace WebCore {

static constexpr auto s_repeatedHeaderSeparator = ", "_s;

HTTPHeaderMap::HTTPHeaderMap() = default;

HTTPHeaderMap::HTTPHeaderMap(CommonHeadersVector&& commonHeaders, UncommonHeadersVector&& uncommonHeaders, RepeatedValueOffsetsMap&& repeatedValueOffsets)
    : m_commonHeaders(WTF::move(commonHeaders))
    , m_uncommonHeaders(WTF::move(uncommonHeaders))
    , m_repeatedValueOffsets(WTF::move(repeatedValueOffsets))
{
}

std::optional<HTTPHeaderMap> HTTPHeaderMap::fromIPCData(CommonHeadersVector&& commonHeaders, UncommonHeadersVector&& uncommonHeaders, RepeatedValueOffsetsMap&& repeatedValueOffsets)
{
    HTTPHeaderMap headers(WTF::move(commonHeaders), WTF::move(uncommonHeaders), WTF::move(repeatedValueOffsets));
    for (const auto& [name, offsets] : headers.m_repeatedValueOffsets) {
        auto combined = headers.get(name);
        if (combined.isNull() || offsets.isEmpty())
            return std::nullopt;
        unsigned previousOffset = 0;
        for (auto offset : offsets) {
            if (offset > combined.length() || offset < previousOffset || offset - previousOffset < s_repeatedHeaderSeparator.length())
                return std::nullopt;
            if (StringView(combined).substring(offset - s_repeatedHeaderSeparator.length(), s_repeatedHeaderSeparator.length()) != s_repeatedHeaderSeparator)
                return std::nullopt;
            previousOffset = offset;
        }
    }
    return headers;
}

HTTPHeaderMap HTTPHeaderMap::isolatedCopy() const &
{
    HTTPHeaderMap map;
    map.m_commonHeaders = crossThreadCopy(m_commonHeaders);
    map.m_uncommonHeaders = crossThreadCopy(m_uncommonHeaders);
    map.m_repeatedValueOffsets = crossThreadCopy(m_repeatedValueOffsets);
    return map;
}

HTTPHeaderMap HTTPHeaderMap::isolatedCopy() &&
{
    HTTPHeaderMap map;
    map.m_commonHeaders = crossThreadCopy(WTF::move(m_commonHeaders));
    map.m_uncommonHeaders = crossThreadCopy(WTF::move(m_uncommonHeaders));
    map.m_repeatedValueOffsets = crossThreadCopy(WTF::move(m_repeatedValueOffsets));
    return map;
}

static bool headerKeyEquals(HTTPHeaderName a, HTTPHeaderName b)
{
    return a == b;
}

static bool headerKeyEquals(StringView a, StringView b)
{
    return equalIgnoringASCIICase(a, b);
}

template<typename Headers, typename Key>
static std::pair<size_t /* index */, bool /* repeated */> addHeader(Headers& headers, Key key, String value)
{
    auto index = headers.findIf([&](auto& header) {
        return headerKeyEquals(header.key, key);
    });
    if (index != notFound)
        return { index, true };
    headers.append({ WTF::move(key), WTF::move(value) });
    return { headers.size() - 1, false };
}

void HTTPHeaderMap::addRepeatedHeader(StringView name, String& combined, String value)
{
    auto newCombined = makeString(combined, s_repeatedHeaderSeparator, value);
    auto result = m_repeatedValueOffsets.ensure<ASCIICaseInsensitiveStringViewHashTranslator>(name, [] {
        return RepeatedValueOffsets();
    });
    result.iterator->value.append(combined.length() + s_repeatedHeaderSeparator.length());
    combined = WTF::move(newCombined);
}

String HTTPHeaderMap::RepeatedValueSeparator::operator()(size_t index) const
{
    auto start = index ? m_offsets[index - 1] : 0;
    auto end = index < m_offsets.size() ? m_offsets[index] - s_repeatedHeaderSeparator.length() : m_combined.length();
    return m_combined.substringSharingImpl(start, end - start);
}

HTTPHeaderMap::RepeatedValueSeparator HTTPHeaderMap::separateRepeatedValues(StringView name, const String& combined) const
{
    auto iterator = m_repeatedValueOffsets.find<ASCIICaseInsensitiveStringViewHashTranslator>(name);
    if (iterator == m_repeatedValueOffsets.end())
        return { combined, { } };
    return { combined, iterator->value.span() };
}

String HTTPHeaderMap::get(StringView name) const
{
    HTTPHeaderName headerName;
    if (findHTTPHeaderName(name, headerName))
        return get(headerName);

    return getUncommonHeader(name);
}

String HTTPHeaderMap::getUncommonHeader(StringView name) const
{
    auto index = m_uncommonHeaders.findIf([&](auto& header) {
        return equalIgnoringASCIICase(header.key, name);
    });
    return index != notFound ? m_uncommonHeaders[index].value : String();
}

std::optional<HTTPHeaderMap::RepeatedValueSeparator> HTTPHeaderMap::separateRepeatedValues(StringView name) const
{
    HTTPHeaderName headerName;
    if (findHTTPHeaderName(name, headerName))
        return separateRepeatedValues(headerName);

    auto index = m_uncommonHeaders.findIf([&](auto& header) {
        return equalIgnoringASCIICase(header.key, name);
    });
    if (index == notFound)
        return std::nullopt;
    auto& header = m_uncommonHeaders[index];
    return separateRepeatedValues(header.key, header.value);
}

#if USE(CF)

void HTTPHeaderMap::set(CFStringRef name, const String& value)
{
    // Fast path: avoid constructing a temporary String in the common header case.
    if (auto asciiCharacters = CFStringGetASCIICStringSpan(name); asciiCharacters.data()) {
        HTTPHeaderName headerName;
        if (findHTTPHeaderName(StringView(asciiCharacters), headerName))
            set(headerName, value);
        else
            setUncommonHeader(String::fromLatin1(asciiCharacters), value);

        return;
    }

    set(String(name), value);
}

#endif // USE(CF)

void HTTPHeaderMap::set(const String& name, const String& value)
{
    HTTPHeaderName headerName;
    if (findHTTPHeaderName(name, headerName)) {
        set(headerName, value);
        return;
    }

    setUncommonHeader(name, value);
}

void HTTPHeaderMap::setUncommonHeader(const String& name, const String& value)
{
#if ASSERT_ENABLED
    HTTPHeaderName headerName;
    ASSERT(!findHTTPHeaderName(name, headerName));
#endif

    auto index = m_uncommonHeaders.findIf([&](auto& header) {
        return equalIgnoringASCIICase(header.key, name);
    });
    if (index == notFound)
        m_uncommonHeaders.append(UncommonHeader { name, value });
    else {
        m_uncommonHeaders[index].value = value;
        m_repeatedValueOffsets.remove<ASCIICaseInsensitiveStringViewHashTranslator>(m_uncommonHeaders[index].key);
    }
}

void HTTPHeaderMap::add(const String& name, const String& value)
{
    HTTPHeaderName headerName;
    if (findHTTPHeaderName(name, headerName)) {
        add(headerName, value);
        return;
    }
    addUncommonHeader(name, value);
}

void HTTPHeaderMap::addUncommonHeader(const String& name, const String& value)
{
#if ASSERT_ENABLED
    HTTPHeaderName headerName;
    ASSERT(!findHTTPHeaderName(name, headerName));
#endif

    auto [index, repeated] = addHeader(m_uncommonHeaders, name, value);
    if (repeated)
        addRepeatedHeader(name, m_uncommonHeaders[index].value, value);
}

void HTTPHeaderMap::append(const String& name, const String& value)
{
    ASSERT(!contains(name));

    HTTPHeaderName headerName;
    if (findHTTPHeaderName(name, headerName))
        m_commonHeaders.append(CommonHeader { headerName, value });
    else
        m_uncommonHeaders.append(UncommonHeader { name, value });
}

bool HTTPHeaderMap::addIfNotPresent(HTTPHeaderName headerName, const String& value)
{
    if (contains(headerName))
        return false;

    m_commonHeaders.append(CommonHeader { headerName, value });
    return true;
}

bool HTTPHeaderMap::contains(const String& name) const
{
    HTTPHeaderName headerName;
    if (findHTTPHeaderName(name, headerName))
        return contains(headerName);

    return m_uncommonHeaders.findIf([&](auto& header) {
        return equalIgnoringASCIICase(header.key, name);
    }) != notFound;
}

bool HTTPHeaderMap::remove(const String& name)
{
    HTTPHeaderName headerName;
    if (findHTTPHeaderName(name, headerName))
        return remove(headerName);

    auto index = m_uncommonHeaders.findIf([&](auto& header) {
        return equalIgnoringASCIICase(header.key, name);
    });
    if (index == notFound)
        return false;
    m_repeatedValueOffsets.remove<ASCIICaseInsensitiveStringViewHashTranslator>(m_uncommonHeaders[index].key);
    m_uncommonHeaders.removeAt(index);
    return true;
}

String HTTPHeaderMap::get(HTTPHeaderName name) const
{
    auto index = m_commonHeaders.findIf([&](auto& header) {
        return header.key == name;
    });
    return index != notFound ? m_commonHeaders[index].value : String();
}

std::optional<HTTPHeaderMap::RepeatedValueSeparator> HTTPHeaderMap::separateRepeatedValues(HTTPHeaderName name) const
{
    auto index = m_commonHeaders.findIf([&](auto& header) {
        return header.key == name;
    });
    if (index == notFound)
        return std::nullopt;
    return separateRepeatedValues(httpHeaderNameString(name), m_commonHeaders[index].value);
}

void HTTPHeaderMap::set(HTTPHeaderName name, const String& value)
{
    auto index = m_commonHeaders.findIf([&](auto& header) {
        return header.key == name;
    });
    if (index == notFound)
        m_commonHeaders.append(CommonHeader { name, value });
    else {
        m_commonHeaders[index].value = value;
        m_repeatedValueOffsets.remove<ASCIICaseInsensitiveStringViewHashTranslator>(httpHeaderNameString(name));
    }
}

bool HTTPHeaderMap::contains(HTTPHeaderName name) const
{
    return m_commonHeaders.findIf([&](auto& header) {
        return header.key == name;
    }) != notFound;
}

bool HTTPHeaderMap::remove(HTTPHeaderName name)
{
    auto index = m_commonHeaders.findIf([&](auto& header) {
        return header.key == name;
    });
    if (index == notFound)
        return false;
    m_repeatedValueOffsets.remove<ASCIICaseInsensitiveStringViewHashTranslator>(httpHeaderNameString(name));
    m_commonHeaders.removeAt(index);
    return true;
}

void HTTPHeaderMap::add(HTTPHeaderName name, const String& value)
{
    auto [index, repeated] = addHeader(m_commonHeaders, name, value);
    if (repeated)
        addRepeatedHeader(httpHeaderNameString(name), m_commonHeaders[index].value, value);
}

} // namespace WebCore
