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

#include <WebCore/BackgroundFetchStore.h>
#include <WebCore/ClientOrigin.h>
#include <wtf/CompletionHandler.h>
#include <wtf/HashMap.h>

namespace WTF {
class WorkQueue;
}

namespace WebCore {
class SWServer;
}

namespace WebKit {

class NetworkStorageManager;
struct BackgroundFetchState;

class BackgroundFetchStoreImpl :  public WebCore::BackgroundFetchStore {
public:
    static Ref<BackgroundFetchStoreImpl> create(WeakPtr<NetworkStorageManager>&& manager, WeakPtr<WebCore::SWServer>&& server) { return adoptRef(*new BackgroundFetchStoreImpl(WTFMove(manager), WTFMove(server))); }
    ~BackgroundFetchStoreImpl();

    void getAllBackgroundFetchIdentifiers(CompletionHandler<void(Vector<String>&&)>&&);
    void getBackgroundFetchState(const String&, CompletionHandler<void(std::optional<BackgroundFetchState>&&)>&&);
    void abortBackgroundFetch(const String&, CompletionHandler<void()>&&);
    void pauseBackgroundFetch(const String&, CompletionHandler<void()>&&);
    void resumeBackgroundFetch(const String&, CompletionHandler<void()>&&);
    void clickBackgroundFetch(const String&, CompletionHandler<void()>&&);

private:
    BackgroundFetchStoreImpl(WeakPtr<NetworkStorageManager>&&, WeakPtr<WebCore::SWServer>&&);

    void initializeFetches(const WebCore::ServiceWorkerRegistrationKey&, CompletionHandler<void()>&&) final;
    void clearFetch(const WebCore::ServiceWorkerRegistrationKey&, const String&, CompletionHandler<void()>&&) final;
    void clearAllFetches(const WebCore::ServiceWorkerRegistrationKey&, CompletionHandler<void()>&&) final;
    void storeFetch(const WebCore::ServiceWorkerRegistrationKey&, const String&, uint64_t downloadTotal, uint64_t uploadTotal, std::optional<size_t> responseBodyIndexToClear, Vector<uint8_t>&&, CompletionHandler<void(StoreResult)>&&) final;
    void storeFetchResponseBodyChunk(const WebCore::ServiceWorkerRegistrationKey&, const String&, size_t, const WebCore::SharedBuffer&, CompletionHandler<void(StoreResult)>&&) final;
    void retrieveResponseBody(const WebCore::ServiceWorkerRegistrationKey&, const String&, size_t, RetrieveRecordResponseBodyCallback&&) final;

    void initializeFetchesInternal(const WebCore::ClientOrigin&, CompletionHandler<void(Vector<std::pair<RefPtr<WebCore::SharedBuffer>, String>>&&)>&&);
    void clearFetchInternal(const WebCore::ClientOrigin&, const String&, CompletionHandler<void()>&&);
    void clearAllFetchesInternal(const WebCore::ClientOrigin&, const Vector<String>&, CompletionHandler<void()>&&);
    void storeFetchInternal(const WebCore::ClientOrigin&, const String&, uint64_t, uint64_t, std::optional<size_t>, Vector<uint8_t>&&, CompletionHandler<void(StoreResult)>&&);
    void storeFetchResponseBodyChunkInternal(const WebCore::ClientOrigin&, const String&, size_t index, const WebCore::SharedBuffer&, CompletionHandler<void(StoreResult)>&&);

    String getFilename(const WebCore::ServiceWorkerRegistrationKey&, const String&);
    void registerFetch(const WebCore::ClientOrigin&, const WebCore::ServiceWorkerRegistrationKey&, const String& backgroundFetchIdentifier, String&& fetchStorageIdentifier);
    void loadAllFetches(CompletionHandler<void()>&&);
    void fetchInformationFromFilename(const String&, CompletionHandler<void(const WebCore::ServiceWorkerRegistrationKey&, const String&)>&&);
    void initializeFetches(const WebCore::ClientOrigin&, CompletionHandler<void()>&&);

    WeakPtr<NetworkStorageManager> m_manager;

    using FetchIdentifier = std::pair<String, String>; // < service worker registration scope, background fetch identifier >
    struct PerClientOriginFetches {
        HashMap<FetchIdentifier, String> fetchToFilenames;
        Vector<CompletionHandler<void()>> initializationCallbacks;
    };
    HashMap<WebCore::ClientOrigin, PerClientOriginFetches> m_perClientOriginFetches;

    struct FetchInformation {
        WebCore::ClientOrigin origin;
        FetchIdentifier identifier;
    };
    HashMap<String, FetchInformation> m_filenameToFetch;
    WeakPtr<WebCore::SWServer> m_server;
};

} // namespace WebKit
