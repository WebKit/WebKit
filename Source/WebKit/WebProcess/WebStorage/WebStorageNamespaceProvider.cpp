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
#include <wtf/HashMap.h>
#include <wtf/NeverDestroyed.h>

namespace WebKit {
using namespace WebCore;

static HashMap<uint64_t, WebStorageNamespaceProvider*>& storageNamespaceProviders()
{
    static NeverDestroyed<HashMap<uint64_t, WebStorageNamespaceProvider*>> storageNamespaceProviders;

    return storageNamespaceProviders;
}

Ref<WebStorageNamespaceProvider> WebStorageNamespaceProvider::getOrCreate(uint64_t identifier)
{
    auto& slot = storageNamespaceProviders().add(identifier, nullptr).iterator->value;
    if (slot)
        return *slot;

    auto storageNamespaceProvider = adoptRef(*new WebStorageNamespaceProvider(identifier));
    slot = storageNamespaceProvider.ptr();

    return storageNamespaceProvider;
}

WebStorageNamespaceProvider::WebStorageNamespaceProvider(uint64_t identifier)
    : m_identifier(identifier)
{
}

WebStorageNamespaceProvider::~WebStorageNamespaceProvider()
{
    ASSERT(storageNamespaceProviders().contains(m_identifier));

    storageNamespaceProviders().remove(m_identifier);
}

Ref<WebCore::StorageNamespace> WebStorageNamespaceProvider::createSessionStorageNamespace(Page& page, unsigned quota)
{
    return StorageNamespaceImpl::createSessionStorageNamespace(WebPage::fromCorePage(&page)->pageID(), quota);
}

Ref<WebCore::StorageNamespace> WebStorageNamespaceProvider::createEphemeralLocalStorageNamespace(Page& page, unsigned quota)
{
    return StorageNamespaceImpl::createEphemeralLocalStorageNamespace(WebPage::fromCorePage(&page)->pageID(), quota);
}

Ref<WebCore::StorageNamespace> WebStorageNamespaceProvider::createLocalStorageNamespace(unsigned quota)
{
    return StorageNamespaceImpl::createLocalStorageNamespace(m_identifier, quota);
}

Ref<WebCore::StorageNamespace> WebStorageNamespaceProvider::createTransientLocalStorageNamespace(WebCore::SecurityOrigin& topLevelOrigin, unsigned quota)
{
    return StorageNamespaceImpl::createTransientLocalStorageNamespace(m_identifier, topLevelOrigin, quota);
}

}
