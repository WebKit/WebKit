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
#include "DOMCacheStorage.h"

#include "CacheQueryOptions.h"
#include "ClientOrigin.h"
#include "EventLoop.h"
#include "JSDOMCache.h"
#include "JSFetchResponse.h"
#include "ScriptExecutionContext.h"
#include "SecurityOrigin.h"

namespace WebCore {

DOMCacheStorage::DOMCacheStorage(ScriptExecutionContext& context, Ref<CacheStorageConnection>&& connection)
    : ActiveDOMObject(&context)
    , m_connection(WTFMove(connection))
{
    suspendIfNeeded();
}

DOMCacheStorage::~DOMCacheStorage() = default;

Optional<ClientOrigin> DOMCacheStorage::origin() const
{
    auto* origin = scriptExecutionContext() ? scriptExecutionContext()->securityOrigin() : nullptr;
    if (!origin)
        return WTF::nullopt;

    return ClientOrigin { scriptExecutionContext()->topOrigin().data(), origin->data() };
}

static void doSequentialMatch(size_t index, Vector<Ref<DOMCache>>&& caches, DOMCache::RequestInfo&& info, CacheQueryOptions&& options, DOMCache::MatchCallback&& completionHandler)
{
    if (index >= caches.size()) {
        completionHandler(nullptr);
        return;
    }

    auto& cache = caches[index].get();
    cache.doMatch(WTFMove(info), WTFMove(options), [caches = WTFMove(caches), info, options, completionHandler = WTFMove(completionHandler), index](auto&& result) mutable {
        if (result.hasException()) {
            completionHandler(result.releaseException());
            return;
        }
        if (result.returnValue()) {
            completionHandler(result.releaseReturnValue());
            return;
        }
        doSequentialMatch(++index, WTFMove(caches), WTFMove(info), WTFMove(options), WTFMove(completionHandler));
    });
}

static inline void startSequentialMatch(Vector<Ref<DOMCache>>&& caches, DOMCache::RequestInfo&& info, CacheQueryOptions&& options, DOMCache::MatchCallback&& completionHandler)
{
    doSequentialMatch(0, WTFMove(caches), WTFMove(info), WTFMove(options), WTFMove(completionHandler));
}

static inline Ref<DOMCache> copyCache(const Ref<DOMCache>& cache)
{
    return cache.copyRef();
}

void DOMCacheStorage::doSequentialMatch(DOMCache::RequestInfo&& info, CacheQueryOptions&& options, Ref<DeferredPromise>&& promise)
{
    startSequentialMatch(WTF::map(m_caches, copyCache), WTFMove(info), WTFMove(options), [pendingActivity = makePendingActivity(*this), promise = WTFMove(promise)](auto&& result) mutable {
        if (result.hasException()) {
            promise->reject(result.releaseException());
            return;
        }
        if (!result.returnValue()) {
            promise->resolve();
            return;
        }
        promise->resolve<IDLInterface<FetchResponse>>(*result.returnValue());
    });
}

void DOMCacheStorage::match(DOMCache::RequestInfo&& info, CacheQueryOptions&& options, Ref<DeferredPromise>&& promise)
{
    retrieveCaches([this, info = WTFMove(info), options = WTFMove(options), promise = WTFMove(promise)](Optional<Exception>&& exception) mutable {
        if (exception) {
            promise->reject(WTFMove(*exception));
            return;
        }

        if (!options.cacheName.isNull()) {
            auto position = m_caches.findMatching([&](auto& item) { return item->name() == options.cacheName; });
            if (position != notFound) {
                m_caches[position]->match(WTFMove(info), WTFMove(options), WTFMove(promise));
                return;
            }
            promise->resolve();
            return;
        }

        this->doSequentialMatch(WTFMove(info), WTFMove(options), WTFMove(promise));
    });
}

void DOMCacheStorage::has(const String& name, DOMPromiseDeferred<IDLBoolean>&& promise)
{
    retrieveCaches([this, name, promise = WTFMove(promise)](Optional<Exception>&& exception) mutable {
        if (exception) {
            promise.reject(WTFMove(exception.value()));
            return;
        }
        promise.resolve(m_caches.findMatching([&](auto& item) { return item->name() == name; }) != notFound);
    });
}

Ref<DOMCache> DOMCacheStorage::findCacheOrCreate(DOMCacheEngine::CacheInfo&& info)
{
   auto position = m_caches.findMatching([&] (const auto& cache) { return info.identifier == cache->identifier(); });
   if (position != notFound)
       return m_caches[position].copyRef();
   return DOMCache::create(*scriptExecutionContext(), WTFMove(info.name), info.identifier, m_connection.copyRef());
}

void DOMCacheStorage::retrieveCaches(CompletionHandler<void(Optional<Exception>&&)>&& callback)
{
    auto origin = this->origin();
    if (!origin) {
        callback(convertToExceptionAndLog(scriptExecutionContext(), DOMCacheEngine::Error::Stopped));
        return;
    }

    m_connection->retrieveCaches(*origin, m_updateCounter, [this, callback = WTFMove(callback), pendingActivity = makePendingActivity(*this)](DOMCacheEngine::CacheInfosOrError&& result) mutable {
        if (m_isStopped) {
            callback(DOMCacheEngine::convertToException(DOMCacheEngine::Error::Stopped));
            return;
        }
        if (!result.has_value()) {
            callback(DOMCacheEngine::convertToExceptionAndLog(scriptExecutionContext(), result.error()));
            return;
        }

        auto& cachesInfo = result.value();

        if (m_updateCounter != cachesInfo.updateCounter) {
            m_updateCounter = cachesInfo.updateCounter;

            m_caches = WTF::map(WTFMove(cachesInfo.infos), [this] (DOMCacheEngine::CacheInfo&& info) {
                return findCacheOrCreate(WTFMove(info));
            });
        }
        callback(WTF::nullopt);
    });
}

static void logConsolePersistencyError(ScriptExecutionContext* context, const String& cacheName)
{
    if (!context)
        return;

    context->addConsoleMessage(MessageSource::JS, MessageLevel::Error, makeString("There was an error making ", cacheName, " persistent on the filesystem"));
}

void DOMCacheStorage::open(const String& name, DOMPromiseDeferred<IDLInterface<DOMCache>>&& promise)
{
    retrieveCaches([this, name, promise = WTFMove(promise)](Optional<Exception>&& exception) mutable {
        if (exception) {
            promise.reject(WTFMove(*exception));
            return;
        }
        doOpen(name, WTFMove(promise));
    });
}

void DOMCacheStorage::doOpen(const String& name, DOMPromiseDeferred<IDLInterface<DOMCache>>&& promise)
{
    auto position = m_caches.findMatching([&](auto& item) { return item->name() == name; });
    if (position != notFound) {
        promise.resolve(DOMCache::create(*scriptExecutionContext(), String { m_caches[position]->name() }, m_caches[position]->identifier(), m_connection.copyRef()));
        return;
    }

    m_connection->open(*origin(), name, [this, name, promise = WTFMove(promise), pendingActivity = makePendingActivity(*this)](const DOMCacheEngine::CacheIdentifierOrError& result) mutable {
        if (!result.has_value())
            promise.reject(DOMCacheEngine::convertToExceptionAndLog(scriptExecutionContext(), result.error()));
        else {
            if (result.value().hadStorageError)
                logConsolePersistencyError(scriptExecutionContext(), name);

            auto cache = DOMCache::create(*scriptExecutionContext(), String { name }, result.value().identifier, m_connection.copyRef());
            promise.resolve(cache);
            m_caches.append(WTFMove(cache));
        }
    });
}

void DOMCacheStorage::remove(const String& name, DOMPromiseDeferred<IDLBoolean>&& promise)
{
    retrieveCaches([this, name, promise = WTFMove(promise)](Optional<Exception>&& exception) mutable {
        if (exception) {
            promise.reject(WTFMove(*exception));
            return;
        }
        doRemove(name, WTFMove(promise));
    });
}

void DOMCacheStorage::doRemove(const String& name, DOMPromiseDeferred<IDLBoolean>&& promise)
{
    auto position = m_caches.findMatching([&](auto& item) { return item->name() == name; });
    if (position == notFound) {
        promise.resolve(false);
        return;
    }

    m_connection->remove(m_caches[position]->identifier(), [this, name, promise = WTFMove(promise), pendingActivity = makePendingActivity(*this)](const DOMCacheEngine::CacheIdentifierOrError& result) mutable {
        if (!result.has_value())
            promise.reject(DOMCacheEngine::convertToExceptionAndLog(scriptExecutionContext(), result.error()));
        else {
            if (result.value().hadStorageError)
                logConsolePersistencyError(scriptExecutionContext(), name);
            promise.resolve(!!result.value().identifier);
        }
    });
}

void DOMCacheStorage::keys(KeysPromise&& promise)
{
    retrieveCaches([this, promise = WTFMove(promise)](Optional<Exception>&& exception) mutable {
        if (exception) {
            promise.reject(WTFMove(exception.value()));
            return;
        }

        promise.resolve(WTF::map(m_caches, [] (const auto& cache) {
            return cache->name();
        }));
    });
}

void DOMCacheStorage::stop()
{
    m_isStopped = true;
}

const char* DOMCacheStorage::activeDOMObjectName() const
{
    return "CacheStorage";
}

} // namespace WebCore
