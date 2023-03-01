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


#if ENABLE(SERVICE_WORKER)

#include "BackgroundFetchStoreManager.h"
#include "NetworkStorageManager.h"
#include <WebCore/BackgroundFetchEngine.h>
#include <WebCore/BackgroundFetchOptions.h>
#include <WebCore/BackgroundFetchRecordInformation.h>
#include <WebCore/DOMCacheEngine.h>
#include <WebCore/SWServerRegistration.h>

namespace WebKit {

using namespace WebCore;

// FIXME: Handle quota.

BackgroundFetchStoreImpl::BackgroundFetchStoreImpl(WeakPtr<NetworkStorageManager>&& manager)
    : m_manager(WTFMove(manager))
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

void BackgroundFetchStoreImpl::initializeFetches(BackgroundFetchEngine& engine, const ServiceWorkerRegistrationKey& key, CompletionHandler<void()>&& callback)
{
    if (!m_manager) {
        callback();
        return;
    }

    auto origin = key.clientOrigin();
    auto addResult = m_perClientOriginFetches.add(origin, PerClientOriginFetches { });
    if (!addResult.isNewEntry && addResult.iterator->value.initializationCallbacks.isEmpty()) {
        callback();
        return;
    }

    addResult.iterator->value.initializationCallbacks.append(WTFMove(callback));
    auto internalCallback = [origin, weakEngine = WeakPtr { engine }, protectedThis = Ref { *this }, manager = m_manager](Vector<std::pair<RefPtr<WebCore::SharedBuffer>, String>>&& fetches) {
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
                    auto iterator = protectedThis->m_perClientOriginFetches.find(origin);
                    if (iterator != protectedThis->m_perClientOriginFetches.end())
                        iterator->value.fetchToFilenames.add(std::make_pair(key.scope().string(), identifier), WTFMove(fetch.second));
                });
            }
        }

        auto iterator = protectedThis->m_perClientOriginFetches.find(origin);
        if (iterator == protectedThis->m_perClientOriginFetches.end())
            return;

        auto callbacks = std::exchange(iterator->value.initializationCallbacks, { });
        for (auto& callback : callbacks)
            callback();
    };

    m_manager->dispatchTaskToBackgroundFetchManager(origin, [internalCallback = WTFMove(internalCallback)](auto* backgroundFetchManager) mutable {
        if (!backgroundFetchManager) {
            callOnMainRunLoop([internalCallback = WTFMove(internalCallback)]() mutable {
                internalCallback({ });
            });
            return;
        }
        backgroundFetchManager->initializeFetches([internalCallback = WTFMove(internalCallback)](auto&& fetches) mutable {
            callOnMainRunLoop([fetches = WTFMove(fetches), internalCallback = WTFMove(internalCallback)]() mutable {
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

    m_manager->dispatchTaskToBackgroundFetchManager(origin, [fetchStorageIdentifier = crossThreadCopy(fetchStorageIdentifier), callback = WTFMove(callback)](auto* backgroundFetchManager) mutable {
        if (!backgroundFetchManager) {
            callOnMainRunLoop(WTFMove(callback));
            return;
        }
        backgroundFetchManager->clearFetch(fetchStorageIdentifier, [callback = WTFMove(callback)]() mutable {
            callOnMainRunLoop(WTFMove(callback));
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
    iterator->value.fetchToFilenames.removeIf([&fetchStorageIdentifiers, &key] (auto& iterator) {
        if (iterator.key.first != key.scope().string())
            return false;
        fetchStorageIdentifiers.append(iterator.value);
        return true;
    });

    m_manager->dispatchTaskToBackgroundFetchManager(origin, [fetchStorageIdentifiers = crossThreadCopy(WTFMove(fetchStorageIdentifiers)), callback = WTFMove(callback)](auto* backgroundFetchManager) mutable {
        if (!backgroundFetchManager) {
            callOnMainRunLoop(WTFMove(callback));
            return;
        }
        backgroundFetchManager->clearAllFetches(fetchStorageIdentifiers, [callback = WTFMove(callback)]() mutable {
            callOnMainRunLoop(WTFMove(callback));
        });
    });
}

void BackgroundFetchStoreImpl::storeFetch(const ServiceWorkerRegistrationKey& key, const String& identifier, Vector<uint8_t>&& fetch, CompletionHandler<void(StoreResult)>&& callback)
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
    if (fetchStorageIdentifier.isEmpty())
        fetchStorageIdentifier = BackgroundFetchStoreManager::createNewStorageIdentifier();

    auto internalCallback = [protectedThis = Ref { *this }, origin, key, identifier, fetchStorageIdentifier, callback = WTFMove(callback)](StoreResult result) mutable {
        if (result == StoreResult::OK) {
            auto iterator = protectedThis->m_perClientOriginFetches.find(origin);
            if (iterator != protectedThis->m_perClientOriginFetches.end())
                iterator->value.fetchToFilenames.add(std::make_pair(key.scope().string(), identifier), WTFMove(fetchStorageIdentifier));
        }
        callback(result);
    };

    m_manager->dispatchTaskToBackgroundFetchManager(origin, [fetchStorageIdentifier = crossThreadCopy(fetchStorageIdentifier), fetch = WTFMove(fetch), internalCallback = WTFMove(internalCallback)](auto* backgroundFetchManager) mutable {
        if (!backgroundFetchManager) {
            callOnMainRunLoop([internalCallback = WTFMove(internalCallback)]() mutable {
                internalCallback(StoreResult::InternalError);
            });
            return;
        }
        backgroundFetchManager->storeFetch(fetchStorageIdentifier, WTFMove(fetch), [internalCallback = WTFMove(internalCallback)](auto result) mutable {
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

    m_manager->dispatchTaskToBackgroundFetchManager(origin, [fetchStorageIdentifier = crossThreadCopy(fetchStorageIdentifier), index, data = Ref { data }, callback = WTFMove(callback)](auto* backgroundFetchManager) mutable {
        if (!backgroundFetchManager) {
            callOnMainRunLoop([callback = WTFMove(callback)]() mutable {
                callback(StoreResult::InternalError);
            });
            return;
        }
        backgroundFetchManager->storeFetchResponseBodyChunk(fetchStorageIdentifier, index, data.get(), [callback = WTFMove(callback)](auto result) mutable {
            callOnMainRunLoop([result, callback = WTFMove(callback)]() mutable {
                callback(result);
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

} // namespace WebKit

#endif // ENABLE(SERVICE_WORKER)
