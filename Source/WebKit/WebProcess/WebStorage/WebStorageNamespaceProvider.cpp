/*
 * Copyright (C) 2014-2017 Apple Inc. All rights reserved.
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
#include "WebStorageNamespaceProvider.h"

#include "NetworkProcessConnection.h"
#include "NetworkStorageManagerMessages.h"
#include "WebPage.h"
#include "WebPageGroupProxy.h"
#include "WebProcess.h"
#include <wtf/HashMap.h>
#include <wtf/NeverDestroyed.h>

namespace WebKit {
using namespace WebCore;

static HashMap<StorageNamespaceIdentifier, WebStorageNamespaceProvider*>& storageNamespaceProviders()
{
    static NeverDestroyed<HashMap<StorageNamespaceIdentifier, WebStorageNamespaceProvider*>> storageNamespaceProviders;

    return storageNamespaceProviders;
}

Ref<WebStorageNamespaceProvider> WebStorageNamespaceProvider::getOrCreate(WebPageGroupProxy& pageGroup)
{
    RefPtr<WebStorageNamespaceProvider> storageNamespaceProvider;
    auto* result = storageNamespaceProviders().ensure(pageGroup.localStorageNamespaceIdentifier(), [&]() {
        storageNamespaceProvider = adoptRef(*new WebStorageNamespaceProvider(pageGroup.localStorageNamespaceIdentifier()));
        return storageNamespaceProvider.get();
    }).iterator->value;
    return *result;
}

void WebStorageNamespaceProvider::incrementUseCount(const WebPageGroupProxy& pageGroup, const StorageNamespaceImpl::Identifier identifier)
{
    auto storageNamespaceIt = storageNamespaceProviders().find(pageGroup.localStorageNamespaceIdentifier());
    ASSERT(storageNamespaceIt != storageNamespaceProviders().end());
    ASSERT(storageNamespaceIt->value);
    auto& sessionStorageNamespaces = storageNamespaceIt->value->m_sessionStorageNamespaces.add(identifier, SessionStorageNamespaces { }).iterator->value;
    ++sessionStorageNamespaces.useCount;
}

void WebStorageNamespaceProvider::decrementUseCount(const WebPageGroupProxy& pageGroup, const StorageNamespaceImpl::Identifier identifier)
{
    auto namespaceProviderIt = storageNamespaceProviders().find(pageGroup.localStorageNamespaceIdentifier());

    if (namespaceProviderIt == storageNamespaceProviders().end() || !namespaceProviderIt->value)
        return;

    auto& namespaces = namespaceProviderIt->value->m_sessionStorageNamespaces;

    auto it = namespaces.find(identifier);
    ASSERT(it != namespaces.end());
    auto& sessionStorageNamespaces = it->value;
    ASSERT(sessionStorageNamespaces.useCount);
    --sessionStorageNamespaces.useCount;
    if (!sessionStorageNamespaces.useCount)
        namespaces.remove(identifier);
}

WebStorageNamespaceProvider::WebStorageNamespaceProvider(StorageNamespaceIdentifier localStorageNamespaceIdentifier)
    : m_localStorageNamespaceIdentifier(localStorageNamespaceIdentifier)
{
}

WebStorageNamespaceProvider::~WebStorageNamespaceProvider()
{
    ASSERT(storageNamespaceProviders().contains(m_localStorageNamespaceIdentifier));

    storageNamespaceProviders().remove(m_localStorageNamespaceIdentifier);
}

Ref<WebCore::StorageNamespace> WebStorageNamespaceProvider::createLocalStorageNamespace(unsigned quota, PAL::SessionID sessionID)
{
    ASSERT_UNUSED(sessionID, sessionID == WebProcess::singleton().sessionID());
    return StorageNamespaceImpl::createLocalStorageNamespace(m_localStorageNamespaceIdentifier, quota);
}

Ref<WebCore::StorageNamespace> WebStorageNamespaceProvider::createTransientLocalStorageNamespace(WebCore::SecurityOrigin& topLevelOrigin, unsigned quota, PAL::SessionID sessionID)
{
    ASSERT_UNUSED(sessionID, sessionID == WebProcess::singleton().sessionID());
    return StorageNamespaceImpl::createTransientLocalStorageNamespace(m_localStorageNamespaceIdentifier, topLevelOrigin, quota);
}

RefPtr<WebCore::StorageNamespace> WebStorageNamespaceProvider::sessionStorageNamespace(const WebCore::SecurityOrigin& topLevelOrigin, WebCore::Page& page, ShouldCreateNamespace shouldCreate)
{
    ASSERT(sessionStorageQuota() != WebCore::StorageMap::noQuota);

    auto& webPage = WebPage::fromCorePage(page);

    // The identifier of a session storage namespace is the WebPageProxyIdentifier. It is possible we have several WebPage objects in a single process for the same
    // WebPageProxyIdentifier and these need to share the same namespace instance so we know where to route the IPC to.
    auto namespacesIt = m_sessionStorageNamespaces.find(webPage.sessionStorageNamespaceIdentifier());
    if (namespacesIt == m_sessionStorageNamespaces.end()) {
        if (shouldCreate == ShouldCreateNamespace::No)
            return nullptr;
        namespacesIt = m_sessionStorageNamespaces.set(webPage.sessionStorageNamespaceIdentifier(), SessionStorageNamespaces { }).iterator;
    }

    auto& sessionStorageNamespacesMap = namespacesIt->value.map;
    auto it = sessionStorageNamespacesMap.find(topLevelOrigin.data());
    if (it == sessionStorageNamespacesMap.end()) {
        if (shouldCreate == ShouldCreateNamespace::No)
            return nullptr;
        auto sessionStorageNamespace = StorageNamespaceImpl::createSessionStorageNamespace(webPage.sessionStorageNamespaceIdentifier(), webPage.identifier(), topLevelOrigin, sessionStorageQuota());
        it = sessionStorageNamespacesMap.set(topLevelOrigin.data(), WTFMove(sessionStorageNamespace)).iterator;
    }
    return it->value;
}

void WebStorageNamespaceProvider::copySessionStorageNamespace(WebCore::Page& srcPage, WebCore::Page& dstPage)
{
    ASSERT(sessionStorageQuota() != WebCore::StorageMap::noQuota);

    const auto& srcWebPage = WebPage::fromCorePage(srcPage);
    const auto& dstWebPage = WebPage::fromCorePage(dstPage);

    auto srcNamespacesIt = m_sessionStorageNamespaces.find(srcWebPage.sessionStorageNamespaceIdentifier());
    if (srcNamespacesIt == m_sessionStorageNamespaces.end())
        return;

    ASSERT(srcNamespacesIt->value.useCount);

    auto& srcNamespacesMap = srcNamespacesIt->value.map;

    auto& dstSessionStorageNamespaces = static_cast<WebStorageNamespaceProvider&>(dstPage.storageNamespaceProvider()).m_sessionStorageNamespaces;
    auto dstNamespacesIt = dstSessionStorageNamespaces.find(dstWebPage.sessionStorageNamespaceIdentifier());
    ASSERT(dstNamespacesIt != dstSessionStorageNamespaces.end());
    ASSERT(dstNamespacesIt->value.useCount == 1);
    auto& dstNamespacesMap = dstNamespacesIt->value.map;

    if (auto networkProcessConnection = WebProcess::singleton().existingNetworkProcessConnection())
        networkProcessConnection->connection().send(Messages::NetworkStorageManager::CloneSessionStorageNamespace(srcWebPage.sessionStorageNamespaceIdentifier(), dstWebPage.sessionStorageNamespaceIdentifier()), 0);

    for (auto& [origin, srcNamespace] : srcNamespacesMap)
        dstNamespacesMap.set(origin, srcNamespace->copy(dstPage));
}

}
