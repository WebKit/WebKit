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

VirtualAuthenticatorManager::VirtualAuthenticatorManager()
    : AuthenticatorManager()
{
}

String VirtualAuthenticatorManager::createAuthenticator(const VirtualAuthenticatorConfiguration& config)
{
    if (config.transport != WebCore::AuthenticatorTransport::Internal)
        UNIMPLEMENTED();
    auto id = createVersion4UUIDString();
    m_virtualAuthenticators.set(id, makeUniqueRef<VirtualAuthenticatorConfiguration>(config));

    return id;
}

bool VirtualAuthenticatorManager::removeAuthenticator(const String& id)
{
    return m_virtualAuthenticators.remove(id);
}

UniqueRef<AuthenticatorTransportService> VirtualAuthenticatorManager::createService(WebCore::AuthenticatorTransport transport, AuthenticatorTransportService::Observer& observer) const
{
    Vector<VirtualAuthenticatorConfiguration> configs;
    for (auto& config : m_virtualAuthenticators.values()) {
        if (config.get().transport == transport)
            configs.append(config.get());
    }
    return VirtualService::createVirtual(transport, observer, configs);
}

} // namespace WebKit

#endif // ENABLE(WEB_AUTHN)
