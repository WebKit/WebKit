/*
 * Copyright (C) 2026 Shopify Inc. All rights reserved.
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

#if ENABLE(FEDCM)

#include <WebCore/BasicCredential.h>
#include <WebCore/JSDOMPromiseDeferredForward.h>
#include <WebCore/MockFedCMConfiguration.h>
#include <wtf/Ref.h>
#include <wtf/RefCounted.h>

namespace WebCore {

class Document;
class FedCMProxyClient;
class Page;
struct IdentityCredentialCreateOptions;
struct IdentityCredentialRequestOptions;
using CredentialPromise = DOMPromiseDeferred<IDLNullable<IDLInterface<BasicCredential>>>;

class FedCMProxy : public RefCounted<FedCMProxy> {
    WTF_MAKE_TZONE_ALLOCATED(FedCMProxy);
public:
    static Ref<FedCMProxy> create(Ref<FedCMProxyClient>&&, Page&);

    void requestCredential(const Document&, CredentialPromise&&, IdentityCredentialRequestOptions&&);
    void createCredential(const Document&, CredentialPromise&&, IdentityCredentialCreateOptions&&);

    WEBCORE_EXPORT void setMockConfiguration(MockFedCMConfiguration&&);
    WEBCORE_EXPORT void clearMockConfiguration();

private:
    FedCMProxy(Ref<FedCMProxyClient>&&, Page&);

    const Ref<FedCMProxyClient> m_client;
    WeakPtr<Page> m_page;
    std::optional<MockFedCMConfiguration> m_mockConfig;
};

} // namespace WebCore

#endif // ENABLE(FEDCM)
