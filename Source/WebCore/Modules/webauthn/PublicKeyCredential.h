/*
 * Copyright (C) 2017-2025 Apple Inc. All rights reserved.
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

#include <WebCore/AuthenticationResponseJSON.h>
#include <WebCore/BasicCredential.h>
#include <WebCore/IDLTypes.h>
#include <WebCore/RegistrationResponseJSON.h>
#include <wtf/Forward.h>

namespace WebCore {

enum class AuthenticatorAttachment : uint8_t;
class AuthenticatorResponse;
class Document;

typedef IDLRecord<IDLDOMString, IDLBoolean> PublicKeyCredentialClientCapabilities;
typedef Variant<RegistrationResponseJSON, AuthenticationResponseJSON> PublicKeyCredentialJSON;

struct PublicKeyCredentialCreationOptions;
struct PublicKeyCredentialCreationOptionsJSON;
struct PublicKeyCredentialRequestOptions;
struct PublicKeyCredentialRequestOptionsJSON;
struct AuthenticationExtensionsClientOutputs;
struct UnknownCredentialOptions;
struct AllAcceptedCredentialsOptions;
struct CurrentUserDetailsOptions;

template<typename IDLType> class DOMPromiseDeferred;
template<typename> class ExceptionOr;

class PublicKeyCredential final : public BasicCredential {
public:
    static Ref<PublicKeyCredential> create(Ref<AuthenticatorResponse>&&);

    ArrayBuffer& rawId() const;
    AuthenticatorResponse& response() const { return m_response; }
    AuthenticatorAttachment authenticatorAttachment() const;
    AuthenticationExtensionsClientOutputs getClientExtensionResults() const;
    PublicKeyCredentialJSON toJSON();

    static void isUserVerifyingPlatformAuthenticatorAvailable(Document&, DOMPromiseDeferred<IDLBoolean>&&);

    static void getClientCapabilities(Document&, DOMPromiseDeferred<PublicKeyCredentialClientCapabilities>&&);

    static ExceptionOr<PublicKeyCredentialCreationOptions> parseCreationOptionsFromJSON(PublicKeyCredentialCreationOptionsJSON&&);

    static ExceptionOr<PublicKeyCredentialRequestOptions> parseRequestOptionsFromJSON(PublicKeyCredentialRequestOptionsJSON&&);

    static void signalUnknownCredential(Document&, UnknownCredentialOptions&&, DOMPromiseDeferred<void>&&);
    static void signalAllAcceptedCredentials(Document&, AllAcceptedCredentialsOptions&&, DOMPromiseDeferred<void>&&);
    static void signalCurrentUserDetails(Document&, CurrentUserDetailsOptions&&, DOMPromiseDeferred<void>&&);

private:
    PublicKeyCredential(Ref<AuthenticatorResponse>&&);

    Type credentialType() const final { return Type::PublicKey; }

    const Ref<AuthenticatorResponse> m_response;
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_BASIC_CREDENTIAL(PublicKeyCredential, BasicCredential::Type::PublicKey)

#endif // ENABLE(WEB_AUTHN)
