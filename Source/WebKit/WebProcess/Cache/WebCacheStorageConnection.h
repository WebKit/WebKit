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
    void open(const WebCore::ClientOrigin&, const String& cacheName, WebCore::DOMCacheEngine::CacheIdentifierCallback&&) final;
    void remove(uint64_t cacheIdentifier, WebCore::DOMCacheEngine::CacheIdentifierCallback&&) final;
    void retrieveCaches(const WebCore::ClientOrigin&, uint64_t updateCounter, WebCore::DOMCacheEngine::CacheInfosCallback&&) final;

    void retrieveRecords(uint64_t cacheIdentifier, const URL&, WebCore::DOMCacheEngine::RecordsCallback&&) final;
    void batchDeleteOperation(uint64_t cacheIdentifier, const WebCore::ResourceRequest&, WebCore::CacheQueryOptions&&, WebCore::DOMCacheEngine::RecordIdentifiersCallback&&) final;
    void batchPutOperation(uint64_t cacheIdentifier, Vector<WebCore::DOMCacheEngine::Record>&&, WebCore::DOMCacheEngine::RecordIdentifiersCallback&&) final;

    void reference(uint64_t cacheIdentifier) final;
    void dereference(uint64_t cacheIdentifier) final;

    void clearMemoryRepresentation(const WebCore::ClientOrigin&, WebCore::DOMCacheEngine::CompletionCallback&&) final;
    void engineRepresentation(CompletionHandler<void(const String&)>&&) final;
    void updateQuotaBasedOnSpaceUsage(const WebCore::ClientOrigin&) final;

    PAL::SessionID sessionID() const final { return m_sessionID; }

    WebCacheStorageProvider& m_provider;
    PAL::SessionID m_sessionID;
};

}
