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

#include "GUniquePtrSoup.h"
#include "HTTPParsers.h"
#include "MIMETypeRegistry.h"
#include <wtf/text/CString.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

void ResourceRequest::updateSoupMessageMembers(SoupMessage* soupMessage) const
{
    updateSoupMessageHeaders(soupMessage->request_headers);

    GUniquePtr<SoupURI> firstParty = firstPartyForCookies().createSoupURI();
    if (firstParty)
        soup_message_set_first_party(soupMessage, firstParty.get());

    soup_message_set_flags(soupMessage, m_soupFlags);

    if (!acceptEncoding())
        soup_message_disable_feature(soupMessage, SOUP_TYPE_CONTENT_DECODER);
}

void ResourceRequest::updateSoupMessageHeaders(SoupMessageHeaders* soupHeaders) const
{
    const HTTPHeaderMap& headers = httpHeaderFields();
    if (!headers.isEmpty()) {
        HTTPHeaderMap::const_iterator end = headers.end();
        for (HTTPHeaderMap::const_iterator it = headers.begin(); it != end; ++it)
            soup_message_headers_append(soupHeaders, it->key.string().utf8().data(), it->value.utf8().data());
    }
}

void ResourceRequest::updateFromSoupMessageHeaders(SoupMessageHeaders* soupHeaders)
{
    m_httpHeaderFields.clear();
    SoupMessageHeadersIter headersIter;
    soup_message_headers_iter_init(&headersIter, soupHeaders);
    const char* headerName;
    const char* headerValue;
    while (soup_message_headers_iter_next(&headersIter, &headerName, &headerValue))
        m_httpHeaderFields.set(String::fromUTF8(headerName), String::fromUTF8(headerValue));
}

void ResourceRequest::updateSoupMessage(SoupMessage* soupMessage) const
{
    g_object_set(soupMessage, SOUP_MESSAGE_METHOD, httpMethod().utf8().data(), NULL);

    GUniquePtr<SoupURI> uri = createSoupURI();
    soup_message_set_uri(soupMessage, uri.get());

    updateSoupMessageMembers(soupMessage);
}

SoupMessage* ResourceRequest::toSoupMessage() const
{
    SoupMessage* soupMessage = soup_message_new(httpMethod().utf8().data(), url().string().utf8().data());
    if (!soupMessage)
        return 0;

    updateSoupMessageMembers(soupMessage);

    // Body data is only handled at ResourceHandleSoup::startHttp for
    // now; this is because this may not be a good place to go
    // openning and mmapping files. We should maybe revisit this.
    return soupMessage;
}

void ResourceRequest::updateFromSoupMessage(SoupMessage* soupMessage)
{
    bool shouldPortBeResetToZero = m_url.hasPort() && !m_url.port();
    m_url = URL(soup_message_get_uri(soupMessage));

    // SoupURI cannot differeniate between an explicitly specified port 0 and
    // no port specified.
    if (shouldPortBeResetToZero)
        m_url.setPort(0);

    m_httpMethod = String::fromUTF8(soupMessage->method);

    updateFromSoupMessageHeaders(soupMessage->request_headers);

    if (soupMessage->request_body->data)
        m_httpBody = FormData::create(soupMessage->request_body->data, soupMessage->request_body->length);

    if (SoupURI* firstParty = soup_message_get_first_party(soupMessage))
        m_firstPartyForCookies = URL(firstParty);

    m_soupFlags = soup_message_get_flags(soupMessage);

    // FIXME: m_allowCookies should probably be handled here and on
    // doUpdatePlatformRequest somehow.
}

static const char* gSoupRequestInitiatingPageIDKey = "wk-soup-request-initiating-page-id";

void ResourceRequest::updateSoupRequest(SoupRequest* soupRequest) const
{
    if (!m_initiatingPageID)
        return;

    uint64_t* initiatingPageIDPtr = static_cast<uint64_t*>(fastMalloc(sizeof(uint64_t)));
    *initiatingPageIDPtr = m_initiatingPageID;
    g_object_set_data_full(G_OBJECT(soupRequest), g_intern_static_string(gSoupRequestInitiatingPageIDKey), initiatingPageIDPtr, fastFree);
}

void ResourceRequest::updateFromSoupRequest(SoupRequest* soupRequest)
{
    uint64_t* initiatingPageIDPtr = static_cast<uint64_t*>(g_object_get_data(G_OBJECT(soupRequest), gSoupRequestInitiatingPageIDKey));
    m_initiatingPageID = initiatingPageIDPtr ? *initiatingPageIDPtr : 0;
}

unsigned initializeMaximumHTTPConnectionCountPerHost()
{
    // Soup has its own queue control; it wants to have all requests
    // given to it, so that it is able to look ahead, and schedule
    // them in a good way.
    return 10000;
}

GUniquePtr<SoupURI> ResourceRequest::createSoupURI() const
{
    URL url = m_url;
    url.removeFragmentIdentifier();
    return url.createSoupURI();
}

}
