/*
 * Copyright (C) 2016 Canon Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted, provided that the following conditions
 * are required to be met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Canon Inc. nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY CANON INC. AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL CANON INC. AND ITS CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "FetchHeaders.h"

#if ENABLE(FETCH_API)

#include "ExceptionCode.h"
#include "HTTPParsers.h"

namespace WebCore {

static ExceptionOr<bool> canWriteHeader(const String& name, const String& value, FetchHeaders::Guard guard)
{
    if (!isValidHTTPToken(name) || !isValidHTTPHeaderValue(value))
        return Exception { TypeError };
    if (guard == FetchHeaders::Guard::Immutable)
        return Exception { TypeError };
    if (guard == FetchHeaders::Guard::Request && isForbiddenHeaderName(name))
        return false;
    if (guard == FetchHeaders::Guard::RequestNoCors && !isSimpleHeader(name, value))
        return false;
    if (guard == FetchHeaders::Guard::Response && isForbiddenResponseHeaderName(name))
        return false;
    return true;
}

ExceptionOr<void> FetchHeaders::append(const String& name, const String& value)
{
    String normalizedValue = stripLeadingAndTrailingHTTPSpaces(value);
    auto canWriteResult = canWriteHeader(name, normalizedValue, m_guard);
    if (canWriteResult.hasException())
        return canWriteResult.releaseException();
    if (!canWriteResult.releaseReturnValue())
        return { };
    m_headers.add(name, normalizedValue);
    return { };
}

ExceptionOr<void> FetchHeaders::remove(const String& name)
{
    auto canWriteResult = canWriteHeader(name, { }, m_guard);
    if (canWriteResult.hasException())
        return canWriteResult.releaseException();
    if (!canWriteResult.releaseReturnValue())
        return { };
    m_headers.remove(name);
    return { };
}

ExceptionOr<String> FetchHeaders::get(const String& name) const
{
    if (!isValidHTTPToken(name))
        return Exception { TypeError };
    return m_headers.get(name);
}

ExceptionOr<bool> FetchHeaders::has(const String& name) const
{
    if (!isValidHTTPToken(name))
        return Exception { TypeError };
    return m_headers.contains(name);
}

ExceptionOr<void> FetchHeaders::set(const String& name, const String& value)
{
    String normalizedValue = stripLeadingAndTrailingHTTPSpaces(value);
    auto canWriteResult = canWriteHeader(name, normalizedValue, m_guard);
    if (canWriteResult.hasException())
        return canWriteResult.releaseException();
    if (!canWriteResult.releaseReturnValue())
        return { };
    m_headers.set(name, normalizedValue);
    return { };
}

void FetchHeaders::fill(const FetchHeaders* headers)
{
    ASSERT(m_guard != Guard::Immutable);
    if (!headers)
        return;
    filterAndFill(headers->m_headers, m_guard);
}

void FetchHeaders::filterAndFill(const HTTPHeaderMap& headers, Guard guard)
{
    for (auto& header : headers) {
        auto canWriteResult = canWriteHeader(header.key, header.value, guard);
        if (canWriteResult.hasException())
            continue;
        if (!canWriteResult.releaseReturnValue())
            continue;
        if (header.keyAsHTTPHeaderName)
            m_headers.add(header.keyAsHTTPHeaderName.value(), header.value);
        else
            m_headers.add(header.key, header.value);
    }
}

std::optional<WTF::KeyValuePair<String, String>> FetchHeaders::Iterator::next()
{
    while (m_currentIndex < m_keys.size()) {
        auto key = m_keys[m_currentIndex++];
        auto value = m_headers->m_headers.get(key);
        if (!value.isNull())
            return WTF::KeyValuePair<String, String> { WTFMove(key), WTFMove(value) };
    }
    return std::nullopt;
}

FetchHeaders::Iterator::Iterator(FetchHeaders& headers)
    : m_headers(headers)
{
    m_keys.reserveInitialCapacity(headers.m_headers.size());
    for (auto& header : headers.m_headers)
        m_keys.uncheckedAppend(header.key.convertToASCIILowercase());
    std::sort(m_keys.begin(), m_keys.end(), WTF::codePointCompareLessThan);
}

} // namespace WebCore

#endif // ENABLE(FETCH_API)
