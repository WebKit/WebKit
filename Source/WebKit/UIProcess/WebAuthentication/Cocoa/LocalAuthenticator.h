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

#include "Authenticator.h"
#include "LocalConnection.h"
#include <wtf/UniqueRef.h>

OBJC_CLASS LAContext;

namespace WebCore {
class AuthenticatorAttestationResponse;
class AuthenticatorAssertionResponse;
}

namespace WebKit {

BOOL shouldUseAlternateKeychainAttribute();

class LocalAuthenticator final : public Authenticator {
public:
    // Here is the FSM.
    // MakeCredential: Init => RequestReceived => PolicyDecided => UserVerified => (Attested) => End
    // GetAssertion: Init => RequestReceived => ResponseSelected => UserVerified => End
    enum class State {
        Init,
        RequestReceived,
        UserVerified,
        Attested,
        ResponseSelected,
        PolicyDecided,
    };

    static Ref<LocalAuthenticator> create(Ref<LocalConnection>&& connection)
    {
        return adoptRef(*new LocalAuthenticator(WTFMove(connection)));
    }

    static void clearAllCredentials();

private:
    explicit LocalAuthenticator(Ref<LocalConnection>&&);

    std::optional<WebCore::ExceptionData> processClientExtensions(std::variant<Ref<WebCore::AuthenticatorAttestationResponse>, Ref<WebCore::AuthenticatorAssertionResponse>>);

    void makeCredential() final;
    void continueMakeCredentialAfterReceivingLAContext(LAContext *);
    void continueMakeCredentialAfterUserVerification(SecAccessControlRef, LocalConnection::UserVerification, LAContext *);
    void continueMakeCredentialAfterAttested(Vector<uint8_t>&& credentialId, Vector<uint8_t>&& authData, NSArray *certificates, NSError *);
    void finishMakeCredential(Vector<uint8_t>&& credentialId, Vector<uint8_t>&& attestationObject, std::optional<WebCore::ExceptionData>);

    void getAssertion() final;
    void continueGetAssertionAfterResponseSelected(Ref<WebCore::AuthenticatorAssertionResponse>&&);
    void continueGetAssertionAfterUserVerification(Ref<WebCore::AuthenticatorAssertionResponse>&&, LocalConnection::UserVerification, LAContext *);

    void receiveException(WebCore::ExceptionData&&, WebAuthenticationStatus = WebAuthenticationStatus::LAError) const;
    void deleteDuplicateCredential() const;
    bool validateUserVerification(LocalConnection::UserVerification) const;

    std::optional<WebCore::ExceptionData> processLargeBlobExtension(const WebCore::PublicKeyCredentialCreationOptions&, WebCore::AuthenticationExtensionsClientOutputs& extensionOutputs);
    std::optional<WebCore::ExceptionData> processLargeBlobExtension(const WebCore::PublicKeyCredentialRequestOptions&, WebCore::AuthenticationExtensionsClientOutputs& extensionOutputs, const Ref<WebCore::AuthenticatorAssertionResponse>&);

    std::optional<Vector<Ref<WebCore::AuthenticatorAssertionResponse>>> getExistingCredentials(const String& rpId);

    Ref<LocalConnection> protectedConnection() const { return m_connection; }

    State m_state { State::Init };
    Ref<LocalConnection> m_connection;
    Vector<Ref<WebCore::AuthenticatorAssertionResponse>> m_existingCredentials;
    RetainPtr<NSData> m_provisionalCredentialId;
};

} // namespace WebKit

#endif // ENABLE(WEB_AUTHN)
