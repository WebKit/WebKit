/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 * 1. Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY GOOGLE INC. AND ITS CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL GOOGLE INC.
 * OR ITS CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef NetworkResourcesData_h
#define NetworkResourcesData_h

#include "CachedResourceHandle.h"
#include "InspectorPageAgent.h"
#include "TextResourceDecoder.h"

#include <wtf/Deque.h>
#include <wtf/HashMap.h>
#include <wtf/RefCounted.h>
#include <wtf/text/StringBuilder.h>
#include <wtf/text/WTFString.h>

#if ENABLE(INSPECTOR)

namespace WebCore {

class CachedResource;
class SharedBuffer;
class TextResourceDecoder;

class NetworkResourcesData {
public:
    class ResourceData {
        friend class NetworkResourcesData;
    public:
        ResourceData(const String& requestId, const String& loaderId);

        String requestId() const { return m_requestId; }
        String loaderId() const { return m_loaderId; }

        String frameId() const { return m_frameId; }
        void setFrameId(const String& frameId) { m_frameId = frameId; }

        String url() const { return m_url; }
        void setUrl(const String& url) { m_url = url; }

        bool hasContent() const { return !m_content.isNull(); }
        String content() const { return m_content; }
        void setContent(const String&);

        unsigned removeContent();
        bool isContentPurged() const { return m_isContentPurged; }
        unsigned purgeContent();

        InspectorPageAgent::ResourceType type() const { return m_type; }
        void setType(InspectorPageAgent::ResourceType type) { m_type = type; }

        int httpStatusCode() const { return m_httpStatusCode; }
        void setHTTPStatusCode(int httpStatusCode) { m_httpStatusCode = httpStatusCode; }

        String textEncodingName() const { return m_textEncodingName; }
        void setTextEncodingName(const String& textEncodingName) { m_textEncodingName = textEncodingName; }

        PassRefPtr<TextResourceDecoder> decoder() const { return m_decoder; }
        void setDecoder(PassRefPtr<TextResourceDecoder> decoder) { m_decoder = decoder; }

        PassRefPtr<SharedBuffer> buffer() const { return m_buffer; }
        void setBuffer(PassRefPtr<SharedBuffer> buffer) { m_buffer = buffer; }

        CachedResource* cachedResource() const { return m_cachedResource.get(); }
        void setCachedResource(CachedResource* cachedResource) { m_cachedResource = cachedResource; }

    private:
        bool hasData() const { return m_dataBuffer; }
        int dataLength() const;
        void appendData(const char* data, int dataLength);
        int decodeDataToContent();

        String m_requestId;
        String m_loaderId;
        String m_frameId;
        String m_url;
        String m_content;
        RefPtr<SharedBuffer> m_dataBuffer;
        bool m_isContentPurged;
        InspectorPageAgent::ResourceType m_type;
        int m_httpStatusCode;

        String m_textEncodingName;
        RefPtr<TextResourceDecoder> m_decoder;

        RefPtr<SharedBuffer> m_buffer;
        CachedResourceHandle<CachedResource> m_cachedResource;
    };

    NetworkResourcesData();

    ~NetworkResourcesData();

    void resourceCreated(const String& requestId, const String& loaderId);
    void responseReceived(const String& requestId, const String& frameId, const ResourceResponse&);
    void setResourceType(const String& requestId, InspectorPageAgent::ResourceType);
    InspectorPageAgent::ResourceType resourceType(const String& requestId);
    void setResourceContent(const String& requestId, const String& content);
    void maybeAddResourceData(const String& requestId, const char* data, int dataLength);
    void maybeDecodeDataToContent(const String& requestId);
    void addCachedResource(const String& requestId, CachedResource*);
    void addResourceSharedBuffer(const String& requestId, PassRefPtr<SharedBuffer>, const String& textEncodingName);
    ResourceData const* data(const String& requestId);
    void clear(const String& preservedLoaderId = String());

    void setResourcesDataSizeLimits(int maximumResourcesContentSize, int maximumSingleResourceContentSize);

private:
    void ensureNoDataForRequestId(const String& requestId);
    bool ensureFreeSpace(int size);

    Deque<String> m_requestIdsDeque;

    typedef HashMap<String, ResourceData*> ResourceDataMap;
    ResourceDataMap m_requestIdToResourceDataMap;
    int m_contentSize;
    int m_maximumResourcesContentSize;
    int m_maximumSingleResourceContentSize;
};

} // namespace WebCore

#endif // ENABLE(INSPECTOR)

#endif // !defined(NetworkResourcesData_h)
