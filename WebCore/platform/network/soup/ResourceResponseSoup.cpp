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

#include "CString.h"
#include "GOwnPtr.h"
#include "PlatformString.h"

using namespace std;

namespace WebCore {

SoupMessage* ResourceResponse::toSoupMessage() const
{
    // This GET here is just because SoupMessage wants it, we dn't really know.
    SoupMessage* soupMessage = soup_message_new("GET", url().string().utf8().data());
    if (!soupMessage)
        return 0;

    soupMessage->status_code = httpStatusCode();

    HTTPHeaderMap headers = httpHeaderFields();
    SoupMessageHeaders* soupHeaders = soupMessage->response_headers;
    if (!headers.isEmpty()) {
        HTTPHeaderMap::const_iterator end = headers.end();
        for (HTTPHeaderMap::const_iterator it = headers.begin(); it != end; ++it)
            soup_message_headers_append(soupHeaders, it->first.string().utf8().data(), it->second.utf8().data());
    }

    // Body data is not in the message.
    return soupMessage;
}

void ResourceResponse::updateFromSoupMessage(SoupMessage* soupMessage)
{
    SoupURI* soupURI = soup_message_get_uri(soupMessage);
    GOwnPtr<gchar> uri(soup_uri_to_string(soupURI, FALSE));
    m_url = KURL(KURL(), String::fromUTF8(uri.get()));

    m_httpStatusCode = soupMessage->status_code;

    SoupMessageHeadersIter headersIter;
    const char* headerName;
    const char* headerValue;

    soup_message_headers_iter_init(&headersIter, soupMessage->response_headers);
    while (soup_message_headers_iter_next(&headersIter, &headerName, &headerValue))
        m_httpHeaderFields.set(String::fromUTF8(headerName), String::fromUTF8(headerValue));
}

}
