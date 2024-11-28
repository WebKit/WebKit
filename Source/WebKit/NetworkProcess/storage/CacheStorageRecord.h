/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */
#pragma once

#include "NetworkCacheKey.h"
#include <WebCore/DOMCacheEngine.h>
#include <WebCore/HTTPParsers.h>
#include <WebCore/ResourceResponse.h>

namespace WebKit {

class CacheStorageRecordInformation {
public:
    CacheStorageRecordInformation() = default;
    CacheStorageRecordInformation(NetworkCache::Key&&, double insertionTime, uint64_t identifier, uint64_t updateResponseCounter, uint64_t size, URL&&, bool hasVaryStar, HashMap<String, String>&& varyHeaders);
    void updateVaryHeaders(const WebCore::ResourceRequest&, const WebCore::ResourceResponse::CrossThreadData&);
    CacheStorageRecordInformation isolatedCopy() &&;

    NetworkCache::Key key() const { return m_key; }
    double insertionTime() const { return m_insertionTime; }
    uint64_t identifier() const { return m_identifier; }
    uint64_t updateResponseCounter() const { return m_updateResponseCounter; }
    uint64_t size() const { return m_size; }
    URL url() const { return m_url; }
    bool hasVaryStar() const { return m_hasVaryStar; }
    HashMap<String, String> varyHeaders() const { return m_varyHeaders; }

    void setKey(NetworkCache::Key&& key) { m_key = WTFMove(key); }
    void setSize(uint64_t size) { m_size = size; }
    void setIdentifier(uint64_t identifier) { m_identifier = identifier; }
    void setUpdateResponseCounter(double updateResponseCounter) { m_updateResponseCounter = updateResponseCounter; }
    void setInsertionTime(double insertionTime) { m_insertionTime = insertionTime; }
    void setURL(URL&&);

private:
    NetworkCache::Key m_key;
    double m_insertionTime { 0 };
    uint64_t m_identifier { 0 };
    uint64_t m_updateResponseCounter { 0 };
    uint64_t m_size { 0 };
    URL m_url;
    bool m_hasVaryStar { false };
    HashMap<String, String> m_varyHeaders;
};

struct CacheStorageRecord {
    WTF_MAKE_STRUCT_FAST_ALLOCATED;
    CacheStorageRecord(const CacheStorageRecord&) = delete;
    CacheStorageRecord& operator=(const CacheStorageRecord&) = delete;
    CacheStorageRecord() = default;
    CacheStorageRecord(CacheStorageRecord&&) = default;
    CacheStorageRecord& operator=(CacheStorageRecord&&) = default;
    CacheStorageRecord(const CacheStorageRecordInformation& info, WebCore::FetchHeaders::Guard requestHeadersGuard, const WebCore::ResourceRequest& request, WebCore::FetchOptions options, const String& referrer, WebCore::FetchHeaders::Guard responseHeadersGuard, WebCore::ResourceResponse::CrossThreadData&& responseData, uint64_t responseBodySize, WebCore::DOMCacheEngine::ResponseBody&& responseBody)
        : info(info)
        , requestHeadersGuard(requestHeadersGuard)
        , request(request)
        , options(options)
        , referrer(referrer)
        , responseHeadersGuard(responseHeadersGuard)
        , responseData(WTFMove(responseData))
        , responseBodySize(responseBodySize)
        , responseBody(WTFMove(responseBody))
    {
    }

    CacheStorageRecord isolatedCopy() && {
        return {
            crossThreadCopy(WTFMove(info)),
            requestHeadersGuard,
            crossThreadCopy(WTFMove(request)),
            options,
            crossThreadCopy(WTFMove(referrer)),
            responseHeadersGuard,
            crossThreadCopy(WTFMove(responseData)),
            responseBodySize,
            WebCore::DOMCacheEngine::isolatedResponseBody(WTFMove(responseBody))
        };
    }

    CacheStorageRecordInformation info;
    WebCore::FetchHeaders::Guard requestHeadersGuard;
    WebCore::ResourceRequest request;
    WebCore::FetchOptions options;
    String referrer;
    WebCore::FetchHeaders::Guard responseHeadersGuard;
    WebCore::ResourceResponse::CrossThreadData responseData;
    uint64_t responseBodySize;
    WebCore::DOMCacheEngine::ResponseBody responseBody;
};

}
