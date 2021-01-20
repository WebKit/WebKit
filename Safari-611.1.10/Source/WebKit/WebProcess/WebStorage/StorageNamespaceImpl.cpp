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

#include "config.h"
#include "StorageNamespaceImpl.h"

#include "NetworkProcessConnection.h"
#include "StorageAreaImpl.h"
#include "StorageAreaMap.h"
#include "StorageManagerSetMessages.h"
#include "WebPage.h"
#include "WebPageGroupProxy.h"
#include "WebProcess.h"
#include <WebCore/Frame.h>
#include <WebCore/PageGroup.h>
#include <WebCore/SecurityOrigin.h>
#include <WebCore/Settings.h>
#include <WebCore/StorageType.h>

namespace WebKit {
using namespace WebCore;

static HashMap<StorageNamespaceImpl::Identifier, StorageNamespaceImpl*>& sessionStorageNamespaces()
{
    static NeverDestroyed<HashMap<StorageNamespaceImpl::Identifier, StorageNamespaceImpl*>> map;
    return map;
}

Ref<StorageNamespaceImpl> StorageNamespaceImpl::createSessionStorageNamespace(Identifier identifier, PageIdentifier pageID, unsigned quotaInBytes)
{
    // The identifier of a session storage namespace is the WebPageProxyIdentifier. It is possible we have several WebPage objects in a single process for the same
    // WebPageProxyIdentifier and these need to share the same namespace instance so we know where to route the IPC to.
    if (auto* existingNamespace = sessionStorageNamespaces().get(identifier))
        return *existingNamespace;
    return adoptRef(*new StorageNamespaceImpl(StorageType::Session, identifier, pageID, nullptr, quotaInBytes));
}

Ref<StorageNamespaceImpl> StorageNamespaceImpl::createLocalStorageNamespace(Identifier identifier, unsigned quotaInBytes)
{
    return adoptRef(*new StorageNamespaceImpl(StorageType::Local, identifier, WTF::nullopt, nullptr, quotaInBytes));
}

Ref<StorageNamespaceImpl> StorageNamespaceImpl::createTransientLocalStorageNamespace(Identifier identifier, WebCore::SecurityOrigin& topLevelOrigin, uint64_t quotaInBytes)
{
    return adoptRef(*new StorageNamespaceImpl(StorageType::TransientLocal, identifier, WTF::nullopt, &topLevelOrigin, quotaInBytes));
}

StorageNamespaceImpl::StorageNamespaceImpl(WebCore::StorageType storageType, Identifier storageNamespaceID, const Optional<PageIdentifier>& pageIdentifier, WebCore::SecurityOrigin* topLevelOrigin, unsigned quotaInBytes)
    : m_storageType(storageType)
    , m_storageNamespaceID(storageNamespaceID)
    , m_sessionPageID(pageIdentifier)
    , m_topLevelOrigin(topLevelOrigin)
    , m_quotaInBytes(quotaInBytes)
{
    ASSERT(storageType == StorageType::Session || !m_sessionPageID);
    
    if (m_storageType == StorageType::Session) {
        ASSERT(!sessionStorageNamespaces().contains(m_storageNamespaceID));
        sessionStorageNamespaces().add(m_storageNamespaceID, this);
    }
}

StorageNamespaceImpl::~StorageNamespaceImpl()
{
    if (m_storageType == StorageType::Session) {
        bool wasRemoved = sessionStorageNamespaces().remove(m_storageNamespaceID);
        ASSERT_UNUSED(wasRemoved, wasRemoved);
    }
}

PAL::SessionID StorageNamespaceImpl::sessionID() const
{
    return WebProcess::singleton().sessionID();
}

void StorageNamespaceImpl::destroyStorageAreaMap(StorageAreaMap& map)
{
    m_storageAreaMaps.remove(map.securityOrigin().data());
}

Ref<StorageArea> StorageNamespaceImpl::storageArea(const SecurityOriginData& securityOriginData)
{
    auto& map = m_storageAreaMaps.ensure(securityOriginData, [&] {
        return makeUnique<StorageAreaMap>(*this, securityOriginData.securityOrigin());
    }).iterator->value;
    return StorageAreaImpl::create(*map);
}

Ref<StorageNamespace> StorageNamespaceImpl::copy(Page& newPage)
{
    ASSERT(m_storageNamespaceID);
    ASSERT(m_storageType == StorageType::Session);

    if (auto networkProcessConnection = WebProcess::singleton().existingNetworkProcessConnection())
        networkProcessConnection->connection().send(Messages::StorageManagerSet::CloneSessionStorageNamespace(sessionID(), m_storageNamespaceID, WebPage::fromCorePage(newPage).sessionStorageNamespaceIdentifier()), 0);

    return adoptRef(*new StorageNamespaceImpl(m_storageType, WebPage::fromCorePage(newPage).sessionStorageNamespaceIdentifier(), WebPage::fromCorePage(newPage).identifier(), m_topLevelOrigin.get(), m_quotaInBytes));
}

void StorageNamespaceImpl::setSessionIDForTesting(PAL::SessionID)
{
    ASSERT_NOT_REACHED();
}

PageIdentifier StorageNamespaceImpl::sessionStoragePageID() const
{
    ASSERT(m_storageType == StorageType::Session);
    return *m_sessionPageID;
}

uint64_t StorageNamespaceImpl::pageGroupID() const
{
    ASSERT(m_storageType == StorageType::Local || m_storageType == StorageType::TransientLocal);
    return m_storageNamespaceID.toUInt64();
}

} // namespace WebKit
