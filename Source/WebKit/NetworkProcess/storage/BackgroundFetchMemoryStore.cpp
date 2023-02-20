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
#include "BackgroundFetchMemoryStore.h"

#if ENABLE(SERVICE_WORKER)

#include <WebCore/BackgroundFetchOptions.h>
#include <WebCore/BackgroundFetchRecordInformation.h>
#include <WebCore/DOMCacheEngine.h>
#include <WebCore/SWServerRegistration.h>

namespace WebKit {

using namespace WebCore;

BackgroundFetchMemoryStore::BackgroundFetchMemoryStore()
{
}

void BackgroundFetchMemoryStore::initialize(BackgroundFetchEngine&, const ServiceWorkerRegistrationKey&, CompletionHandler<void()>&& callback)
{
    callback();
}

void BackgroundFetchMemoryStore::clearRecords(const ServiceWorkerRegistrationKey& key, const String& identifier, CompletionHandler<void()>&& callback)
{
    // FIXME: update quota usage.

    auto iterator = m_entries.find(key);
    if (iterator != m_entries.end())
        iterator->value.remove(identifier);
    callback();
}

void BackgroundFetchMemoryStore::clearAllRecords(const ServiceWorkerRegistrationKey& key, CompletionHandler<void()>&& callback)
{
    // FIXME: update quota usage.

    m_entries.remove(key);
    callback();
}

void BackgroundFetchMemoryStore::storeNewRecord(const ServiceWorkerRegistrationKey& key, const String& identifier, size_t index, const BackgroundFetchRequest&, CompletionHandler<void(StoreResult)>&& callback)
{
    // FIXME: check quota and update quota usage.

    auto& entryMap = m_entries.ensure(key, [] {
        return EntriesMap();
    }).iterator->value;
    auto& recordMap = entryMap.ensure(identifier, [] {
        return RecordMap();
    }).iterator->value;

    ASSERT(!recordMap.contains(index + 1));
    recordMap.add(index + 1, makeUnique<Record>());
    callback(StoreResult::OK);
}

void BackgroundFetchMemoryStore::storeRecordResponse(const ServiceWorkerRegistrationKey& key, const String& identifier, size_t index, ResourceResponse&& response, CompletionHandler<void(StoreResult)>&& callback)
{
    // FIXME: check quota and update quota usage.

    auto& entryMap = m_entries.ensure(key, [] {
        return EntriesMap();
    }).iterator->value;
    auto& recordMap = entryMap.ensure(identifier, [] {
        return RecordMap();
    }).iterator->value;

    ASSERT(recordMap.contains(index + 1));
    auto iterator = recordMap.find(index + 1);
    if (iterator == recordMap.end()) {
        callback(StoreResult::InternalError);
        return;
    }
    iterator->value->response = WTFMove(response);
    callback(StoreResult::OK);
}

void BackgroundFetchMemoryStore::storeRecordResponseBodyChunk(const ServiceWorkerRegistrationKey& key, const String& identifier, size_t index, const SharedBuffer& data, CompletionHandler<void(StoreResult)>&& callback)
{
    // FIXME: check quota and update quota usage.

    auto& entryMap = m_entries.ensure(key, [] {
        return EntriesMap();
    }).iterator->value;
    auto& recordMap = entryMap.ensure(identifier, [] {
        return RecordMap();
    }).iterator->value;

    auto iterator = recordMap.find(index + 1);
    if (iterator == recordMap.end()) {
        callback(StoreResult::InternalError);
        return;
    }

    iterator->value->buffer.append(data);
    callback(StoreResult::OK);
}

void BackgroundFetchMemoryStore::retrieveResponseBody(const ServiceWorkerRegistrationKey& key, const String& identifier, size_t index, RetrieveRecordResponseBodyCallback&& callback)
{
    auto& entryMap = m_entries.ensure(key, [] {
        return EntriesMap();
    }).iterator->value;
    auto& recordMap = entryMap.ensure(identifier, [] {
        return RecordMap();
    }).iterator->value;

    auto iterator = recordMap.find(index + 1);
    if (iterator == recordMap.end()) {
        callback(makeUnexpected(ResourceError { errorDomainWebKitInternal, 0, { }, "Record not found"_s }));
        return;
    }
    callback(RefPtr { iterator->value->buffer.copy()->makeContiguous() });
}

} // namespace WebKit

#endif // ENABLE(SERVICE_WORKER)
