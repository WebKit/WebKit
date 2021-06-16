
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
    for (auto& references : m_cachesLocks) {
        ASSERT(references.value);
        Engine::unlock(m_connection.networkProcess(), sessionID(), references.key);
    }
}

void CacheStorageEngineConnection::open(WebCore::ClientOrigin&& origin, String&& cacheName, CacheIdentifierCallback&& callback)
{
    CACHE_STORAGE_RELEASE_LOG("open cache");
    Engine::open(m_connection.networkProcess(), sessionID(), WTFMove(origin), WTFMove(cacheName), [callback = WTFMove(callback), sessionID = this->sessionID()](auto& result) mutable {
        CACHE_STORAGE_RELEASE_LOG_FUNCTION_IN_CALLBACK("open", "cache identifier is %" PRIu64, [](const auto& value) { return value.identifier; });
        callback(result);
    });
}

void CacheStorageEngineConnection::remove(uint64_t cacheIdentifier, CacheIdentifierCallback&& callback)
{
    CACHE_STORAGE_RELEASE_LOG("remove cache %" PRIu64, cacheIdentifier);
    Engine::remove(m_connection.networkProcess(), sessionID(), cacheIdentifier, [callback = WTFMove(callback), sessionID = this->sessionID()](auto& result) mutable {
        CACHE_STORAGE_RELEASE_LOG_FUNCTION_IN_CALLBACK("remove", "removed cache %" PRIu64, [](const auto& value) { return value.identifier; });
        callback(result);
    });
}

void CacheStorageEngineConnection::caches(WebCore::ClientOrigin&& origin, uint64_t updateCounter, CacheInfosCallback&& callback)
{
    CACHE_STORAGE_RELEASE_LOG("caches");
    Engine::retrieveCaches(m_connection.networkProcess(), sessionID(), WTFMove(origin), updateCounter, [callback = WTFMove(callback), origin, sessionID = this->sessionID()](auto&& result) mutable {
        CACHE_STORAGE_RELEASE_LOG_FUNCTION_IN_CALLBACK("caches", "caches size is %lu", [](const auto& value) { return value.infos.size(); });
        callback(WTFMove(result));
    });
}

void CacheStorageEngineConnection::retrieveRecords(uint64_t cacheIdentifier, WebCore::RetrieveRecordsOptions&& options, RecordsCallback&& callback)
{
    CACHE_STORAGE_RELEASE_LOG("retrieveRecords in cache %" PRIu64, cacheIdentifier);
    Engine::retrieveRecords(m_connection.networkProcess(), sessionID(), cacheIdentifier, WTFMove(options), [callback = WTFMove(callback), sessionID = this->sessionID()](auto&& result) mutable {
        CACHE_STORAGE_RELEASE_LOG_FUNCTION_IN_CALLBACK("retrieveRecords", "records size is %lu", [](const auto& value) { return value.size(); });
        callback(WTFMove(result));
    });
}

void CacheStorageEngineConnection::deleteMatchingRecords(uint64_t cacheIdentifier, WebCore::ResourceRequest&& request, WebCore::CacheQueryOptions&& options, RecordIdentifiersCallback&& callback)
{
    CACHE_STORAGE_RELEASE_LOG("deleteMatchingRecords in cache %" PRIu64, cacheIdentifier);
    Engine::deleteMatchingRecords(m_connection.networkProcess(), sessionID(), cacheIdentifier, WTFMove(request), WTFMove(options), [callback = WTFMove(callback), sessionID = this->sessionID()](auto&& result) mutable {
        CACHE_STORAGE_RELEASE_LOG_FUNCTION_IN_CALLBACK("deleteMatchingRecords", "deleted %lu records",  [](const auto& value) { return value.size(); });
        callback(WTFMove(result));
    });
}

void CacheStorageEngineConnection::putRecords(uint64_t cacheIdentifier, Vector<Record>&& records, RecordIdentifiersCallback&& callback)
{
    CACHE_STORAGE_RELEASE_LOG("putRecords in cache %" PRIu64 ", %lu records", cacheIdentifier, records.size());
    Engine::putRecords(m_connection.networkProcess(), sessionID(), cacheIdentifier, WTFMove(records), [callback = WTFMove(callback), sessionID = this->sessionID()](auto&& result) mutable {
        CACHE_STORAGE_RELEASE_LOG_FUNCTION_IN_CALLBACK("putRecords", "put %lu records",  [](const auto& value) { return value.size(); });
        callback(WTFMove(result));
    });
}

void CacheStorageEngineConnection::reference(uint64_t cacheIdentifier)
{
    CACHE_STORAGE_RELEASE_LOG("reference cache %" PRIu64, cacheIdentifier);
    ASSERT(m_cachesLocks.isValidKey(cacheIdentifier));
    if (!m_cachesLocks.isValidKey(cacheIdentifier))
        return;

    auto& counter = m_cachesLocks.ensure(cacheIdentifier, []() {
        return 0;
    }).iterator->value;
    if (!counter++)
        Engine::lock(m_connection.networkProcess(), sessionID(), cacheIdentifier);
}

void CacheStorageEngineConnection::dereference(uint64_t cacheIdentifier)
{
    CACHE_STORAGE_RELEASE_LOG("dereference cache %" PRIu64, cacheIdentifier);
    ASSERT(m_cachesLocks.isValidKey(cacheIdentifier));
    if (!m_cachesLocks.isValidKey(cacheIdentifier))
        return;

    auto referenceResult = m_cachesLocks.find(cacheIdentifier);
    if (referenceResult == m_cachesLocks.end())
        return;

    ASSERT(referenceResult->value);
    if (--referenceResult->value)
        return;

    Engine::unlock(m_connection.networkProcess(), sessionID(), cacheIdentifier);
    m_cachesLocks.remove(referenceResult);
}

void CacheStorageEngineConnection::clearMemoryRepresentation(WebCore::ClientOrigin&& origin, CompletionHandler<void(std::optional<Error>&&)>&& completionHandler)
{
    Engine::clearMemoryRepresentation(m_connection.networkProcess(), sessionID(), WTFMove(origin), WTFMove(completionHandler));
}

void CacheStorageEngineConnection::engineRepresentation(CompletionHandler<void(String&&)>&& completionHandler)
{
    Engine::representation(m_connection.networkProcess(), sessionID(), WTFMove(completionHandler));
}

PAL::SessionID CacheStorageEngineConnection::sessionID() const
{
    return m_connection.sessionID();
}

}

#undef CACHE_STORAGE_RELEASE_LOG
#undef CACHE_STORAGE_RELEASE_LOG_FUNCTION_IN_CALLBACK
