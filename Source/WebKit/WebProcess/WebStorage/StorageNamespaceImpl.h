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

#include "IdentifierTypes.h"
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

class StorageNamespaceImpl final : public WebCore::StorageNamespace {
public:
    using Identifier = StorageNamespaceIdentifier;

    static Ref<StorageNamespaceImpl> createSessionStorageNamespace(Identifier, WebCore::PageIdentifier, const WebCore::SecurityOrigin&, unsigned quotaInBytes);
    static Ref<StorageNamespaceImpl> createLocalStorageNamespace(unsigned quotaInBytes);
    static Ref<StorageNamespaceImpl> createTransientLocalStorageNamespace(WebCore::SecurityOrigin& topLevelOrigin, uint64_t quotaInBytes);

    virtual ~StorageNamespaceImpl() = default;

    WebCore::StorageType storageType() const { return m_storageType; }
    std::optional<Identifier> storageNamespaceID() const { return m_storageNamespaceID; }
    WebCore::PageIdentifier sessionStoragePageID() const;
    const WebCore::SecurityOrigin* topLevelOrigin() const final { return m_topLevelOrigin.get(); }
    unsigned quotaInBytes() const { return m_quotaInBytes; }
    PAL::SessionID sessionID() const override;

    void destroyStorageAreaMap(StorageAreaMap&);

    void setSessionIDForTesting(PAL::SessionID) override;

private:
    StorageNamespaceImpl(WebCore::StorageType, const std::optional<WebCore::PageIdentifier>&, const WebCore::SecurityOrigin* topLevelOrigin, unsigned quotaInBytes, std::optional<Identifier> = std::nullopt);

    Ref<WebCore::StorageArea> storageArea(const WebCore::SecurityOrigin&) final;
    uint64_t storageAreaMapCountForTesting() const final { return m_storageAreaMaps.size(); }

    // FIXME: This is only valid for session storage and should probably be moved to a subclass.
    Ref<WebCore::StorageNamespace> copy(WebCore::Page&) override;

    const WebCore::StorageType m_storageType;
    std::optional<WebCore::PageIdentifier> m_sessionPageID;

    // Used for transient local storage and session storage namespaces, nullptr otherwise.
    const RefPtr<const WebCore::SecurityOrigin> m_topLevelOrigin;
    const unsigned m_quotaInBytes;
    std::optional<Identifier> m_storageNamespaceID;

    HashMap<WebCore::SecurityOriginData, std::unique_ptr<StorageAreaMap>> m_storageAreaMaps;
};

} // namespace WebKit
