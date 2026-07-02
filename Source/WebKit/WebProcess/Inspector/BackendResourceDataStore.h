/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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

#include <WebCore/InspectorResourceType.h>
#include <WebCore/ResourceLoaderIdentifier.h>
#include <WebCore/SharedBuffer.h>
#include <WebCore/TextResourceDecoder.h>
#include <wtf/CheckedRef.h>
#include <wtf/Expected.h>
#include <wtf/ListHashSet.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/UniqueRef.h>
#include <wtf/WallTime.h>
#include <wtf/WeakPtr.h>
#include <wtf/text/WTFString.h>

namespace WebCore {
class CertificateInfo;
class ResourceResponse;
}

namespace WebKit {

// BackendResourceDataStore buffers response metadata and content in the WebProcess
// for later retrieval by ProxyingNetworkAgent (UIProcess) via reverse IPC. This is
// the Site Isolation equivalent of WebCore's NetworkResourcesData, which is used by
// InspectorNetworkAgent in the single-process (WebKitLegacy) and non-SI paths.
//
// Unlike NetworkResourcesData, this store does NOT hold references to CachedResource.
// All data is copied out of WebCore types at instrumentation time, making the store
// fully independent of the loader/cache lifecycle.
//
// Keyed by ResourceLoaderIdentifier (local to this WebProcess). Content is buffered
// as raw data during loading, decoded to a String on completion, and subject to LRU
// eviction when the total content size exceeds the configured maximum.
class BackendResourceDataStore : public CanMakeWeakPtr<BackendResourceDataStore>, public CanMakeCheckedPtr<BackendResourceDataStore> {
    WTF_MAKE_TZONE_ALLOCATED(BackendResourceDataStore);
    WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(BackendResourceDataStore);
public:
    class ResourceData {
        WTF_MAKE_TZONE_ALLOCATED(ResourceData);
        friend class BackendResourceDataStore;
    public:
        using ResourceID = WebCore::ResourceLoaderIdentifier;

        ResourceData(ResourceID, Inspector::ResourceType);
        ~ResourceData();

        ResourceID resourceID() const { return m_resourceID; }

        const String& url() const LIFETIME_BOUND { return m_url; }
        void setURL(const String& url) { m_url = url; }

        const String& mimeType() const LIFETIME_BOUND { return m_mimeType; }
        void setMIMEType(const String& mimeType) { m_mimeType = mimeType; }

        const String& textEncodingName() const LIFETIME_BOUND { return m_textEncodingName; }
        void setTextEncodingName(const String& textEncodingName) { m_textEncodingName = textEncodingName; }

        int httpStatusCode() const { return m_httpStatusCode; }
        void setHTTPStatusCode(int code) { m_httpStatusCode = code; }

        const String& httpStatusText() const LIFETIME_BOUND { return m_httpStatusText; }
        void setHTTPStatusText(const String& text) { m_httpStatusText = text; }

        bool hasContent() const { return !m_content.isNull(); }
        const String& content() const LIFETIME_BOUND { return m_content; }
        void setContent(const String&, bool base64Encoded);

        bool base64Encoded() const { return m_base64Encoded; }

        unsigned removeContent();
        unsigned evictContent();
        bool isContentEvicted() const { return m_isContentEvicted; }

        Inspector::ResourceType type() const { return m_type; }
        void setType(Inspector::ResourceType type) { m_type = type; }

        RefPtr<WebCore::TextResourceDecoder> decoder() const { return m_decoder.copyRef(); }
        void setDecoder(RefPtr<WebCore::TextResourceDecoder>&& decoder) { m_decoder = WTF::move(decoder); }

        RefPtr<WebCore::FragmentedSharedBuffer> buffer() const { return m_buffer.copyRef(); }
        void setBuffer(RefPtr<WebCore::FragmentedSharedBuffer>&& buffer) { m_buffer = WTF::move(buffer); }

        void setCertificateInfo(std::unique_ptr<WebCore::CertificateInfo>&&);

        bool hasBufferedData() const { return hasData(); }

    private:
        bool hasData() const;
        size_t dataLength() const;
        void appendData(const WebCore::SharedBuffer&);
        void decodeDataToContent();

        WebCore::ResourceLoaderIdentifier m_resourceID;
        String m_url;
        String m_mimeType;
        String m_textEncodingName;
        String m_content;
        String m_httpStatusText;
        RefPtr<WebCore::TextResourceDecoder> m_decoder;
        WebCore::SharedBufferBuilder m_dataBuffer;
        RefPtr<WebCore::FragmentedSharedBuffer> m_buffer;
        std::unique_ptr<WebCore::CertificateInfo> m_certificateInfo;
        Inspector::ResourceType m_type { Inspector::ResourceType::Other };
        int m_httpStatusCode { 0 };
        bool m_base64Encoded { false };
        bool m_isContentEvicted { false };
        WallTime m_responseTimestamp;
    };

    struct Settings {
        size_t maximumResourcesContentSize { 200 * MB };
        size_t maximumSingleResourceContentSize { 50 * MB };
        bool supportsShowingCertificate { false };
    };

    BackendResourceDataStore(const Settings&);
    ~BackendResourceDataStore();

    void resourceCreated(WebCore::ResourceLoaderIdentifier, Inspector::ResourceType);
    void responseReceived(WebCore::ResourceLoaderIdentifier, const WebCore::ResourceResponse&, Inspector::ResourceType);
    void setResourceType(WebCore::ResourceLoaderIdentifier, Inspector::ResourceType);
    void setResourceContent(WebCore::ResourceLoaderIdentifier, const String& content, bool base64Encoded = false);
    ResourceData const* maybeAddResourceData(WebCore::ResourceLoaderIdentifier, const WebCore::SharedBuffer&);
    void maybeDecodeDataToContent(WebCore::ResourceLoaderIdentifier);
    void addResourceSharedBuffer(WebCore::ResourceLoaderIdentifier, RefPtr<WebCore::FragmentedSharedBuffer>&&, const String& textEncodingName);
    ResourceData const* data(WebCore::ResourceLoaderIdentifier);
    void clear();

    Expected<std::tuple<String, bool>, String> getResponseBody(WebCore::ResourceLoaderIdentifier);

private:
    ResourceData* resourceDataForId(WebCore::ResourceLoaderIdentifier);
    void ensureNoDataForId(WebCore::ResourceLoaderIdentifier);
    bool ensureFreeSpace(size_t);

    ListHashSet<WebCore::ResourceLoaderIdentifier> m_resourceIdsDeque;
    HashMap<WebCore::ResourceLoaderIdentifier, UniqueRef<ResourceData>> m_resourceDataMap;
    size_t m_contentSize { 0 };
    Settings m_settings;
};

} // namespace WebKit
