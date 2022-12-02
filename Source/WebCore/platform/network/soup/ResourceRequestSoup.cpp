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
#include "RegistrableDomain.h"
#include "SharedBuffer.h"
#include "SoupVersioning.h"
#include "URLSoup.h"
#include "WebKitFormDataInputStream.h"
#include <wtf/text/CString.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

static inline SoupMessagePriority toSoupMessagePriority(ResourceLoadPriority priority)
{
    switch (priority) {
    case ResourceLoadPriority::VeryLow:
        return SOUP_MESSAGE_PRIORITY_VERY_LOW;
    case ResourceLoadPriority::Low:
        return SOUP_MESSAGE_PRIORITY_LOW;
    case ResourceLoadPriority::Medium:
        return SOUP_MESSAGE_PRIORITY_NORMAL;
    case ResourceLoadPriority::High:
        return SOUP_MESSAGE_PRIORITY_HIGH;
    case ResourceLoadPriority::VeryHigh:
        return SOUP_MESSAGE_PRIORITY_VERY_HIGH;
    }

    RELEASE_ASSERT_NOT_REACHED();
}

GRefPtr<SoupMessage> ResourceRequest::createSoupMessage(BlobRegistryImpl& blobRegistry) const
{
    auto uri = createSoupURI();
    if (!uri)
        return nullptr;

    auto soupMessage = adoptGRef(soup_message_new_from_uri(httpMethod().ascii().data(), uri.get()));

    soup_message_set_priority(soupMessage.get(), toSoupMessagePriority(priority()));

    updateSoupMessageHeaders(soup_message_get_request_headers(soupMessage.get()));

    if (firstPartyForCookies().protocolIsInHTTPFamily()) {
        if (auto firstParty = urlToSoupURI(firstPartyForCookies()))
            soup_message_set_first_party(soupMessage.get(), firstParty.get());
    }

#if SOUP_CHECK_VERSION(2, 69, 90)
    if (!isSameSiteUnspecified()) {
        if (isSameSite()) {
            auto siteForCookies = urlToSoupURI(m_requestData.m_url);
            soup_message_set_site_for_cookies(soupMessage.get(), siteForCookies.get());
        }
        soup_message_set_is_top_level_navigation(soupMessage.get(), isTopSite());
    }
#endif

    if (!acceptEncoding())
        soup_message_disable_feature(soupMessage.get(), SOUP_TYPE_CONTENT_DECODER);
    if (!allowCookies())
        soup_message_disable_feature(soupMessage.get(), SOUP_TYPE_COOKIE_JAR);

    updateSoupMessageBody(soupMessage.get(), blobRegistry);

    return soupMessage;
}

void ResourceRequest::updateSoupMessageBody(SoupMessage* soupMessage, BlobRegistryImpl& blobRegistry) const
{
    auto* formData = httpBody();
    if (!formData || formData->isEmpty())
        return;

    // Handle the common special case of one piece of form data, with no files.
    auto& elements = formData->elements();
    if (elements.size() == 1 && !formData->alwaysStream()) {
        if (auto* vector = std::get_if<Vector<uint8_t>>(&elements[0].data)) {
#if USE(SOUP2)
            soup_message_body_append(soupMessage->request_body, SOUP_MEMORY_TEMPORARY, vector->data(), vector->size());
#else
            GRefPtr<GBytes> bytes = adoptGRef(g_bytes_new_static(vector->data(), vector->size()));
            soup_message_set_request_body_from_bytes(soupMessage, nullptr, bytes.get());
#endif
            return;
        }
    }

    // Precompute the content length.
    auto resolvedFormData = formData->resolveBlobReferences();
    uint64_t length = 0;
    for (auto& element : resolvedFormData->elements()) {
        length += element.lengthInBytes([&](auto& url) {
            return blobRegistry.blobSize(url);
        });
    }

    if (!length)
        return;

    GRefPtr<GInputStream> stream = webkitFormDataInputStreamNew(WTFMove(resolvedFormData));
#if USE(SOUP2)
    if (GBytes* data = webkitFormDataInputStreamReadAll(WEBKIT_FORM_DATA_INPUT_STREAM(stream.get()))) {
        soup_message_body_set_accumulate(soupMessage->request_body, FALSE);
        auto* soupBuffer = soup_buffer_new_with_owner(g_bytes_get_data(data, nullptr),
            g_bytes_get_size(data), data, reinterpret_cast<GDestroyNotify>(g_bytes_unref));
        soup_message_body_append_buffer(soupMessage->request_body, soupBuffer);
        soup_buffer_free(soupBuffer);
    }
    ASSERT(length == static_cast<uint64_t>(soupMessage->request_body->length));
#else
    soup_message_set_request_body(soupMessage, nullptr, stream.get(), length);
#endif

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
    m_requestData.m_httpHeaderFields.clear();
    SoupMessageHeadersIter headersIter;
    soup_message_headers_iter_init(&headersIter, soupHeaders);
    const char* headerName;
    const char* headerValue;
    while (soup_message_headers_iter_next(&headersIter, &headerName, &headerValue))
        m_requestData.m_httpHeaderFields.set(String::fromLatin1(headerName), String::fromLatin1(headerValue));
}

