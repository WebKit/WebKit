/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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
#include "StorageNamespaceProxy.h"

#include "StorageAreaProxy.h"
#include "WebPage.h"
#include <WebCore/SecurityOrigin.h>
#include <WebCore/Settings.h>

using namespace WebCore;

namespace WebKit {

PassRefPtr<StorageNamespaceProxy> StorageNamespaceProxy::createSessionStorageNamespace(WebPage* webPage)
{
    return adoptRef(new StorageNamespaceProxy(webPage->pageID(), webPage->corePage()->settings()->sessionStorageQuota()));
}

StorageNamespaceProxy::StorageNamespaceProxy(uint64_t storageNamespaceID, unsigned quotaInBytes)
    : m_storageNamespaceID(storageNamespaceID)
    , m_quotaInBytes(quotaInBytes)
{
}

StorageNamespaceProxy::~StorageNamespaceProxy()
{
}

PassRefPtr<StorageArea> StorageNamespaceProxy::storageArea(PassRefPtr<SecurityOrigin> securityOrigin)
{
    printf("%p - looking for storage area in %s\n", this, securityOrigin->toString().utf8().data());
    HashMap<RefPtr<WebCore::SecurityOrigin>, RefPtr<StorageAreaProxy> >::AddResult result = m_storageAreaMap.add(securityOrigin.get(), 0);
    if (result.isNewEntry) {
        result.iterator->value = StorageAreaProxy::create(this, securityOrigin);
        printf("new entry!!\n");
    } else {
        printf("reusing!!\n");
    }

    printf("returning %p\n", result.iterator->value.get());
    return result.iterator->value;
}

PassRefPtr<StorageNamespace> StorageNamespaceProxy::copy()
{
    // FIXME: Implement this.
    ASSERT_NOT_REACHED();
    return 0;
}

void StorageNamespaceProxy::close()
{
    // FIXME: Implement this.
    ASSERT_NOT_REACHED();
}

void StorageNamespaceProxy::clearOriginForDeletion(SecurityOrigin*)
{
    // FIXME: Implement this.
    ASSERT_NOT_REACHED();
}

void StorageNamespaceProxy::clearAllOriginsForDeletion()
{
    // FIXME: Implement this.
    ASSERT_NOT_REACHED();
}

void StorageNamespaceProxy::sync()
{
    // FIXME: Implement this.
    ASSERT_NOT_REACHED();
}

void StorageNamespaceProxy::closeIdleLocalStorageDatabases()
{
    // FIXME: Implement this.
    ASSERT_NOT_REACHED();
}

} // namespace WebKit
