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

#include "config.h"
#include "StorageNamespaceProvider.h"

#include "Document.h"
#include "Page.h"
#include "SecurityOriginData.h"
#include "StorageArea.h"
#include "StorageNamespace.h"

namespace WebCore {

// Suggested by the HTML5 spec.
unsigned localStorageDatabaseQuotaInBytes = 5 * 1024 * 1024;

StorageNamespaceProvider::StorageNamespaceProvider()
{
}

StorageNamespaceProvider::~StorageNamespaceProvider()
{
}

Ref<StorageArea> StorageNamespaceProvider::localStorageArea(Document& document)
{
    // This StorageNamespaceProvider was retrieved from the Document's Page,
    // so the Document had better still actually have a Page.
    ASSERT(document.page());

    bool transient = !document.securityOrigin().canAccessLocalStorage(&document.topOrigin());

    RefPtr<StorageNamespace> storageNamespace;

    if (transient)
        storageNamespace = &transientLocalStorageNamespace(document.topOrigin(), document.page()->sessionID());
    else
        storageNamespace = &localStorageNamespace(document.page()->sessionID());

    return storageNamespace->storageArea(document.securityOrigin().data());
}

StorageNamespace& StorageNamespaceProvider::localStorageNamespace(PAL::SessionID sessionID)
{
    if (!m_localStorageNamespace)
        m_localStorageNamespace = createLocalStorageNamespace(localStorageDatabaseQuotaInBytes, sessionID);

    ASSERT(m_localStorageNamespace->sessionID() == sessionID);
    return *m_localStorageNamespace;
}

StorageNamespace& StorageNamespaceProvider::transientLocalStorageNamespace(SecurityOrigin& securityOrigin, PAL::SessionID sessionID)
{
    auto& slot = m_transientLocalStorageNamespaces.add(securityOrigin.data(), nullptr).iterator->value;
    if (!slot)
        slot = createTransientLocalStorageNamespace(securityOrigin, localStorageDatabaseQuotaInBytes, sessionID);

    ASSERT(slot->sessionID() == sessionID);
    return *slot;
}

void StorageNamespaceProvider::setSessionIDForTesting(const PAL::SessionID& newSessionID)
{
    if (m_localStorageNamespace && newSessionID != m_localStorageNamespace->sessionID())
        m_localStorageNamespace->setSessionIDForTesting(newSessionID);
    
    for (auto& transientLocalStorageNamespace : m_transientLocalStorageNamespaces.values()) {
        if (newSessionID != transientLocalStorageNamespace->sessionID())
            m_localStorageNamespace->setSessionIDForTesting(newSessionID);
    }
}

}
