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
#include "WebCacheStorageConnection.h"

#include "CacheStorageEngine.h"
#include "CacheStorageEngineConnectionMessages.h"
#include "NetworkConnectionToWebProcessMessages.h"
#include "NetworkProcessConnection.h"
#include "WebCacheStorageProvider.h"
#include "WebCoreArgumentCoders.h"
#include "WebProcess.h"
#include <wtf/MainThread.h>

using namespace WebCore::DOMCacheEngine;
using namespace WebKit::CacheStorage;

namespace WebKit {

WebCacheStorageConnection::WebCacheStorageConnection(WebCacheStorageProvider& provider, PAL::SessionID sessionID)
    : m_provider(provider)
    , m_sessionID(sessionID)
{
}

WebCacheStorageConnection::~WebCacheStorageConnection()
{
    ASSERT(isMainThread());
}

IPC::Connection& WebCacheStorageConnection::connection()
{
    return WebProcess::singleton().networkConnection().connection();
}

void WebCacheStorageConnection::doOpen(uint64_t requestIdentifier, const String& origin, const String& cacheName)
{
    connection().send(Messages::CacheStorageEngineConnection::Open(m_sessionID, requestIdentifier, origin, cacheName), 0);
}

void WebCacheStorageConnection::doRemove(uint64_t requestIdentifier, uint64_t cacheIdentifier)
{
    connection().send(Messages::CacheStorageEngineConnection::Remove(m_sessionID, requestIdentifier, cacheIdentifier), 0);
}

void WebCacheStorageConnection::doRetrieveCaches(uint64_t requestIdentifier, const String& origin, uint64_t updateCounter)
{
    connection().send(Messages::CacheStorageEngineConnection::Caches(m_sessionID, requestIdentifier, origin, updateCounter), 0);
}

void WebCacheStorageConnection::doRetrieveRecords(uint64_t requestIdentifier, uint64_t cacheIdentifier)
{
    connection().send(Messages::CacheStorageEngineConnection::Records(m_sessionID, requestIdentifier, cacheIdentifier), 0);
}

void WebCacheStorageConnection::doBatchDeleteOperation(uint64_t requestIdentifier, uint64_t cacheIdentifier, const WebCore::ResourceRequest& request, WebCore::CacheQueryOptions&& options)
{
    connection().send(Messages::CacheStorageEngineConnection::DeleteMatchingRecords(m_sessionID, requestIdentifier, cacheIdentifier, request, options), 0);
}

void WebCacheStorageConnection::doBatchPutOperation(uint64_t requestIdentifier, uint64_t cacheIdentifier, Vector<Record>&& records)
{
    connection().send(Messages::CacheStorageEngineConnection::PutRecords(m_sessionID, requestIdentifier, cacheIdentifier, records), 0);
}

void WebCacheStorageConnection::clearMemoryRepresentation(const String& origin, CompletionCallback&& callback)
{
    connection().send(Messages::CacheStorageEngineConnection::ClearMemoryRepresentation(m_sessionID, origin), 0);
    callback(std::nullopt);
}

void WebCacheStorageConnection::openCompleted(uint64_t requestIdentifier, const CacheIdentifierOrError& result)
{
    CacheStorageConnection::openCompleted(requestIdentifier, result);
}

void WebCacheStorageConnection::removeCompleted(uint64_t requestIdentifier, const CacheIdentifierOrError& result)
{
    CacheStorageConnection::removeCompleted(requestIdentifier, result);
}

void WebCacheStorageConnection::updateCaches(uint64_t requestIdentifier, CacheInfosOrError&& result)
{
    CacheStorageConnection::updateCaches(requestIdentifier, WTFMove(result));
}

void WebCacheStorageConnection::updateRecords(uint64_t requestIdentifier, RecordsOrError&& result)
{
    CacheStorageConnection::updateRecords(requestIdentifier, WTFMove(result));
}

void WebCacheStorageConnection::deleteRecordsCompleted(uint64_t requestIdentifier, RecordIdentifiersOrError&& result)
{
    CacheStorageConnection::deleteRecordsCompleted(requestIdentifier, WTFMove(result));
}

void WebCacheStorageConnection::putRecordsCompleted(uint64_t requestIdentifier, RecordIdentifiersOrError&& result)
{
    CacheStorageConnection::putRecordsCompleted(requestIdentifier, WTFMove(result));
}

}
