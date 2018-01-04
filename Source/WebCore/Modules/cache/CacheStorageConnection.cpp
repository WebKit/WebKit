
/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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

#include "config.h"
#include "CacheStorageConnection.h"

#include "FetchResponse.h"
#include <wtf/RandomNumber.h>

namespace WebCore {
using namespace WebCore::DOMCacheEngine;

void CacheStorageConnection::open(const ClientOrigin& origin, const String& cacheName, CacheIdentifierCallback&& callback)
{
    uint64_t requestIdentifier = ++m_lastRequestIdentifier;
    m_openAndRemoveCachePendingRequests.add(requestIdentifier, WTFMove(callback));

    doOpen(requestIdentifier, origin, cacheName);
}

void CacheStorageConnection::remove(uint64_t cacheIdentifier, CacheIdentifierCallback&& callback)
{
    uint64_t requestIdentifier = ++m_lastRequestIdentifier;
    m_openAndRemoveCachePendingRequests.add(requestIdentifier, WTFMove(callback));

    doRemove(requestIdentifier, cacheIdentifier);
}

void CacheStorageConnection::retrieveCaches(const ClientOrigin& origin, uint64_t updateCounter, CacheInfosCallback&& callback)
{
    uint64_t requestIdentifier = ++m_lastRequestIdentifier;
    m_retrieveCachesPendingRequests.add(requestIdentifier, WTFMove(callback));

    doRetrieveCaches(requestIdentifier, origin, updateCounter);
}

void CacheStorageConnection::retrieveRecords(uint64_t cacheIdentifier, const URL& url, RecordsCallback&& callback)
{
    uint64_t requestIdentifier = ++m_lastRequestIdentifier;
    m_retrieveRecordsPendingRequests.add(requestIdentifier, WTFMove(callback));

    doRetrieveRecords(requestIdentifier, cacheIdentifier, url);
}

void CacheStorageConnection::batchDeleteOperation(uint64_t cacheIdentifier, const ResourceRequest& request, CacheQueryOptions&& options, RecordIdentifiersCallback&& callback)
{
    uint64_t requestIdentifier = ++m_lastRequestIdentifier;
    m_batchDeleteAndPutPendingRequests.add(requestIdentifier, WTFMove(callback));

    doBatchDeleteOperation(requestIdentifier, cacheIdentifier, request, WTFMove(options));
}

static inline uint64_t computeRealBodySize(const DOMCacheEngine::ResponseBody& body)
{
    uint64_t result = 0;
    WTF::switchOn(body, [&] (const Ref<WebCore::FormData>& formData) {
        result = formData->lengthInBytes();
    }, [&] (const Ref<WebCore::SharedBuffer>& buffer) {
        result = buffer->size();
    }, [] (const std::nullptr_t&) {
    });
    return result;
}

uint64_t CacheStorageConnection::computeRecordBodySize(const FetchResponse& response, const DOMCacheEngine::ResponseBody& body, ResourceResponse::Tainting tainting)
{
    if (!response.opaqueLoadIdentifier()) {
        ASSERT_UNUSED(tainting, tainting != ResourceResponse::Tainting::Opaque);
        return computeRealBodySize(body);
    }

    return m_opaqueResponseToSizeWithPaddingMap.ensure(response.opaqueLoadIdentifier(), [&] () {
        uint64_t realSize = computeRealBodySize(body);

        // Padding the size as per https://github.com/whatwg/storage/issues/31.
        uint64_t sizeWithPadding = realSize + static_cast<uint64_t>(randomNumber() * 128000);
        sizeWithPadding = ((sizeWithPadding / 32000) + 1) * 32000;

        m_opaqueResponseToSizeWithPaddingMap.set(response.opaqueLoadIdentifier(), sizeWithPadding);
        return sizeWithPadding;
    }).iterator->value;
}

void CacheStorageConnection::batchPutOperation(uint64_t cacheIdentifier, Vector<Record>&& records, RecordIdentifiersCallback&& callback)
{
    uint64_t requestIdentifier = ++m_lastRequestIdentifier;
    m_batchDeleteAndPutPendingRequests.add(requestIdentifier, WTFMove(callback));

    doBatchPutOperation(requestIdentifier, cacheIdentifier, WTFMove(records));
}

void CacheStorageConnection::openOrRemoveCompleted(uint64_t requestIdentifier, const CacheIdentifierOrError& result)
{
    if (auto callback = m_openAndRemoveCachePendingRequests.take(requestIdentifier))
        callback(result);
}

void CacheStorageConnection::updateCaches(uint64_t requestIdentifier, CacheInfosOrError&& result)
{
    if (auto callback = m_retrieveCachesPendingRequests.take(requestIdentifier))
        callback(WTFMove(result));
}

void CacheStorageConnection::updateRecords(uint64_t requestIdentifier, RecordsOrError&& result)
{
    if (auto callback = m_retrieveRecordsPendingRequests.take(requestIdentifier))
        callback(WTFMove(result));
}

void CacheStorageConnection::deleteRecordsCompleted(uint64_t requestIdentifier, Expected<Vector<uint64_t>, Error>&& result)
{
    if (auto callback = m_batchDeleteAndPutPendingRequests.take(requestIdentifier))
        callback(WTFMove(result));
}

void CacheStorageConnection::putRecordsCompleted(uint64_t requestIdentifier, Expected<Vector<uint64_t>, Error>&& result)
{
    if (auto callback = m_batchDeleteAndPutPendingRequests.take(requestIdentifier))
        callback(WTFMove(result));
}

} // namespace WebCore
