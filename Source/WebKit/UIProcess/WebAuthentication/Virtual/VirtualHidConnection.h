/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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

#include "HidConnection.h"
#include "VirtualAuthenticatorConfiguration.h"
#include <WebCore/FidoHidMessage.h>
#include <wtf/WeakPtr.h>

namespace WebKit {
class VirtualHidConnection;
}

namespace WTF {
template<typename T> struct IsDeprecatedWeakRefSmartPointerException;
template<> struct IsDeprecatedWeakRefSmartPointerException<WebKit::VirtualHidConnection> : std::true_type { };
}

namespace WebKit {
struct VirtualAuthenticatorConfiguration;
class VirtualAuthenticatorManager;

class VirtualHidConnection final : public CanMakeWeakPtr<VirtualHidConnection>, public HidConnection {
public:
    explicit VirtualHidConnection(const String& authenticatorId, const VirtualAuthenticatorConfiguration&, const WeakPtr<VirtualAuthenticatorManager>&);

private:
    void initialize() final;
    void terminate() final;
    DataSent sendSync(const Vector<uint8_t>& data) final;
    void send(Vector<uint8_t>&& data, DataSentCallback&&) final;
    void assembleRequest(Vector<uint8_t>&&);
    void parseRequest();

    void receiveHidMessage(fido::FidoHidMessage&&);
    void recieveResponseCode(fido::CtapDeviceResponseCode);

    WeakPtr<VirtualAuthenticatorManager> m_manager;
    VirtualAuthenticatorConfiguration m_configuration;
    std::optional<fido::FidoHidMessage> m_requestMessage;
    Vector<uint8_t> m_nonce;
    uint32_t m_currentChannel { fido::kHidBroadcastChannel };
    String m_authenticatorId;
};
} // namespace WebKit
#endif
