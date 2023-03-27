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
#include "NetworkStorageManagerMessages.h"
#include "StorageAreaImpl.h"
#include "StorageAreaMap.h"
#include "WebPage.h"
#include "WebPageInlines.h"
#include "WebProcess.h"
#include <WebCore/LocalFrame.h>
#include <WebCore/SecurityOrigin.h>
#include <WebCore/Settings.h>
#include <WebCore/StorageType.h>

namespace WebKit {
using namespace WebCore;

Ref<StorageNamespaceImpl> StorageNamespaceImpl::createSessionStorageNamespace(Identifier identifier, PageIdentifier pageID, const WebCore::SecurityOrigin& topLevelOrigin, unsigned quotaInBytes)
{
    return adoptRef(*new StorageNamespaceImpl(StorageType::Session, pageID, &topLevelOrigin, quotaInBytes, identifier));
}

Ref<StorageNamespaceImpl> StorageNamespaceImpl::createLocalStorageNamespace(unsigned quotaInBytes)
{
    return adoptRef(*new StorageNamespaceImpl(StorageType::Local, std::nullopt, nullptr, quotaInBytes));
}

Ref<StorageNamespaceImpl> StorageNamespaceImpl::createTransientLocalStorageNamespace(WebCore::SecurityOrigin& topLevelOrigin, uint64_t quotaInBytes)
{
    return adoptRef(*new StorageNamespaceImpl(StorageType::TransientLocal, std::nullopt, &topLevelOrigin, quotaInBytes));
}

StorageNamespaceImpl::StorageNamespaceImpl(WebCore::StorageType storageType, const std::optional<PageIdentifier>& pageIdentifier, const WebCore::SecurityOrigin* topLevelOrigin, unsigned quotaInBytes, std::optional<Identifier> storageNamespaceID)
    : m_storageType(storageType)
    , m_sessionPageID(pageIdentifier)
    , m_topLevelOrigin(topLevelOrigin)
    , m_quotaInBytes(quotaInBytes)
    , m_storageNamespaceID(storageNamespaceID)
{
    ASSERT(storageType == StorageType::Session || !m_sessionPageID);
}

PAL::SessionID StorageNamespaceImpl::sessionID() const
{
    return WebProcess::singleton().sessionID();
}

void StorageNamespaceImpl::destroyStorageAreaMap(StorageAreaMap& map)
{
    m_storageAreaMaps.remove(map.securityOrigin().data());
}

Ref<StorageArea> StorageNamespaceImpl::storageArea(const SecurityOrigin& securityOrigin)
{
    auto& map = m_storageAreaMaps.ensure(securityOrigin.data(), [&] {
        return makeUnique<StorageAreaMap>(*this, securityOrigin);
    }).iterator->value;
    return StorageAreaImpl::create(*map);
}

Ref<StorageNamespace> StorageNamespaceImpl::copy(Page& newPage)
{
    ASSERT(m_storageNamespaceID);
    ASSERT(m_storageType == StorageType::Session);

    auto* webPage = WebPage::fromCorePage(newPage);
    return adoptRef(*new StorageNamespaceImpl(m_storageType, webPage->identifier(), m_topLevelOrigin.get(), m_quotaInBytes, webPage->sessionStorageNamespaceIdentifier()));
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

} // namespace WebKit
