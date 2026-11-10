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

#include "ExceptionOr.h"
#include "HTTPParsers.h"
#include <ranges>
#include <wtf/text/MakeString.h>

namespace WebCore {

// https://fetch.spec.whatwg.org/#concept-headers-remove-privileged-no-cors-request-headers
static void removePrivilegedNoCORSRequestHeaders(HTTPHeaderMap& headers)
{
    headers.remove(HTTPHeaderName::Range);
}

static ExceptionOr<bool> canWriteHeader(StringImpl* name, StringImpl* value, StringImpl* combinedValue, FetchHeaders::Guard guard)
{
    if (!isValidHTTPToken(name))
        return Exception { ExceptionCode::TypeError, makeString("Invalid header name: '"_s, name, '\'') };
    ASSERT(isEmpty(value) || (!isASCIIWhitespaceWithoutFF(codeUnitAt(value, 0)) && !isASCIIWhitespaceWithoutFF(codeUnitAt(value, length(value) - 1))));
    if (!isValidHTTPHeaderValue((value)))
        return Exception { ExceptionCode::TypeError, makeString("Header '"_s, name, "' has invalid value: '"_s, value, '\'') };
    if (guard == FetchHeaders::Guard::Immutable)
        return Exception { ExceptionCode::TypeError, "Headers object's guard is 'immutable'"_si };
    if (guard == FetchHeaders::Guard::Request && isForbiddenHeader(name, value))
        return false;
    if (guard == FetchHeaders::Guard::RequestNoCors && !isSimpleHeader(name, combinedValue))
        return false;
    if (guard == FetchHeaders::Guard::Response && isForbiddenResponseHeaderName(name))
        return false;
    return true;
}

static ExceptionOr<void> appendSetCookie(StringImpl* value, Vector<RefPtr<StringImpl>>& setCookieValues, FetchHeaders::Guard guard)
{
    if (!isValidHTTPHeaderValue((value)))
        return Exception { ExceptionCode::TypeError, makeString("Header 'Set-Cookie' has invalid value: '"_s, value, '\'') };
    if (guard == FetchHeaders::Guard::Immutable)
        return Exception { ExceptionCode::TypeError, "Headers object's guard is 'immutable'"_si };

    if (guard == FetchHeaders::Guard::None)
        setCookieValues.append(value);

    return { };
}

static ExceptionOr<void> appendToHeaderMap(StringImpl* name, StringImpl* value, HTTPHeaderMap& headers, Vector<RefPtr<StringImpl>>& setCookieValues, FetchHeaders::Guard guard)
{
    RefPtr<StringImpl> normalizedValue = trim(value, isASCIIWhitespaceWithoutFF<char16_t>);
    if (equalIgnoringASCIICase(name, "set-cookie"_s))
        return appendSetCookie(normalizedValue, setCookieValues, guard);

    RefPtr<StringImpl> combinedValue = normalizedValue;
    RefPtr<StringImpl> existingValue = headers.get(name);
    ASSERT(existingValue || !headers.contains(name));
    if (existingValue)
        combinedValue = makeString(existingValue, ", "_s, normalizedValue);
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
    RefPtr<StringImpl> normalizedValue = trim(header.value, isASCIIWhitespaceWithoutFF<char16_t>);
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
static ExceptionOr<void> fillHeaderMap(HTTPHeaderMap& headers, Vector<RefPtr<StringImpl>>& setCookieValues, const FetchHeaders::Init& headersInit, FetchHeaders::Guard guard)
{
    if (std::holds_alternative<Vector<Vector<RefPtr<StringImpl>>>>(headersInit)) {
        auto& sequence = std::get<Vector<Vector<RefPtr<StringImpl>>>>(headersInit);
        for (auto& header : sequence) {
            if (header.size() != 2)
                return Exception { ExceptionCode::TypeError, "Header sub-sequence must contain exactly two items"_si };
            auto result = appendToHeaderMap(header[0], header[1], headers, setCookieValues, guard);
            if (result.hasException())
                return result.releaseException();
        }
    } else {
        auto& record = std::get<Vector<KeyValuePair<RefPtr<StringImpl>, RefPtr<StringImpl>>>>(headersInit);
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
    Vector<RefPtr<StringImpl>> setCookieValues;

    if (headersInit) {
        auto result = fillHeaderMap(headers, setCookieValues, *headersInit, Guard::None);
        if (result.hasException())
            return result.releaseException();
    }

    return adoptRef(*new FetchHeaders { Guard::None, WTF::move(headers), WTF::move(setCookieValues) });
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

ExceptionOr<void> FetchHeaders::append(StringImpl* name, StringImpl* value)
{
    ++m_updateCounter;
    return appendToHeaderMap(name, value, m_headers, m_setCookieValues, m_guard);
}

// https://fetch.spec.whatwg.org/#dom-headers-delete
ExceptionOr<void> FetchHeaders::remove(StringImpl* name)
{
    if (!isValidHTTPToken(name))
        return Exception { ExceptionCode::TypeError, makeString("Invalid header name: '"_s, name, '\'') };
    if (m_guard == FetchHeaders::Guard::Immutable)
        return Exception { ExceptionCode::TypeError, "Headers object's guard is 'immutable'"_si };
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

ExceptionOr<RefPtr<StringImpl>> FetchHeaders::get(StringImpl* name) const
{
    if (!isValidHTTPToken(name))
        return Exception { ExceptionCode::TypeError, makeString("Invalid header name: '"_s, name, '\'') };

    if (equalIgnoringASCIICase(name, "set-cookie"_s)) {
        if (m_setCookieValues.isEmpty())
            return RefPtr<StringImpl> { };
        return makeString(interleave(m_setCookieValues, ", "_s));
    }
    return m_headers.get(name);
}

const Vector<RefPtr<StringImpl>>& FetchHeaders::getSetCookie() const
{
    return m_setCookieValues;
}

ExceptionOr<bool> FetchHeaders::has(StringImpl* name) const
{
    if (!isValidHTTPToken(name))
        return Exception { ExceptionCode::TypeError, makeString("Invalid header name: '"_s, name, '\'') };

    if (equalIgnoringASCIICase(name, "set-cookie"_s))
        return !m_setCookieValues.isEmpty();
    return m_headers.contains(name);
}

ExceptionOr<void> FetchHeaders::set(StringImpl* name, StringImpl* value)
{
    RefPtr<StringImpl> normalizedValue = trim(value, isASCIIWhitespaceWithoutFF<char16_t>);
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
        RefPtr<StringImpl> normalizedValue = trim(header.value, isASCIIWhitespaceWithoutFF<char16_t>);
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

static bool compareIteratorKeys(StringImpl* a, StringImpl* b)
{
    // null in the iterator's m_keys represents Set-Cookie.
    return WTF::codePointCompareLessThan(
        !a ? "set-cookie"_s : StringView { a },
        !b ? "set-cookie"_s : StringView { b });
}

std::optional<KeyValuePair<RefPtr<StringImpl>, RefPtr<StringImpl>>> FetchHeaders::Iterator::next()
{
    if (m_keys.isEmpty() || m_updateCounter != m_headers->m_updateCounter) {
        bool hasSetCookie = !m_headers->m_setCookieValues.isEmpty();
        m_keys.shrink(0);
        m_keys.reserveCapacity(m_headers->m_headers.size() + (hasSetCookie ? 1 : 0));
        m_keys.appendContainerWithMapping(m_headers->m_headers, [](auto& header) {
            ASSERT(header.key);
            return convertToASCIILowercase(header.key);
        });
        if (hasSetCookie)
            m_keys.append(nullString());
        std::ranges::sort(m_keys, compareIteratorKeys);

        // We adjust the current index to work with Set-Cookie headers.
        // This relies on the fact that `m_currentIndex + m_setCookieIndex`
        // gives you the current total index into the iteration.
        m_currentIndex += m_setCookieIndex;
        if (hasSetCookie) {
            size_t setCookieKeyIndex = std::ranges::lower_bound(m_keys, nullString(), compareIteratorKeys) - m_keys.begin();
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
        if (!key) {
            if (m_setCookieIndex < m_headers->m_setCookieValues.size()) {
                RefPtr<StringImpl> value = m_headers->m_setCookieValues[m_setCookieIndex++];
                ASSERT(value);
                return KeyValuePair<RefPtr<StringImpl>, RefPtr<StringImpl>> { "set-cookie"_s, WTF::move(value) };
            }
            m_currentIndex++;
            continue;
        }
        m_currentIndex++;
        RefPtr<StringImpl> value = m_headers->m_headers.get(key);
        if (value)
            return KeyValuePair<RefPtr<StringImpl>, RefPtr<StringImpl>> { WTF::move(key), WTF::move(value) };
    }
    return std::nullopt;
}

FetchHeaders::Iterator::Iterator(FetchHeaders& headers)
    : m_headers(headers)
{
}

} // namespace WebCore
