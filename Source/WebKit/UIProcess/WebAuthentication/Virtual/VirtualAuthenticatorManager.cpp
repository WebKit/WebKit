/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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
#include "VirtualAuthenticatorManager.h"

#if ENABLE(WEB_AUTHN)

#include <VirtualService.h>
#include <wtf/UUID.h>

namespace WebKit {

struct VirtualCredential;

Ref<VirtualAuthenticatorManager> VirtualAuthenticatorManager::create()
{
    return adoptRef(*new VirtualAuthenticatorManager);
}

VirtualAuthenticatorManager::VirtualAuthenticatorManager() = default;

String VirtualAuthenticatorManager::createAuthenticator(const VirtualAuthenticatorConfiguration& config)
{
    auto id = createVersion4UUIDString();
    m_virtualAuthenticators.set(id, makeUniqueRef<VirtualAuthenticatorConfiguration>(config));
    Vector<VirtualCredential> credentials;
    m_credentialsByAuthenticator.set(id, WTFMove(credentials));

    return id;
}

bool VirtualAuthenticatorManager::removeAuthenticator(const String& id)
{
    return m_virtualAuthenticators.remove(id);
}

void VirtualAuthenticatorManager::addCredential(const String& authenticatorId, VirtualCredential& credential)
{
    m_credentialsByAuthenticator.find(authenticatorId)->value.append(WTFMove(credential));
}

Vector<VirtualCredential> VirtualAuthenticatorManager::credentialsMatchingList(const String& authenticatorId, const String& rpId, const Vector<Vector<uint8_t>>& credentialIds)
{
    Vector<VirtualCredential> matching;
    auto it = m_credentialsByAuthenticator.find(authenticatorId);
    for (auto& credential : it->value) {
        if (credential.rpId == rpId && ((credentialIds.isEmpty() && credential.isResidentCredential) || credentialIds.contains(credential.credentialId)))
            matching.append(credential);
    }
    return matching;
}

Ref<AuthenticatorTransportService> VirtualAuthenticatorManager::createService(WebCore::AuthenticatorTransport transport, AuthenticatorTransportServiceObserver& observer) const
{
    Vector<std::pair<String, VirtualAuthenticatorConfiguration>> configs;
    for (auto& id : m_virtualAuthenticators.keys()) {
        auto config = m_virtualAuthenticators.get(id);
        if (config->transport == transport)
            configs.append(std::pair { id, *config });
    }
    return VirtualService::createVirtual(transport, observer, configs);
}

void VirtualAuthenticatorManager::runPanel()
{
    auto transports = getTransports();
    if (transports.isEmpty()) {
        cancel();
        return;
    }

    startDiscovery(transports);
}

void VirtualAuthenticatorManager::selectAssertionResponse(Vector<Ref<WebCore::AuthenticatorAssertionResponse>>&& responses, WebAuthenticationSource source, CompletionHandler<void(WebCore::AuthenticatorAssertionResponse*)>&& completionHandler)
{
    completionHandler(responses[0].ptr());
}

void VirtualAuthenticatorManager::decidePolicyForLocalAuthenticator(CompletionHandler<void(LocalAuthenticatorPolicy)>&& completionHandler)
{
    completionHandler(LocalAuthenticatorPolicy::Allow);
}

} // namespace WebKit

#endif // ENABLE(WEB_AUTHN)
