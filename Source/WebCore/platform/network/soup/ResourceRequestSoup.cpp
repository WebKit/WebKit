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

#if USE(SOUP)
#include "ResourceRequest.h"

#include "BlobData.h"
#include "BlobRegistryImpl.h"
#include "GUniquePtrSoup.h"
#include "HTTPParsers.h"
#include "MIMETypeRegistry.h"
#include "SharedBuffer.h"
#include "WebKitSoupRequestGeneric.h"
#include <wtf/text/CString.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

static uint64_t appendEncodedBlobItemToSoupMessageBody(SoupMessage* soupMessage, const BlobDataItem& blobItem)
{
    switch (blobItem.type()) {
    case BlobDataItem::Type::Data:
        soup_message_body_append(soupMessage->request_body, SOUP_MEMORY_TEMPORARY, blobItem.data().data()->data() + blobItem.offset(), blobItem.length());
        return blobItem.length();
    case BlobDataItem::Type::File: {
        if (!FileSystem::isValidFileTime(blobItem.file()->expectedModificationTime()))
            return 0;

        time_t fileModificationTime;
        if (!FileSystem::getFileModificationTime(blobItem.file()->path(), fileModificationTime)
            || fileModificationTime != static_cast<time_t>(blobItem.file()->expectedModificationTime()))
            return 0;

        if (RefPtr<SharedBuffer> buffer = SharedBuffer::createWithContentsOfFile(blobItem.file()->path())) {
            if (buffer->isEmpty())
                return 0;

            GUniquePtr<SoupBuffer> soupBuffer(buffer->createSoupBuffer(blobItem.offset(), blobItem.length() == BlobDataItem::toEndOfFile ? 0 : blobItem.length()));
            if (soupBuffer->length)
                soup_message_body_append_buffer(soupMessage->request_body, soupBuffer.get());
            return soupBuffer->length;
        }
        break;
    }
    }

    return 0;
}

void ResourceRequest::updateSoupMessageBody(SoupMessage* soupMessage) const
{
    auto* formData = httpBody();
    if (!formData || formData->isEmpty())
        return;

    soup_message_body_set_accumulate(soupMessage->request_body, FALSE);
    uint64_t bodySize = 0;
    for (const auto& element : formData->elements()) {
        switchOn(element.data,
            [&] (const Vector<char>& bytes) {
                bodySize += bytes.size();
                soup_message_body_append(soupMessage->request_body, SOUP_MEMORY_TEMPORARY, bytes.data(), bytes.size());
            }, [&] (const FormDataElement::EncodedFileData& fileData) {
                if (RefPtr<SharedBuffer> buffer = SharedBuffer::createWithContentsOfFile(fileData.filename)) {
                    if (buffer->isEmpty())
                        return;
                    
                    GUniquePtr<SoupBuffer> soupBuffer(buffer->createSoupBuffer());
                    bodySize += buffer->size();
                    if (soupBuffer->length)
                        soup_message_body_append_buffer(soupMessage->request_body, soupBuffer.get());
                }
            }, [&] (const FormDataElement::EncodedBlobData& blob) {
                if (auto* blobData = static_cast<BlobRegistryImpl&>(blobRegistry()).getBlobDataFromURL(blob.url)) {
                    for (const auto& item : blobData->items())
                        bodySize += appendEncodedBlobItemToSoupMessageBody(soupMessage, item);
                }
            }
        );
    }

    ASSERT(bodySize == static_cast<uint64_t>(soupMessage->request_body->length));
}

void ResourceRequest::updateSoupMessageMembers(SoupMessage* soupMessage) const
{
    updateSoupMessageHeaders(soupMessage->request_headers);

    GUniquePtr<SoupURI> firstParty = firstPartyForCookies().createSoupURI();
    if (firstParty)
        soup_message_set_first_party(soupMessage, firstParty.get());

    soup_message_set_flags(soupMessage, m_soupFlags);

    if (!acceptEncoding())
        soup_message_disable_feature(soupMessage, SOUP_TYPE_CONTENT_DECODER);
    if (!allowCookies())
        soup_message_disable_feature(soupMessage, SOUP_TYPE_COOKIE_JAR);
}

void ResourceRequest::updateSoupMessageHeaders(SoupMessageHeaders* soupHeaders) const
{
    const HTTPHeaderMap& headers = httpHeaderFields();
    if (!headers.isEmpty()) {
        HTTPHeaderMap::const_iterator end = headers.end();
        for (HTTPHeaderMap::const_iterator it = headers.begin(); it != end; ++it)
            soup_message_headers_append(soupHeaders, it->key.utf8().data(), it->value.utf8().data());
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
        m_httpHeaderFields.set(String(headerName), String(headerValue));
}

void ResourceRequest::updateSoupMessage(SoupMessage* soupMessage) const
{
    g_object_set(soupMessage, SOUP_MESSAGE_METHOD, httpMethod().ascii().data(), NULL);

    GUniquePtr<SoupURI> uri = createSoupURI();
    soup_message_set_uri(soupMessage, uri.get());

    updateSoupMessageMembers(soupMessage);
    updateSoupMessageBody(soupMessage);
}

void ResourceRequest::updateFromSoupMessage(SoupMessage* soupMessage)
{
    bool shouldPortBeResetToZero = m_url.port() && !m_url.port().value();
    m_url = URL(soup_message_get_uri(soupMessage));

    // SoupURI cannot differeniate between an explicitly specified port 0 and
    // no port specified.
    if (shouldPortBeResetToZero)
        m_url.setPort(0);

    m_httpMethod = String(soupMessage->method);

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
    if (m_initiatingPageID) {
        uint64_t* initiatingPageIDPtr = static_cast<uint64_t*>(fastMalloc(sizeof(uint64_t)));
        *initiatingPageIDPtr = m_initiatingPageID;
        g_object_set_data_full(G_OBJECT(soupRequest), g_intern_static_string(gSoupRequestInitiatingPageIDKey), initiatingPageIDPtr, fastFree);
    }

    if (WEBKIT_IS_SOUP_REQUEST_GENERIC(soupRequest))
        webkitSoupRequestGenericSetRequest(WEBKIT_SOUP_REQUEST_GENERIC(soupRequest), *this);
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
    // WebKit does not support fragment identifiers in data URLs, but soup does.
    // Before passing the URL to soup, we should make sure to urlencode any '#'
    // characters, so that soup does not interpret them as fragment identifiers.
    // See http://wkbug.com/68089
    if (m_url.protocolIsData()) {
        String urlString = m_url.string();
        urlString.replace("#", "%23");
        return GUniquePtr<SoupURI>(soup_uri_new(urlString.utf8().data()));
    }

    GUniquePtr<SoupURI> soupURI = m_url.createSoupURI();

    // Versions of libsoup prior to 2.42 have a soup_uri_new that will convert empty passwords that are not
    // prefixed by a colon into null. Some parts of soup like the SoupAuthenticationManager will only be active
    // when both the username and password are non-null. When we have credentials, empty usernames and passwords
    // should be empty strings instead of null.
    String urlUser = m_url.user();
    String urlPass = m_url.pass();
    if (!urlUser.isEmpty() || !urlPass.isEmpty()) {
        soup_uri_set_user(soupURI.get(), urlUser.utf8().data());
        soup_uri_set_password(soupURI.get(), urlPass.utf8().data());
    }

    return soupURI;
}

}

#endif
