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

#include "NetworkConnectionToWebProcessMessages.h"
#include "NetworkProcessConnection.h"
#include "NetworkProcessMessages.h"
#include "NetworkStorageManagerMessages.h"
#include "WebCacheStorageProvider.h"
#include "WebCoreArgumentCoders.h"
#include "WebProcess.h"
#include <wtf/MainThread.h>

namespace WebKit {
using namespace WebCore::DOMCacheEngine;

WebCacheStorageConnection::WebCacheStorageConnection(WebCacheStorageProvider& provider)
    : m_provider(provider)
{
}

WebCacheStorageConnection::~WebCacheStorageConnection()
{
    ASSERT(isMainRunLoop());
}

IPC::Connection& WebCacheStorageConnection::connection()
{
    return WebProcess::singleton().ensureNetworkProcessConnection().connection();
}

void WebCacheStorageConnection::open(const WebCore::ClientOrigin& origin, const String& cacheName, WebCore::DOMCacheEngine::CacheIdentifierCallback&& callback)
{
    auto newCallback = [weakThis = WeakPtr { *this }, callback = WTFMove(callback)](auto&& result) mutable {
        if (weakThis && result) {
            if (auto identifier = result.value().identifier)
                weakThis->m_connectedIdentifiers.add(identifier);
        }
        callback(WTFMove(result));
    };
    connection().sendWithAsyncReply(Messages::NetworkStorageManager::CacheStorageOpenCache(origin, cacheName), WTFMove(newCallback));
}

void WebCacheStorageConnection::remove(WebCore::DOMCacheIdentifier cacheIdentifier, WebCore::DOMCacheEngine::RemoveCacheIdentifierCallback&& callback)
{
    connection().sendWithAsyncReply(Messages::NetworkStorageManager::CacheStorageRemoveCache(cacheIdentifier), WTFMove(callback));
}

void WebCacheStorageConnection::retrieveCaches(const WebCore::ClientOrigin& origin, uint64_t updateCounter, WebCore::DOMCacheEngine::CacheInfosCallback&& callback)
{
    connection().sendWithAsyncReply(Messages::NetworkStorageManager::CacheStorageAllCaches(origin, updateCounter), WTFMove(callback));
}

void WebCacheStorageConnection::retrieveRecords(WebCore::DOMCacheIdentifier cacheIdentifier, WebCore::RetrieveRecordsOptions&& options, WebCore::DOMCacheEngine::RecordsCallback&& callback)
{
    connection().sendWithAsyncReply(Messages::NetworkStorageManager::CacheStorageRetrieveRecords(cacheIdentifier, options), WTFMove(callback));
}

void WebCacheStorageConnection::batchDeleteOperation(WebCore::DOMCacheIdentifier cacheIdentifier, const WebCore::ResourceRequest& request, WebCore::CacheQueryOptions&& options, WebCore::DOMCacheEngine::RecordIdentifiersCallback&& callback)
{
    connection().sendWithAsyncReply(Messages::NetworkStorageManager::CacheStorageRemoveRecords(cacheIdentifier, request, options), WTFMove(callback));
}

void WebCacheStorageConnection::batchPutOperation(WebCore::DOMCacheIdentifier cacheIdentifier, Vector<Record>&& records, WebCore::DOMCacheEngine::RecordIdentifiersCallback&& callback)
{
    connection().sendWithAsyncReply(Messages::NetworkStorageManager::CacheStoragePutRecords(cacheIdentifier, records), WTFMove(callback));
}

void WebCacheStorageConnection::reference(WebCore::DOMCacheIdentifier cacheIdentifier)
{
    if (!m_connectedIdentifiers.contains(cacheIdentifier))
        return;

    connection().send(Messages::NetworkStorageManager::CacheStorageReference(cacheIdentifier), 0);
}

void WebCacheStorageConnection::dereference(WebCore::DOMCacheIdentifier cacheIdentifier)
{
    if (!m_connectedIdentifiers.contains(cacheIdentifier))
        return;

    connection().send(Messages::NetworkStorageManager::CacheStorageDereference(cacheIdentifier), 0);
}

void WebCacheStorageConnection::clearMemoryRepresentation(const WebCore::ClientOrigin& origin, CompletionCallback&& callback)
{
    connection().sendWithAsyncReply(Messages::NetworkStorageManager::CacheStorageClearMemoryRepresentation { origin }, WTFMove(callback));
}

void WebCacheStorageConnection::engineRepresentation(CompletionHandler<void(const String&)>&& callback)
{
    connection().sendWithAsyncReply(Messages::NetworkStorageManager::CacheStorageRepresentation { }, WTFMove(callback));
}

void WebCacheStorageConnection::updateQuotaBasedOnSpaceUsage(const WebCore::ClientOrigin& origin)
{
    connection().send(Messages::NetworkConnectionToWebProcess::UpdateQuotaBasedOnSpaceUsageForTesting(origin), 0);
}

void WebCacheStorageConnection::networkProcessConnectionClosed()
{
    m_connectedIdentifiers.clear();
}

}
