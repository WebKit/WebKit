/*
 * Copyright (C) 2003, 2006 Apple Inc.  All rights reserved.
 * Copyright (C) 2006 Samuel Weinig <sam.weinig@gmail.com>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "PageIdentifier.h"
#include "ResourceRequestBase.h"
#include "URLSoup.h"
#include <wtf/glib/GRefPtr.h>

namespace WebCore {

class BlobRegistryImpl;

struct ResourceRequestPlatformData {
    ResourceRequestBase::RequestData requestData;
    std::optional<String> flattenedHTTPBody;
    bool acceptEncoding;
    uint16_t redirectCount;
};
using ResourceRequestData = std::variant<ResourceRequestBase::RequestData, ResourceRequestPlatformData>;

class ResourceRequest : public ResourceRequestBase {
public:
    explicit ResourceRequest(const String& url)
        : ResourceRequestBase(URL({ }, url), ResourceRequestCachePolicy::UseProtocolCachePolicy)
    {
    }

    ResourceRequest(const URL& url)
        : ResourceRequestBase(url, ResourceRequestCachePolicy::UseProtocolCachePolicy)
    {
    }

    ResourceRequest(const URL& url, const String& referrer, ResourceRequestCachePolicy policy = ResourceRequestCachePolicy::UseProtocolCachePolicy)
        : ResourceRequestBase(url, policy)
    {
        setHTTPReferrer(referrer);
    }

    ResourceRequest()
        : ResourceRequestBase(URL(), ResourceRequestCachePolicy::UseProtocolCachePolicy)
    {
    }

    ResourceRequest(ResourceRequestBase&& base)
        : ResourceRequestBase(WTFMove(base))
    {
    }

    ResourceRequest(ResourceRequestPlatformData&& platformData)
        : ResourceRequestBase(WTFMove(platformData.requestData))
    {
        if (platformData.flattenedHTTPBody)
            setHTTPBody(FormData::create(platformData.flattenedHTTPBody->utf8()));

        m_acceptEncoding = platformData.acceptEncoding;
        m_redirectCount =platformData.redirectCount;
    }

    GRefPtr<SoupMessage> createSoupMessage(BlobRegistryImpl&) const;

    void updateFromDelegatePreservingOldProperties(const ResourceRequest& delegateProvidedRequest) { *this = delegateProvidedRequest; }

    bool acceptEncoding() const { return m_acceptEncoding; }
    void setAcceptEncoding(bool acceptEncoding) { m_acceptEncoding = acceptEncoding; }

    void incrementRedirectCount() { m_redirectCount++; }
    uint16_t redirectCount() const { return m_redirectCount; }

    void updateSoupMessageBody(SoupMessage*, BlobRegistryImpl&) const;
    void updateSoupMessageHeaders(SoupMessageHeaders*) const;
    void updateFromSoupMessageHeaders(SoupMessageHeaders*);

    ResourceRequestPlatformData getResourceRequestPlatformData() const;
    WEBCORE_EXPORT static ResourceRequest fromResourceRequestData(ResourceRequestData);
    WEBCORE_EXPORT ResourceRequestData getRequestDataToSerialize() const;

private:
    friend class ResourceRequestBase;

#if USE(SOUP2)
    GUniquePtr<SoupURI> createSoupURI() const;
#else
    GRefPtr<GUri> createSoupURI() const;
#endif

    void doUpdatePlatformRequest() { }
    void doUpdateResourceRequest() { }
    void doUpdatePlatformHTTPBody() { }
    void doUpdateResourceHTTPBody() { }

    void doPlatformSetAsIsolatedCopy(const ResourceRequest&) { }

    bool m_acceptEncoding { true };
    uint16_t m_redirectCount { 0 };
};

} // namespace WebCore

