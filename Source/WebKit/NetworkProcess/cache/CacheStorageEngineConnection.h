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
#include <WebCore/CacheStorageConnection.h>
#include <pal/SessionID.h>
#include <wtf/Forward.h>
#include <wtf/RefCounted.h>

namespace IPC {
class Connection;
class Decoder;
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

    void open(PAL::SessionID, uint64_t openRequestIdentifier, WebCore::ClientOrigin&&, String&& cacheName);
    void remove(PAL::SessionID, uint64_t removeRequestIdentifier, uint64_t cacheIdentifier);
    void caches(PAL::SessionID, uint64_t retrieveCachesIdentifier, WebCore::ClientOrigin&&, uint64_t updateCounter);

    void retrieveRecords(PAL::SessionID, uint64_t requestIdentifier, uint64_t cacheIdentifier, URL&&);
    void deleteMatchingRecords(PAL::SessionID, uint64_t requestIdentifier, uint64_t cacheIdentifier, WebCore::ResourceRequest&&, WebCore::CacheQueryOptions&&);
    void putRecords(PAL::SessionID, uint64_t requestIdentifier, uint64_t cacheIdentifier, Vector<WebCore::DOMCacheEngine::Record>&&);

    void reference(PAL::SessionID, uint64_t cacheIdentifier);
    void dereference(PAL::SessionID, uint64_t cacheIdentifier);

    void clearMemoryRepresentation(PAL::SessionID, uint64_t requestIdentifier, WebCore::ClientOrigin&&);
    void engineRepresentation(PAL::SessionID, uint64_t requestIdentifier);

    NetworkConnectionToWebProcess& m_connection;
    HashMap<PAL::SessionID, HashMap<CacheStorage::CacheIdentifier, CacheStorage::LockCount>> m_cachesLocks;
};

}
