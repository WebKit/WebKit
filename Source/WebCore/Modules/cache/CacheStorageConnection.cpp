
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

#include "CacheQueryOptions.h"
#include "Exception.h"
#include "HTTPParsers.h"

namespace WebCore {

ExceptionOr<void> CacheStorageConnection::errorToException(Error error)
{
    switch (error) {
    case Error::None:
        return { };
    case Error::NotImplemented:
        return Exception { NotSupportedError, ASCIILiteral("Not implemented") };
    default:
        return Exception { NotSupportedError, ASCIILiteral("Internal error") };
    }
}

bool CacheStorageConnection::queryCacheMatch(const ResourceRequest& request, const ResourceRequest& cachedRequest, const ResourceResponse& cachedResponse, const CacheQueryOptions& options)
{
    ASSERT(options.ignoreMethod || request.httpMethod() == "GET");

    URL requestURL = request.url();
    URL cachedRequestURL = cachedRequest.url();

    if (options.ignoreSearch) {
        requestURL.setQuery({ });
        cachedRequestURL.setQuery({ });
    }
    if (!equalIgnoringFragmentIdentifier(requestURL, cachedRequestURL))
        return false;

    if (options.ignoreVary)
        return true;

    String varyValue = cachedResponse.httpHeaderField(WebCore::HTTPHeaderName::Vary);
    if (varyValue.isNull())
        return true;

    // FIXME: This is inefficient, we should be able to split and trim whitespaces at the same time.
    Vector<String> varyHeaderNames;
    varyValue.split(',', false, varyHeaderNames);
    for (auto& name : varyHeaderNames) {
        if (stripLeadingAndTrailingHTTPSpaces(name) == "*")
            return false;
        if (cachedRequest.httpHeaderField(name) != request.httpHeaderField(name))
            return false;
    }
    return true;
}

void CacheStorageConnection::open(const String& origin, const String& cacheName, OpenRemoveCallback&& callback)
{
    uint64_t requestIdentifier = ++m_lastRequestIdentifier;
    m_openAndRemoveCachePendingRequests.add(requestIdentifier, WTFMove(callback));

    doOpen(requestIdentifier, origin, cacheName);
}

void CacheStorageConnection::remove(uint64_t cacheIdentifier, OpenRemoveCallback&& callback)
{
    uint64_t requestIdentifier = ++m_lastRequestIdentifier;
    m_openAndRemoveCachePendingRequests.add(requestIdentifier, WTFMove(callback));

    doRemove(requestIdentifier, cacheIdentifier);
}

void CacheStorageConnection::retrieveCaches(const String& origin, CachesCallback&& callback)
{
    uint64_t requestIdentifier = ++m_lastRequestIdentifier;
    m_retrieveCachesPendingRequests.add(requestIdentifier, WTFMove(callback));

    doRetrieveCaches(requestIdentifier, origin);
}

void CacheStorageConnection::retrieveRecords(uint64_t cacheIdentifier, RecordsCallback&& callback)
{
    uint64_t requestIdentifier = ++m_lastRequestIdentifier;
    m_retrieveRecordsPendingRequests.add(requestIdentifier, WTFMove(callback));

    doRetrieveRecords(requestIdentifier, cacheIdentifier);
}

void CacheStorageConnection::batchDeleteOperation(uint64_t cacheIdentifier, const WebCore::ResourceRequest& request, WebCore::CacheQueryOptions&& options, BatchOperationCallback&& callback)
{
    uint64_t requestIdentifier = ++m_lastRequestIdentifier;
    m_batchDeleteAndPutPendingRequests.add(requestIdentifier, WTFMove(callback));

    doBatchDeleteOperation(requestIdentifier, cacheIdentifier, request, WTFMove(options));
}

void CacheStorageConnection::batchPutOperation(uint64_t cacheIdentifier, Vector<WebCore::CacheStorageConnection::Record>&& records, BatchOperationCallback&& callback)
{
    uint64_t requestIdentifier = ++m_lastRequestIdentifier;
    m_batchDeleteAndPutPendingRequests.add(requestIdentifier, WTFMove(callback));

    doBatchPutOperation(requestIdentifier, cacheIdentifier, WTFMove(records));
}

void CacheStorageConnection::openOrRemoveCompleted(uint64_t requestIdentifier, uint64_t cacheIdentifier, Error error)
{
    if (auto callback = m_openAndRemoveCachePendingRequests.take(requestIdentifier))
        callback(cacheIdentifier, error);
}

void CacheStorageConnection::updateCaches(uint64_t requestIdentifier, Vector<CacheInfo>&& caches)
{
    if (auto callback = m_retrieveCachesPendingRequests.take(requestIdentifier))
        callback(WTFMove(caches));
}

void CacheStorageConnection::updateRecords(uint64_t requestIdentifier, Vector<Record>&& records)
{
    if (auto callback = m_retrieveRecordsPendingRequests.take(requestIdentifier))
        callback(WTFMove(records));
}

void CacheStorageConnection::deleteRecordsCompleted(uint64_t requestIdentifier, Vector<uint64_t>&& records, Error error)
{
    if (auto callback = m_batchDeleteAndPutPendingRequests.take(requestIdentifier))
        callback(WTFMove(records), error);
}

void CacheStorageConnection::putRecordsCompleted(uint64_t requestIdentifier, Vector<uint64_t>&& records, Error error)
{
    if (auto callback = m_batchDeleteAndPutPendingRequests.take(requestIdentifier))
        callback(WTFMove(records), error);
}

} // namespace WebCore
