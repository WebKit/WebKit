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

BackgroundFetchStoreImpl::BackgroundFetchStoreImpl(WeakPtr<NetworkStorageManager>&& manager, WeakPtr<WebCore::SWServer>&& server)
    : m_manager(WTFMove(manager))
    , m_server(WTFMove(server))
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
        iterator->value.fetchToFilenames.add(std::make_pair(key.scope().string(), backgroundFetchIdentifier), WTFMove(fetchStorageIdentifier));
}

void BackgroundFetchStoreImpl::initializeFetches(const ServiceWorkerRegistrationKey& key, CompletionHandler<void()>&& callback)
{
    initializeFetches({ key.topOrigin(), SecurityOriginData::fromURL(key.scope()) }, WTFMove(callback));
}

void BackgroundFetchStoreImpl::initializeFetches(const WebCore::ClientOrigin& origin, CompletionHandler<void()>&& callback)
{
    if (!m_manager) {
        callback();
        return;
    }

    auto addResult = m_perClientOriginFetches.add(origin, PerClientOriginFetches { });
    if (!addResult.isNewEntry && addResult.iterator->value.initializationCallbacks.isEmpty()) {
        callback();
        return;
    }

    addResult.iterator->value.initializationCallbacks.append(WTFMove(callback));

    initializeFetchesInternal(origin, [origin, weakEngine = WeakPtr { m_server->backgroundFetchEngine() }, protectedThis = Ref { *this }, manager = m_manager](Vector<std::pair<RefPtr<WebCore::SharedBuffer>, String>>&& fetches) {
        if (weakEngine && manager) {
            for (auto& fetch : fetches) {
                weakEngine->addFetchFromStore({ fetch.first->data(), fetch.first->size() }, [&](auto& key, auto& identifier) {
                    if (identifier.isEmpty()) {
                        manager->dispatchTaskToBackgroundFetchManager(origin, [identifier = crossThreadCopy(WTFMove(fetch.second))](auto* backgroundFetchManager) {
                            if (backgroundFetchManager)
                                backgroundFetchManager->clearFetch(identifier, [] { });
                        });
                        return;
                    }

                    protectedThis->registerFetch(origin, key, identifier, WTFMove(fetch.second));
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
    m_manager->dispatchTaskToBackgroundFetchManager(origin, [internalCallback = WTFMove(internalCallback)](auto* backgroundFetchManager) mutable {
        if (!backgroundFetchManager) {
            callOnMainRunLoop([internalCallback = WTFMove(internalCallback)]() mutable {
                internalCallback({ });
            });
            return;
        }
        backgroundFetchManager->initializeFetches([internalCallback = WTFMove(internalCallback)](auto&& fetches) mutable {
            callOnMainRunLoop([fetches = std::forward<decltype(fetches)>(fetches), internalCallback = WTFMove(internalCallback)]() mutable {
                internalCallback(WTFMove(fetches));
            });
        });
    });
}

void BackgroundFetchStoreImpl::clearFetch(const ServiceWorkerRegistrationKey& key, const String& identifier, CompletionHandler<void()>&& callback)
{
    if (!m_manager) {
        callback();
        return;
    }

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

    clearFetchInternal(origin, fetchStorageIdentifier, [protectedThis = Ref { *this }, fetchStorageIdentifier, callback = WTFMove(callback)]() mutable {
        if (protectedThis->m_manager)
            protectedThis->m_manager->notifyBackgroundFetchChange(fetchStorageIdentifier, BackgroundFetchChange::Removal);
        callback();
    });
}

void BackgroundFetchStoreImpl::clearFetchInternal(const WebCore::ClientOrigin& origin, const String& fetchStorageIdentifier, CompletionHandler<void()>&& internalCallback)
{
    m_manager->dispatchTaskToBackgroundFetchManager(origin, [fetchStorageIdentifier = crossThreadCopy(fetchStorageIdentifier), internalCallback = WTFMove(internalCallback)](auto* backgroundFetchManager) mutable {
        if (!backgroundFetchManager) {
            callOnMainRunLoop(WTFMove(internalCallback));
            return;
        }
        backgroundFetchManager->clearFetch(fetchStorageIdentifier, [internalCallback = WTFMove(internalCallback)]() mutable {
            callOnMainRunLoop(WTFMove(internalCallback));
        });
    });
}

void BackgroundFetchStoreImpl::clearAllFetches(const ServiceWorkerRegistrationKey& key, CompletionHandler<void()>&& callback)
{
    if (!m_manager) {
        callback();
        return;
    }

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

    clearAllFetchesInternal(origin, fetchStorageIdentifiers, [protectedThis = Ref { *this }, fetchStorageIdentifiers, callback = WTFMove(callback)]() mutable {
        if (protectedThis->m_manager) {
            for (auto& fetchStorageIdentifier : fetchStorageIdentifiers)
                protectedThis->m_manager->notifyBackgroundFetchChange(fetchStorageIdentifier, BackgroundFetchChange::Removal);
        }
        callback();
    });
}

void BackgroundFetchStoreImpl::clearAllFetchesInternal(const WebCore::ClientOrigin& origin, const Vector<String>& fetchStorageIdentifiers, CompletionHandler<void()>&& internalCallback)
{
    m_manager->dispatchTaskToBackgroundFetchManager(origin, [fetchStorageIdentifiers = crossThreadCopy(fetchStorageIdentifiers), internalCallback = WTFMove(internalCallback)](auto* backgroundFetchManager) mutable {
        if (!backgroundFetchManager) {
            callOnMainRunLoop(WTFMove(internalCallback));
            return;
        }
        backgroundFetchManager->clearAllFetches(fetchStorageIdentifiers, [internalCallback = WTFMove(internalCallback)]() mutable {
            callOnMainRunLoop(WTFMove(internalCallback));
        });
    });
}

void BackgroundFetchStoreImpl::storeFetch(const ServiceWorkerRegistrationKey& key, const String& identifier, uint64_t downloadTotal, uint64_t uploadTotal, std::optional<size_t> responseBodyIndexToClear, Vector<uint8_t>&& fetch, CompletionHandler<void(StoreResult)>&& callback)
{
    if (!m_manager) {
        callback(StoreResult::InternalError);
        return;
    }

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

    storeFetchInternal(origin, fetchStorageIdentifier, downloadTotal, uploadTotal, responseBodyIndexToClear, WTFMove(fetch), [protectedThis = Ref { *this }, origin, key, identifier, fetchStorageIdentifier, isNewFetchStorageIdentifier, callback = WTFMove(callback)](StoreResult result) mutable {
        if (result == StoreResult::OK) {
            if (protectedThis->m_manager)
                protectedThis->m_manager->notifyBackgroundFetchChange(fetchStorageIdentifier, isNewFetchStorageIdentifier ? BackgroundFetchChange::Addition : BackgroundFetchChange::Update);
            protectedThis->registerFetch(origin, key, identifier, WTFMove(fetchStorageIdentifier));
        }
        callback(result);
    });
}

void BackgroundFetchStoreImpl::storeFetchInternal(const WebCore::ClientOrigin& origin, const String& fetchStorageIdentifier, uint64_t downloadTotal, uint64_t uploadTotal, std::optional<size_t> responseBodyIndexToClear, Vector<uint8_t>&& fetch, CompletionHandler<void(StoreResult)>&& internalCallback)
{
    m_manager->dispatchTaskToBackgroundFetchManager(origin, [fetchStorageIdentifier = crossThreadCopy(fetchStorageIdentifier), downloadTotal, uploadTotal, responseBodyIndexToClear, fetch = WTFMove(fetch), internalCallback = WTFMove(internalCallback)](auto* backgroundFetchManager) mutable {
        if (!backgroundFetchManager) {
            callOnMainRunLoop([internalCallback = WTFMove(internalCallback)]() mutable {
                internalCallback(StoreResult::InternalError);
            });
            return;
        }
        backgroundFetchManager->storeFetch(fetchStorageIdentifier, downloadTotal, uploadTotal, responseBodyIndexToClear, WTFMove(fetch), [internalCallback = WTFMove(internalCallback)](auto result) mutable {
            callOnMainRunLoop([result, internalCallback = WTFMove(internalCallback)]() mutable {
                internalCallback(result);
            });
        });
    });
}

void BackgroundFetchStoreImpl::storeFetchResponseBodyChunk(const ServiceWorkerRegistrationKey& key, const String& identifier, size_t index, const SharedBuffer& data, CompletionHandler<void(StoreResult)>&& callback)
{
    if (!m_manager) {
        callback(StoreResult::InternalError);
        return;
    }

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

    storeFetchResponseBodyChunkInternal(origin, fetchStorageIdentifier, index, data, [protectedThis = Ref { *this }, fetchStorageIdentifier, callback = WTFMove(callback)](StoreResult result) mutable {
        if (result == StoreResult::OK && protectedThis->m_manager)
            protectedThis->m_manager->notifyBackgroundFetchChange(fetchStorageIdentifier, BackgroundFetchChange::Update);
        callback(result);
    });
}

void BackgroundFetchStoreImpl::storeFetchResponseBodyChunkInternal(const WebCore::ClientOrigin& origin, const String& fetchStorageIdentifier, size_t index, const SharedBuffer& data, CompletionHandler<void(StoreResult)>&& internalCallback)
{
    m_manager->dispatchTaskToBackgroundFetchManager(origin, [fetchStorageIdentifier = crossThreadCopy(fetchStorageIdentifier), index, data = Ref { data }, internalCallback = WTFMove(internalCallback)](auto* backgroundFetchManager) mutable {
        if (!backgroundFetchManager) {
            callOnMainRunLoop([internalCallback = WTFMove(internalCallback)]() mutable {
                internalCallback(StoreResult::InternalError);
            });
            return;
        }
        backgroundFetchManager->storeFetchResponseBodyChunk(fetchStorageIdentifier, index, data.get(), [internalCallback = WTFMove(internalCallback)](auto result) mutable {
            callOnMainRunLoop([result, internalCallback = WTFMove(internalCallback)]() mutable {
                internalCallback(result);
            });
        });
    });
}

void BackgroundFetchStoreImpl::retrieveResponseBody(const ServiceWorkerRegistrationKey& key, const String& identifier, size_t index, RetrieveRecordResponseBodyCallback&& callback)
{
    if (!m_manager) {
        callback(makeUnexpected(ResourceError { errorDomainWebKitInternal, 0, { }, "Record not found"_s }));
        return;
    }

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

    m_manager->dispatchTaskToBackgroundFetchManager(origin, [fetchStorageIdentifier = crossThreadCopy(fetchStorageIdentifier), index, callback = WTFMove(callback)](auto* backgroundFetchManager) mutable {
        if (!backgroundFetchManager) {
            callOnMainRunLoop([callback = WTFMove(callback)]() mutable {
                callback(RefPtr<SharedBuffer> { });
            });
            return;
        }
        backgroundFetchManager->retrieveResponseBody(fetchStorageIdentifier, index, [callback = WTFMove(callback)](auto result) mutable {
            callOnMainRunLoop([result = WTFMove(result), callback = WTFMove(callback)]() mutable {
                callback(result);
            });
        });
    });
}

void BackgroundFetchStoreImpl::fetchInformationFromFilename(const String& filename, CompletionHandler<void(const ServiceWorkerRegistrationKey&, const String&)>&& callback)
{
    loadAllFetches([protectedThis = Ref { *this }, filename, callback = WTFMove(callback)]() mutable {
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
    if (!m_server) {
        callback();
        return;
    }

    m_server->getAllOrigins([protectedThis = Ref { *this }, callback = WTFMove(callback)](auto&& origins) mutable {
        auto callbackAggregator = MainRunLoopCallbackAggregator::create(WTFMove(callback));
        for (auto& origin : origins)
            protectedThis->initializeFetches(origin, [callbackAggregator] { });
    });
}

void BackgroundFetchStoreImpl::getAllBackgroundFetchIdentifiers(CompletionHandler<void(Vector<String>&&)>&& callback)
{
    loadAllFetches([protectedThis = Ref { *this }, callback = WTFMove(callback)]() mutable {
        callback(copyToVector(protectedThis->m_filenameToFetch.keys()));
    });
}

void BackgroundFetchStoreImpl::getBackgroundFetchState(const String& backgroundFetchIdentifier, CompletionHandler<void(std::optional<BackgroundFetchState>&&)>&& callback)
{
    fetchInformationFromFilename(backgroundFetchIdentifier, [engine = WeakPtr { m_server->backgroundFetchEngine() }, callback = WTFMove(callback)](auto key, auto identifier) mutable {
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
    fetchInformationFromFilename(filename, [engine = WeakPtr { m_server->backgroundFetchEngine() }, callback = WTFMove(callback)](auto key, auto identifier) mutable {
        if (engine && !identifier.isNull())
            engine->abortBackgroundFetch(key, identifier);
        callback();
    });
}

void BackgroundFetchStoreImpl::pauseBackgroundFetch(const String& filename, CompletionHandler<void()>&& callback)
{
    fetchInformationFromFilename(filename, [engine = WeakPtr { m_server->backgroundFetchEngine() }, callback = WTFMove(callback)](auto key, auto identifier) mutable {
        if (engine && !identifier.isNull())
            engine->pauseBackgroundFetch(key, identifier);
        callback();
    });
}

void BackgroundFetchStoreImpl::resumeBackgroundFetch(const String& filename, CompletionHandler<void()>&& callback)
{
    fetchInformationFromFilename(filename, [engine = WeakPtr { m_server->backgroundFetchEngine() }, callback = WTFMove(callback)](auto key, auto identifier) mutable {
        if (engine && !identifier.isNull())
            engine->resumeBackgroundFetch(key, identifier);
        callback();
    });
}

void BackgroundFetchStoreImpl::clickBackgroundFetch(const String& filename, CompletionHandler<void()>&& callback)
{
    fetchInformationFromFilename(filename, [engine = WeakPtr { m_server->backgroundFetchEngine() }, callback = WTFMove(callback)](auto key, auto identifier) mutable {
        if (engine && !identifier.isNull())
            engine->clickBackgroundFetch(key, identifier);
        callback();
    });
}

} // namespace WebKit
