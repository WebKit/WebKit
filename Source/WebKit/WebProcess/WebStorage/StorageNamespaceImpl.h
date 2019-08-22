/*
 * Copyright (C) 2013-2017 Apple Inc. All rights reserved.
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

#include "StorageNamespaceIdentifier.h"
#include <WebCore/PageIdentifier.h>
#include <WebCore/SecurityOriginData.h>
#include <WebCore/SecurityOriginHash.h>
#include <WebCore/StorageArea.h>
#include <WebCore/StorageMap.h>
#include <WebCore/StorageNamespace.h>
#include <pal/SessionID.h>
#include <wtf/HashMap.h>

namespace WebKit {

class StorageAreaMap;
class WebPage;

class StorageNamespaceImpl : public WebCore::StorageNamespace {
public:
    using Identifier = StorageNamespaceIdentifier;

    static Ref<StorageNamespaceImpl> createSessionStorageNamespace(Identifier, unsigned quotaInBytes, PAL::SessionID);
    static Ref<StorageNamespaceImpl> createLocalStorageNamespace(Identifier, unsigned quotaInBytes, PAL::SessionID);
    static Ref<StorageNamespaceImpl> createTransientLocalStorageNamespace(Identifier, WebCore::SecurityOrigin& topLevelOrigin, uint64_t quotaInBytes, PAL::SessionID);

    virtual ~StorageNamespaceImpl();

    WebCore::StorageType storageType() const { return m_storageType; }
    Identifier storageNamespaceID() const { return m_storageNamespaceID; }
    WebCore::PageIdentifier sessionStoragePageID() const;
    uint64_t pageGroupID() const;
    WebCore::SecurityOrigin* topLevelOrigin() const { return m_topLevelOrigin.get(); }
    unsigned quotaInBytes() const { return m_quotaInBytes; }
    PAL::SessionID sessionID() const override { return m_sessionID; }

    void didDestroyStorageAreaMap(StorageAreaMap&);

    void setSessionIDForTesting(PAL::SessionID) override;

private:
    StorageNamespaceImpl(WebCore::StorageType, Identifier, WebCore::SecurityOrigin* topLevelOrigin, unsigned quotaInBytes, PAL::SessionID);

    Ref<WebCore::StorageArea> storageArea(const WebCore::SecurityOriginData&) override;

    // FIXME: This is only valid for session storage and should probably be moved to a subclass.
    Ref<WebCore::StorageNamespace> copy(WebCore::Page*) override;

    const WebCore::StorageType m_storageType;
    const Identifier m_storageNamespaceID;

    // Only used for transient local storage namespaces.
    const RefPtr<WebCore::SecurityOrigin> m_topLevelOrigin;

    const unsigned m_quotaInBytes;

    PAL::SessionID m_sessionID;

    HashMap<WebCore::SecurityOriginData, RefPtr<StorageAreaMap>> m_storageAreaMaps;
};

} // namespace WebKit
