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

#include <WebCore/CacheStorageConnection.h>
#include <WebCore/ClientOrigin.h>
#include <wtf/HashCountedSet.h>
#include <wtf/Lock.h>

namespace IPC {
class Connection;
class Decoder;
class Encoder;
}

namespace WebKit {

class WebCacheStorageProvider;

class WebCacheStorageConnection final : public WebCore::CacheStorageConnection {
public:
    static Ref<WebCacheStorageConnection> create() { return adoptRef(*new WebCacheStorageConnection); }

    ~WebCacheStorageConnection();

    void didReceiveMessage(IPC::Connection&, IPC::Decoder&);
    void networkProcessConnectionClosed();

private:
    WebCacheStorageConnection();

    struct PromiseConverter;

    // WebCore::CacheStorageConnection
    Ref<OpenPromise> open(const WebCore::ClientOrigin&, const String& cacheName) final;
    Ref<RemovePromise> remove(WebCore::DOMCacheIdentifier) final;
    Ref<RetrieveCachesPromise> retrieveCaches(const WebCore::ClientOrigin&, uint64_t)  final;
    Ref<RetrieveRecordsPromise> retrieveRecords(WebCore::DOMCacheIdentifier, WebCore::RetrieveRecordsOptions&&)  final;
    Ref<BatchPromise> batchDeleteOperation(WebCore::DOMCacheIdentifier, const WebCore::ResourceRequest&, WebCore::CacheQueryOptions&&)  final;
    Ref<BatchPromise> batchPutOperation(WebCore::DOMCacheIdentifier, Vector<WebCore::DOMCacheEngine::CrossThreadRecord>&&)  final;
    void reference(WebCore::DOMCacheIdentifier) final;
    void dereference(WebCore::DOMCacheIdentifier) final;
    void lockStorage(const WebCore::ClientOrigin&) final;
    void unlockStorage(const WebCore::ClientOrigin&) final;
    Ref<CompletionPromise> clearMemoryRepresentation(const WebCore::ClientOrigin&) final;
    Ref<EngineRepresentationPromise> engineRepresentation() final;
    void updateQuotaBasedOnSpaceUsage(const WebCore::ClientOrigin&) final;

    Ref<IPC::Connection> connection();

    Lock m_connectionLock;
    RefPtr<IPC::Connection> m_connection WTF_GUARDED_BY_LOCK(m_connectionLock);
    HashCountedSet<WebCore::DOMCacheIdentifier> m_connectedIdentifierCounters WTF_GUARDED_BY_LOCK(m_connectionLock);
    HashCountedSet<WebCore::ClientOrigin> m_clientOriginLockRequestCounters WTF_GUARDED_BY_LOCK(m_connectionLock);
};

}
