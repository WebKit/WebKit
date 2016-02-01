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

void FetchHeaders::initializeWith(const FetchHeaders* headers, ExceptionCode&)
{
    if (!headers)
        return;
    m_headers = headers->m_headers;
}

// FIXME: Optimize these routines for HTTPHeaderMap keys and/or refactor them with XMLHttpRequest code.
static bool isForbiddenHeaderName(const String& name)
{
    HTTPHeaderName headerName;
    if (findHTTPHeaderName(name, headerName)) {
        switch (headerName) {
        case HTTPHeaderName::AcceptCharset:
        case HTTPHeaderName::AcceptEncoding:
        case HTTPHeaderName::AccessControlRequestHeaders:
        case HTTPHeaderName::AccessControlRequestMethod:
        case HTTPHeaderName::Connection:
        case HTTPHeaderName::ContentLength:
        case HTTPHeaderName::Cookie:
        case HTTPHeaderName::Cookie2:
        case HTTPHeaderName::Date:
        case HTTPHeaderName::DNT:
        case HTTPHeaderName::Expect:
        case HTTPHeaderName::Host:
        case HTTPHeaderName::KeepAlive:
        case HTTPHeaderName::Origin:
        case HTTPHeaderName::Referer:
        case HTTPHeaderName::TE:
        case HTTPHeaderName::Trailer:
        case HTTPHeaderName::TransferEncoding:
        case HTTPHeaderName::Upgrade:
        case HTTPHeaderName::Via:
            return true;
        default:
            break;
        }
    }
    return name.startsWithIgnoringASCIICase(ASCIILiteral("Sec-")) || name.startsWithIgnoringASCIICase(ASCIILiteral("Proxy-"));
}

static bool isForbiddenResponseHeaderName(const String& name)
{
    return equalLettersIgnoringASCIICase(name, "set-cookie") || equalLettersIgnoringASCIICase(name, "set-cookie2");
}

static bool isSimpleHeader(const String& name, const String& value)
{
    HTTPHeaderName headerName;
    if (!findHTTPHeaderName(name, headerName))
        return false;
    switch (headerName) {
    case HTTPHeaderName::Accept:
    case HTTPHeaderName::AcceptLanguage:
    case HTTPHeaderName::ContentLanguage:
        return true;
    case HTTPHeaderName::ContentType: {
        String mimeType = extractMIMETypeFromMediaType(value);
        return equalLettersIgnoringASCIICase(mimeType, "application/x-www-form-urlencoded") || equalLettersIgnoringASCIICase(mimeType, "multipart/form-data") || equalLettersIgnoringASCIICase(mimeType, "text/plain");
    }
    default:
        return false;
    }
}

static bool canWriteHeader(const String& name, const String& value, FetchHeaders::Guard guard, ExceptionCode& ec)
{
    if (!isValidHTTPToken(name) || !isValidHTTPHeaderValue(value)) {
        ec = TypeError;
        return false;
    }
    if (guard == FetchHeaders::Guard::Immutable) {
        ec = TypeError;
        return false;
    }
    if (guard == FetchHeaders::Guard::Request && isForbiddenHeaderName(name))
        return false;
    if (guard == FetchHeaders::Guard::RequestNoCors && !isSimpleHeader(name, value))
        return false;
    if (guard == FetchHeaders::Guard::Response && isForbiddenResponseHeaderName(name))
        return false;
    return true;
}

void FetchHeaders::append(const String& name, const String& value, ExceptionCode& ec)
{
    String normalizedValue = stripLeadingAndTrailingHTTPSpaces(value);
    if (!canWriteHeader(name, normalizedValue, m_guard, ec))
        return;
    m_headers.add(name, normalizedValue);
}

void FetchHeaders::remove(const String& name, ExceptionCode& ec)
{
    if (!canWriteHeader(name, String(), m_guard, ec))
        return;
    m_headers.remove(name);
}

String FetchHeaders::get(const String& name, ExceptionCode& ec) const
{
    if (!isValidHTTPToken(name)) {
        ec = TypeError;
        return String();
    }
    return m_headers.get(name);
}

bool FetchHeaders::has(const String& name, ExceptionCode& ec) const
{
    if (!isValidHTTPToken(name)) {
        ec = TypeError;
        return false;
    }
    return m_headers.contains(name);
}

void FetchHeaders::set(const String& name, const String& value, ExceptionCode& ec)
{
    String normalizedValue = stripLeadingAndTrailingHTTPSpaces(value);
    if (!canWriteHeader(name, normalizedValue, m_guard, ec))
        return;
    m_headers.set(name, normalizedValue);
}

void FetchHeaders::fill(const FetchHeaders* headers)
{
    if (!headers)
        return;

    ASSERT(m_guard != Guard::Immutable);

    ExceptionCode ec;
    for (auto& header : headers->m_headers) {
        if (canWriteHeader(header.key, header.value, m_guard, ec)) {
            if (header.keyAsHTTPHeaderName)
                m_headers.add(header.keyAsHTTPHeaderName.value(), header.value);
            else
                m_headers.add(header.key, header.value);
        }
    }
}

} // namespace WebCore

#endif // ENABLE(FETCH_API)
