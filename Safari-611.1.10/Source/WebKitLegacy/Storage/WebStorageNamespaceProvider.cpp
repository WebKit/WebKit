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

static HashSet<WebStorageNamespaceProvider*>& storageNamespaceProviders()
{
    static NeverDestroyed<HashSet<WebStorageNamespaceProvider*>> storageNamespaceProviders;

    return storageNamespaceProviders;
}

Ref<WebStorageNamespaceProvider> WebStorageNamespaceProvider::create(const String& localStorageDatabasePath)
{
    return adoptRef(*new WebStorageNamespaceProvider(localStorageDatabasePath));
}

WebStorageNamespaceProvider::WebStorageNamespaceProvider(const String& localStorageDatabasePath)
    : m_localStorageDatabasePath(localStorageDatabasePath.isNull() ? emptyString() : localStorageDatabasePath)
{
    storageNamespaceProviders().add(this);
}

WebStorageNamespaceProvider::~WebStorageNamespaceProvider()
{
    ASSERT(storageNamespaceProviders().contains(this));
    storageNamespaceProviders().remove(this);
}

void WebStorageNamespaceProvider::closeLocalStorage()
{
    for (const auto& storageNamespaceProvider : storageNamespaceProviders()) {
        if (auto* localStorageNamespace = storageNamespaceProvider->optionalLocalStorageNamespace())
            static_cast<StorageNamespaceImpl*>(localStorageNamespace)->close();
    }
}

void WebStorageNamespaceProvider::clearLocalStorageForAllOrigins()
{
    for (const auto& storageNamespaceProvider : storageNamespaceProviders()) {
        if (auto* localStorageNamespace = storageNamespaceProvider->optionalLocalStorageNamespace())
            static_cast<StorageNamespaceImpl*>(localStorageNamespace)->clearAllOriginsForDeletion();
    }
}

void WebStorageNamespaceProvider::clearLocalStorageForOrigin(const SecurityOriginData& origin)
{
    for (const auto& storageNamespaceProvider : storageNamespaceProviders()) {
        if (auto* localStorageNamespace = storageNamespaceProvider->optionalLocalStorageNamespace())
            static_cast<StorageNamespaceImpl*>(localStorageNamespace)->clearOriginForDeletion(origin);
    }
}

void WebStorageNamespaceProvider::closeIdleLocalStorageDatabases()
{
    for (const auto& storageNamespaceProvider : storageNamespaceProviders()) {
        if (auto* localStorageNamespace = storageNamespaceProvider->optionalLocalStorageNamespace())
            static_cast<StorageNamespaceImpl*>(localStorageNamespace)->closeIdleLocalStorageDatabases();
    }
}

void WebStorageNamespaceProvider::syncLocalStorage()
{
    for (const auto& storageNamespaceProvider : storageNamespaceProviders()) {
        if (auto* localStorageNamespace = storageNamespaceProvider->optionalLocalStorageNamespace())
            static_cast<StorageNamespaceImpl*>(localStorageNamespace)->sync();
    }
}

Ref<StorageNamespace> WebStorageNamespaceProvider::createSessionStorageNamespace(Page& page, unsigned quota)
{
    return StorageNamespaceImpl::createSessionStorageNamespace(quota, page.sessionID());
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

}
