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

#if ENABLE(SERVICE_WORKER)

#include "ExceptionData.h"
#include "Logging.h"
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
        m_store->initializeFetches(*this, registration.key(), [weakThis = WeakPtr { *this }, registration = WeakPtr { registration }, backgroundFetchIdentifier, requests = WTFMove(requests), options = WTFMove(options), callback = WTFMove(callback)]() mutable {
            if (!weakThis || !registration) {
                callback(makeUnexpected(ExceptionData { InvalidStateError, "BackgroundFetchEngine is gone"_s }));
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
        return makeUnique<BackgroundFetch>(registration, backgroundFetchIdentifier, WTFMove(requests), WTFMove(options), Ref { m_store }, [weakThis = WeakPtr { *this }](auto&& information) {
            if (weakThis)
                weakThis->notifyBackgroundFetchUpdate(WTFMove(information));
        });
    });
    if (!result.isNewEntry) {
        callback(makeUnexpected(ExceptionData { TypeError, "A background fetch registration already exists"_s }));
        return;
    }

    WeakPtr fetch { *result.iterator->value };

    // FIXME: We should wait for upload data to be fully read.
    uint64_t expectedSpace;
    bool isSafe = WTF::safeAdd(fetch->downloadTotal(), fetch->uploadTotal(), expectedSpace);
    if (!isSafe) {
        callback(makeUnexpected(ExceptionData { QuotaExceededError, "Background fetch requested space is above quota"_s }));
        return;
    }

    m_server->requestBackgroundFetchSpace(fetch->origin(), expectedSpace, [server = m_server, fetch = WTFMove(fetch), callback = WTFMove(callback)](bool result) mutable {
        if (!fetch || !server) {
            callback(makeUnexpected(ExceptionData { TypeError, "Background fetch is gone"_s }));
            return;
        }
        if (!result) {
            callback(makeUnexpected(ExceptionData { QuotaExceededError, "Background fetch requested space is above quota"_s }));
            return;
        }

        fetch->perform([server = WTFMove(server)](auto& client, auto&& request, auto&& options, auto& origin) mutable {
            return server ? server->createBackgroundFetchRecordLoader(client, WTFMove(request), WTFMove(options), origin) : nullptr;
        });

        callback(fetch->information());
    });
}

// https://wicg.github.io/background-fetch/#update-background-fetch-instance-algorithm
void BackgroundFetchEngine::notifyBackgroundFetchUpdate(BackgroundFetchInformation&& information)
{
    auto* registration = m_server->getRegistration(information.registrationIdentifier);
    if (!registration)
        return;

    // Progress event.
    registration->forEachConnection([&](auto& connection) {
        connection.updateBackgroundFetchRegistration(information);
    });

    if (information.result == BackgroundFetchResult::EmptyString)
        return;

    m_server->fireBackgroundFetchEvent(*registration, WTFMove(information));
}

void BackgroundFetchEngine::backgroundFetchInformation(SWServerRegistration& registration, const String& backgroundFetchIdentifier, ExceptionOrBackgroundFetchInformationCallback&& callback)
{
    auto iterator = m_fetches.find(registration.key());
    if (iterator == m_fetches.end()) {
        m_store->initializeFetches(*this, registration.key(), [weakThis = WeakPtr { *this }, registration = WeakPtr { registration }, backgroundFetchIdentifier, callback = WTFMove(callback)]() mutable {
            if (!weakThis || !registration) {
                callback(makeUnexpected(ExceptionData { InvalidStateError, "BackgroundFetchEngine is gone"_s }));
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
        m_store->initializeFetches(*this, registration.key(), [weakThis = WeakPtr { *this }, registration = WeakPtr { registration }, callback = WTFMove(callback)]() mutable {
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

    Vector<String> identifiers;
    identifiers.reserveInitialCapacity(iterator->value.size());
    for (auto& keyValue : iterator->value) {
        if (keyValue.value->isActive())
            identifiers.uncheckedAppend(keyValue.key);
    }
    callback(WTFMove(identifiers));
}

// https://wicg.github.io/background-fetch/#background-fetch-registration-abort starting from step 3
void BackgroundFetchEngine::abortBackgroundFetch(SWServerRegistration& registration, const String& backgroundFetchIdentifier, AbortBackgroundFetchCallback&& callback)
{
    auto iterator = m_fetches.find(registration.key());
    if (iterator == m_fetches.end()) {
        m_store->initializeFetches(*this, registration.key(), [weakThis = WeakPtr { *this }, registration = WeakPtr { registration }, backgroundFetchIdentifier, callback = WTFMove(callback)]() mutable {
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
        m_store->initializeFetches(*this, registration.key(), [weakThis = WeakPtr { *this }, registration = WeakPtr { registration }, backgroundFetchIdentifier, options = WTFMove(options), callback = WTFMove(callback)]() mutable {
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
        Vector<BackgroundFetchRecordInformation> recordsInformation;
        recordsInformation.reserveInitialCapacity(records.size());
        for (auto& record : records) {
            // FIXME: We need a way to remove the record from m_records.
            auto information = record->information();
            weakThis->m_records.add(information.identifier, WTFMove(record));
            recordsInformation.uncheckedAppend(WTFMove(information));
        }
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
        callback(makeUnexpected(ExceptionData { InvalidStateError, "Record not found"_s }));
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

} // namespace WebCore

#endif // ENABLE(SERVICE_WORKER)
