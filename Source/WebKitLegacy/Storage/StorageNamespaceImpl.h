/*
 * Copyright (C) 2008 Apple Inc. All Rights Reserved.
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
#include <WebCore/StorageNamespace.h>
#include <pal/SessionID.h>
#include <wtf/HashMap.h>
#include <wtf/RefPtr.h>
#include <wtf/text/WTFString.h>

namespace WebKit {

class StorageAreaImpl;

class StorageNamespaceImpl : public WebCore::StorageNamespace {
public:
    static Ref<StorageNamespaceImpl> createSessionStorageNamespace(unsigned quota, PAL::SessionID);
    static Ref<StorageNamespaceImpl> getOrCreateLocalStorageNamespace(const String& databasePath, unsigned quota, PAL::SessionID);
    virtual ~StorageNamespaceImpl();

    void close();

    // Not removing the origin's StorageArea from m_storageAreaMap because
    // we're just deleting the underlying db file. If an item is added immediately
    // after file deletion, we want the same StorageArea to eventually trigger
    // a sync and for StorageAreaSync to recreate the backing db file.
    void clearOriginForDeletion(const WebCore::SecurityOriginData&);
    void clearAllOriginsForDeletion();
    void sync();
    void closeIdleLocalStorageDatabases();

    PAL::SessionID sessionID() const override { return m_sessionID; }
    void setSessionIDForTesting(PAL::SessionID) override;

private:
    StorageNamespaceImpl(WebCore::StorageType, const String& path, unsigned quota, PAL::SessionID);

    Ref<WebCore::StorageArea> storageArea(const WebCore::SecurityOriginData&) override;
    Ref<StorageNamespace> copy(WebCore::Page& newPage) override;

    typedef HashMap<WebCore::SecurityOriginData, RefPtr<StorageAreaImpl>> StorageAreaMap;
    StorageAreaMap m_storageAreaMap;

    WebCore::StorageType m_storageType;

    // Only used if m_storageType == LocalStorage and the path was not "" in our constructor.
    String m_path;
    RefPtr<WebCore::StorageSyncManager> m_syncManager;

    // The default quota for each new storage area.
    unsigned m_quota;

    bool m_isShutdown;

    PAL::SessionID m_sessionID;
};

} // namespace WebCore
