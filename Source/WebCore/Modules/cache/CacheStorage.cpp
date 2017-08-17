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
#include "CacheStorage.h"

#include "CacheQueryOptions.h"
#include "JSCache.h"
#include "ScriptExecutionContext.h"

namespace WebCore {

CacheStorage::CacheStorage(ScriptExecutionContext& context, Ref<CacheStorageConnection>&& connection)
    : ActiveDOMObject(&context)
    , m_connection(WTFMove(connection))
{
    suspendIfNeeded();
}

String CacheStorage::origin() const
{
    // FIXME: Do we really need to check for origin being null?
    auto* origin = scriptExecutionContext() ? scriptExecutionContext()->securityOrigin() : nullptr;
    return origin ? origin->toString() : String();
}

void CacheStorage::match(Cache::RequestInfo&&, CacheQueryOptions&&, Ref<DeferredPromise>&& promise)
{
    promise->reject(Exception { NotSupportedError, ASCIILiteral("Not implemented")});
}

void CacheStorage::has(const String& name, DOMPromiseDeferred<IDLBoolean>&& promise)
{
    retrieveCaches([this, name, promise = WTFMove(promise)]() mutable {
        promise.resolve(m_caches.findMatching([&](auto& item) { return item->name() == name; }) != notFound);
    });
}

void CacheStorage::retrieveCaches(WTF::Function<void()>&& callback)
{
    String origin = this->origin();
    if (origin.isNull())
        return;

    setPendingActivity(this);
    m_connection->retrieveCaches(origin, [this, callback = WTFMove(callback)](Vector<CacheStorageConnection::CacheInfo>&& cachesInfo) {
        if (!m_isStopped) {
            ASSERT(scriptExecutionContext());
            m_caches.removeAllMatching([&](auto& cache) {
                return cachesInfo.findMatching([&](const auto& info) { return info.identifier == cache->identifier(); }) == notFound;
            });
            for (auto& info : cachesInfo) {
                if (m_caches.findMatching([&](const auto& cache) { return info.identifier == cache->identifier(); }) == notFound)
                    m_caches.append(Cache::create(*scriptExecutionContext(), WTFMove(info.name), info.identifier, m_connection.copyRef()));
            }

            std::sort(m_caches.begin(), m_caches.end(), [&](auto& a, auto& b) {
                return a->identifier() < b->identifier();
            });

            callback();
        }
        unsetPendingActivity(this);
    });
}

void CacheStorage::open(const String& name, DOMPromiseDeferred<IDLInterface<Cache>>&& promise)
{
    retrieveCaches([this, name, promise = WTFMove(promise)]() mutable {
        auto position = m_caches.findMatching([&](auto& item) { return item->name() == name; });
        if (position != notFound) {
            auto& cache = m_caches[position];
            promise.resolve(Cache::create(*scriptExecutionContext(), String { cache->name() }, cache->identifier(), m_connection.copyRef()));
            return;
        }

        String origin = this->origin();
        ASSERT(!origin.isNull());

        setPendingActivity(this);
        m_connection->open(origin, name, [this, name, promise = WTFMove(promise)](uint64_t cacheIdentifier, CacheStorageConnection::Error error) mutable {
            if (!m_isStopped) {
                auto result = CacheStorageConnection::errorToException(error);
                if (result.hasException())
                    promise.reject(result.releaseException());
                else {
                    auto cache = Cache::create(*scriptExecutionContext(), String { name }, cacheIdentifier, m_connection.copyRef());
                    promise.resolve(cache);
                    m_caches.append(WTFMove(cache));
                }
            }
            unsetPendingActivity(this);
        });
    });
}

void CacheStorage::remove(const String& name, DOMPromiseDeferred<IDLBoolean>&& promise)
{
    retrieveCaches([this, name, promise = WTFMove(promise)]() mutable {
        auto position = m_caches.findMatching([&](auto& item) { return item->name() == name; });
        if (position == notFound) {
            promise.resolve(false);
            return;
        }

        String origin = this->origin();
        ASSERT(!origin.isNull());

        setPendingActivity(this);
        m_connection->remove(m_caches[position]->identifier(), [this, name, promise = WTFMove(promise)](uint64_t cacheIdentifier, CacheStorageConnection::Error error) mutable {
            UNUSED_PARAM(cacheIdentifier);
            if (!m_isStopped)
                promise.settle(CacheStorageConnection::exceptionOrResult(true, error));

            unsetPendingActivity(this);
        });
        m_caches.remove(position);
    });
}

void CacheStorage::keys(KeysPromise&& promise)
{
    retrieveCaches([this, promise = WTFMove(promise)]() mutable {
        Vector<String> keys;
        keys.reserveInitialCapacity(m_caches.size());
        for (auto& cache : m_caches)
            keys.uncheckedAppend(cache->name());
        promise.resolve(keys);
    });
}

void CacheStorage::stop()
{
    m_isStopped = true;
}

const char* CacheStorage::activeDOMObjectName() const
{
    return "CacheStorage";
}

bool CacheStorage::canSuspendForDocumentSuspension() const
{
    return !hasPendingActivity();
}

} // namespace WebCore
