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
#include <wtf/NativePromise.h>

namespace WebKit {

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

template<typename P>
struct WebCacheStorageConnection::PromiseConverter {
    using Promise = P;

    template<typename T>
    static typename Promise::Result convertResult(T&& result)
    {
        if constexpr (std::is_void<typename Promise::ResolveValueType>::value)
            return { };
        else
            return { WTFMove(result) };
    }

    static WebCore::DOMCacheEngine::Error convertError(IPC::Error)
    {
        return WebCore::DOMCacheEngine::Error::Internal;
    }
};

auto WebCacheStorageConnection::open(const WebCore::ClientOrigin& origin, const String& cacheName) -> Ref<OpenPromise>
{
    return connection().sendWithPromisedReply<Messages::NetworkStorageManager::CacheStorageOpenCache, PromiseConverter<OpenPromise>>({ origin, cacheName });
}

auto WebCacheStorageConnection::remove(WebCore::DOMCacheIdentifier cacheIdentifier) -> Ref<RemovePromise>
{
    return connection().sendWithPromisedReply<Messages::NetworkStorageManager::CacheStorageRemoveCache, PromiseConverter<RemovePromise>>(cacheIdentifier);
}

auto WebCacheStorageConnection::retrieveCaches(const WebCore::ClientOrigin& origin, uint64_t updateCounter) -> Ref<RetrieveCachesPromise>
{
    return connection().sendWithPromisedReply<Messages::NetworkStorageManager::CacheStorageAllCaches, PromiseConverter<RetrieveCachesPromise>>({ origin, updateCounter });
}

auto WebCacheStorageConnection::retrieveRecords(WebCore::DOMCacheIdentifier cacheIdentifier, WebCore::RetrieveRecordsOptions&& options) -> Ref<RetrieveRecordsPromise>
{
    return connection().sendWithPromisedReply<Messages::NetworkStorageManager::CacheStorageRetrieveRecords, PromiseConverter<RetrieveRecordsPromise>>({ cacheIdentifier, options });
}

auto WebCacheStorageConnection::batchDeleteOperation(WebCore::DOMCacheIdentifier cacheIdentifier, const WebCore::ResourceRequest& request, WebCore::CacheQueryOptions&& options) -> Ref<BatchPromise>
{
    return connection().sendWithPromisedReply<Messages::NetworkStorageManager::CacheStorageRemoveRecords, PromiseConverter<BatchPromise>>({ cacheIdentifier, request, options });
}

auto WebCacheStorageConnection::batchPutOperation(WebCore::DOMCacheIdentifier cacheIdentifier, Vector<WebCore::DOMCacheEngine::CrossThreadRecord>&& records) -> Ref<BatchPromise>
{
    return connection().sendWithPromisedReply<Messages::NetworkStorageManager::CacheStoragePutRecords, PromiseConverter<BatchPromise>>({ cacheIdentifier, WTFMove(records) });
}

void WebCacheStorageConnection::reference(WebCore::DOMCacheIdentifier cacheIdentifier)
{
    if (m_connectedIdentifierCounters.add(cacheIdentifier).isNewEntry)
        connection().send(Messages::NetworkStorageManager::CacheStorageReference(cacheIdentifier), 0);
}

void WebCacheStorageConnection::dereference(WebCore::DOMCacheIdentifier cacheIdentifier)
{
    if (m_connectedIdentifierCounters.remove(cacheIdentifier))
        connection().send(Messages::NetworkStorageManager::CacheStorageDereference(cacheIdentifier), 0);
}

void WebCacheStorageConnection::lockStorage(const WebCore::ClientOrigin& origin)
{
    if (m_clientOriginLockRequestCounters.add(origin).isNewEntry)
        connection().send(Messages::NetworkStorageManager::LockCacheStorage { origin }, 0);
}

void WebCacheStorageConnection::unlockStorage(const WebCore::ClientOrigin& origin)
{
    if (m_clientOriginLockRequestCounters.remove(origin))
        connection().send(Messages::NetworkStorageManager::UnlockCacheStorage { origin }, 0);
}

auto WebCacheStorageConnection::clearMemoryRepresentation(const WebCore::ClientOrigin& origin) -> Ref<CompletionPromise>
{
    return connection().sendWithPromisedReply<Messages::NetworkStorageManager::CacheStorageClearMemoryRepresentation, PromiseConverter<CompletionPromise>>(origin);
}

auto WebCacheStorageConnection::engineRepresentation() -> Ref<EngineRepresentationPromise>
{
    return connection().sendWithPromisedReply<Messages::NetworkStorageManager::CacheStorageRepresentation, PromiseConverter<EngineRepresentationPromise>>({ });
}

void WebCacheStorageConnection::updateQuotaBasedOnSpaceUsage(const WebCore::ClientOrigin& origin)
{
    connection().send(Messages::NetworkStorageManager::ResetQuotaUpdatedBasedOnUsageForTesting(origin), 0);
}

void WebCacheStorageConnection::networkProcessConnectionClosed()
{
    m_connectedIdentifierCounters.clear();
    m_clientOriginLockRequestCounters.clear();
}

}
