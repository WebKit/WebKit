
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
#include "RetrieveRecordsOptions.h"
#include <wtf/HashMap.h>
#include <wtf/ThreadSafeRefCounted.h>

namespace WebCore {

struct ClientOrigin;
class FetchResponse;

class CacheStorageConnection : public ThreadSafeRefCounted<CacheStorageConnection> {
public:
    virtual ~CacheStorageConnection() = default;

    virtual void open(const ClientOrigin&, const String& cacheName, DOMCacheEngine::CacheIdentifierCallback&&) = 0;
    virtual void remove(uint64_t cacheIdentifier, DOMCacheEngine::CacheIdentifierCallback&&) = 0;
    virtual void retrieveCaches(const ClientOrigin&, uint64_t updateCounter, DOMCacheEngine::CacheInfosCallback&&) = 0;

    virtual void retrieveRecords(uint64_t cacheIdentifier, const RetrieveRecordsOptions&, DOMCacheEngine::RecordsCallback&&) = 0;
    virtual void batchDeleteOperation(uint64_t cacheIdentifier, const ResourceRequest&, CacheQueryOptions&&, DOMCacheEngine::RecordIdentifiersCallback&&) = 0;
    virtual void batchPutOperation(uint64_t cacheIdentifier, Vector<DOMCacheEngine::Record>&&, DOMCacheEngine::RecordIdentifiersCallback&&) = 0;

    virtual void reference(uint64_t /* cacheIdentifier */) = 0;
    virtual void dereference(uint64_t /* cacheIdentifier */) = 0;

    uint64_t computeRecordBodySize(const FetchResponse&, const DOMCacheEngine::ResponseBody&);

    // Used only for testing purposes.
    virtual void clearMemoryRepresentation(const ClientOrigin&, DOMCacheEngine::CompletionCallback&& callback) { callback(DOMCacheEngine::Error::NotImplemented); }
    virtual void engineRepresentation(CompletionHandler<void(const String&)>&& callback) { callback(String { }); }
    virtual void updateQuotaBasedOnSpaceUsage(const ClientOrigin&) { }

private:
    uint64_t computeRealBodySize(const DOMCacheEngine::ResponseBody&);

protected:
    HashMap<uint64_t, uint64_t> m_opaqueResponseToSizeWithPaddingMap;
};

} // namespace WebCore
