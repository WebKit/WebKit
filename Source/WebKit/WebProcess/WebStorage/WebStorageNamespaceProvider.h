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

#pragma once

#include "StorageNamespaceIdentifier.h"
#include "StorageNamespaceImpl.h"
#include <WebCore/StorageNamespaceProvider.h>

namespace WebKit {

class WebStorageNamespaceProvider final : public WebCore::StorageNamespaceProvider, public CanMakeWeakPtr<WebStorageNamespaceProvider> {
public:
    static Ref<WebStorageNamespaceProvider> getOrCreate();
    virtual ~WebStorageNamespaceProvider();

    static void incrementUseCount(const StorageNamespaceImpl::Identifier);
    static void decrementUseCount(const StorageNamespaceImpl::Identifier);

private:
    explicit WebStorageNamespaceProvider();

    Ref<WebCore::StorageNamespace> createLocalStorageNamespace(unsigned quota, PAL::SessionID) override;
    Ref<WebCore::StorageNamespace> createTransientLocalStorageNamespace(WebCore::SecurityOrigin&, unsigned quota, PAL::SessionID) override;

    RefPtr<WebCore::StorageNamespace> sessionStorageNamespace(const WebCore::SecurityOrigin&, WebCore::Page&, ShouldCreateNamespace) final;
    void copySessionStorageNamespace(WebCore::Page&, WebCore::Page&) final;
    struct SessionStorageNamespaces {
        unsigned useCount { 0 };
        HashMap<WebCore::SecurityOriginData, RefPtr<WebCore::StorageNamespace>> map;
    };

    HashMap<StorageNamespaceImpl::Identifier, SessionStorageNamespaces> m_sessionStorageNamespaces;
};

}
