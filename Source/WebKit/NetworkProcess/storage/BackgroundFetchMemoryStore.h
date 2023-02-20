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

#include <WebCore/BackgroundFetchStore.h>
#include <WebCore/ResourceResponse.h>
#include <WebCore/SharedBuffer.h>
#include <wtf/HashMap.h>

namespace WebKit {

class BackgroundFetchMemoryStore :  public WebCore::BackgroundFetchStore {
public:
    static Ref<BackgroundFetchMemoryStore> create() { return adoptRef(*new BackgroundFetchMemoryStore()); }

private:
    BackgroundFetchMemoryStore();

    void initialize(WebCore::BackgroundFetchEngine&, const WebCore::ServiceWorkerRegistrationKey&, CompletionHandler<void()>&&) final;
    void clearRecords(const WebCore::ServiceWorkerRegistrationKey&, const String&, CompletionHandler<void()>&&) final;
    void clearAllRecords(const WebCore::ServiceWorkerRegistrationKey&, CompletionHandler<void()>&&) final;
    void storeNewRecord(const WebCore::ServiceWorkerRegistrationKey&, const String&, size_t, const WebCore::BackgroundFetchRequest&, CompletionHandler<void(StoreResult)>&&) final;
    void storeRecordResponse(const WebCore::ServiceWorkerRegistrationKey&, const String&, size_t, WebCore::ResourceResponse&&, CompletionHandler<void(StoreResult)>&&) final;
    void storeRecordResponseBodyChunk(const WebCore::ServiceWorkerRegistrationKey&, const String&, size_t, const WebCore::SharedBuffer&, CompletionHandler<void(StoreResult)>&&) final;
    void retrieveResponseBody(const WebCore::ServiceWorkerRegistrationKey&, const String&, size_t, RetrieveRecordResponseBodyCallback&&) final;

    struct Record {
        WTF_MAKE_FAST_ALLOCATED;
    public:
        WebCore::ResourceResponse response;
        WebCore::SharedBufferBuilder buffer;
    };

    using RecordMap = HashMap<size_t, std::unique_ptr<Record>>;
    using EntriesMap = HashMap<String, RecordMap>;
    HashMap<WebCore::ServiceWorkerRegistrationKey, EntriesMap> m_entries;
};

} // namespace WebKit

#endif // ENABLE(SERVICE_WORKER)
