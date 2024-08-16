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
#include <WebCore/SharedBuffer.h>
#include <wtf/Function.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/WeakPtr.h>
#include <wtf/text/WTFString.h>

namespace WebKit {
class BackgroundFetchStoreManager;
}

namespace WTF {
template<typename T> struct IsDeprecatedWeakRefSmartPointerException;
template<> struct IsDeprecatedWeakRefSmartPointerException<WebKit::BackgroundFetchStoreManager> : std::true_type { };
}

namespace WTF {
class WorkQueue;
}

namespace WebKit {

class BackgroundFetchStoreManager : public CanMakeWeakPtr<BackgroundFetchStoreManager> {
    WTF_MAKE_TZONE_ALLOCATED(BackgroundFetchStoreManager);
public:
    using QuotaCheckFunction = Function<void(uint64_t spaceRequested, CompletionHandler<void(bool)>&&)>;
    BackgroundFetchStoreManager(const String&, Ref<WTF::WorkQueue>&&, QuotaCheckFunction&&);
    ~BackgroundFetchStoreManager();

    static String createNewStorageIdentifier();

    using InitializeCallback = CompletionHandler<void(Vector<std::pair<RefPtr<WebCore::SharedBuffer>, String>>&&)>;
    void initializeFetches(InitializeCallback&&);

    void clearFetch(const String&);
    void clearFetch(const String&, CompletionHandler<void()>&&);
    void clearAllFetches(const Vector<String>&, CompletionHandler<void()>&&);

    using StoreResult = WebCore::BackgroundFetchStore::StoreResult;
    void storeFetch(const String&, uint64_t downloadTotal, uint64_t uploadTotal, std::optional<size_t> responseBodyIndexToClear, Vector<uint8_t>&&, CompletionHandler<void(StoreResult)>&&);
    void storeFetchResponseBodyChunk(const String&, size_t, const WebCore::SharedBuffer&, CompletionHandler<void(StoreResult)>&&);

    void retrieveResponseBody(const String&, size_t, CompletionHandler<void(RefPtr<WebCore::SharedBuffer>&&)>&&);

private:
    void storeFetchAfterQuotaCheck(const String&, uint64_t downloadTotal, uint64_t uploadTotal, std::optional<size_t> responseBodyIndexToClear, Vector<uint8_t>&&, CompletionHandler<void(StoreResult)>&&);

    String m_path;
    Ref<WTF::WorkQueue> m_taskQueue;
    Ref<WTF::WorkQueue> m_ioQueue;
    QuotaCheckFunction m_quotaCheckFunction;

    HashMap<String, Vector<WebCore::SharedBufferBuilder>> m_nonPersistentChunks;
};

} // namespace WebKit
