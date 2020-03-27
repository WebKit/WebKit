
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

#pragma once

#include "CacheStorageConnection.h"

namespace WebCore {

class WorkerGlobalScope;
class WorkerLoaderProxy;

class WorkerCacheStorageConnection final : public CacheStorageConnection {
public:
    static Ref<WorkerCacheStorageConnection> create(WorkerGlobalScope&);
    ~WorkerCacheStorageConnection();

    void clearPendingRequests();

private:
    explicit WorkerCacheStorageConnection(WorkerGlobalScope&);

    // WebCore::CacheStorageConnection.
    void open(const ClientOrigin&, const String& cacheName, DOMCacheEngine::CacheIdentifierCallback&&) final;
    void remove(uint64_t cacheIdentifier, DOMCacheEngine::CacheIdentifierCallback&&) final;
    void retrieveCaches(const ClientOrigin&, uint64_t updateCounter, DOMCacheEngine::CacheInfosCallback&&) final;

    void retrieveRecords(uint64_t cacheIdentifier, const RetrieveRecordsOptions&, DOMCacheEngine::RecordsCallback&&) final;
    void batchDeleteOperation(uint64_t cacheIdentifier, const ResourceRequest&, CacheQueryOptions&&, DOMCacheEngine::RecordIdentifiersCallback&&) final;
    void batchPutOperation(uint64_t cacheIdentifier, Vector<DOMCacheEngine::Record>&&, DOMCacheEngine::RecordIdentifiersCallback&&) final;

    void reference(uint64_t cacheIdentifier) final;
    void dereference(uint64_t cacheIdentifier) final;

    void doOpen(uint64_t requestIdentifier, const ClientOrigin&, const String& cacheName);
    void doRemove(uint64_t requestIdentifier, uint64_t cacheIdentifier);
    void doRetrieveCaches(uint64_t requestIdentifier, const ClientOrigin&, uint64_t updateCounter);
    void doRetrieveRecords(uint64_t requestIdentifier, uint64_t cacheIdentifier, const URL&);
    void doBatchDeleteOperation(uint64_t requestIdentifier, uint64_t cacheIdentifier, const WebCore::ResourceRequest&, WebCore::CacheQueryOptions&&);
    void doBatchPutOperation(uint64_t requestIdentifier, uint64_t cacheIdentifier, Vector<DOMCacheEngine::Record>&&);

    void openOrRemoveCompleted(uint64_t requestIdentifier, const DOMCacheEngine::CacheIdentifierOrError&);
    void retrieveCachesCompleted(uint64_t requestIdentifier, DOMCacheEngine::CacheInfosOrError&&);
    void retrieveRecordsCompleted(uint64_t requestIdentifier, DOMCacheEngine::RecordsOrError&&);
    void deleteRecordsCompleted(uint64_t requestIdentifier, DOMCacheEngine::RecordIdentifiersOrError&&);
    void putRecordsCompleted(uint64_t requestIdentifier, DOMCacheEngine::RecordIdentifiersOrError&&);

    WorkerGlobalScope& m_scope;

    RefPtr<CacheStorageConnection> m_mainThreadConnection;

    HashMap<uint64_t, DOMCacheEngine::CacheIdentifierCallback> m_openAndRemoveCachePendingRequests;
    HashMap<uint64_t, DOMCacheEngine::CacheInfosCallback> m_retrieveCachesPendingRequests;
    HashMap<uint64_t, DOMCacheEngine::RecordsCallback> m_retrieveRecordsPendingRequests;
    HashMap<uint64_t, DOMCacheEngine::RecordIdentifiersCallback> m_batchDeleteAndPutPendingRequests;

    uint64_t m_lastRequestIdentifier { 0 };
};

} // namespace WebCore
