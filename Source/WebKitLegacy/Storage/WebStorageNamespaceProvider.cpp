/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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

#include "WebStorageNamespaceProvider.h"

#include "StorageNamespaceImpl.h"
#include <WebCore/Page.h>
#include <wtf/NeverDestroyed.h>

using namespace WebCore;

namespace WebKit {

static HashSet<WeakRef<WebStorageNamespaceProvider>>& storageNamespaceProviders()
{
    static NeverDestroyed<HashSet<WeakRef<WebStorageNamespaceProvider>>> storageNamespaceProviders;

    return storageNamespaceProviders;
}

Ref<WebStorageNamespaceProvider> WebStorageNamespaceProvider::create(const String& localStorageDatabasePath)
{
    return adoptRef(*new WebStorageNamespaceProvider(localStorageDatabasePath));
}

WebStorageNamespaceProvider::WebStorageNamespaceProvider(const String& localStorageDatabasePath)
    : m_localStorageDatabasePath(localStorageDatabasePath.isNull() ? emptyString() : localStorageDatabasePath)
{
    storageNamespaceProviders().add(*this);
}

WebStorageNamespaceProvider::~WebStorageNamespaceProvider()
{
    ASSERT(storageNamespaceProviders().contains(*this));
    storageNamespaceProviders().remove(*this);
}

void WebStorageNamespaceProvider::closeLocalStorage()
{
    for (const auto& storageNamespaceProvider : storageNamespaceProviders()) {
        if (RefPtr localStorageNamespace = storageNamespaceProvider->optionalLocalStorageNamespace())
            static_cast<StorageNamespaceImpl&>(*localStorageNamespace).close();
    }
}

void WebStorageNamespaceProvider::clearLocalStorageForAllOrigins()
{
    for (const auto& storageNamespaceProvider : storageNamespaceProviders()) {
        if (RefPtr localStorageNamespace = storageNamespaceProvider->optionalLocalStorageNamespace())
            static_cast<StorageNamespaceImpl&>(*localStorageNamespace).clearAllOriginsForDeletion();
    }
}

void WebStorageNamespaceProvider::clearLocalStorageForOrigin(const SecurityOriginData& origin)
{
    for (const auto& storageNamespaceProvider : storageNamespaceProviders()) {
        if (RefPtr localStorageNamespace = storageNamespaceProvider->optionalLocalStorageNamespace())
            static_cast<StorageNamespaceImpl&>(*localStorageNamespace).clearOriginForDeletion(origin);
    }
}

void WebStorageNamespaceProvider::closeIdleLocalStorageDatabases()
{
    for (const auto& storageNamespaceProvider : storageNamespaceProviders()) {
        if (RefPtr localStorageNamespace = storageNamespaceProvider->optionalLocalStorageNamespace())
            static_cast<StorageNamespaceImpl&>(*localStorageNamespace).closeIdleLocalStorageDatabases();
    }
}

void WebStorageNamespaceProvider::syncLocalStorage()
{
    for (const auto& storageNamespaceProvider : storageNamespaceProviders()) {
        if (RefPtr localStorageNamespace = storageNamespaceProvider->optionalLocalStorageNamespace())
            static_cast<StorageNamespaceImpl&>(*localStorageNamespace).sync();
    }
}

Ref<StorageNamespace> WebStorageNamespaceProvider::createLocalStorageNamespace(unsigned quota, PAL::SessionID sessionID)
{
    return StorageNamespaceImpl::getOrCreateLocalStorageNamespace(m_localStorageDatabasePath, quota, sessionID);
}

Ref<StorageNamespace> WebStorageNamespaceProvider::createTransientLocalStorageNamespace(SecurityOrigin&, unsigned quota, PAL::SessionID sessionID)
{
    // FIXME: A smarter implementation would create a special namespace type instead of just piggy-backing off
    // SessionStorageNamespace here.
    return StorageNamespaceImpl::createSessionStorageNamespace(quota, sessionID);
}

RefPtr<StorageNamespace> WebStorageNamespaceProvider::sessionStorageNamespace(const SecurityOrigin& topLevelOrigin, Page& page, ShouldCreateNamespace shouldCreate)
{
    ASSERT(sessionStorageQuota() != WebCore::StorageMap::noQuota);

    if (m_sessionStorageNamespaces.find(page) == m_sessionStorageNamespaces.end()) {
        if (shouldCreate == ShouldCreateNamespace::No)
            return nullptr;
        HashMap<SecurityOriginData, RefPtr<StorageNamespace>> map;
        m_sessionStorageNamespaces.set(page, map);
    }
    auto& sessionStorageNamespaces = m_sessionStorageNamespaces.find(page)->value;

    auto sessionStorageNamespaceIt = sessionStorageNamespaces.find(topLevelOrigin.data());
    if (sessionStorageNamespaceIt == sessionStorageNamespaces.end()) {
        if (shouldCreate == ShouldCreateNamespace::No)
            return nullptr;
        return sessionStorageNamespaces.add(topLevelOrigin.data(), StorageNamespaceImpl::createSessionStorageNamespace(sessionStorageQuota(), page.sessionID())).iterator->value;
    }
    return sessionStorageNamespaceIt->value;
}

void WebStorageNamespaceProvider::copySessionStorageNamespace(WebCore::Page& srcPage, WebCore::Page& dstPage)
{
    ASSERT(sessionStorageQuota() != WebCore::StorageMap::noQuota);

    auto& srcSessionStorageNamespaces = static_cast<WebStorageNamespaceProvider&>(srcPage.storageNamespaceProvider()).m_sessionStorageNamespaces;
    auto srcPageIt = srcSessionStorageNamespaces.find(srcPage);
    if (srcPageIt == srcSessionStorageNamespaces.end())
        return;

    auto& srcPageSessionStorageNamespaces = srcPageIt->value;
    HashMap<SecurityOriginData, RefPtr<StorageNamespace>> dstPageSessionStorageNamespaces;
    for (auto& [origin, srcNamespace] : srcPageSessionStorageNamespaces)
        dstPageSessionStorageNamespaces.set(origin, srcNamespace->copy(dstPage));

    auto& dstSessionStorageNamespaces = static_cast<WebStorageNamespaceProvider&>(dstPage.storageNamespaceProvider()).m_sessionStorageNamespaces;
    ASSERT(!dstSessionStorageNamespaces.contains(dstPage));
    dstSessionStorageNamespaces.set(dstPage, WTFMove(dstPageSessionStorageNamespaces));
}

}
