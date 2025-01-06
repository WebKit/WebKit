/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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

#if ENABLE(WEB_AUTHN)

#include "AuthenticatorManager.h"
#include "VirtualAuthenticatorConfiguration.h"
#include "VirtualCredential.h"

namespace WebKit {
struct VirtualCredential;

class VirtualAuthenticatorManager final : public AuthenticatorManager {
public:
    static Ref<VirtualAuthenticatorManager> create();

    String createAuthenticator(const VirtualAuthenticatorConfiguration& /*config/*/);
    bool removeAuthenticator(const String& /*authenticatorId*/);

    bool isVirtual() const final { return true; }

    void addCredential(const String&, VirtualCredential&);
    Vector<VirtualCredential> credentialsMatchingList(const String& authenticatorId, const String& rpId, const Vector<Vector<uint8_t>>& credentialIds);

protected:
    void decidePolicyForLocalAuthenticator(CompletionHandler<void(LocalAuthenticatorPolicy)>&&) override;
    void selectAssertionResponse(Vector<Ref<WebCore::AuthenticatorAssertionResponse>>&&, WebAuthenticationSource, CompletionHandler<void(WebCore::AuthenticatorAssertionResponse*)>&&) override;
    
    
private:
    VirtualAuthenticatorManager();

    Ref<AuthenticatorTransportService> createService(WebCore::AuthenticatorTransport, AuthenticatorTransportServiceObserver&) const final;
    void runPanel() override;
    void filterTransports(TransportSet&) const override { };

    HashMap<String, UniqueRef<VirtualAuthenticatorConfiguration>> m_virtualAuthenticators;
    HashMap<String, Vector<VirtualCredential>> m_credentialsByAuthenticator;
};

} // namespace WebKit

SPECIALIZE_TYPE_TRAITS_BEGIN(WebKit::VirtualAuthenticatorManager)
static bool isType(const WebKit::AuthenticatorManager& manager) { return manager.isVirtual(); }
SPECIALIZE_TYPE_TRAITS_END()

#endif // ENABLE(WEB_AUTHN)
