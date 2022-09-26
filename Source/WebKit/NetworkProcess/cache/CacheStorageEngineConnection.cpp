
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

#include "Logging.h"
#include "NetworkConnectionToWebProcess.h"
#include "WebCoreArgumentCoders.h"
#include <WebCore/CacheQueryOptions.h>

namespace WebKit {
using namespace WebCore::DOMCacheEngine;
using namespace CacheStorage;

#define CACHE_STORAGE_RELEASE_LOG(fmt, ...) RELEASE_LOG(CacheStorage, "%p - CacheStorageEngineConnection::" fmt, &m_connection.connection(), ##__VA_ARGS__)
#define CACHE_STORAGE_RELEASE_LOG_FUNCTION_IN_CALLBACK(functionName, fmt, resultGetter) \
    if (!result.has_value())\
        RELEASE_LOG_ERROR(CacheStorage, "CacheStorageEngineConnection::%s - failed - error %d", functionName, (int)result.error()); \
    else {\
        auto value = resultGetter(result.value()); \
        UNUSED_PARAM(value); \
        RELEASE_LOG(CacheStorage, "CacheStorageEngineConnection::%s - succeeded - " fmt, functionName, value); \
    }
CacheStorageEngineConnection::CacheStorageEngineConnection(NetworkConnectionToWebProcess& connection)
    : m_connection(connection)
{
}

CacheStorageEngineConnection::~CacheStorageEngineConnection()
{
    if (auto* session = m_connection.networkSession()) {
        for (auto& references : m_cachesLocks) {
            ASSERT(references.value);
            Engine::unlock(*session, references.key);
        }
    }
}

void CacheStorageEngineConnection::open(WebCore::ClientOrigin&& origin, String&& cacheName, CacheIdentifierCallback&& callback)
{
    CACHE_STORAGE_RELEASE_LOG("open cache");
    auto* session = m_connection.networkSession();
    if (!session)
        return callback(makeUnexpected(WebCore::DOMCacheEngine::Error::Internal));

    Engine::open(*session, WTFMove(origin), WTFMove(cacheName), [callback = WTFMove(callback), sessionID = this->sessionID()](auto& result) mutable {
        CACHE_STORAGE_RELEASE_LOG_FUNCTION_IN_CALLBACK("open", "cache identifier is %" PRIu64, [](const auto& value) { return value.identifier.toUInt64(); });
        callback(result);
    });
}

void CacheStorageEngineConnection::remove(WebCore::DOMCacheIdentifier cacheIdentifier, RemoveCacheIdentifierCallback&& callback)
{
    CACHE_STORAGE_RELEASE_LOG("remove cache %" PRIu64, cacheIdentifier.toUInt64());
    auto* session = m_connection.networkSession();
    if (!session)
        return callback(makeUnexpected(WebCore::DOMCacheEngine::Error::Internal));

    Engine::remove(*session, cacheIdentifier, [callback = WTFMove(callback), sessionID = this->sessionID()](auto& result) mutable {
        CACHE_STORAGE_RELEASE_LOG_FUNCTION_IN_CALLBACK("caches", "removed cache %d", [](const auto& value) { return value; });
        callback(result);
    });
}

void CacheStorageEngineConnection::caches(WebCore::ClientOrigin&& origin, uint64_t updateCounter, CacheInfosCallback&& callback)
{
    CACHE_STORAGE_RELEASE_LOG("caches");
    auto* session = m_connection.networkSession();
    if (!session)
        return callback(makeUnexpected(WebCore::DOMCacheEngine::Error::Internal));

    Engine::retrieveCaches(*session, WTFMove(origin), updateCounter, [callback = WTFMove(callback), origin, sessionID = this->sessionID()](auto&& result) mutable {
        CACHE_STORAGE_RELEASE_LOG_FUNCTION_IN_CALLBACK("caches", "caches size is %lu", [](const auto& value) { return value.infos.size(); });
        callback(WTFMove(result));
    });
}

void CacheStorageEngineConnection::retrieveRecords(WebCore::DOMCacheIdentifier cacheIdentifier, WebCore::RetrieveRecordsOptions&& options, RecordsCallback&& callback)
{
    CACHE_STORAGE_RELEASE_LOG("retrieveRecords in cache %" PRIu64, cacheIdentifier.toUInt64());
    auto* session = m_connection.networkSession();
    if (!session)
        return callback(makeUnexpected(WebCore::DOMCacheEngine::Error::Internal));

    Engine::retrieveRecords(*session, cacheIdentifier, WTFMove(options), [callback = WTFMove(callback), sessionID = this->sessionID()](auto&& result) mutable {
        CACHE_STORAGE_RELEASE_LOG_FUNCTION_IN_CALLBACK("retrieveRecords", "records size is %lu", [](const auto& value) { return value.size(); });
        callback(WTFMove(result));
    });
}

void CacheStorageEngineConnection::deleteMatchingRecords(WebCore::DOMCacheIdentifier cacheIdentifier, WebCore::ResourceRequest&& request, WebCore::CacheQueryOptions&& options, RecordIdentifiersCallback&& callback)
{
    CACHE_STORAGE_RELEASE_LOG("deleteMatchingRecords in cache %" PRIu64, cacheIdentifier.toUInt64());
    auto* session = m_connection.networkSession();
    if (!session)
        return callback(makeUnexpected(WebCore::DOMCacheEngine::Error::Internal));

    Engine::deleteMatchingRecords(*session, cacheIdentifier, WTFMove(request), WTFMove(options), [callback = WTFMove(callback), sessionID = this->sessionID()](auto&& result) mutable {
        CACHE_STORAGE_RELEASE_LOG_FUNCTION_IN_CALLBACK("deleteMatchingRecords", "deleted %lu records",  [](const auto& value) { return value.size(); });
        callback(WTFMove(result));
    });
}

void CacheStorageEngineConnection::putRecords(WebCore::DOMCacheIdentifier cacheIdentifier, Vector<Record>&& records, RecordIdentifiersCallback&& callback)
{
    CACHE_STORAGE_RELEASE_LOG("putRecords in cache %" PRIu64 ", %lu records", cacheIdentifier.toUInt64(), records.size());
    auto* session = m_connection.networkSession();
    if (!session)
        return callback(makeUnexpected(WebCore::DOMCacheEngine::Error::Internal));

    Engine::putRecords(*session, cacheIdentifier, WTFMove(records), [callback = WTFMove(callback), sessionID = this->sessionID()](auto&& result) mutable {
        CACHE_STORAGE_RELEASE_LOG_FUNCTION_IN_CALLBACK("putRecords", "put %lu records",  [](const auto& value) { return value.size(); });
        callback(WTFMove(result));
    });
}

void CacheStorageEngineConnection::reference(WebCore::DOMCacheIdentifier cacheIdentifier)
{
    CACHE_STORAGE_RELEASE_LOG("reference cache %" PRIu64, cacheIdentifier.toUInt64());
    auto* session = m_connection.networkSession();
    if (!session)
        return;

    ASSERT(m_cachesLocks.isValidKey(cacheIdentifier));
    if (!m_cachesLocks.isValidKey(cacheIdentifier))
        return;

    auto& counter = m_cachesLocks.ensure(cacheIdentifier, []() {
        return 0;
    }).iterator->value;
    if (!counter++)
        Engine::lock(*session, cacheIdentifier);
}

void CacheStorageEngineConnection::dereference(WebCore::DOMCacheIdentifier cacheIdentifier)
{
    CACHE_STORAGE_RELEASE_LOG("dereference cache %" PRIu64, cacheIdentifier.toUInt64());
    auto* session = m_connection.networkSession();
    if (!session)
        return;

    ASSERT(m_cachesLocks.isValidKey(cacheIdentifier));
    if (!m_cachesLocks.isValidKey(cacheIdentifier))
        return;

    auto referenceResult = m_cachesLocks.find(cacheIdentifier);
    if (referenceResult == m_cachesLocks.end())
        return;

    ASSERT(referenceResult->value);
    if (--referenceResult->value)
        return;

    Engine::unlock(*session, cacheIdentifier);
    m_cachesLocks.remove(referenceResult);
}

void CacheStorageEngineConnection::clearMemoryRepresentation(WebCore::ClientOrigin&& origin, CompletionHandler<void(std::optional<Error>&&)>&& completionHandler)
{
    auto* session = m_connection.networkSession();
    if (!session)
        return completionHandler(WebCore::DOMCacheEngine::Error::Internal);

    Engine::clearMemoryRepresentation(*session, WTFMove(origin), WTFMove(completionHandler));
}

void CacheStorageEngineConnection::engineRepresentation(CompletionHandler<void(String&&)>&& completionHandler)
{
    auto* session = m_connection.networkSession();
    if (!session)
        return completionHandler({ });

    Engine::representation(*session, WTFMove(completionHandler));
}

PAL::SessionID CacheStorageEngineConnection::sessionID() const
{
    return m_connection.sessionID();
}

}

#undef CACHE_STORAGE_RELEASE_LOG
#undef CACHE_STORAGE_RELEASE_LOG_FUNCTION_IN_CALLBACK
