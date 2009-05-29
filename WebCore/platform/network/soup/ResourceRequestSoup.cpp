/*
 * Copyright (C) 2009 Gustavo Noronha Silva
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
#include "ResourceRequest.h"

#include "CString.h"
#include "GOwnPtr.h"
#include "PlatformString.h"

#include <libsoup/soup.h>

using namespace std;

namespace WTF {

template <> void freeOwnedGPtr<SoupURI>(SoupURI* uri)
{
    if (uri)
        soup_uri_free(uri);
}

}

namespace WebCore {

SoupMessage* ResourceRequest::soupMessage() const
{
    updatePlatformRequest();

    return m_soupMessage;
}

// Based on ResourceRequestBase::copyData and ResourceRequestBase::adopt.
ResourceRequest::ResourceRequest(const ResourceRequest& resourceRequest)
    : ResourceRequestBase(resourceRequest)
    , m_soupMessage(0)
{
    m_url = resourceRequest.url().copy();
    m_firstPartyForCookies = resourceRequest.firstPartyForCookies().copy();
    m_httpMethod = resourceRequest.httpMethod().copy();
    m_httpHeaderFields.adopt(auto_ptr<CrossThreadHTTPHeaderMapData>(resourceRequest.m_httpHeaderFields.copyData().release()));

    size_t encodingCount = resourceRequest.m_responseContentDispositionEncodingFallbackArray.size();
    if (encodingCount > 0) {
        String encoding1 = resourceRequest.m_responseContentDispositionEncodingFallbackArray[0];
        String encoding2;
        String encoding3;
        if (encodingCount > 1) {
            encoding2 = resourceRequest.m_responseContentDispositionEncodingFallbackArray[1];
            if (encodingCount > 2)
                encoding3 = resourceRequest.m_responseContentDispositionEncodingFallbackArray[2];
        }
        ASSERT(encodingCount <= 3);
        setResponseContentDispositionEncodingFallbackArray(encoding1, encoding2, encoding3);
    }
    if (resourceRequest.m_httpBody)
        setHTTPBody(resourceRequest.httpBody());

    m_resourceRequestUpdated = true;
    m_platformRequestUpdated = false;
}

void ResourceRequest::doUpdateResourceRequest()
{
    if (!m_soupMessage)
        return;

    SoupURI* soupURI = soup_message_get_uri(m_soupMessage);
    GOwnPtr<gchar> uri(soup_uri_to_string(soupURI, FALSE));
    m_url = KURL(KURL(), String::fromUTF8(uri.get()));

    m_httpMethod = String::fromUTF8(m_soupMessage->method);

    SoupMessageHeadersIter headersIter;
    const char* headerName;
    const char* headerValue;

    soup_message_headers_iter_init(&headersIter, m_soupMessage->request_headers);
    while (soup_message_headers_iter_next(&headersIter, &headerName, &headerValue))
        m_httpHeaderFields.set(String::fromUTF8(headerName), String::fromUTF8(headerValue));

    m_httpBody = FormData::create(m_soupMessage->request_body->data, m_soupMessage->request_body->length);

    // FIXME: m_allowHTTPCookies and m_firstPartyForCookies should
    // probably be handled here and on doUpdatePlatformRequest
    // somehow.
}

void ResourceRequest::doUpdatePlatformRequest()
{
    GOwnPtr<SoupURI> soupURI(soup_uri_new(url().string().utf8().data()));
    ASSERT(soupURI);

    if (!m_soupMessage) {
        m_soupMessage = soup_message_new_from_uri(httpMethod().utf8().data(), soupURI.get());
    } else {
        soup_message_set_uri(m_soupMessage, soupURI.get());

        g_object_set(m_soupMessage, "method", httpMethod().utf8().data(), NULL);
        soup_message_headers_clear(m_soupMessage->request_headers);
    }

    ASSERT(m_soupMessage);

    HTTPHeaderMap headers = httpHeaderFields();
    SoupMessageHeaders* soupHeaders = m_soupMessage->request_headers;
    if (!headers.isEmpty()) {
        HTTPHeaderMap::const_iterator end = headers.end();
        for (HTTPHeaderMap::const_iterator it = headers.begin(); it != end; ++it)
            soup_message_headers_append(soupHeaders, it->first.string().utf8().data(), it->second.utf8().data());
    }

    // Body data is only handled at ResourceHandleSoup::startHttp for
    // now; this is because this may not be a good place to go
    // openning and mmapping files. We should maybe revisit this.
}

}
