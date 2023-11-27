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
#include "BackgroundFetchEngine.h"

#include "BackgroundFetchInformation.h"
#include "BackgroundFetchRecordInformation.h"
#include "ExceptionData.h"
#include "Logging.h"
#include "RetrieveRecordsOptions.h"
#include "SWServerRegistration.h"
#include "SWServerToContextConnection.h"

namespace WebCore {

BackgroundFetchEngine::BackgroundFetchEngine(SWServer& server)
    : m_server(server)
    , m_store(server.createBackgroundFetchStore())
{
}

// https://wicg.github.io/background-fetch/#dom-backgroundfetchmanager-fetch starting from step 8.3
void BackgroundFetchEngine::startBackgroundFetch(SWServerRegistration& registration, const String& backgroundFetchIdentifier, Vector<BackgroundFetchRequest>&& requests, BackgroundFetchOptions&& options, ExceptionOrBackgroundFetchInformationCallback&& callback)
{
    auto iterator = m_fetches.find(registration.key());
    if (iterator == m_fetches.end()) {
        m_store->initializeFetches(registration.key(), [weakThis = WeakPtr { *this }, registration = WeakPtr { registration }, backgroundFetchIdentifier, requests = WTFMove(requests), options = WTFMove(options), callback = WTFMove(callback)]() mutable {
            if (!weakThis || !registration) {
                callback(makeUnexpected(ExceptionData { ExceptionCode::InvalidStateError, "BackgroundFetchEngine is gone"_s }));
                return;
            }
            weakThis->m_fetches.ensure(registration->key(), [] {
                return FetchesMap();
            });
            weakThis->startBackgroundFetch(*registration, backgroundFetchIdentifier, WTFMove(requests), WTFMove(options), WTFMove(callback));
        });
        return;
    }

    auto result = iterator->value.ensure(backgroundFetchIdentifier, [&]() mutable {
        return makeUnique<BackgroundFetch>(registration, backgroundFetchIdentifier, WTFMove(requests), WTFMove(options), Ref { m_store }, [weakThis = WeakPtr { *this }](auto& fetch) {
            if (weakThis)
                weakThis->notifyBackgroundFetchUpdate(fetch);
        });
    });
    if (!result.isNewEntry) {
        callback(makeUnexpected(ExceptionData { ExceptionCode::TypeError, "A background fetch registration already exists"_s }));
        return;
    }

    auto& fetch = *result.iterator->value;
    fetch.doStore([server = m_server, fetch = WeakPtr { fetch }, callback = WTFMove(callback)](auto result) mutable {
        if (!fetch || !server) {
            callback(makeUnexpected(ExceptionData { ExceptionCode::TypeError, "Background fetch is gone"_s }));
            return;
        }
        switch (result) {
        case BackgroundFetchStore::StoreResult::QuotaError:
            callback(makeUnexpected(ExceptionData { ExceptionCode::QuotaExceededError, "Background fetch requested space is above quota"_s }));
            break;
        case BackgroundFetchStore::StoreResult::InternalError:
            callback(makeUnexpected(ExceptionData { ExceptionCode::TypeError, "Background fetch store operation failed"_s }));
            break;
        case BackgroundFetchStore::StoreResult::OK:
            if (!fetch->pausedFlagIsSet()) {
                fetch->perform([server = WTFMove(server)](auto& client, auto& request, auto responseDataSize, auto& origin) mutable {
                    return server ? server->createBackgroundFetchRecordLoader(client, request, responseDataSize, origin) : nullptr;
                });
            }
            callback(fetch->information());
            break;
        };
    });
}

// https://wicg.github.io/background-fetch/#update-background-fetch-instance-algorithm
void BackgroundFetchEngine::notifyBackgroundFetchUpdate(BackgroundFetch& fetch)
{
    auto information = fetch.information();
    auto* registration = m_server->getRegistration(information.registrationIdentifier);
    if (!registration)
        return;

    // Progress event.
    registration->forEachConnection([&](auto& connection) {
        connection.updateBackgroundFetchRegistration(information);
    });

    if (information.result == BackgroundFetchResult::EmptyString || !information.recordsAvailable)
        return;

    // FIXME: We should delay events if the service worker (or related page) is not running.
    m_server->fireBackgroundFetchEvent(*registration, WTFMove(information), [weakFetch = WeakPtr { fetch }]() {
        if (weakFetch)
            weakFetch->unsetRecordsAvailableFlag();
    });
}

void BackgroundFetchEngine::backgroundFetchInformation(SWServerRegistration& registration, const String& backgroundFetchIdentifier, ExceptionOrBackgroundFetchInformationCallback&& callback)
{
    auto iterator = m_fetches.find(registration.key());
    if (iterator == m_fetches.end()) {
        m_store->initializeFetches(registration.key(), [weakThis = WeakPtr { *this }, registration = WeakPtr { registration }, backgroundFetchIdentifier, callback = WTFMove(callback)]() mutable {
            if (!weakThis || !registration) {
                callback(makeUnexpected(ExceptionData { ExceptionCode::InvalidStateError, "BackgroundFetchEngine is gone"_s }));
                return;
            }
            weakThis->m_fetches.ensure(registration->key(), [] {
                return FetchesMap();
            });
            weakThis->backgroundFetchInformation(*registration, backgroundFetchIdentifier, WTFMove(callback));
        });
        return;
    }

    auto& map = iterator->value;
    auto fetchIterator = map.find(backgroundFetchIdentifier);
    if (fetchIterator == map.end()) {
        callback(BackgroundFetchInformation { });
        return;
    }
    callback(fetchIterator->value->information());
}

// https://wicg.github.io/background-fetch/#dom-backgroundfetchmanager-getids
void BackgroundFetchEngine::backgroundFetchIdentifiers(SWServerRegistration& registration, BackgroundFetchIdentifiersCallback&& callback)
{
    auto iterator = m_fetches.find(registration.key());
    if (iterator == m_fetches.end()) {
        m_store->initializeFetches(registration.key(), [weakThis = WeakPtr { *this }, registration = WeakPtr { registration }, callback = WTFMove(callback)]() mutable {
            if (!weakThis || !registration) {
                callback({ });
                return;
            }
            weakThis->m_fetches.ensure(registration->key(), [] {
                return FetchesMap();
            });
            weakThis->backgroundFetchIdentifiers(*registration, WTFMove(callback));
        });
        return;
    }

    Vector<String> identifiers = WTF::compactMap(iterator->value, [](auto& keyValue) -> std::optional<String> {
        if (keyValue.value->isActive())
            return keyValue.key;
        return std::nullopt;
    });
    callback(WTFMove(identifiers));
}

// https://wicg.github.io/background-fetch/#background-fetch-registration-abort starting from step 3
void BackgroundFetchEngine::abortBackgroundFetch(SWServerRegistration& registration, const String& backgroundFetchIdentifier, AbortBackgroundFetchCallback&& callback)
{
    auto iterator = m_fetches.find(registration.key());
    if (iterator == m_fetches.end()) {
        m_store->initializeFetches(registration.key(), [weakThis = WeakPtr { *this }, registration = WeakPtr { registration }, backgroundFetchIdentifier, callback = WTFMove(callback)]() mutable {
            if (!weakThis || !registration) {
                callback(false);
                return;
            }
            weakThis->m_fetches.ensure(registration->key(), [] {
                return FetchesMap();
            });
            weakThis->abortBackgroundFetch(*registration, backgroundFetchIdentifier, WTFMove(callback));
        });
        return;
    }

    auto& map = iterator->value;
    auto fetchIterator = map.find(backgroundFetchIdentifier);
    if (fetchIterator == map.end()) {
        callback(false);
        return;
    }
    callback(fetchIterator->value->abort());
}

// https://wicg.github.io/background-fetch/#dom-backgroundfetchregistration-matchall starting from step 3
void BackgroundFetchEngine::matchBackgroundFetch(SWServerRegistration& registration, const String& backgroundFetchIdentifier, RetrieveRecordsOptions&& options, MatchBackgroundFetchCallback&& callback)
{
    auto iterator = m_fetches.find(registration.key());
    if (iterator == m_fetches.end()) {
        m_store->initializeFetches(registration.key(), [weakThis = WeakPtr { *this }, registration = WeakPtr { registration }, backgroundFetchIdentifier, options = WTFMove(options), callback = WTFMove(callback)]() mutable {
            if (!weakThis || !registration) {
                callback({ });
                return;
            }
            weakThis->m_fetches.ensure(registration->key(), [] {
                return FetchesMap();
            });
            weakThis->matchBackgroundFetch(*registration, backgroundFetchIdentifier, WTFMove(options), WTFMove(callback));
        });
        return;
    }

    auto& map = iterator->value;
    auto fetchIterator = map.find(backgroundFetchIdentifier);
    if (fetchIterator == map.end()) {
        callback({ });
        return;
    }
    fetchIterator->value->match(options, [weakThis = WeakPtr { *this }, callback = WTFMove(callback)](auto&& records) mutable {
        if (!weakThis) {
            callback({ });
            return;
        }
        auto recordsInformation = WTF::map(WTFMove(records), [&](auto&& record) {
            // FIXME: We need a way to remove the record from m_records.
            auto information = record->information();
            weakThis->m_records.add(information.identifier, WTFMove(record));
            return information;
        });
        callback(WTFMove(recordsInformation));
    });
}

void BackgroundFetchEngine::remove(SWServerRegistration& registration)
{
    // FIXME: We skip the initialization step, which might invalidate some results, maybe we should have a specific handling here.
    auto fetches = m_fetches.take(registration.key());
    for (auto& fetch : fetches.values())
        fetch->abort();
    m_store->clearAllFetches(registration.key());
}

void BackgroundFetchEngine::retrieveRecordResponse(BackgroundFetchRecordIdentifier recordIdentifier, RetrieveRecordResponseCallback&& callback)
{
    auto record = m_records.get(recordIdentifier);
    if (!record) {
        callback(makeUnexpected(ExceptionData { ExceptionCode::InvalidStateError, "Record not found"_s }));
        return;
    }
    record->retrieveResponse(m_store.get(), WTFMove(callback));
}

void BackgroundFetchEngine::retrieveRecordResponseBody(BackgroundFetchRecordIdentifier recordIdentifier, RetrieveRecordResponseBodyCallback&& callback)
{
    auto record = m_records.get(recordIdentifier);
    if (!record) {
        callback(makeUnexpected(ResourceError { errorDomainWebKitInternal, 0, { }, "Record not found"_s }));
        return;
    }
    record->retrieveRecordResponseBody(m_store.get(), WTFMove(callback));
}

void BackgroundFetchEngine::addFetchFromStore(std::span<const uint8_t> data, CompletionHandler<void(const ServiceWorkerRegistrationKey&, const String&)>&& callback)
{
    auto fetch = BackgroundFetch::createFromStore(data, *m_server, m_store.get(), [weakThis = WeakPtr { *this }](auto& fetch) {
        if (weakThis)
            weakThis->notifyBackgroundFetchUpdate(fetch);
    });
    if (!fetch) {
        RELEASE_LOG_ERROR(ServiceWorker, "BackgroundFetchEngine failed adding fetch entry as registration is missing");
        callback({ }, { });
        return;
    }

    callback(fetch->registrationKey(), fetch->identifier());

    auto& fetchMap = m_fetches.ensure(fetch->registrationKey(), [] {
        return FetchesMap();
    }).iterator->value;

    auto backgroundFetchIdentifier = fetch->identifier();
    ASSERT(!fetchMap.contains(backgroundFetchIdentifier));
    fetchMap.add(WTFMove(backgroundFetchIdentifier), WTFMove(fetch));
}

void BackgroundFetchEngine::abortBackgroundFetch(const ServiceWorkerRegistrationKey& key, const String& identifier)
{
    if (auto *registration = m_server ? m_server->getRegistration(key) : nullptr)
        abortBackgroundFetch(*registration, identifier, [](auto) { });
}

void BackgroundFetchEngine::pauseBackgroundFetch(const ServiceWorkerRegistrationKey& key, const String& identifier)
{
    auto* registration = m_server ? m_server->getRegistration(key) : nullptr;
    if (!registration)
        return;

    auto iterator = m_fetches.find(key);
    if (iterator == m_fetches.end())
        return;

    auto& map = iterator->value;
    auto fetchIterator = map.find(identifier);
    if (fetchIterator == map.end())
        return;

    fetchIterator->value->pause();
}

void BackgroundFetchEngine::resumeBackgroundFetch(const ServiceWorkerRegistrationKey& key, const String& identifier)
{
    auto* registration = m_server ? m_server->getRegistration(key) : nullptr;
    if (!registration)
        return;

    auto iterator = m_fetches.find(key);
    if (iterator == m_fetches.end())
        return;

    auto& map = iterator->value;
    auto fetchIterator = map.find(identifier);
    if (fetchIterator == map.end())
        return;

    fetchIterator->value->resume([server = m_server](auto& client, auto& request, auto responseDataSize, auto& origin) mutable {
        return server ? server->createBackgroundFetchRecordLoader(client, request, responseDataSize, origin) : nullptr;
    });
}

void BackgroundFetchEngine::clickBackgroundFetch(const ServiceWorkerRegistrationKey& key, const String& backgroundFetchIdentifier)
{
    auto* registration = m_server ? m_server->getRegistration(key) : nullptr;
    if (!registration)
        return;

    auto iterator = m_fetches.find(key);
    if (iterator == m_fetches.end())
        return;

    auto& map = iterator->value;
    auto fetchIterator = map.find(backgroundFetchIdentifier);
    if (fetchIterator == map.end())
        return;

    m_server->fireBackgroundFetchClickEvent(*registration, fetchIterator->value->information());
}

WeakPtr<BackgroundFetch> BackgroundFetchEngine::backgroundFetch(const ServiceWorkerRegistrationKey& key, const String& identifier) const
{
    auto iterator = m_fetches.find(key);
    if (iterator == m_fetches.end())
        return { };

    return iterator->value.get(identifier);
}

} // namespace WebCore
