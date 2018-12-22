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
    ASSERT(m_pages.isEmpty());
}

void StorageNamespaceProvider::addPage(Page& page)
{
    ASSERT(!m_pages.contains(&page));

    m_pages.add(&page);
}

void StorageNamespaceProvider::removePage(Page& page)
{
    ASSERT(m_pages.contains(&page));

    m_pages.remove(&page);
}

Ref<StorageArea> StorageNamespaceProvider::localStorageArea(Document& document)
{
    // This StorageNamespaceProvider was retrieved from the Document's Page,
    // so the Document had better still actually have a Page.
    ASSERT(document.page());

    bool ephemeral = document.page()->usesEphemeralSession();
    bool transient = !document.securityOrigin().canAccessLocalStorage(&document.topOrigin());

    RefPtr<StorageNamespace> storageNamespace;

    if (transient)
        storageNamespace = &transientLocalStorageNamespace(document.topOrigin());
    else if (ephemeral)
        storageNamespace = document.page()->ephemeralLocalStorage();
    else
        storageNamespace = &localStorageNamespace();

    return storageNamespace->storageArea(document.securityOrigin().data());
}

StorageNamespace& StorageNamespaceProvider::localStorageNamespace()
{
    if (!m_localStorageNamespace)
        m_localStorageNamespace = createLocalStorageNamespace(localStorageDatabaseQuotaInBytes);

    return *m_localStorageNamespace;
}

StorageNamespace& StorageNamespaceProvider::transientLocalStorageNamespace(SecurityOrigin& securityOrigin)
{
    auto& slot = m_transientLocalStorageMap.add(&securityOrigin, nullptr).iterator->value;
    if (!slot)
        slot = createTransientLocalStorageNamespace(securityOrigin, localStorageDatabaseQuotaInBytes);

    return *slot;
}

}
