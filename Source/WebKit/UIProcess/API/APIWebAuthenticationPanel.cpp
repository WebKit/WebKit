/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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
#include "APIWebAuthenticationPanel.h"

#if ENABLE(WEB_AUTHN)

#include "APIWebAuthenticationPanelClient.h"
#include "AuthenticatorManager.h"
#include <WebCore/WebAuthenticationConstants.h>

namespace API {
using namespace WebCore;
using namespace WebKit;

Ref<WebAuthenticationPanel> WebAuthenticationPanel::create(const AuthenticatorManager& manager, const WTF::String& rpId, const TransportSet& transports, ClientDataType type)
{
    return adoptRef(*new WebAuthenticationPanel(manager, rpId, transports, type));
}

WebAuthenticationPanel::WebAuthenticationPanel(const AuthenticatorManager& manager, const WTF::String& rpId, const TransportSet& transports, ClientDataType type)
    : m_manager(makeWeakPtr(manager))
    , m_rpId(rpId)
    , m_client(WTF::makeUniqueRef<WebAuthenticationPanelClient>())
    , m_clientDataType(type)
{
    m_transports = Vector<AuthenticatorTransport>();
    m_transports.reserveInitialCapacity(AuthenticatorManager::maxTransportNumber);
    if (transports.contains(AuthenticatorTransport::Usb))
        m_transports.uncheckedAppend(AuthenticatorTransport::Usb);
    if (transports.contains(AuthenticatorTransport::Nfc))
        m_transports.uncheckedAppend(AuthenticatorTransport::Nfc);
}

WebAuthenticationPanel::~WebAuthenticationPanel() = default;

void WebAuthenticationPanel::cancel() const
{
    if (m_manager)
        m_manager->cancelRequest(*this);
}

void WebAuthenticationPanel::setClient(UniqueRef<WebAuthenticationPanelClient>&& client)
{
    m_client = WTFMove(client);
}

} // namespace API

#endif // ENABLE(WEB_AUTHN)
