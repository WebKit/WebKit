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

#include "config.h"
#include "MockAuthenticatorManager.h"

#if ENABLE(WEB_AUTHN)

namespace WebKit {

MockAuthenticatorManager::MockAuthenticatorManager(WebCore::MockWebAuthenticationConfiguration&& configuration)
    : m_testConfiguration(WTFMove(configuration))
{
}

UniqueRef<AuthenticatorTransportService> MockAuthenticatorManager::createService(WebCore::AuthenticatorTransport transport, AuthenticatorTransportService::Observer& observer) const
{
    return AuthenticatorTransportService::createMock(transport, observer, m_testConfiguration);
}

void MockAuthenticatorManager::respondReceivedInternal(Respond&& respond)
{
    if (m_testConfiguration.silentFailure)
        return;

    invokePendingCompletionHandler(WTFMove(respond));
    clearStateAsync();
    requestTimeOutTimer().stop();
}

void MockAuthenticatorManager::filterTransports(TransportSet& transports) const
{
    if (!m_testConfiguration.nfc)
        transports.remove(WebCore::AuthenticatorTransport::Nfc);
    if (!m_testConfiguration.local)
        transports.remove(WebCore::AuthenticatorTransport::Internal);
}

} // namespace WebKit

#endif // ENABLE(WEB_AUTHN)
