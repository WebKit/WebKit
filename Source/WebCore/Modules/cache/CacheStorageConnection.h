
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

    WEBCORE_EXPORT static bool queryCacheMatch(const ResourceRequest& request, const ResourceRequest& cachedRequest, const ResourceResponse&, const CacheQueryOptions&);

    static Exception exceptionFromError(Error);

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
    using CacheMapCallback = WTF::Function<void(Vector<CacheInfo>&&)>;
    using RecordsCallback = WTF::Function<void(Vector<Record>&&)>;
    using BatchOperationCallback = WTF::Function<void(Vector<uint64_t>&&, Error)>;

    virtual void open(const String& /* origin */, const String& /* cacheName */, OpenRemoveCallback&& callback) { callback(0, Error::NotImplemented); }
    virtual void remove(uint64_t /* cacheIdentifier */, OpenRemoveCallback&& callback) { callback(0, Error::NotImplemented); }
    virtual void retrieveCaches(const String& /* origin */, CacheMapCallback&& callback) { callback({ }); }

    virtual void retrieveRecords(uint64_t /* cacheIdentifier */, RecordsCallback&& callback) { callback({ }); }
    virtual void batchDeleteOperation(uint64_t /* cacheIdentifier */, const ResourceRequest&, CacheQueryOptions&&, BatchOperationCallback&& callback) { callback({ }, Error::NotImplemented); }
    virtual void batchPutOperation(uint64_t /* cacheIdentifier */, Vector<Record>&&, BatchOperationCallback&& callback) { callback({ }, Error::NotImplemented); }

protected:
    CacheStorageConnection() =  default;
};

} // namespace WebCore

