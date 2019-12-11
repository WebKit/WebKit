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

#include "StorageNamespaceImpl.h"
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

WebStorageNamespaceProvider::WebStorageNamespaceProvider(StorageNamespaceIdentifier localStorageNamespaceIdentifier)
    : m_localStorageNamespaceIdentifier(localStorageNamespaceIdentifier)
{
}

WebStorageNamespaceProvider::~WebStorageNamespaceProvider()
{
    ASSERT(storageNamespaceProviders().contains(m_localStorageNamespaceIdentifier));

    storageNamespaceProviders().remove(m_localStorageNamespaceIdentifier);
}

Ref<WebCore::StorageNamespace> WebStorageNamespaceProvider::createSessionStorageNamespace(Page& page, unsigned quota)
{
    auto& webPage = WebPage::fromCorePage(page);
    return StorageNamespaceImpl::createSessionStorageNamespace(webPage.sessionStorageNamespaceIdentifier(), webPage.identifier(), quota);
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

}
