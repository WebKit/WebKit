
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

#include "config.h"
#include "CacheStorageEngineConnection.h"

#include "NetworkConnectionToWebProcess.h"
#include "WebCacheStorageConnectionMessages.h"
#include "WebCoreArgumentCoders.h"
#include <WebCore/CacheQueryOptions.h>

using namespace WebCore::DOMCacheEngine;
using namespace WebKit::CacheStorage;

namespace WebKit {

CacheStorageEngineConnection::CacheStorageEngineConnection(NetworkConnectionToWebProcess& connection)
    : m_connection(connection)
{
}

void CacheStorageEngineConnection::open(PAL::SessionID sessionID, uint64_t requestIdentifier, const String& origin, const String& cacheName)
{
    Engine::from(sessionID).open(origin, cacheName, [protectedThis = makeRef(*this), this, sessionID, requestIdentifier](const CacheIdentifierOrError& result) {
        m_connection.connection().send(Messages::WebCacheStorageConnection::OpenCompleted(requestIdentifier, result), sessionID.sessionID());
    });
}

void CacheStorageEngineConnection::remove(PAL::SessionID sessionID, uint64_t requestIdentifier, uint64_t cacheIdentifier)
{
    Engine::from(sessionID).remove(cacheIdentifier, [protectedThis = makeRef(*this), this, sessionID, requestIdentifier](const CacheIdentifierOrError& result) {
        m_connection.connection().send(Messages::WebCacheStorageConnection::RemoveCompleted(requestIdentifier, result), sessionID.sessionID());
    });
}

void CacheStorageEngineConnection::caches(PAL::SessionID sessionID, uint64_t requestIdentifier, const String& origin, uint64_t updateCounter)
{
    Engine::from(sessionID).retrieveCaches(origin, updateCounter, [protectedThis = makeRef(*this), this, sessionID, origin, requestIdentifier](CacheInfosOrError&& result) {
        m_connection.connection().send(Messages::WebCacheStorageConnection::UpdateCaches(requestIdentifier, result), sessionID.sessionID());
    });
}

void CacheStorageEngineConnection::clearMemoryRepresentation(PAL::SessionID sessionID, const String& origin)
{
    Engine::from(sessionID).clearMemoryRepresentation(origin);
}

void CacheStorageEngineConnection::retrieveRecords(PAL::SessionID sessionID, uint64_t requestIdentifier, uint64_t cacheIdentifier, WebCore::URL&& url)
{
    Engine::from(sessionID).retrieveRecords(cacheIdentifier, WTFMove(url), [protectedThis = makeRef(*this), this, sessionID, requestIdentifier](RecordsOrError&& result) {
        m_connection.connection().send(Messages::WebCacheStorageConnection::UpdateRecords(requestIdentifier, result), sessionID.sessionID());
    });
}

void CacheStorageEngineConnection::deleteMatchingRecords(PAL::SessionID sessionID, uint64_t requestIdentifier, uint64_t cacheIdentifier, WebCore::ResourceRequest&& request, WebCore::CacheQueryOptions&& options)
{
    Engine::from(sessionID).deleteMatchingRecords(cacheIdentifier, WTFMove(request), WTFMove(options), [protectedThis = makeRef(*this), this, sessionID, requestIdentifier](RecordIdentifiersOrError&& result) {
        m_connection.connection().send(Messages::WebCacheStorageConnection::DeleteRecordsCompleted(requestIdentifier, result), sessionID.sessionID());
    });
}

void CacheStorageEngineConnection::putRecords(PAL::SessionID sessionID, uint64_t requestIdentifier, uint64_t cacheIdentifier, Vector<Record>&& records)
{
    Engine::from(sessionID).putRecords(cacheIdentifier, WTFMove(records), [protectedThis = makeRef(*this), this, sessionID, requestIdentifier](RecordIdentifiersOrError&& result) {
        m_connection.connection().send(Messages::WebCacheStorageConnection::PutRecordsCompleted(requestIdentifier, result), sessionID.sessionID());
    });
}

}