unsigned initializeMaximumHTTPConnectionCountPerHost()
{
    // Soup has its own queue control; it wants to have all requests
    // given to it, so that it is able to look ahead, and schedule
    // them in a good way.
    return 10000;
}

#if USE(SOUP2)
GUniquePtr<SoupURI> ResourceRequest::createSoupURI() const
{
    // WebKit does not support fragment identifiers in data URLs, but soup does.
    // Before passing the URL to soup, we should make sure to urlencode any '#'
    // characters, so that soup does not interpret them as fragment identifiers.
    // See http://wkbug.com/68089
    if (m_requestData.m_url.protocolIsData()) {
        String urlString = makeStringByReplacingAll(m_requestData.m_url.string(), '#', "%23"_s);
        return GUniquePtr<SoupURI>(soup_uri_new(urlString.utf8().data()));
    }

    GUniquePtr<SoupURI> soupURI = urlToSoupURI(m_requestData.m_url);

    // Versions of libsoup prior to 2.42 have a soup_uri_new that will convert empty passwords that are not
    // prefixed by a colon into null. Some parts of soup like the SoupAuthenticationManager will only be active
    // when both the username and password are non-null. When we have credentials, empty usernames and passwords
    // should be empty strings instead of null.
    String urlUser = m_requestData.m_url.user();
    String urlPass = m_requestData.m_url.password();
    if (!urlUser.isEmpty() || !urlPass.isEmpty()) {
        soup_uri_set_user(soupURI.get(), urlUser.utf8().data());
        soup_uri_set_password(soupURI.get(), urlPass.utf8().data());
    }

    return soupURI;
}
#else
GRefPtr<GUri> ResourceRequest::createSoupURI() const
{
    return m_requestData.m_url.createGUri();
}
#endif

ResourceRequestPlatformData ResourceRequest::getResourceRequestPlatformData() const
{
    if (m_httpBody)
        return ResourceRequestPlatformData { m_requestData, m_httpBody->flattenToString(), m_acceptEncoding, m_redirectCount };
    return ResourceRequestPlatformData { m_requestData, std::nullopt, m_acceptEncoding, m_redirectCount };
}

ResourceRequest ResourceRequest::fromResourceRequestData(ResourceRequestData requestData)
{
    if (std::holds_alternative<ResourceRequestBase::RequestData>(requestData)) 
        return ResourceRequest(WTFMove(std::get<ResourceRequestBase::RequestData>(requestData)));
    return ResourceRequest(WTFMove(std::get<ResourceRequestPlatformData>(requestData)));
}

ResourceRequestData ResourceRequest::getRequestDataToSerialize() const
{
    if (encodingRequiresPlatformData())
        return getResourceRequestPlatformData();
    return m_requestData;
}

} // namespace WebCore

#endif // USE(SOUP)
