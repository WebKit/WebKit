
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

#include "DOMCacheEngine.h"
#include <wtf/HashMap.h>
#include <wtf/ThreadSafeRefCounted.h>

namespace WebCore {

struct ClientOrigin;
class FetchResponse;

class CacheStorageConnection : public ThreadSafeRefCounted<CacheStorageConnection> {
public:
    static Ref<CacheStorageConnection> create() { return adoptRef(*new CacheStorageConnection()); }
    virtual ~CacheStorageConnection() = default;

    void open(const ClientOrigin&, const String& cacheName, DOMCacheEngine::CacheIdentifierCallback&&);
    void remove(uint64_t cacheIdentifier, DOMCacheEngine::CacheIdentifierCallback&&);
    void retrieveCaches(const ClientOrigin&, uint64_t updateCounter, DOMCacheEngine::CacheInfosCallback&&);

    void retrieveRecords(uint64_t cacheIdentifier, const URL&, DOMCacheEngine::RecordsCallback&&);
    void batchDeleteOperation(uint64_t cacheIdentifier, const ResourceRequest&, CacheQueryOptions&&, DOMCacheEngine::RecordIdentifiersCallback&&);
    void batchPutOperation(uint64_t cacheIdentifier, Vector<DOMCacheEngine::Record>&&, DOMCacheEngine::RecordIdentifiersCallback&&);
    uint64_t computeRecordBodySize(const FetchResponse&, const DOMCacheEngine::ResponseBody&, ResourceResponse::Tainting);

    virtual void reference(uint64_t /* cacheIdentifier */) { }
    virtual void dereference(uint64_t /* cacheIdentifier */) { }

    // Used only for testing purposes.
    virtual void clearMemoryRepresentation(const ClientOrigin&, DOMCacheEngine::CompletionCallback&& callback) { callback(DOMCacheEngine::Error::NotImplemented); }
    virtual void engineRepresentation(WTF::Function<void(const String&)>&& callback) { callback(String { }); }

protected:
    CacheStorageConnection() =  default;

    void openCompleted(uint64_t identifier, const DOMCacheEngine::CacheIdentifierOrError& result) { openOrRemoveCompleted(identifier, result); }
    void removeCompleted(uint64_t identifier, const DOMCacheEngine::CacheIdentifierOrError& result) { openOrRemoveCompleted(identifier, result); }
    WEBCORE_EXPORT void updateCaches(uint64_t requestIdentifier, DOMCacheEngine::CacheInfosOrError&&);

    WEBCORE_EXPORT void updateRecords(uint64_t requestIdentifier, DOMCacheEngine::RecordsOrError&&);
    WEBCORE_EXPORT void deleteRecordsCompleted(uint64_t requestIdentifier, DOMCacheEngine::RecordIdentifiersOrError&&);
    WEBCORE_EXPORT void putRecordsCompleted(uint64_t requestIdentifier, DOMCacheEngine::RecordIdentifiersOrError&&);

private:
    virtual void doOpen(uint64_t requestIdentifier, const ClientOrigin&, const String& /* cacheName */) { openCompleted(requestIdentifier, makeUnexpected(DOMCacheEngine::Error::NotImplemented)); }
    virtual void doRemove(uint64_t requestIdentifier, uint64_t /* cacheIdentifier */) { removeCompleted(requestIdentifier, makeUnexpected(DOMCacheEngine::Error::NotImplemented)); }
    virtual void doRetrieveCaches(uint64_t requestIdentifier, const ClientOrigin&, uint64_t /* updateCounter */) { updateCaches(requestIdentifier, { }); }

    virtual void doRetrieveRecords(uint64_t requestIdentifier, uint64_t /* cacheIdentifier */, const URL& /* url */) { updateRecords(requestIdentifier, { }); }
    virtual void doBatchDeleteOperation(uint64_t requestIdentifier, uint64_t /* cacheIdentifier */, const ResourceRequest&, CacheQueryOptions&&) { deleteRecordsCompleted(requestIdentifier, makeUnexpected(DOMCacheEngine::Error::NotImplemented)); }
    virtual void doBatchPutOperation(uint64_t requestIdentifier, uint64_t /* cacheIdentifier */, Vector<DOMCacheEngine::Record>&&) { putRecordsCompleted(requestIdentifier, makeUnexpected(DOMCacheEngine::Error::NotImplemented)); }

    WEBCORE_EXPORT void openOrRemoveCompleted(uint64_t requestIdentifier, const DOMCacheEngine::CacheIdentifierOrError&);

    HashMap<uint64_t, DOMCacheEngine::CacheIdentifierCallback> m_openAndRemoveCachePendingRequests;
    HashMap<uint64_t, DOMCacheEngine::CacheInfosCallback> m_retrieveCachesPendingRequests;
    HashMap<uint64_t, DOMCacheEngine::RecordsCallback> m_retrieveRecordsPendingRequests;
    HashMap<uint64_t, DOMCacheEngine::RecordIdentifiersCallback> m_batchDeleteAndPutPendingRequests;
    HashMap<uint64_t, uint64_t> m_opaqueResponseToSizeWithPaddingMap;

    uint64_t m_lastRequestIdentifier { 0 };
};
} // namespace WebCore
