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
#include <wtf/NativePromise.h>
#include <wtf/RefCounted.h>

namespace WebCore {

class CacheStorageProvider : public RefCounted<CacheStorageProvider> {
public:
    class DummyCacheStorageConnection final : public WebCore::CacheStorageConnection {
    public:
        static Ref<DummyCacheStorageConnection> create() { return adoptRef(*new DummyCacheStorageConnection()); }

    private:
        DummyCacheStorageConnection()
        {
        }

        Ref<OpenPromise> open(const ClientOrigin&, const String&) final { return OpenPromise::createAndReject(DOMCacheEngine::Error::Stopped); }
        void remove(DOMCacheIdentifier, DOMCacheEngine::RemoveCacheIdentifierCallback&&) final { }
        void retrieveCaches(const ClientOrigin&, uint64_t, DOMCacheEngine::CacheInfosCallback&&) final { }
        void retrieveRecords(DOMCacheIdentifier, RetrieveRecordsOptions&&, DOMCacheEngine::CrossThreadRecordsCallback&&) final { }
        void batchDeleteOperation(DOMCacheIdentifier, const ResourceRequest&, CacheQueryOptions&&, DOMCacheEngine::RecordIdentifiersCallback&&) final { }
        void batchPutOperation(DOMCacheIdentifier, Vector<DOMCacheEngine::CrossThreadRecord>&&, DOMCacheEngine::RecordIdentifiersCallback&&) final { }
        void reference(DOMCacheIdentifier) final { }
        void dereference(DOMCacheIdentifier) final { }
        void lockStorage(const ClientOrigin&) final { }
        void unlockStorage(const ClientOrigin&) final { }
    };

    static Ref<CacheStorageProvider> create() { return adoptRef(*new CacheStorageProvider); }
    virtual Ref<CacheStorageConnection> createCacheStorageConnection() { return DummyCacheStorageConnection::create(); }
    virtual ~CacheStorageProvider() { };

protected:
    CacheStorageProvider() = default;
};

}
