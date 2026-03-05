/*
 * Copyright (C) 2023 Apple Inc. All rights reserved.
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
#include "BackgroundFetchStoreImpl.h"

#include "BackgroundFetchChange.h"
#include "BackgroundFetchState.h"
#include "BackgroundFetchStoreManager.h"
#include "NetworkStorageManager.h"
#include <WebCore/BackgroundFetch.h>
#include <WebCore/BackgroundFetchEngine.h>
#include <WebCore/BackgroundFetchInformation.h>
#include <WebCore/BackgroundFetchOptions.h>
#include <WebCore/BackgroundFetchRecordInformation.h>
#include <WebCore/DOMCacheEngine.h>
#include <WebCore/SWServer.h>
#include <WebCore/SWServerRegistration.h>
#include <wtf/CallbackAggregator.h>
#include <wtf/CrossThreadCopier.h>
#include <wtf/WorkQueue.h>

namespace WebKit {

using namespace WebCore;

// FIXME: Handle quota.

BackgroundFetchStoreImpl::BackgroundFetchStoreImpl(ThreadSafeWeakPtr<NetworkStorageManager>&& manager, WeakPtr<WebCore::SWServer>&& server)
    : m_manager(WTF::move(manager))
    , m_server(WTF::move(server))
{
}

BackgroundFetchStoreImpl::~BackgroundFetchStoreImpl()
{
}

String BackgroundFetchStoreImpl::getFilename(const ServiceWorkerRegistrationKey& key, const String& identifier)
{
    auto iterator = m_perClientOriginFetches.find(key.clientOrigin());
    if (iterator == m_perClientOriginFetches.end())
        return { };

    return iterator->value.fetchToFilenames.get(std::make_pair(key.scope().string(), identifier));
}

void BackgroundFetchStoreImpl::registerFetch(const ClientOrigin& origin, const ServiceWorkerRegistrationKey& key, const String& backgroundFetchIdentifier, String&& fetchStorageIdentifier)
{
    m_filenameToFetch.add(fetchStorageIdentifier, FetchInformation { origin, std::make_pair(key.scope().string(), backgroundFetchIdentifier) });

    auto iterator = m_perClientOriginFetches.find(origin);
    if (iterator != m_perClientOriginFetches.end())
        iterator->value.fetchToFilenames.add(std::make_pair(key.scope().string(), backgroundFetchIdentifier), WTF::move(fetchStorageIdentifier));
}

void BackgroundFetchStoreImpl::initializeFetches(const ServiceWorkerRegistrationKey& key, CompletionHandler<void()>&& callback)
{
    initializeFetches({ key.topOrigin(), SecurityOriginData::fromURL(key.scope()) }, WTF::move(callback));
}

void BackgroundFetchStoreImpl::initializeFetches(const WebCore::ClientOrigin& origin, CompletionHandler<void()>&& callback)
{
    RefPtr manager = m_manager.get();
    if (!manager) {
        callback();
        return;
    }

    auto addResult = m_perClientOriginFetches.add(origin, PerClientOriginFetches { });
    if (!addResult.isNewEntry && addResult.iterator->value.initializationCallbacks.isEmpty()) {
        callback();
        return;
    }

    addResult.iterator->value.initializationCallbacks.append(WTF::move(callback));

    initializeFetchesInternal(origin, [origin, weakEngine = WeakPtr { protect(m_server)->backgroundFetchEngine() }, protectedThis = Ref { *this }, manager](Vector<std::pair<RefPtr<WebCore::SharedBuffer>, String>>&& fetches) {
        if (weakEngine && manager) {
            for (auto& fetch : fetches) {
                weakEngine->addFetchFromStore(Ref { *fetch.first }->span(), [&](auto& key, auto& identifier) {
                    if (identifier.isEmpty()) {
                        manager->dispatchTaskToBackgroundFetchManager(origin, [identifier = crossThreadCopy(WTF::move(fetch.second))](auto* backgroundFetchManager) {
                            if (backgroundFetchManager)
                                backgroundFetchManager->clearFetch(identifier, [] { });
                        });
                        return;
                    }

                    protectedThis->registerFetch(origin, key, identifier, WTF::move(fetch.second));
                });
            }
        }

        auto iterator = protectedThis->m_perClientOriginFetches.find(origin);
        if (iterator == protectedThis->m_perClientOriginFetches.end())
            return;

        auto callbacks = std::exchange(iterator->value.initializationCallbacks, { });
        for (auto& callback : callbacks)
            callback();
    });
}

void BackgroundFetchStoreImpl::initializeFetchesInternal(const WebCore::ClientOrigin& origin, CompletionHandler<void(Vector<std::pair<RefPtr<WebCore::SharedBuffer>, String>>&&)>&& internalCallback)
{
    RefPtr manager = m_manager.get();
    if (!manager)
        return internalCallback({ });
    manager->dispatchTaskToBackgroundFetchManager(origin, [internalCallback = WTF::move(internalCallback)](auto* backgroundFetchManager) mutable {
        if (!backgroundFetchManager) {
            callOnMainRunLoop([internalCallback = WTF::move(internalCallback)]() mutable {
                internalCallback({ });
            });
            return;
        }
        backgroundFetchManager->initializeFetches([internalCallback = WTF::move(internalCallback)](auto&& fetches) mutable {
            callOnMainRunLoop([fetches = std::forward<decltype(fetches)>(fetches), internalCallback = WTF::move(internalCallback)]() mutable {
                internalCallback(WTF::move(fetches));
            });
        });
    });
}

void BackgroundFetchStoreImpl::clearFetch(const ServiceWorkerRegistrationKey& key, const String& identifier, CompletionHandler<void()>&& callback)
{
    RefPtr manager = m_manager.get();
    if (!manager)
        return callback();

    auto origin = key.clientOrigin();
    auto iterator = m_perClientOriginFetches.find(origin);
    if (iterator == m_perClientOriginFetches.end()) {
        callback();
        return;
    }

    auto fetchStorageIdentifier = iterator->value.fetchToFilenames.take(std::make_pair(key.scope().string(), identifier));
    if (fetchStorageIdentifier.isEmpty()) {
        callback();
        return;
    }
    m_filenameToFetch.remove(fetchStorageIdentifier);

    clearFetchInternal(origin, fetchStorageIdentifier, [protectedThis = Ref { *this }, fetchStorageIdentifier, callback = WTF::move(callback)]() mutable {
        RefPtr manager = protectedThis->m_manager.get();
        if (manager)
            manager->notifyBackgroundFetchChange(fetchStorageIdentifier, BackgroundFetchChange::Removal);
        callback();
    });
}

void BackgroundFetchStoreImpl::clearFetchInternal(const WebCore::ClientOrigin& origin, const String& fetchStorageIdentifier, CompletionHandler<void()>&& internalCallback)
{
    RefPtr manager = m_manager.get();
    if (!manager)
        return internalCallback();

    manager->dispatchTaskToBackgroundFetchManager(origin, [fetchStorageIdentifier = crossThreadCopy(fetchStorageIdentifier), internalCallback = WTF::move(internalCallback)](auto* backgroundFetchManager) mutable {
        if (!backgroundFetchManager) {
            callOnMainRunLoop(WTF::move(internalCallback));
            return;
        }
        backgroundFetchManager->clearFetch(fetchStorageIdentifier, [internalCallback = WTF::move(internalCallback)]() mutable {
            callOnMainRunLoop(WTF::move(internalCallback));
        });
    });
}

void BackgroundFetchStoreImpl::clearAllFetches(const ServiceWorkerRegistrationKey& key, CompletionHandler<void()>&& callback)
{
    RefPtr manager = m_manager.get();
    if (!manager)
        return callback();

    auto origin = key.clientOrigin();
    auto iterator = m_perClientOriginFetches.find(origin);
    if (iterator == m_perClientOriginFetches.end()) {
        callback();
        return;
    }

    Vector<String> fetchStorageIdentifiers;
    iterator->value.fetchToFilenames.removeIf([&] (auto& iterator) {
        if (iterator.key.first != key.scope().string())
            return false;
        fetchStorageIdentifiers.append(iterator.value);
        m_filenameToFetch.remove(iterator.value);
        return true;
    });

    clearAllFetchesInternal(origin, fetchStorageIdentifiers, [protectedThis = Ref { *this }, fetchStorageIdentifiers, callback = WTF::move(callback)]() mutable {
        RefPtr manager = protectedThis->m_manager.get();
        if (manager) {
            for (auto& fetchStorageIdentifier : fetchStorageIdentifiers)
                manager->notifyBackgroundFetchChange(fetchStorageIdentifier, BackgroundFetchChange::Removal);
        }
        callback();
    });
}

void BackgroundFetchStoreImpl::clearAllFetchesInternal(const WebCore::ClientOrigin& origin, const Vector<String>& fetchStorageIdentifiers, CompletionHandler<void()>&& internalCallback)
{
    RefPtr manager = m_manager.get();
    if (!manager)
        return internalCallback();

    manager->dispatchTaskToBackgroundFetchManager(origin, [fetchStorageIdentifiers = crossThreadCopy(fetchStorageIdentifiers), internalCallback = WTF::move(internalCallback)](auto* backgroundFetchManager) mutable {
        if (!backgroundFetchManager) {
            callOnMainRunLoop(WTF::move(internalCallback));
            return;
        }
        backgroundFetchManager->clearAllFetches(fetchStorageIdentifiers, [internalCallback = WTF::move(internalCallback)]() mutable {
            callOnMainRunLoop(WTF::move(internalCallback));
        });
    });
}

void BackgroundFetchStoreImpl::storeFetch(const ServiceWorkerRegistrationKey& key, const String& identifier, uint64_t downloadTotal, uint64_t uploadTotal, std::optional<size_t> responseBodyIndexToClear, Vector<uint8_t>&& fetch, CompletionHandler<void(StoreResult)>&& callback)
{
    RefPtr manager = m_manager.get();
    if (!manager)
        return callback(StoreResult::InternalError);

    auto origin = key.clientOrigin();
    auto iterator = m_perClientOriginFetches.find(origin);
    if (iterator == m_perClientOriginFetches.end()) {
        callback(StoreResult::InternalError);
        return;
    }

    auto fetchStorageIdentifier = iterator->value.fetchToFilenames.get(std::make_pair(key.scope().string(), identifier));
    bool isNewFetchStorageIdentifier = fetchStorageIdentifier.isEmpty();
    if (isNewFetchStorageIdentifier)
        fetchStorageIdentifier = BackgroundFetchStoreManager::createNewStorageIdentifier();

    storeFetchInternal(origin, fetchStorageIdentifier, downloadTotal, uploadTotal, responseBodyIndexToClear, WTF::move(fetch), [protectedThis = Ref { *this }, origin, key, identifier, fetchStorageIdentifier, isNewFetchStorageIdentifier, callback = WTF::move(callback)](StoreResult result) mutable {
        if (result == StoreResult::OK) {
            RefPtr manager = protectedThis->m_manager.get();
            if (manager)
                manager->notifyBackgroundFetchChange(fetchStorageIdentifier, isNewFetchStorageIdentifier ? BackgroundFetchChange::Addition : BackgroundFetchChange::Update);
            protectedThis->registerFetch(origin, key, identifier, WTF::move(fetchStorageIdentifier));
        }
        callback(result);
    });
}

void BackgroundFetchStoreImpl::storeFetchInternal(const WebCore::ClientOrigin& origin, const String& fetchStorageIdentifier, uint64_t downloadTotal, uint64_t uploadTotal, std::optional<size_t> responseBodyIndexToClear, Vector<uint8_t>&& fetch, CompletionHandler<void(StoreResult)>&& internalCallback)
{
    RefPtr manager = m_manager.get();
    if (!manager)
        return internalCallback(StoreResult::InternalError);

    manager->dispatchTaskToBackgroundFetchManager(origin, [fetchStorageIdentifier = crossThreadCopy(fetchStorageIdentifier), downloadTotal, uploadTotal, responseBodyIndexToClear, fetch = WTF::move(fetch), internalCallback = WTF::move(internalCallback)](auto* backgroundFetchManager) mutable {
        if (!backgroundFetchManager) {
            callOnMainRunLoop([internalCallback = WTF::move(internalCallback)]() mutable {
                internalCallback(StoreResult::InternalError);
            });
            return;
        }
        backgroundFetchManager->storeFetch(fetchStorageIdentifier, downloadTotal, uploadTotal, responseBodyIndexToClear, WTF::move(fetch), [internalCallback = WTF::move(internalCallback)](auto result) mutable {
            callOnMainRunLoop([result, internalCallback = WTF::move(internalCallback)]() mutable {
                internalCallback(result);
            });
        });
    });
}

void BackgroundFetchStoreImpl::storeFetchResponseBodyChunk(const ServiceWorkerRegistrationKey& key, const String& identifier, size_t index, const SharedBuffer& data, CompletionHandler<void(StoreResult)>&& callback)
{
    RefPtr manager = m_manager.get();
    if (!manager)
        return callback(StoreResult::InternalError);

    auto origin = key.clientOrigin();
    auto iterator = m_perClientOriginFetches.find(origin);
    if (iterator == m_perClientOriginFetches.end()) {
        callback(StoreResult::InternalError);
        return;
    }

    auto fetchStorageIdentifier = iterator->value.fetchToFilenames.get(std::make_pair(key.scope().string(), identifier));
    if (fetchStorageIdentifier.isEmpty()) {
        callback(StoreResult::InternalError);
        return;
    }

    storeFetchResponseBodyChunkInternal(origin, fetchStorageIdentifier, index, data, [protectedThis = Ref { *this }, fetchStorageIdentifier, callback = WTF::move(callback)](StoreResult result) mutable {
        RefPtr manager = protectedThis->m_manager.get();
        if (result == StoreResult::OK && manager)
            manager->notifyBackgroundFetchChange(fetchStorageIdentifier, BackgroundFetchChange::Update);
        callback(result);
    });
}

void BackgroundFetchStoreImpl::storeFetchResponseBodyChunkInternal(const WebCore::ClientOrigin& origin, const String& fetchStorageIdentifier, size_t index, const SharedBuffer& data, CompletionHandler<void(StoreResult)>&& internalCallback)
{
    RefPtr manager = m_manager.get();
    if (!manager)
        return internalCallback(StoreResult::InternalError);

    manager->dispatchTaskToBackgroundFetchManager(origin, [fetchStorageIdentifier = crossThreadCopy(fetchStorageIdentifier), index, data = Ref { data }, internalCallback = WTF::move(internalCallback)](auto* backgroundFetchManager) mutable {
        if (!backgroundFetchManager) {
            callOnMainRunLoop([internalCallback = WTF::move(internalCallback)]() mutable {
                internalCallback(StoreResult::InternalError);
            });
            return;
        }
        backgroundFetchManager->storeFetchResponseBodyChunk(fetchStorageIdentifier, index, data.get(), [internalCallback = WTF::move(internalCallback)](auto result) mutable {
            callOnMainRunLoop([result, internalCallback = WTF::move(internalCallback)]() mutable {
                internalCallback(result);
            });
        });
    });
}

void BackgroundFetchStoreImpl::retrieveResponseBody(const ServiceWorkerRegistrationKey& key, const String& identifier, size_t index, RetrieveRecordResponseBodyCallback&& callback)
{
    RefPtr manager = m_manager.get();
    if (!manager)
        return callback(makeUnexpected(ResourceError { errorDomainWebKitInternal, 0, { }, "Record not found"_s }));

    auto origin = key.clientOrigin();
    auto iterator = m_perClientOriginFetches.find(origin);
    if (iterator == m_perClientOriginFetches.end()) {
        callback(makeUnexpected(ResourceError { errorDomainWebKitInternal, 0, { }, "Record not found"_s }));
        return;
    }

    auto fetchStorageIdentifier = iterator->value.fetchToFilenames.get(std::make_pair(key.scope().string(), identifier));
    if (fetchStorageIdentifier.isEmpty()) {
        callback(makeUnexpected(ResourceError { errorDomainWebKitInternal, 0, { }, "Record not found"_s }));
        return;
    }

    manager->dispatchTaskToBackgroundFetchManager(origin, [fetchStorageIdentifier = crossThreadCopy(fetchStorageIdentifier), index, callback = WTF::move(callback)](auto* backgroundFetchManager) mutable {
        if (!backgroundFetchManager) {
            callOnMainRunLoop([callback = WTF::move(callback)]() mutable {
                callback(RefPtr<SharedBuffer> { });
            });
            return;
        }
        backgroundFetchManager->retrieveResponseBody(fetchStorageIdentifier, index, [callback = WTF::move(callback)](auto result) mutable {
            callOnMainRunLoop([result = WTF::move(result), callback = WTF::move(callback)]() mutable {
                callback(result);
            });
        });
    });
}

void BackgroundFetchStoreImpl::fetchInformationFromFilename(const String& filename, CompletionHandler<void(const ServiceWorkerRegistrationKey&, const String&)>&& callback)
{
    loadAllFetches([protectedThis = Ref { *this }, filename, callback = WTF::move(callback)]() mutable {
        auto iterator = protectedThis->m_filenameToFetch.find(filename);
        if (iterator == protectedThis->m_filenameToFetch.end()) {
            callback({ }, { });
            return;
        }

        ServiceWorkerRegistrationKey key { SecurityOriginData { iterator->value.origin.topOrigin }, URL { { }, iterator->value.identifier.first } };
        callback(key, iterator->value.identifier.second);
    });
}

void BackgroundFetchStoreImpl::loadAllFetches(CompletionHandler<void()>&& callback)
{
    RefPtr server = m_server.get();
    if (!server) {
        callback();
        return;
    }

    server->getAllOrigins([protectedThis = Ref { *this }, callback = WTF::move(callback)](auto&& origins) mutable {
        auto callbackAggregator = MainRunLoopCallbackAggregator::create(WTF::move(callback));
        for (auto& origin : origins)
            protectedThis->initializeFetches(origin, [callbackAggregator] { });
    });
}

void BackgroundFetchStoreImpl::getAllBackgroundFetchIdentifiers(CompletionHandler<void(Vector<String>&&)>&& callback)
{
    loadAllFetches([protectedThis = Ref { *this }, callback = WTF::move(callback)]() mutable {
        callback(copyToVector(protectedThis->m_filenameToFetch.keys()));
    });
}

void BackgroundFetchStoreImpl::getBackgroundFetchState(const String& backgroundFetchIdentifier, CompletionHandler<void(std::optional<BackgroundFetchState>&&)>&& callback)
{
    fetchInformationFromFilename(backgroundFetchIdentifier, [engine = WeakPtr { protect(m_server)->backgroundFetchEngine() }, callback = WTF::move(callback)](auto key, auto identifier) mutable {
        WeakPtr<BackgroundFetch> fetch = engine ? engine->backgroundFetch(key, identifier) : nullptr;
        if (!fetch) {
            callback({ });
            return;
        }
        auto information = fetch->information();
        callback({ { key.topOrigin(), key.scope(), fetch->identifier(), fetch->options(), information.downloadTotal, information.downloaded, information.uploadTotal, information.uploaded, information.result, information.failureReason, fetch->pausedFlagIsSet() } });
    });
}

void BackgroundFetchStoreImpl::abortBackgroundFetch(const String& filename, CompletionHandler<void()>&& callback)
{
    fetchInformationFromFilename(filename, [engine = WeakPtr { protect(m_server)->backgroundFetchEngine() }, callback = WTF::move(callback)](auto key, auto identifier) mutable {
        if (engine && !identifier.isNull())
            engine->abortBackgroundFetch(key, identifier);
        callback();
    });
}

void BackgroundFetchStoreImpl::pauseBackgroundFetch(const String& filename, CompletionHandler<void()>&& callback)
{
    fetchInformationFromFilename(filename, [engine = WeakPtr { protect(m_server)->backgroundFetchEngine() }, callback = WTF::move(callback)](auto key, auto identifier) mutable {
        if (engine && !identifier.isNull())
            engine->pauseBackgroundFetch(key, identifier);
        callback();
    });
}

void BackgroundFetchStoreImpl::resumeBackgroundFetch(const String& filename, CompletionHandler<void()>&& callback)
{
    fetchInformationFromFilename(filename, [engine = WeakPtr { protect(m_server)->backgroundFetchEngine() }, callback = WTF::move(callback)](auto key, auto identifier) mutable {
        if (engine && !identifier.isNull())
            engine->resumeBackgroundFetch(key, identifier);
        callback();
    });
}

void BackgroundFetchStoreImpl::clickBackgroundFetch(const String& filename, CompletionHandler<void()>&& callback)
{
    fetchInformationFromFilename(filename, [engine = WeakPtr { protect(m_server)->backgroundFetchEngine() }, callback = WTF::move(callback)](auto key, auto identifier) mutable {
        if (engine && !identifier.isNull())
            engine->clickBackgroundFetch(key, identifier);
        callback();
    });
}

} // namespace WebKit
