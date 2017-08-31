
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

#include "DOMCache.h"
#include <wtf/HashMap.h>
#include <wtf/ThreadSafeRefCounted.h>

namespace WebCore {

class CacheStorageConnection : public ThreadSafeRefCounted<CacheStorageConnection> {
public:
    static Ref<CacheStorageConnection> create() { return adoptRef(*new CacheStorageConnection()); }
    virtual ~CacheStorageConnection() = default;

    void open(const String& origin, const String& cacheName, DOMCache::CacheIdentifierCallback&&);
    void remove(uint64_t cacheIdentifier, DOMCache::CacheIdentifierCallback&&);
    void retrieveCaches(const String& origin, uint64_t updateCounter, DOMCache::CacheInfosCallback&&);

    void retrieveRecords(uint64_t cacheIdentifier, DOMCache::RecordsCallback&&);
    void batchDeleteOperation(uint64_t cacheIdentifier, const ResourceRequest&, CacheQueryOptions&&, DOMCache::RecordIdentifiersCallback&&);
    void batchPutOperation(uint64_t cacheIdentifier, Vector<DOMCache::Record>&&, DOMCache::RecordIdentifiersCallback&&);

    // Used only for testing purposes.
    virtual void clearMemoryRepresentation(const String& /* origin */, DOMCache::CompletionCallback&& callback) { callback(DOMCache::Error::NotImplemented); }

protected:
    CacheStorageConnection() =  default;

    void openCompleted(uint64_t identifier, const DOMCache::CacheIdentifierOrError& result) { openOrRemoveCompleted(identifier, result); }
    void removeCompleted(uint64_t identifier, const DOMCache::CacheIdentifierOrError& result) { openOrRemoveCompleted(identifier, result); }
    WEBCORE_EXPORT void updateCaches(uint64_t requestIdentifier, DOMCache::CacheInfosOrError&&);

    WEBCORE_EXPORT void updateRecords(uint64_t requestIdentifier, DOMCache::RecordsOrError&&);
    WEBCORE_EXPORT void deleteRecordsCompleted(uint64_t requestIdentifier, DOMCache::RecordIdentifiersOrError&&);
    WEBCORE_EXPORT void putRecordsCompleted(uint64_t requestIdentifier, DOMCache::RecordIdentifiersOrError&&);

private:
    virtual void doOpen(uint64_t requestIdentifier, const String& /* origin */, const String& /* cacheName */) { openCompleted(requestIdentifier, makeUnexpected(DOMCache::Error::NotImplemented)); }
    virtual void doRemove(uint64_t requestIdentifier, uint64_t /* cacheIdentifier */) { removeCompleted(requestIdentifier, makeUnexpected(DOMCache::Error::NotImplemented)); }
    virtual void doRetrieveCaches(uint64_t requestIdentifier, const String& /* origin */, uint64_t /* updateCounter */) { updateCaches(requestIdentifier, { }); }

    virtual void doRetrieveRecords(uint64_t requestIdentifier, uint64_t /* cacheIdentifier */) { updateRecords(requestIdentifier, { }); }
    virtual void doBatchDeleteOperation(uint64_t requestIdentifier, uint64_t /* cacheIdentifier */, const ResourceRequest&, CacheQueryOptions&&) { deleteRecordsCompleted(requestIdentifier, makeUnexpected(DOMCache::Error::NotImplemented)); }
    virtual void doBatchPutOperation(uint64_t requestIdentifier, uint64_t /* cacheIdentifier */, Vector<DOMCache::Record>&&) { putRecordsCompleted(requestIdentifier, makeUnexpected(DOMCache::Error::NotImplemented)); }

    WEBCORE_EXPORT void openOrRemoveCompleted(uint64_t requestIdentifier, const DOMCache::CacheIdentifierOrError&);

    HashMap<uint64_t, DOMCache::CacheIdentifierCallback> m_openAndRemoveCachePendingRequests;
    HashMap<uint64_t, DOMCache::CacheInfosCallback> m_retrieveCachesPendingRequests;
    HashMap<uint64_t, DOMCache::RecordsCallback> m_retrieveRecordsPendingRequests;
    HashMap<uint64_t, DOMCache::RecordIdentifiersCallback> m_batchDeleteAndPutPendingRequests;

    uint64_t m_lastRequestIdentifier { 0 };
};
} // namespace WebCore
