/*
 * Copyright (C) 2008 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include <WebCore/SecurityOriginData.h>
#include <WebCore/StorageArea.h>
#include <WebCore/StorageMap.h>
#include <WebCore/Timer.h>
#include <wtf/HashMap.h>
#include <wtf/RefPtr.h>

namespace WebCore {
class SecurityOrigin;
class StorageMap;
}

namespace WebKit {

class StorageAreaSync;

class StorageAreaImpl : public WebCore::StorageArea {
public:
    static Ref<StorageAreaImpl> create(WebCore::StorageType, const WebCore::SecurityOrigin&, RefPtr<WebCore::StorageSyncManager>&&, unsigned quota);
    virtual ~StorageAreaImpl();

    unsigned length() override;
    RefPtr<StringImpl> key(unsigned index) override;
    RefPtr<StringImpl> item(StringImpl* key) override;
    void setItem(WebCore::LocalFrame& sourceFrame, StringImpl* key, StringImpl* value, bool& quotaException) override;
    void removeItem(WebCore::LocalFrame& sourceFrame, StringImpl* key) override;
    void clear(WebCore::LocalFrame& sourceFrame) override;
    bool contains(StringImpl* key) override;

    WebCore::StorageType storageType() const override { return m_storageType; }

    size_t memoryBytesUsedByCache() override;

    void incrementAccessCount() override;
    void decrementAccessCount() override;
    void closeDatabaseIfIdle() override;

    Ref<StorageAreaImpl> copy();
    void close();

    // Only called from a background thread.
    void importItems(HashMap<RefPtr<StringImpl>, RefPtr<StringImpl>>&& items);

    // Used to clear a StorageArea and close db before backing db file is deleted.
    void clearForOriginDeletion();

    void sync();

    void sessionChanged(bool isNewSessionPersistent);

private:
    StorageAreaImpl(WebCore::StorageType, const WebCore::SecurityOrigin&, RefPtr<WebCore::StorageSyncManager>&&, unsigned quota);
    explicit StorageAreaImpl(const StorageAreaImpl&);

    void blockUntilImportComplete() const;
    void closeDatabaseTimerFired();

    void dispatchStorageEvent(StringImpl* key, StringImpl* oldValue, StringImpl* newValue, WebCore::LocalFrame& sourceFrame);

    WebCore::StorageType m_storageType;
    const Ref<const WebCore::SecurityOrigin> m_securityOrigin;
    WebCore::StorageMap m_storageMap;

    RefPtr<StorageAreaSync> m_storageAreaSync;
    const RefPtr<WebCore::StorageSyncManager> m_storageSyncManager;

#if ASSERT_ENABLED
    bool m_isShutdown { false };
#endif
    unsigned m_accessCount;
    WebCore::Timer m_closeDatabaseTimer;
};

} // namespace WebCore
