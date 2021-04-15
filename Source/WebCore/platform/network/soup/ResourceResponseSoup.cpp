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

#include "HTTPHeaderNames.h"
#include "HTTPParsers.h"
#include "MIMETypeRegistry.h"
#include "URLSoup.h"
#include <unicode/uset.h>
#include <wtf/text/CString.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

void ResourceResponse::updateSoupMessageHeaders(SoupMessageHeaders* soupHeaders) const
{
    for (const auto& header : httpHeaderFields())
        soup_message_headers_append(soupHeaders, header.key.utf8().data(), header.value.utf8().data());
}

void ResourceResponse::updateFromSoupMessage(SoupMessage* soupMessage)
{
    m_url = soupURIToURL(soup_message_get_uri(soupMessage));

    switch (soup_message_get_http_version(soupMessage)) {
    case SOUP_HTTP_1_0:
        m_httpVersion = AtomString("HTTP/1.0", AtomString::ConstructFromLiteral);
        break;
    case SOUP_HTTP_1_1:
        m_httpVersion = AtomString("HTTP/1.1", AtomString::ConstructFromLiteral);
        break;
    }
    m_httpStatusCode = soupMessage->status_code;
    setHTTPStatusText(soupMessage->reason_phrase);

    m_soupFlags = soup_message_get_flags(soupMessage);

    GTlsCertificate* certificate = 0;
    soup_message_get_https_status(soupMessage, &certificate, &m_tlsErrors);
    m_certificate = certificate;

    updateFromSoupMessageHeaders(soupMessage->response_headers);
}

void ResourceResponse::updateFromSoupMessageHeaders(const SoupMessageHeaders* messageHeaders)
{
    SoupMessageHeaders* headers = const_cast<SoupMessageHeaders*>(messageHeaders);
    SoupMessageHeadersIter headersIter;
    const char* headerName;
    const char* headerValue;

    // updateFromSoupMessage could be called several times for the same ResourceResponse object,
    // thus, we need to clear old header values and update m_httpHeaderFields from soupMessage headers.
    m_httpHeaderFields.clear();

    soup_message_headers_iter_init(&headersIter, headers);
    while (soup_message_headers_iter_next(&headersIter, &headerName, &headerValue))
        addHTTPHeaderField(String(headerName), String(headerValue));

    String contentType;
    const char* officialType = soup_message_headers_get_one(headers, "Content-Type");
    if (!m_sniffedContentType.isEmpty() && m_sniffedContentType != officialType)
        contentType = m_sniffedContentType;
    else
        contentType = officialType;
    setMimeType(extractMIMETypeFromMediaType(contentType));
    setTextEncodingName(extractCharsetFromMediaType(contentType));

    setExpectedContentLength(soup_message_headers_get_content_length(headers));
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
        result = result.replace(character, '_');

    return result;
}

String ResourceResponse::platformSuggestedFilename() const
{
    String contentDisposition(httpHeaderField(HTTPHeaderName::ContentDisposition));
    if (contentDisposition.isEmpty())
        return { };

    if (contentDisposition.is8Bit())
        contentDisposition = String::fromUTF8WithLatin1Fallback(contentDisposition.characters8(), contentDisposition.length());
    SoupMessageHeaders* soupHeaders = soup_message_headers_new(SOUP_MESSAGE_HEADERS_RESPONSE);
    soup_message_headers_append(soupHeaders, "Content-Disposition", contentDisposition.utf8().data());
    GRefPtr<GHashTable> params;
    soup_message_headers_get_content_disposition(soupHeaders, nullptr, &params.outPtr());
    soup_message_headers_free(soupHeaders);
    auto filename = params ? String::fromUTF8(static_cast<char*>(g_hash_table_lookup(params.get(), "filename"))) : String();
    return sanitizeFilename(filename);
}

}

#endif
