/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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

#include "CacheStorageRecord.h"
#include "CacheStorageStore.h"
#include "NetworkCacheKey.h"
#include <WebCore/RetrieveRecordsOptions.h>
#include <wtf/Identified.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/WorkQueue.h>

namespace WebKit {
class CacheStorageCache;
}

namespace WTF {
template<typename T> struct IsDeprecatedWeakRefSmartPointerException;
template<> struct IsDeprecatedWeakRefSmartPointerException<WebKit::CacheStorageCache> : std::true_type { };
}

namespace WebKit {

class CacheStorageManager;

class CacheStorageCache : public CanMakeWeakPtr<CacheStorageCache>, public Identified<WebCore::DOMCacheIdentifier> {
    WTF_MAKE_TZONE_ALLOCATED(CacheStorageCache);
public:
    CacheStorageCache(CacheStorageManager&, const String& name, const String& uniqueName, const String& path, Ref<WorkQueue>&&);
    ~CacheStorageCache();
    const String& name() const { return m_name; }
    const String& uniqueName() const { return m_uniqueName; }
    CacheStorageManager* manager();

    void getSize(CompletionHandler<void(uint64_t)>&&);
    void open(WebCore::DOMCacheEngine::CacheIdentifierCallback&&);
    void retrieveRecords(WebCore::RetrieveRecordsOptions&&, WebCore::DOMCacheEngine::CrossThreadRecordsCallback&&);
    void removeRecords(WebCore::ResourceRequest&&, WebCore::CacheQueryOptions&&, WebCore::DOMCacheEngine::RecordIdentifiersCallback&&);
    void putRecords(Vector<WebCore::DOMCacheEngine::CrossThreadRecord>&&, WebCore::DOMCacheEngine::RecordIdentifiersCallback&&);
    void removeAllRecords();
    void close();

private:
    CacheStorageRecordInformation* findExistingRecord(const WebCore::ResourceRequest&, std::optional<uint64_t> = std::nullopt);
    void putRecordsAfterQuotaCheck(Vector<CacheStorageRecord>&&, WebCore::DOMCacheEngine::RecordIdentifiersCallback&&);
    void putRecordsInStore(Vector<CacheStorageRecord>&&, Vector<std::optional<CacheStorageRecord>>&&, WebCore::DOMCacheEngine::RecordIdentifiersCallback&&);
    void assertIsOnCorrectQueue()
    {
#if ASSERT_ENABLED
        assertIsCurrent(m_queue.get());
#endif
    }

    WeakPtr<CacheStorageManager> m_manager;
    bool m_isInitialized { false };
    Vector<WebCore::DOMCacheEngine::CacheIdentifierCallback> m_pendingInitializationCallbacks;
    String m_name;
    String m_uniqueName;
    HashMap<String, Vector<CacheStorageRecordInformation>> m_records;
#if ASSERT_ENABLED
    Ref<WorkQueue> m_queue;
#endif
    Ref<CacheStorageStore> m_store;
};

} // namespace WebKit
