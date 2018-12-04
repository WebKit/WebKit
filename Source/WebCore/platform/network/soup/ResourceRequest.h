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

#ifndef ResourceRequest_h
#define ResourceRequest_h

#include "ResourceRequestBase.h"
#include "URLSoup.h"

namespace WebCore {

    class ResourceRequest : public ResourceRequestBase {
    public:
        ResourceRequest(const String& url)
            : ResourceRequestBase(URL({ }, url), ResourceRequestCachePolicy::UseProtocolCachePolicy)
            , m_acceptEncoding(true)
            , m_soupFlags(static_cast<SoupMessageFlags>(0))
            , m_initiatingPageID(0)
        {
        }

        ResourceRequest(const URL& url)
            : ResourceRequestBase(url, ResourceRequestCachePolicy::UseProtocolCachePolicy)
            , m_acceptEncoding(true)
            , m_soupFlags(static_cast<SoupMessageFlags>(0))
            , m_initiatingPageID(0)
        {
        }

        ResourceRequest(const URL& url, const String& referrer, ResourceRequestCachePolicy policy = ResourceRequestCachePolicy::UseProtocolCachePolicy)
            : ResourceRequestBase(url, policy)
            , m_acceptEncoding(true)
            , m_soupFlags(static_cast<SoupMessageFlags>(0))
            , m_initiatingPageID(0)
        {
            setHTTPReferrer(referrer);
        }

        ResourceRequest()
            : ResourceRequestBase(URL(), ResourceRequestCachePolicy::UseProtocolCachePolicy)
            , m_acceptEncoding(true)
            , m_soupFlags(static_cast<SoupMessageFlags>(0))
            , m_initiatingPageID(0)
        {
        }

        ResourceRequest(SoupMessage* soupMessage)
            : ResourceRequestBase(URL(), ResourceRequestCachePolicy::UseProtocolCachePolicy)
            , m_acceptEncoding(true)
            , m_soupFlags(static_cast<SoupMessageFlags>(0))
            , m_initiatingPageID(0)
        {
            updateFromSoupMessage(soupMessage);
        }

        ResourceRequest(SoupRequest* soupRequest)
            : ResourceRequestBase(soupURIToURL(soup_request_get_uri(soupRequest)), ResourceRequestCachePolicy::UseProtocolCachePolicy)
            , m_acceptEncoding(true)
            , m_soupFlags(static_cast<SoupMessageFlags>(0))
            , m_initiatingPageID(0)
        {
            updateFromSoupRequest(soupRequest);
        }

        void updateFromDelegatePreservingOldProperties(const ResourceRequest& delegateProvidedRequest) { *this = delegateProvidedRequest; }

        bool acceptEncoding() const { return m_acceptEncoding; }
        void setAcceptEncoding(bool acceptEncoding) { m_acceptEncoding = acceptEncoding; }

        void updateSoupMessageHeaders(SoupMessageHeaders*) const;
        void updateFromSoupMessageHeaders(SoupMessageHeaders*);
        void updateSoupMessage(SoupMessage*) const;
        void updateFromSoupMessage(SoupMessage*);
        void updateSoupRequest(SoupRequest*) const;
        void updateFromSoupRequest(SoupRequest*);

        SoupMessageFlags soupMessageFlags() const { return m_soupFlags; }
        void setSoupMessageFlags(SoupMessageFlags soupFlags) { m_soupFlags = soupFlags; }

        uint64_t initiatingPageID() const { return m_initiatingPageID; }
        void setInitiatingPageID(uint64_t pageID) { m_initiatingPageID = pageID; }

        GUniquePtr<SoupURI> createSoupURI() const;

        template<class Encoder> void encodeWithPlatformData(Encoder&) const;
        template<class Decoder> bool decodeWithPlatformData(Decoder&);

    private:
        friend class ResourceRequestBase;

        bool m_acceptEncoding : 1;
        SoupMessageFlags m_soupFlags;
        uint64_t m_initiatingPageID;

        void updateSoupMessageMembers(SoupMessage*) const;
        void updateSoupMessageBody(SoupMessage*) const;
        void doUpdatePlatformRequest() { }
        void doUpdateResourceRequest() { }
        void doUpdatePlatformHTTPBody() { }
        void doUpdateResourceHTTPBody() { }

        void doPlatformSetAsIsolatedCopy(const ResourceRequest&) { }
    };

template<class Encoder>
void ResourceRequest::encodeWithPlatformData(Encoder& encoder) const
{
    encodeBase(encoder);

    // FIXME: Do not encode HTTP message body.
    // 1. It can be large and thus costly to send across.
    // 2. It is misleading to provide a body with some requests, while others use body streams, which cannot be serialized at all.
    encoder << static_cast<bool>(m_httpBody);
    if (m_httpBody)
        encoder << m_httpBody->flattenToString();

    encoder << static_cast<uint32_t>(m_soupFlags);
    encoder << m_initiatingPageID;
}

template<class Decoder>
bool ResourceRequest::decodeWithPlatformData(Decoder& decoder)
{
    if (!decodeBase(decoder))
        return false;

    bool hasHTTPBody;
    if (!decoder.decode(hasHTTPBody))
        return false;
    if (hasHTTPBody) {
        String httpBody;
        if (!decoder.decode(httpBody))
            return false;
        setHTTPBody(FormData::create(httpBody.utf8()));
    }

    uint32_t soupMessageFlags;
    if (!decoder.decode(soupMessageFlags))
        return false;
    m_soupFlags = static_cast<SoupMessageFlags>(soupMessageFlags);

    uint64_t initiatingPageID;
    if (!decoder.decode(initiatingPageID))
        return false;
    m_initiatingPageID = initiatingPageID;

    return true;
}


#if SOUP_CHECK_VERSION(2, 43, 1)
inline SoupMessagePriority toSoupMessagePriority(ResourceLoadPriority priority)
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

    ASSERT_NOT_REACHED();
    return SOUP_MESSAGE_PRIORITY_VERY_LOW;
}
#endif

} // namespace WebCore

#endif // ResourceRequest_h
