/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 * Copyright (C) 2017 Apple Inc.  All rights reserved.
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

#pragma once

#include "InspectorPageAgent.h"
#include <wtf/Deque.h>
#include <wtf/HashMap.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

class CachedResource;
class ResourceResponse;
class TextResourceDecoder;
class SharedBuffer;

class NetworkResourcesData {
    WTF_MAKE_FAST_ALLOCATED;
public:
    class ResourceData {
        WTF_MAKE_FAST_ALLOCATED;
        friend class NetworkResourcesData;
    public:
        ResourceData(const String& requestId, const String& loaderId);

        const String& requestId() const { return m_requestId; }
        const String& loaderId() const { return m_loaderId; }

        const String& frameId() const { return m_frameId; }
        void setFrameId(const String& frameId) { m_frameId = frameId; }

        const String& url() const { return m_url; }
        void setURL(const String& url) { m_url = url; }

        bool hasContent() const { return !m_content.isNull(); }
        const String& content() const { return m_content; }
        void setContent(const String&, bool base64Encoded);

        bool base64Encoded() const { return m_base64Encoded; }

        unsigned removeContent();
        unsigned evictContent();
        bool isContentEvicted() const { return m_isContentEvicted; }

        InspectorPageAgent::ResourceType type() const { return m_type; }
        void setType(InspectorPageAgent::ResourceType type) { m_type = type; }

        int httpStatusCode() const { return m_httpStatusCode; }
        void setHTTPStatusCode(int httpStatusCode) { m_httpStatusCode = httpStatusCode; }

        const String& textEncodingName() const { return m_textEncodingName; }
        void setTextEncodingName(const String& textEncodingName) { m_textEncodingName = textEncodingName; }

        RefPtr<TextResourceDecoder> decoder() const { return m_decoder.copyRef(); }
        void setDecoder(RefPtr<TextResourceDecoder>&& decoder) { m_decoder = WTFMove(decoder); }

        RefPtr<SharedBuffer> buffer() const { return m_buffer.copyRef(); }
        void setBuffer(RefPtr<SharedBuffer>&& buffer) { m_buffer = WTFMove(buffer); }

        const Optional<CertificateInfo>& certificateInfo() const { return m_certificateInfo; }
        void setCertificateInfo(const Optional<CertificateInfo>& certificateInfo) { m_certificateInfo = certificateInfo; }

        CachedResource* cachedResource() const { return m_cachedResource; }
        void setCachedResource(CachedResource* cachedResource) { m_cachedResource = cachedResource; }

        bool forceBufferData() const { return m_forceBufferData; }
        void setForceBufferData(bool force) { m_forceBufferData = force; }

        bool hasBufferedData() const { return m_dataBuffer; }

    private:
        bool hasData() const { return m_dataBuffer; }
        size_t dataLength() const;
        void appendData(const char* data, size_t dataLength);
        size_t decodeDataToContent();

        String m_requestId;
        String m_loaderId;
        String m_frameId;
        String m_url;
        String m_content;
        String m_textEncodingName;
        RefPtr<TextResourceDecoder> m_decoder;
        RefPtr<SharedBuffer> m_dataBuffer;
        RefPtr<SharedBuffer> m_buffer;
        Optional<CertificateInfo> m_certificateInfo;
        CachedResource* m_cachedResource { nullptr };
        InspectorPageAgent::ResourceType m_type { InspectorPageAgent::OtherResource };
        int m_httpStatusCode { 0 };
        bool m_isContentEvicted { false };
        bool m_base64Encoded { false };
        bool m_forceBufferData { false };
    };

    NetworkResourcesData();
    ~NetworkResourcesData();

    void resourceCreated(const String& requestId, const String& loaderId, InspectorPageAgent::ResourceType);
    void resourceCreated(const String& requestId, const String& loaderId, CachedResource&);
    void responseReceived(const String& requestId, const String& frameId, const ResourceResponse&, InspectorPageAgent::ResourceType, bool forceBufferData);
    void setResourceType(const String& requestId, InspectorPageAgent::ResourceType);
    InspectorPageAgent::ResourceType resourceType(const String& requestId);
    void setResourceContent(const String& requestId, const String& content, bool base64Encoded = false);
    ResourceData const* maybeAddResourceData(const String& requestId, const char* data, size_t dataLength);
    void maybeDecodeDataToContent(const String& requestId);
    void addCachedResource(const String& requestId, CachedResource*);
    void addResourceSharedBuffer(const String& requestId, RefPtr<SharedBuffer>&&, const String& textEncodingName);
    ResourceData const* data(const String& requestId);
    Vector<String> removeCachedResource(CachedResource*);
    void clear(Optional<String> preservedLoaderId = WTF::nullopt);
    Vector<ResourceData*> resources();

private:
    ResourceData* resourceDataForRequestId(const String& requestId);
    void ensureNoDataForRequestId(const String& requestId);
    bool ensureFreeSpace(size_t);

    Deque<String> m_requestIdsDeque;
    HashMap<String, std::unique_ptr<ResourceData>> m_requestIdToResourceDataMap;
    size_t m_contentSize { 0 };
    size_t m_maximumResourcesContentSize;
    size_t m_maximumSingleResourceContentSize;
};

} // namespace WebCore
