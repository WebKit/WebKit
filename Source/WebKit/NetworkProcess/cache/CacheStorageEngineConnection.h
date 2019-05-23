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

#include "ArgumentCoders.h"
#include "CacheStorageEngine.h"
#include "Connection.h"
#include <WebCore/CacheStorageConnection.h>
#include <pal/SessionID.h>
#include <wtf/Forward.h>
#include <wtf/RefCounted.h>

namespace IPC {

template<> struct AsyncReplyError<WebCore::DOMCacheEngine::CacheIdentifierOrError> {
    static WebCore::DOMCacheEngine::CacheIdentifierOrError create() { return makeUnexpected(WebCore::DOMCacheEngine::Error::Internal); };
};
template<> struct AsyncReplyError<WebCore::DOMCacheEngine::RecordIdentifiersOrError> {
    static WebCore::DOMCacheEngine::RecordIdentifiersOrError create() { return makeUnexpected(WebCore::DOMCacheEngine::Error::Internal); };
};
template<> struct AsyncReplyError<WebCore::DOMCacheEngine::CacheInfosOrError> {
    static WebCore::DOMCacheEngine::CacheInfosOrError create() { return makeUnexpected(WebCore::DOMCacheEngine::Error::Internal); };
};
template<> struct AsyncReplyError<WebCore::DOMCacheEngine::RecordsOrError> {
    static WebCore::DOMCacheEngine::RecordsOrError create() { return makeUnexpected(WebCore::DOMCacheEngine::Error::Internal); };
};

}

namespace WebKit {

class NetworkConnectionToWebProcess;

class CacheStorageEngineConnection : public RefCounted<CacheStorageEngineConnection> {
public:
    static Ref<CacheStorageEngineConnection> create(NetworkConnectionToWebProcess& connection) { return adoptRef(*new CacheStorageEngineConnection(connection)); }
    ~CacheStorageEngineConnection();
    void didReceiveMessage(IPC::Connection&, IPC::Decoder&);

private:
    explicit CacheStorageEngineConnection(NetworkConnectionToWebProcess&);

    void open(PAL::SessionID, WebCore::ClientOrigin&&, String&& cacheName, WebCore::DOMCacheEngine::CacheIdentifierCallback&&);
    void remove(PAL::SessionID, uint64_t cacheIdentifier, WebCore::DOMCacheEngine::CacheIdentifierCallback&&);
    void caches(PAL::SessionID, WebCore::ClientOrigin&&, uint64_t updateCounter, WebCore::DOMCacheEngine::CacheInfosCallback&&);

    void retrieveRecords(PAL::SessionID, uint64_t cacheIdentifier, URL&&, WebCore::DOMCacheEngine::RecordsCallback&&);
    void deleteMatchingRecords(PAL::SessionID, uint64_t cacheIdentifier, WebCore::ResourceRequest&&, WebCore::CacheQueryOptions&&, WebCore::DOMCacheEngine::RecordIdentifiersCallback&&);
    void putRecords(PAL::SessionID, uint64_t cacheIdentifier, Vector<WebCore::DOMCacheEngine::Record>&&, WebCore::DOMCacheEngine::RecordIdentifiersCallback&&);

    void reference(PAL::SessionID, uint64_t cacheIdentifier);
    void dereference(PAL::SessionID, uint64_t cacheIdentifier);

    void clearMemoryRepresentation(PAL::SessionID, WebCore::ClientOrigin&&, CompletionHandler<void(Optional<WebCore::DOMCacheEngine::Error>&&)>&&);
    void engineRepresentation(PAL::SessionID, CompletionHandler<void(String&&)>&&);

    NetworkConnectionToWebProcess& m_connection;
    HashMap<PAL::SessionID, HashMap<CacheStorage::CacheIdentifier, CacheStorage::LockCount>> m_cachesLocks;
};

}
