
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

private:
    explicit WorkerCacheStorageConnection(WorkerGlobalScope&);

    // WebCore::CacheStorageConnection.
    void doOpen(uint64_t requestIdentifier, const ClientOrigin&, const String& cacheName) final;
    void doRemove(uint64_t requestIdentifier, uint64_t cacheIdentifier) final;
    void doRetrieveCaches(uint64_t requestIdentifier, const ClientOrigin&, uint64_t updateCounter) final;

    void doRetrieveRecords(uint64_t requestIdentifier, uint64_t cacheIdentifier, const URL&) final;

    void reference(uint64_t cacheIdentifier) final;
    void dereference(uint64_t cacheIdentifier) final;

    void doBatchDeleteOperation(uint64_t requestIdentifier, uint64_t cacheIdentifier, const WebCore::ResourceRequest&, WebCore::CacheQueryOptions&&) final;
    void doBatchPutOperation(uint64_t requestIdentifier, uint64_t cacheIdentifier, Vector<DOMCacheEngine::Record>&&) final;

    WorkerGlobalScope& m_scope;

    RefPtr<CacheStorageConnection> m_mainThreadConnection;
};

} // namespace WebCore
