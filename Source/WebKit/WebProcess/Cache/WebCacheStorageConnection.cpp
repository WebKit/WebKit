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
#include "NetworkProcessMessages.h"
#include "WebCacheStorageProvider.h"
#include "WebCoreArgumentCoders.h"
#include "WebProcess.h"
#include <wtf/MainThread.h>

namespace WebKit {
using namespace WebCore::DOMCacheEngine;
using namespace CacheStorage;

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
    return WebProcess::singleton().ensureNetworkProcessConnection().connection();
}

void WebCacheStorageConnection::open(const WebCore::ClientOrigin& origin, const String& cacheName, WebCore::DOMCacheEngine::CacheIdentifierCallback&& callback)
{
    connection().sendWithAsyncReply(Messages::CacheStorageEngineConnection::Open(m_sessionID, origin, cacheName), WTFMove(callback));
}

void WebCacheStorageConnection::remove(uint64_t cacheIdentifier, WebCore::DOMCacheEngine::CacheIdentifierCallback&& callback)
{
    connection().sendWithAsyncReply(Messages::CacheStorageEngineConnection::Remove(m_sessionID, cacheIdentifier), WTFMove(callback));
}

void WebCacheStorageConnection::retrieveCaches(const WebCore::ClientOrigin& origin, uint64_t updateCounter, WebCore::DOMCacheEngine::CacheInfosCallback&& callback)
{
    connection().sendWithAsyncReply(Messages::CacheStorageEngineConnection::Caches(m_sessionID, origin, updateCounter), WTFMove(callback));
}

void WebCacheStorageConnection::retrieveRecords(uint64_t cacheIdentifier, const URL& url, WebCore::DOMCacheEngine::RecordsCallback&& callback)
{
    connection().sendWithAsyncReply(Messages::CacheStorageEngineConnection::RetrieveRecords(m_sessionID, cacheIdentifier, url), WTFMove(callback));
}

void WebCacheStorageConnection::batchDeleteOperation(uint64_t cacheIdentifier, const WebCore::ResourceRequest& request, WebCore::CacheQueryOptions&& options, WebCore::DOMCacheEngine::RecordIdentifiersCallback&& callback)
{
    connection().sendWithAsyncReply(Messages::CacheStorageEngineConnection::DeleteMatchingRecords(m_sessionID, cacheIdentifier, request, options), WTFMove(callback));
}

void WebCacheStorageConnection::batchPutOperation(uint64_t cacheIdentifier, Vector<Record>&& records, WebCore::DOMCacheEngine::RecordIdentifiersCallback&& callback)
{
    connection().sendWithAsyncReply(Messages::CacheStorageEngineConnection::PutRecords(m_sessionID, cacheIdentifier, records), WTFMove(callback));
}

void WebCacheStorageConnection::reference(uint64_t cacheIdentifier)
{
    connection().send(Messages::CacheStorageEngineConnection::Reference(m_sessionID, cacheIdentifier), 0);
}

void WebCacheStorageConnection::dereference(uint64_t cacheIdentifier)
{
    connection().send(Messages::CacheStorageEngineConnection::Dereference(m_sessionID, cacheIdentifier), 0);
}

void WebCacheStorageConnection::clearMemoryRepresentation(const WebCore::ClientOrigin& origin, CompletionCallback&& callback)
{
    connection().sendWithAsyncReply(Messages::CacheStorageEngineConnection::ClearMemoryRepresentation { m_sessionID, origin }, WTFMove(callback));
}

void WebCacheStorageConnection::engineRepresentation(CompletionHandler<void(const String&)>&& callback)
{
    connection().sendWithAsyncReply(Messages::CacheStorageEngineConnection::EngineRepresentation { m_sessionID }, WTFMove(callback));
}

void WebCacheStorageConnection::updateQuotaBasedOnSpaceUsage(const WebCore::ClientOrigin& origin)
{
    connection().send(Messages::NetworkProcess::UpdateQuotaBasedOnSpaceUsageForTesting(m_sessionID, origin), 0);
}

}
