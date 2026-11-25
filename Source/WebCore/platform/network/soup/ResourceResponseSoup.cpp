/*
 * Copyright (C) 2009 Gustavo Noronha Silva
 * Copyright (C) 2009 Collabora Ltd.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "config.h"

#if USE(SOUP)

#include "ResourceResponse.h"

#include "DNS.h"
#include "GUniquePtrSoup.h"
#include "HTTPHeaderNames.h"
#include "HTTPParsers.h"
#include "IPAddressSpace.h"
#include "MIMETypeRegistry.h"
#include "URLSoup.h"
#include <unicode/uset.h>
#include <wtf/text/CString.h>
#include <wtf/text/UTF8CStringView.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

static IPAddressSpace resolvedIPAddressSpace(SoupMessage* soupMessage)
{
    auto* address = soup_message_get_remote_address(soupMessage);
    if (!G_IS_INET_SOCKET_ADDRESS(address))
        return IPAddressSpace::Unknown;

    GUniquePtr<char> ipAddress(g_inet_address_to_string(g_inet_socket_address_get_address(G_INET_SOCKET_ADDRESS(address))));
    auto resolvedIPAddress = IPAddress::fromString(StringImpl::fromUTF8(ipAddress.get()));
    if (!resolvedIPAddress)
        return IPAddressSpace::Unknown;

    return classifyIPAddressSpace(*resolvedIPAddress);
}

ResourceResponse::ResourceResponse(SoupMessage* soupMessage, const UTF8CString& sniffedContentType)
{
    m_url = soupURIToURL(soup_message_get_uri(soupMessage));

    switch (soup_message_get_http_version(soupMessage)) {
    case SOUP_HTTP_1_0:
        m_httpVersion = "HTTP/1.0"_s;
        break;
    case SOUP_HTTP_1_1:
        m_httpVersion = "HTTP/1.1"_s;
        break;
    case SOUP_HTTP_2_0:
        m_httpVersion = "HTTP/2"_s;
        break;
    }

    m_httpStatusCode = soup_message_get_status(soupMessage);
    setHTTPStatusText(StringImpl::fromLatin1(soup_message_get_reason_phrase(soupMessage)));

    m_certificate = soup_message_get_tls_peer_certificate(soupMessage);
    m_tlsErrors = soup_message_get_tls_peer_certificate_errors(soupMessage);

    auto* responseHeaders = soup_message_get_response_headers(soupMessage);
    updateFromSoupMessageHeaders(responseHeaders);

    RefPtr<StringImpl> contentType;
    const char* officialType = soup_message_headers_get_one(responseHeaders, "Content-Type");
    if (!sniffedContentType.isNull() && m_httpStatusCode != SOUP_STATUS_NOT_MODIFIED && sniffedContentType != UTF8CStringView::unsafeFromUTF8(officialType))
        contentType = StringImpl::fromUTF8(sniffedContentType.span());
    else
        contentType = StringImpl::fromLatin1(officialType);
    setMimeType(extractMIMETypeFromMediaType(contentType));
    if (isEmpty(m_mimeType) && m_httpStatusCode != SOUP_STATUS_NOT_MODIFIED)
        setMimeType(MIMETypeRegistry::mimeTypeForPath(m_url.path()));
    setTextEncodingName(extractCharsetFromMediaType(contentType).toString());

    setExpectedContentLength(soup_message_headers_get_content_length(responseHeaders));

    setIPAddressSpace(resolvedIPAddressSpace(soupMessage));
}

void ResourceResponse::updateSoupMessageHeaders(SoupMessageHeaders* soupHeaders) const
{
    for (const auto& header : httpHeaderFields())
        soup_message_headers_append(soupHeaders, utf8(header.key).legacyCStringPointer(), utf8(header.value).legacyCStringPointer());
}

void ResourceResponse::updateFromSoupMessageHeaders(SoupMessageHeaders* soupHeaders)
{
    SoupMessageHeadersIter headersIter;
    const char* headerName;
    const char* headerValue;
    soup_message_headers_iter_init(&headersIter, soupHeaders);
    while (soup_message_headers_iter_next(&headersIter, &headerName, &headerValue))
        addHTTPHeaderField(StringImpl::fromLatin1(headerName), StringImpl::fromLatin1(headerValue));
}

CertificateInfo ResourceResponse::platformCertificateInfo(std::span<const std::byte>) const
{
    return CertificateInfo(m_certificate.get(), m_tlsErrors);
}

static RefPtr<StringImpl> sanitizeFilename(StringImpl* filename)
{
    if (isEmpty(filename))
        return filename;

    // Trim leading/trailing whitespaces, path separators and dots
    auto result = trim(filename, [](char16_t character) -> bool {
        return deprecatedIsSpaceOrNewline(character) || character == '/' || character == '\\' || character == '.';
    });

    if (isEmpty(result))
        return result;

    // Replace control, formatting and dangerous characters in filenames.
    static USet* illegalCharacterSet = nullptr;
    if (!illegalCharacterSet) {
        UErrorCode errorCode = U_ZERO_ERROR;
        RefPtr<StringImpl> illegalCharacters = "[[\"~*/:<>?\\\\|][:Cc:][:Cf:]]"_si;
        illegalCharacterSet = uset_openPattern(StringView(illegalCharacters).upconvertedCharacters(), length(illegalCharacters), &errorCode);
        ASSERT(U_SUCCESS(errorCode));
    }

    HashSet<uint16_t> illegalCharactersInFilename;
    for (unsigned i = 0; i < length(result); ++i) {
        auto character = codeUnitAt(result, i);
        if (uset_contains(illegalCharacterSet, character))
            illegalCharactersInFilename.add(character);
    }
    for (auto character : illegalCharactersInFilename)
        result = makeStringByReplacingAll(result, character, '_');

    return result;
}

RefPtr<StringImpl> ResourceResponse::platformSuggestedFilename() const
{
    RefPtr<StringImpl> contentDisposition(httpHeaderField(HTTPHeaderName::ContentDisposition));
    if (isEmpty(contentDisposition))
        return { };

    if (is8Bit(contentDisposition))
        contentDisposition = StringImpl::fromUTF8WithLatin1Fallback(span8(contentDisposition));
    GUniquePtr<SoupMessageHeaders> soupHeaders(soup_message_headers_new(SOUP_MESSAGE_HEADERS_RESPONSE));
    soup_message_headers_append(soupHeaders.get(), "Content-Disposition", utf8(contentDisposition).legacyCStringPointer());
    GRefPtr<GHashTable> params;
    soup_message_headers_get_content_disposition(soupHeaders.get(), nullptr, &params.outPtr());
    auto filename = params ? StringImpl::fromUTF8(static_cast<char*>(g_hash_table_lookup(params.get(), "filename"))) : nullptr;
    return sanitizeFilename(filename);
}

}

#endif
