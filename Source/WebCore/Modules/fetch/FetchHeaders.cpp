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

#include "HTTPParsers.h"

namespace WebCore {

// https://fetch.spec.whatwg.org/#concept-headers-remove-privileged-no-cors-request-headers
static void removePrivilegedNoCORSRequestHeaders(HTTPHeaderMap& headers)
{
    headers.remove(HTTPHeaderName::Range);
}

static ExceptionOr<bool> canWriteHeader(const String& name, const String& value, const String& combinedValue, FetchHeaders::Guard guard)
{
    if (!isValidHTTPToken(name))
        return Exception { ExceptionCode::TypeError, makeString("Invalid header name: '", name, "'") };
    ASSERT(value.isEmpty() || (!isASCIIWhitespaceWithoutFF(value[0]) && !isASCIIWhitespaceWithoutFF(value[value.length() - 1])));
    if (!isValidHTTPHeaderValue((value)))
        return Exception { ExceptionCode::TypeError, makeString("Header '", name, "' has invalid value: '", value, "'") };
    if (guard == FetchHeaders::Guard::Immutable)
        return Exception { ExceptionCode::TypeError, "Headers object's guard is 'immutable'"_s };
    if (guard == FetchHeaders::Guard::Request && isForbiddenHeader(name, value))
        return false;
    if (guard == FetchHeaders::Guard::RequestNoCors && !isSimpleHeader(name, combinedValue))
        return false;
    if (guard == FetchHeaders::Guard::Response && isForbiddenResponseHeaderName(name))
        return false;
    return true;
}

static ExceptionOr<void> appendSetCookie(const String& value, Vector<String>& setCookieValues, FetchHeaders::Guard guard)
{
    if (!isValidHTTPHeaderValue((value)))
        return Exception { ExceptionCode::TypeError, makeString("Header 'Set-Cookie' has invalid value: '", value, "'") };
    if (guard == FetchHeaders::Guard::Immutable)
        return Exception { ExceptionCode::TypeError, "Headers object's guard is 'immutable'"_s };

    if (guard == FetchHeaders::Guard::None)
        setCookieValues.append(value);

    return { };
}

static ExceptionOr<void> appendToHeaderMap(const String& name, const String& value, HTTPHeaderMap& headers, Vector<String>& setCookieValues, FetchHeaders::Guard guard)
{
    String normalizedValue = value.trim(isASCIIWhitespaceWithoutFF<UChar>);
    if (equalIgnoringASCIICase(name, "set-cookie"_s))
        return appendSetCookie(normalizedValue, setCookieValues, guard);

    String combinedValue = normalizedValue;
    if (headers.contains(name))
        combinedValue = makeString(headers.get(name), ", ", normalizedValue);
    auto canWriteResult = canWriteHeader(name, normalizedValue, combinedValue, guard);
    if (canWriteResult.hasException())
        return canWriteResult.releaseException();
    if (!canWriteResult.releaseReturnValue())
        return { };
    headers.set(name, combinedValue);

    if (guard == FetchHeaders::Guard::RequestNoCors)
        removePrivilegedNoCORSRequestHeaders(headers);

    return { };
}

static ExceptionOr<void> appendToHeaderMap(const HTTPHeaderMap::HTTPHeaderMapConstIterator::KeyValue& header, HTTPHeaderMap& headers, FetchHeaders::Guard guard)
{
    ASSERT(!equalIgnoringASCIICase(header.key, "set-cookie"_s));
    String normalizedValue = header.value.trim(isASCIIWhitespaceWithoutFF<UChar>);
    auto canWriteResult = canWriteHeader(header.key, normalizedValue, header.value, guard);
    if (canWriteResult.hasException())
        return canWriteResult.releaseException();
    if (!canWriteResult.releaseReturnValue())
        return { };
    if (header.keyAsHTTPHeaderName)
        headers.add(header.keyAsHTTPHeaderName.value(), header.value);
    else
        headers.addUncommonHeader(header.key, header.value);

    if (guard == FetchHeaders::Guard::RequestNoCors)
        removePrivilegedNoCORSRequestHeaders(headers);

    return { };
}

// https://fetch.spec.whatwg.org/#concept-headers-fill
static ExceptionOr<void> fillHeaderMap(HTTPHeaderMap& headers, Vector<String>& setCookieValues, const FetchHeaders::Init& headersInit, FetchHeaders::Guard guard)
{
    if (std::holds_alternative<Vector<Vector<String>>>(headersInit)) {
        auto& sequence = std::get<Vector<Vector<String>>>(headersInit);
        for (auto& header : sequence) {
            if (header.size() != 2)
                return Exception { ExceptionCode::TypeError, "Header sub-sequence must contain exactly two items"_s };
            auto result = appendToHeaderMap(header[0], header[1], headers, setCookieValues, guard);
            if (result.hasException())
                return result.releaseException();
        }
    } else {
        auto& record = std::get<Vector<KeyValuePair<String, String>>>(headersInit);
        for (auto& header : record) {
            auto result = appendToHeaderMap(header.key, header.value, headers, setCookieValues, guard);
            if (result.hasException())
                return result.releaseException();
        }
    }

    return { };
}

ExceptionOr<Ref<FetchHeaders>> FetchHeaders::create(std::optional<Init>&& headersInit)
{
    HTTPHeaderMap headers;
    Vector<String> setCookieValues;

    if (headersInit) {
        auto result = fillHeaderMap(headers, setCookieValues, *headersInit, Guard::None);
        if (result.hasException())
            return result.releaseException();
    }

    return adoptRef(*new FetchHeaders { Guard::None, WTFMove(headers), WTFMove(setCookieValues) });
}

ExceptionOr<void> FetchHeaders::fill(const Init& headerInit)
{
    return fillHeaderMap(m_headers, m_setCookieValues, headerInit, m_guard);
}

ExceptionOr<void> FetchHeaders::fill(const FetchHeaders& otherHeaders)
{
    for (auto& header : otherHeaders.m_headers) {
        auto result = appendToHeaderMap(header, m_headers, m_guard);
        if (result.hasException())
            return result.releaseException();
    }
    for (auto& setCookieValue : otherHeaders.m_setCookieValues) {
        auto result = appendSetCookie(setCookieValue, m_setCookieValues, m_guard);
        if (result.hasException())
            return result.releaseException();
    }

    return { };
}

ExceptionOr<void> FetchHeaders::append(const String& name, const String& value)
{
    ++m_updateCounter;
    return appendToHeaderMap(name, value, m_headers, m_setCookieValues, m_guard);
}

// https://fetch.spec.whatwg.org/#dom-headers-delete
ExceptionOr<void> FetchHeaders::remove(const String& name)
{
    if (!isValidHTTPToken(name))
        return Exception { ExceptionCode::TypeError, makeString("Invalid header name: '", name, "'") };
    if (m_guard == FetchHeaders::Guard::Immutable)
        return Exception { ExceptionCode::TypeError, "Headers object's guard is 'immutable'"_s };
    if (m_guard == FetchHeaders::Guard::Request && isForbiddenHeaderName(name))
        return { };
    if (m_guard == FetchHeaders::Guard::RequestNoCors && !isNoCORSSafelistedRequestHeaderName(name) && !isPriviledgedNoCORSRequestHeaderName(name))
        return { };
    if (m_guard == FetchHeaders::Guard::Response && isForbiddenResponseHeaderName(name))
        return { };

    ++m_updateCounter;
    if (equalIgnoringASCIICase(name, "set-cookie"_s))
        m_setCookieValues.clear();
    else
        m_headers.remove(name);

    if (m_guard == FetchHeaders::Guard::RequestNoCors)
        removePrivilegedNoCORSRequestHeaders(m_headers);

    return { };
}

ExceptionOr<String> FetchHeaders::get(const String& name) const
{
    if (!isValidHTTPToken(name))
        return Exception { ExceptionCode::TypeError, makeString("Invalid header name: '", name, "'") };

    if (equalIgnoringASCIICase(name, "set-cookie"_s)) {
        if (m_setCookieValues.isEmpty())
            return String();
        StringBuilder builder;
        for (const auto& value : m_setCookieValues) {
            if (!builder.isEmpty())
                builder.append(", ");
            builder.append(value);
        }
        return builder.toString();
    }
    return m_headers.get(name);
}

const Vector<String>& FetchHeaders::getSetCookie() const
{
    return m_setCookieValues;
}

ExceptionOr<bool> FetchHeaders::has(const String& name) const
{
    if (!isValidHTTPToken(name))
        return Exception { ExceptionCode::TypeError, makeString("Invalid header name: '", name, "'") };

    if (equalIgnoringASCIICase(name, "set-cookie"_s))
        return !m_setCookieValues.isEmpty();
    return m_headers.contains(name);
}

ExceptionOr<void> FetchHeaders::set(const String& name, const String& value)
{
    String normalizedValue = value.trim(isASCIIWhitespaceWithoutFF<UChar>);
    auto canWriteResult = canWriteHeader(name, normalizedValue, normalizedValue, m_guard);
    if (canWriteResult.hasException())
        return canWriteResult.releaseException();
    if (!canWriteResult.releaseReturnValue())
        return { };

    ++m_updateCounter;
    if (equalIgnoringASCIICase(name, "set-cookie"_s)) {
        m_setCookieValues.clear();
        m_setCookieValues.append(normalizedValue);
    } else
        m_headers.set(name, normalizedValue);

    if (m_guard == FetchHeaders::Guard::RequestNoCors)
        removePrivilegedNoCORSRequestHeaders(m_headers);

    return { };
}

void FetchHeaders::filterAndFill(const HTTPHeaderMap& headers, Guard guard)
{
    for (auto& header : headers) {
        String normalizedValue = header.value.trim(isASCIIWhitespaceWithoutFF<UChar>);
        auto canWriteResult = canWriteHeader(header.key, normalizedValue, header.value, guard);
        if (canWriteResult.hasException())
            continue;
        if (!canWriteResult.releaseReturnValue())
            continue;
        if (header.keyAsHTTPHeaderName)
            m_headers.add(header.keyAsHTTPHeaderName.value(), header.value);
        else
            m_headers.addUncommonHeader(header.key, header.value);
    }
}

static bool compareIteratorKeys(const String& a, const String& b)
{
    // null in the iterator's m_keys represents Set-Cookie.
    return WTF::codePointCompareLessThan(
        a.isNull() ? "set-cookie"_s : a,
        b.isNull() ? "set-cookie"_s : b);
}

std::optional<KeyValuePair<String, String>> FetchHeaders::Iterator::next()
{
    if (m_keys.isEmpty() || m_updateCounter != m_headers->m_updateCounter) {
        bool hasSetCookie = !m_headers->m_setCookieValues.isEmpty();
        m_keys.shrink(0);
        m_keys.reserveCapacity(m_headers->m_headers.size() + (hasSetCookie ? 1 : 0));
        m_keys.appendContainerWithMapping(m_headers->m_headers, [](auto& header) {
            ASSERT(!header.key.isNull());
            return header.key.convertToASCIILowercase();
        });
        if (hasSetCookie)
            m_keys.append(String());
        std::sort(m_keys.begin(), m_keys.end(), compareIteratorKeys);

        // We adjust the current index to work with Set-Cookie headers.
        // This relies on the fact that `m_currentIndex + m_setCookieIndex`
        // gives you the current total index into the iteration.
        m_currentIndex += m_setCookieIndex;
        if (hasSetCookie) {
            size_t setCookieKeyIndex = std::lower_bound(m_keys.begin(), m_keys.end(), String(), compareIteratorKeys) - m_keys.begin();
            if (m_currentIndex < setCookieKeyIndex)
                m_setCookieIndex = 0;
            else {
                m_setCookieIndex = std::min(m_currentIndex - setCookieKeyIndex, m_headers->m_setCookieValues.size());
                m_currentIndex -= m_setCookieIndex;
            }
        } else
            m_setCookieIndex = 0;

        m_updateCounter = m_headers->m_updateCounter;
    }
    while (m_currentIndex < m_keys.size()) {
        auto key = m_keys[m_currentIndex];
        if (key.isNull()) {
            if (m_setCookieIndex < m_headers->m_setCookieValues.size()) {
                String value = m_headers->m_setCookieValues[m_setCookieIndex++];
                ASSERT(!value.isNull());
                return KeyValuePair<String, String> { "set-cookie"_s, WTFMove(value) };
            }
            m_currentIndex++;
            continue;
        }
        m_currentIndex++;
        String value = m_headers->m_headers.get(key);
        if (!value.isNull())
            return KeyValuePair<String, String> { WTFMove(key), WTFMove(value) };
    }
    return std::nullopt;
}

FetchHeaders::Iterator::Iterator(FetchHeaders& headers)
    : m_headers(headers)
{
}

} // namespace WebCore
