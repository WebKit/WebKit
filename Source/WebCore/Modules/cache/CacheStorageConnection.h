
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

#include "FetchHeaders.h"
#include "FetchOptions.h"
#include "ResourceRequest.h"
#include "ResourceResponse.h"
#include <wtf/HashMap.h>
#include <wtf/ThreadSafeRefCounted.h>

namespace WebCore {

struct CacheQueryOptions;

class CacheStorageConnection : public ThreadSafeRefCounted<CacheStorageConnection> {
public:
    static Ref<CacheStorageConnection> create() { return adoptRef(*new CacheStorageConnection()); }
    virtual ~CacheStorageConnection() = default;

    enum class Error {
        None,
        NotImplemented,
    };

    static ExceptionOr<void> errorToException(Error);
    template<typename T> static ExceptionOr<T> exceptionOrResult(T&& value, Error error)
    {
        auto result = errorToException(error);
        if (result.hasException())
            return result.releaseException();
        return std::forward<T>(value);
    }

    WEBCORE_EXPORT static bool queryCacheMatch(const ResourceRequest& request, const ResourceRequest& cachedRequest, const ResourceResponse&, const CacheQueryOptions&);

    struct Record {
        uint64_t identifier;

        FetchHeaders::Guard requestHeadersGuard;
        ResourceRequest request;
        FetchOptions options;
        String referrer;

        FetchHeaders::Guard responseHeadersGuard;
        ResourceResponse response;
    };

    struct CacheInfo {
        uint64_t identifier;
        String name;
    };

    using OpenRemoveCallback = WTF::Function<void(uint64_t, Error)>;
    using CachesCallback = WTF::Function<void(Vector<CacheInfo>&&)>;
    using RecordsCallback = WTF::Function<void(Vector<Record>&&)>;
    using BatchOperationCallback = WTF::Function<void(Vector<uint64_t>&&, Error)>;

    void open(const String& /* origin */, const String& /* cacheName */, OpenRemoveCallback&&);
    void remove(uint64_t /* cacheIdentifier */, OpenRemoveCallback&&);
    void retrieveCaches(const String& /* origin */, CachesCallback&&);

    void retrieveRecords(uint64_t /* cacheIdentifier */, RecordsCallback&&);
    void batchDeleteOperation(uint64_t /* cacheIdentifier */, const ResourceRequest&, CacheQueryOptions&&, BatchOperationCallback&&);
    void batchPutOperation(uint64_t /* cacheIdentifier */, Vector<Record>&&, BatchOperationCallback&&);

protected:
    CacheStorageConnection() =  default;

    void openCompleted(uint64_t identifier, uint64_t cacheIdentifier, Error error) { openOrRemoveCompleted(identifier, cacheIdentifier, error); }
    void removeCompleted(uint64_t identifier, uint64_t cacheIdentifier, Error error) { openOrRemoveCompleted(identifier, cacheIdentifier, error); }
    WEBCORE_EXPORT void updateCaches(uint64_t requestIdentifier, Vector<CacheInfo>&&);

    WEBCORE_EXPORT void updateRecords(uint64_t requestIdentifier, Vector<Record>&&);
    WEBCORE_EXPORT void deleteRecordsCompleted(uint64_t requestIdentifier, Vector<uint64_t>&&, Error);
    WEBCORE_EXPORT void putRecordsCompleted(uint64_t requestIdentifier, Vector<uint64_t>&&, Error);

private:
    virtual void doOpen(uint64_t requestIdentifier, const String& /* origin */, const String& /* cacheName */) { openCompleted(requestIdentifier, 0, Error::NotImplemented); }
    virtual void doRemove(uint64_t requestIdentifier, uint64_t /* cacheIdentifier */) { removeCompleted(requestIdentifier, 0, Error::NotImplemented); }
    virtual void doRetrieveCaches(uint64_t requestIdentifier, const String& /* origin */) { updateCaches(requestIdentifier, { }); }

    virtual void doRetrieveRecords(uint64_t requestIdentifier, uint64_t /* cacheIdentifier */) { updateRecords(requestIdentifier, { }); }
    virtual void doBatchDeleteOperation(uint64_t requestIdentifier, uint64_t /* cacheIdentifier */, const ResourceRequest&, CacheQueryOptions&&) { deleteRecordsCompleted(requestIdentifier, { }, Error::NotImplemented); }
    virtual void doBatchPutOperation(uint64_t requestIdentifier, uint64_t /* cacheIdentifier */, Vector<Record>&&) { putRecordsCompleted(requestIdentifier, { }, Error::NotImplemented); }

    WEBCORE_EXPORT void openOrRemoveCompleted(uint64_t requestIdentifier, uint64_t cacheIdentifier, Error);

    HashMap<uint64_t, OpenRemoveCallback> m_openAndRemoveCachePendingRequests;
    HashMap<uint64_t, CachesCallback> m_retrieveCachesPendingRequests;
    HashMap<uint64_t, RecordsCallback> m_retrieveRecordsPendingRequests;
    HashMap<uint64_t, BatchOperationCallback> m_batchDeleteAndPutPendingRequests;

    uint64_t m_lastRequestIdentifier { 0 };
};

} // namespace WebCore

