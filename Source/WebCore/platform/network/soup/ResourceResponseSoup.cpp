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
#include "ResourceResponse.h"

#include "GOwnPtr.h"
#include "HTTPParsers.h"
#include "MIMETypeRegistry.h"
#include "PlatformString.h"
#include "SoupURIUtils.h"
#include <wtf/text/CString.h>

using namespace std;

namespace WebCore {

SoupMessage* ResourceResponse::toSoupMessage() const
{
    // This GET here is just because SoupMessage wants it, we dn't really know.
    SoupMessage* soupMessage = soup_message_new("GET", url().string().utf8().data());
    if (!soupMessage)
        return 0;

    soupMessage->status_code = httpStatusCode();

    const HTTPHeaderMap& headers = httpHeaderFields();
    SoupMessageHeaders* soupHeaders = soupMessage->response_headers;
    if (!headers.isEmpty()) {
        HTTPHeaderMap::const_iterator end = headers.end();
        for (HTTPHeaderMap::const_iterator it = headers.begin(); it != end; ++it)
            soup_message_headers_append(soupHeaders, it->first.string().utf8().data(), it->second.utf8().data());
    }

    soup_message_set_flags(soupMessage, m_soupFlags);

    // Body data is not in the message.
    return soupMessage;
}

void ResourceResponse::updateFromSoupMessage(SoupMessage* soupMessage)
{
    m_url = soupURIToKURL(soup_message_get_uri(soupMessage));

    m_httpStatusCode = soupMessage->status_code;

    SoupMessageHeadersIter headersIter;
    const char* headerName;
    const char* headerValue;

    soup_message_headers_iter_init(&headersIter, soupMessage->response_headers);
    while (soup_message_headers_iter_next(&headersIter, &headerName, &headerValue))
        m_httpHeaderFields.set(String::fromUTF8(headerName),
                               String::fromUTF8WithLatin1Fallback(headerValue, strlen(headerValue)));

    m_soupFlags = soup_message_get_flags(soupMessage);

    String contentType;
    const char* officialType = soup_message_headers_get_one(soupMessage->response_headers, "Content-Type");
    if (!m_sniffedContentType.isEmpty() && m_sniffedContentType != officialType)
        contentType = m_sniffedContentType;
    else
        contentType = officialType;
    setMimeType(extractMIMETypeFromMediaType(contentType));
    setTextEncodingName(extractCharsetFromMediaType(contentType));

    setExpectedContentLength(soup_message_headers_get_content_length(soupMessage->response_headers));
    setHTTPStatusText(soupMessage->reason_phrase);
    setSuggestedFilename(filenameFromHTTPContentDisposition(httpHeaderField("Content-Disposition")));
}

}
