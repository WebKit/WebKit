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

#pragma once

#if ENABLE(SERVICE_WORKER)

#include "BackgroundFetchFailureReason.h"
#include "BackgroundFetchOptions.h"
#include "BackgroundFetchRecordIdentifier.h"
#include "BackgroundFetchRecordLoader.h"
#include "BackgroundFetchRequest.h"
#include "BackgroundFetchResult.h"
#include "BackgroundFetchStore.h"
#include "ClientOrigin.h"
#include "ResourceResponse.h"
#include "ServiceWorkerRegistrationKey.h"
#include "ServiceWorkerTypes.h"
#include <wtf/WeakPtr.h>

namespace WebCore {

class BackgroundFetchRecordLoader;
class SharedBuffer;

struct BackgroundFetchRequest;
struct CacheQueryOptions;

class BackgroundFetch : public CanMakeWeakPtr<BackgroundFetch> {
    WTF_MAKE_FAST_ALLOCATED;
public:
    using NotificationCallback = Function<void(BackgroundFetchInformation&&)>;
    BackgroundFetch(SWServerRegistration&, const String&, Vector<BackgroundFetchRequest>&&, BackgroundFetchOptions&&, Ref<BackgroundFetchStore>&&, NotificationCallback&&);
    ~BackgroundFetch();

    String identifier() const { return m_identifier; }
    BackgroundFetchInformation information() const;

    using RetrieveRecordResponseCallback = CompletionHandler<void(Expected<ResourceResponse, ExceptionData>&&)>;
    using RetrieveRecordResponseBodyCallback = Function<void(Expected<RefPtr<SharedBuffer>, ResourceError>&&)>;
    using CreateLoaderCallback = Function<std::unique_ptr<BackgroundFetchRecordLoader>(BackgroundFetchRecordLoader::Client&, ResourceRequest&&, FetchOptions&&, const ClientOrigin&)>;

    class Record final : public BackgroundFetchRecordLoader::Client, public RefCounted<Record> {
        WTF_MAKE_FAST_ALLOCATED;
    public:
        static Ref<Record> create(BackgroundFetch& fetch, BackgroundFetchRequest&& request, size_t size) { return adoptRef(*new Record(fetch, WTFMove(request), size)); }
        ~Record();

        void complete(const CreateLoaderCallback&);
        void abort();

        void setAsCompleted() { m_isCompleted = true; }
        bool isCompleted() const { return m_isCompleted; }

        uint64_t responseDataSize() const { return m_responseDataSize; }
        bool isMatching(const ResourceRequest&, const CacheQueryOptions&) const;
        BackgroundFetchRecordInformation information() const;

        void retrieveResponse(BackgroundFetchStore&, RetrieveRecordResponseCallback&&);
        void retrieveRecordResponseBody(BackgroundFetchStore&, RetrieveRecordResponseBodyCallback&&);

    private:
        Record(BackgroundFetch&, BackgroundFetchRequest&&, size_t);

        void didSendData(uint64_t) final;
        void didReceiveResponse(ResourceResponse&&) final;
        void didReceiveResponseBodyChunk(const SharedBuffer&) final;
        void didFinish(const ResourceError&) final;

        WeakPtr<BackgroundFetch> m_fetch;
        BackgroundFetchRecordIdentifier m_identifier;
        String m_fetchIdentifier;
        ServiceWorkerRegistrationKey m_registrationKey;
        BackgroundFetchRequest m_request;
        size_t m_index { 0 };
        ResourceResponse m_response;
        std::unique_ptr<BackgroundFetchRecordLoader> m_loader;
        uint64_t m_responseDataSize { 0 };
        bool m_isCompleted { false };
        bool m_isAborted { false };
        Vector<RetrieveRecordResponseCallback> m_responseCallbacks;
        Vector<RetrieveRecordResponseBodyCallback> m_responseBodyCallbacks;
    };

    using MatchBackgroundFetchCallback = CompletionHandler<void(Vector<Ref<Record>>&&)>;
    void match(const RetrieveRecordsOptions&, MatchBackgroundFetchCallback&&);

    bool abort();

    void perform(const CreateLoaderCallback&);

    bool isActive() const { return m_isActive; }
    const ClientOrigin& origin() const { return m_origin; }
    uint64_t downloadTotal() const { return  m_options.downloadTotal; }
    uint64_t uploadTotal() const { return m_uploadTotal; }

    void unsetRecordsAvailableFlag();

private:
    void didSendData(uint64_t);
    void storeResponse(size_t, ResourceResponse&&);
    void storeResponseBodyChunk(size_t, const SharedBuffer&);
    void didFinishRecord(size_t, const ResourceError&);

    void recordIsCompleted(size_t);
    void handleStoreResult(BackgroundFetchStore::StoreResult);
    void updateBackgroundFetchStatus(BackgroundFetchResult, BackgroundFetchFailureReason);

    void doStore();

    String m_identifier;
    Vector<Ref<Record>> m_records;
    BackgroundFetchOptions m_options;
    ServiceWorkerRegistrationKey m_registrationKey;
    ServiceWorkerRegistrationIdentifier m_registrationIdentifier;

    BackgroundFetchResult m_result { BackgroundFetchResult::EmptyString };
    BackgroundFetchFailureReason m_failureReason { BackgroundFetchFailureReason::EmptyString };

    bool m_recordsAvailableFlag { true };
    bool m_abortFlag { false };
    bool m_isActive { true };

    uint64_t m_uploadTotal { 0 };
    uint64_t m_currentDownloadSize { 0 };
    uint64_t m_currentUploadSize { 0 };

    Ref<BackgroundFetchStore> m_store;
    NotificationCallback m_notificationCallback;
    ClientOrigin m_origin;
};

} // namespace WebCore

#endif // ENABLE(SERVICE_WORKER)
