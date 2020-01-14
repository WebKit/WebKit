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

#include "FidoAuthenticator.h"
#include <WebCore/AuthenticatorGetInfoResponse.h>

namespace fido {
namespace pin {
class TokenRequest;
}
}

namespace WebCore {
class CryptoKeyEC;
}

namespace WebKit {

class CtapDriver;

class CtapAuthenticator final : public FidoAuthenticator {
public:
    static Ref<CtapAuthenticator> create(std::unique_ptr<CtapDriver>&& driver, fido::AuthenticatorGetInfoResponse&& info)
    {
        return adoptRef(*new CtapAuthenticator(WTFMove(driver), WTFMove(info)));
    }

private:
    explicit CtapAuthenticator(std::unique_ptr<CtapDriver>&&, fido::AuthenticatorGetInfoResponse&&);

    void makeCredential() final;
    void continueMakeCredentialAfterResponseReceived(Vector<uint8_t>&&);
    void getAssertion() final;
    void continueGetAssertionAfterResponseReceived(Vector<uint8_t>&&);
    void continueGetNextAssertionAfterResponseReceived(Vector<uint8_t>&&);

    void getRetries();
    void continueGetKeyAgreementAfterGetRetries(Vector<uint8_t>&&);
    void continueRequestPinAfterGetKeyAgreement(Vector<uint8_t>&&, uint64_t retries);
    void continueGetPinTokenAfterRequestPin(const String& pin, const WebCore::CryptoKeyEC&);
    void continueRequestAfterGetPinToken(Vector<uint8_t>&&, const fido::pin::TokenRequest&);

    bool tryDowngrade();
    bool processGoogleLegacyAppIdSupportExtension();

    fido::AuthenticatorGetInfoResponse m_info;
    bool m_isDowngraded { false };
    size_t m_remainingAssertionResponses { 0 };
    HashSet<Ref<WebCore::AuthenticatorAssertionResponse>> m_assertionResponses;
    Vector<uint8_t> m_pinAuth;
};

} // namespace WebKit

#endif // ENABLE(WEB_AUTHN)
