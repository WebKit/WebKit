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

#include "GUniquePtrSoup.h"
#include "HTTPHeaderNames.h"
#include "HTTPParsers.h"
#include "MIMETypeRegistry.h"
#include "SoupVersioning.h"
#include "URLSoup.h"
#include <unicode/uset.h>
#include <wtf/text/CString.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

ResourceResponse::ResourceResponse(SoupMessage* soupMessage, const CString& sniffedContentType)
{
    m_url = soupURIToURL(soup_message_get_uri(soupMessage));

    switch (soup_message_get_http_version(soupMessage)) {
    case SOUP_HTTP_1_0:
        m_httpVersion = "HTTP/1.0"_s;
        break;
    case SOUP_HTTP_1_1:
        m_httpVersion = "HTTP/1.1"_s;
        break;
#if SOUP_CHECK_VERSION(2, 99, 3)
    case SOUP_HTTP_2_0:
        m_httpVersion = "HTTP/2"_s;
        break;
#endif
    }

    m_httpStatusCode = soup_message_get_status(soupMessage);
    setHTTPStatusText(String::fromLatin1(soup_message_get_reason_phrase(soupMessage)));

    m_certificate = soup_message_get_tls_peer_certificate(soupMessage);
    m_tlsErrors = soup_message_get_tls_peer_certificate_errors(soupMessage);

    auto* responseHeaders = soup_message_get_response_headers(soupMessage);
    updateFromSoupMessageHeaders(responseHeaders);

    String contentType;
    const char* officialType = soup_message_headers_get_one(responseHeaders, "Content-Type");
    if (!sniffedContentType.isNull() && m_httpStatusCode != SOUP_STATUS_NOT_MODIFIED && sniffedContentType != officialType)
        contentType = String::fromLatin1(sniffedContentType.data());
    else
        contentType = String::fromLatin1(officialType);
    setMimeType(extractMIMETypeFromMediaType(contentType));
    if (m_mimeType.isEmpty() && m_httpStatusCode != SOUP_STATUS_NOT_MODIFIED)
        setMimeType(MIMETypeRegistry::mimeTypeForPath(m_url.path()));
    setTextEncodingName(extractCharsetFromMediaType(contentType).toString());

    setExpectedContentLength(soup_message_headers_get_content_length(responseHeaders));
}

void ResourceResponse::updateSoupMessageHeaders(SoupMessageHeaders* soupHeaders) const
{
    for (const auto& header : httpHeaderFields())
        soup_message_headers_append(soupHeaders, header.key.utf8().data(), header.value.utf8().data());
}

void ResourceResponse::updateFromSoupMessageHeaders(SoupMessageHeaders* soupHeaders)
{
    SoupMessageHeadersIter headersIter;
    const char* headerName;
    const char* headerValue;
    soup_message_headers_iter_init(&headersIter, soupHeaders);
    while (soup_message_headers_iter_next(&headersIter, &headerName, &headerValue))
        addHTTPHeaderField(String::fromLatin1(headerName), String::fromLatin1(headerValue));
}

CertificateInfo ResourceResponse::platformCertificateInfo() const
{
    return CertificateInfo(m_certificate.get(), m_tlsErrors);
}

static String sanitizeFilename(const String& filename)
{
    if (filename.isEmpty())
        return filename;

    // Strip leading/trailing whitespaces, path separators and dots
    auto result = filename.stripLeadingAndTrailingCharacters([](UChar character) -> bool {
        return isSpaceOrNewline(character) || character == '/' || character == '\\' || character == '.';
    });

    if (result.isEmpty())
        return result;

    // Replace control, formatting and dangerous characters in filenames.
    static USet* illegalCharacterSet = nullptr;
    if (!illegalCharacterSet) {
        UErrorCode errorCode = U_ZERO_ERROR;
        String illegalCharacters = "[[\"~*/:<>?\\\\|][:Cc:][:Cf:]]"_s;
        illegalCharacterSet = uset_openPattern(StringView(illegalCharacters).upconvertedCharacters(), illegalCharacters.length(), &errorCode);
        ASSERT(U_SUCCESS(errorCode));
    }

    HashSet<UChar32> illegalCharactersInFilename;
    for (unsigned i = 0; i < result.length(); ++i) {
        auto character = result[i];
        if (uset_contains(illegalCharacterSet, character))
            illegalCharactersInFilename.add(character);
    }
    for (auto character : illegalCharactersInFilename)
        result = makeStringByReplacingAll(result, character, '_');

    return result;
}

String ResourceResponse::platformSuggestedFilename() const
{
    String contentDisposition(httpHeaderField(HTTPHeaderName::ContentDisposition));
    if (contentDisposition.isEmpty())
        return { };

    if (contentDisposition.is8Bit())
        contentDisposition = String::fromUTF8WithLatin1Fallback(contentDisposition.characters8(), contentDisposition.length());
    GUniquePtr<SoupMessageHeaders> soupHeaders(soup_message_headers_new(SOUP_MESSAGE_HEADERS_RESPONSE));
    soup_message_headers_append(soupHeaders.get(), "Content-Disposition", contentDisposition.utf8().data());
    GRefPtr<GHashTable> params;
    soup_message_headers_get_content_disposition(soupHeaders.get(), nullptr, &params.outPtr());
    auto filename = params ? String::fromUTF8(static_cast<char*>(g_hash_table_lookup(params.get(), "filename"))) : String();
    return sanitizeFilename(filename);
}

}

#endif
