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
#include <wtf/HashMap.h>

namespace IPC {
class Connection;
class Decoder;
class Encoder;
}

namespace WebKit {

class WebCacheStorageProvider;

class WebCacheStorageConnection final : public WebCore::CacheStorageConnection {
public:
    static Ref<WebCacheStorageConnection> create(WebCacheStorageProvider& provider, PAL::SessionID sessionID) { return adoptRef(*new WebCacheStorageConnection(provider, sessionID)); }

    ~WebCacheStorageConnection();

    void didReceiveMessage(IPC::Connection&, IPC::Decoder&);

private:
    WebCacheStorageConnection(WebCacheStorageProvider&, PAL::SessionID);

    IPC::Connection& connection();

    // WebCore::CacheStorageConnection
    void doOpen(uint64_t requestIdentifier, const WebCore::ClientOrigin&, const String& cacheName) final;
    void doRemove(uint64_t requestIdentifier, uint64_t cacheIdentifier) final;
    void doRetrieveCaches(uint64_t requestIdentifier, const WebCore::ClientOrigin&, uint64_t updateCounter) final;

    void doRetrieveRecords(uint64_t requestIdentifier, uint64_t cacheIdentifier, const URL&) final;
    void doBatchDeleteOperation(uint64_t requestIdentifier, uint64_t cacheIdentifier, const WebCore::ResourceRequest&, WebCore::CacheQueryOptions&&) final;
    void doBatchPutOperation(uint64_t requestIdentifier, uint64_t cacheIdentifier, Vector<WebCore::DOMCacheEngine::Record>&&) final;

    void reference(uint64_t cacheIdentifier) final;
    void dereference(uint64_t cacheIdentifier) final;

    void clearMemoryRepresentation(const WebCore::ClientOrigin&, WebCore::DOMCacheEngine::CompletionCallback&&) final;
    void engineRepresentation(WTF::Function<void(const String&)>&&) final;

    void openCompleted(uint64_t requestIdentifier, const WebCore::DOMCacheEngine::CacheIdentifierOrError&);
    void removeCompleted(uint64_t requestIdentifier, const WebCore::DOMCacheEngine::CacheIdentifierOrError&);
    void updateCaches(uint64_t requestIdentifier, WebCore::DOMCacheEngine::CacheInfosOrError&&);

    void updateRecords(uint64_t requestIdentifier, WebCore::DOMCacheEngine::RecordsOrError&&);
    void deleteRecordsCompleted(uint64_t requestIdentifier, WebCore::DOMCacheEngine::RecordIdentifiersOrError&&);
    void putRecordsCompleted(uint64_t requestIdentifier, WebCore::DOMCacheEngine::RecordIdentifiersOrError&&);

    void engineRepresentationCompleted(uint64_t requestIdentifier, const String& representation);
    void clearMemoryRepresentationCompleted(uint64_t requestIdentifier, std::optional<WebCore::DOMCacheEngine::Error>&&);

    WebCacheStorageProvider& m_provider;
    PAL::SessionID m_sessionID;
    uint64_t m_engineRepresentationNextIdentifier { 0 };
    HashMap<uint64_t, WTF::Function<void(const String&)>> m_engineRepresentationCallbacks;
    HashMap<uint64_t, WebCore::DOMCacheEngine::CompletionCallback> m_clearRepresentationCallbacks;
};

}
