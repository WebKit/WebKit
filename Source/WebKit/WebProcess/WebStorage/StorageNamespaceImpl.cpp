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

Ref<StorageNamespaceImpl> StorageNamespaceImpl::createSessionStorageNamespace(Identifier identifier, unsigned quotaInBytes, PAL::SessionID sessionID)
{
    return adoptRef(*new StorageNamespaceImpl(StorageType::Session, identifier, nullptr, quotaInBytes, sessionID));
}

Ref<StorageNamespaceImpl> StorageNamespaceImpl::createLocalStorageNamespace(Identifier identifier, unsigned quotaInBytes, PAL::SessionID sessionID)
{
    return adoptRef(*new StorageNamespaceImpl(StorageType::Local, identifier, nullptr, quotaInBytes, sessionID));
}

Ref<StorageNamespaceImpl> StorageNamespaceImpl::createTransientLocalStorageNamespace(Identifier identifier, WebCore::SecurityOrigin& topLevelOrigin, uint64_t quotaInBytes, PAL::SessionID sessionID)
{
    return adoptRef(*new StorageNamespaceImpl(StorageType::TransientLocal, identifier, &topLevelOrigin, quotaInBytes, sessionID));
}

StorageNamespaceImpl::StorageNamespaceImpl(WebCore::StorageType storageType, Identifier storageNamespaceID, WebCore::SecurityOrigin* topLevelOrigin, unsigned quotaInBytes, PAL::SessionID sessionID)
    : m_storageType(storageType)
    , m_storageNamespaceID(storageNamespaceID)
    , m_topLevelOrigin(topLevelOrigin)
    , m_quotaInBytes(quotaInBytes)
    , m_sessionID(sessionID)
{
}

StorageNamespaceImpl::~StorageNamespaceImpl()
{
}

void StorageNamespaceImpl::didDestroyStorageAreaMap(StorageAreaMap& map)
{
    m_storageAreaMaps.remove(map.securityOrigin().data());
}

Ref<StorageArea> StorageNamespaceImpl::storageArea(const SecurityOriginData& securityOriginData)
{
    RefPtr<StorageAreaMap> map;

    auto securityOrigin = securityOriginData.securityOrigin();
    auto& slot = m_storageAreaMaps.add(securityOrigin->data(), nullptr).iterator->value;
    if (!slot) {
        map = StorageAreaMap::create(this, WTFMove(securityOrigin));
        slot = map.get();
    } else
        map = slot;

    return StorageAreaImpl::create(map.releaseNonNull());
}

Ref<StorageNamespace> StorageNamespaceImpl::copy(Page* newPage)
{
    ASSERT(m_storageNamespaceID);
    ASSERT(m_storageType == StorageType::Session);

    if (auto networkProcessConnection = WebProcess::singleton().existingNetworkProcessConnection())
        networkProcessConnection->connection().send(Messages::StorageManagerSet::CloneSessionStorageNamespace(newPage->sessionID(), m_storageNamespaceID, WebPage::fromCorePage(newPage)->sessionStorageNamespaceIdentifier()), 0);

    return adoptRef(*new StorageNamespaceImpl(m_storageType, WebPage::fromCorePage(newPage)->sessionStorageNamespaceIdentifier(), m_topLevelOrigin.get(), m_quotaInBytes, newPage->sessionID()));
}

void StorageNamespaceImpl::setSessionIDForTesting(PAL::SessionID sessionID)
{
    m_sessionID = sessionID;
    for (auto storageAreaMap : m_storageAreaMaps.values())
        storageAreaMap->disconnect();
}

PageIdentifier StorageNamespaceImpl::sessionStoragePageID() const
{
    ASSERT(m_storageType == StorageType::Session);
    return makeObjectIdentifier<PageIdentifierType>(m_storageNamespaceID.toUInt64());
}

uint64_t StorageNamespaceImpl::pageGroupID() const
{
    ASSERT(m_storageType == StorageType::Local || m_storageType == StorageType::TransientLocal);
    return m_storageNamespaceID.toUInt64();
}

} // namespace WebKit
