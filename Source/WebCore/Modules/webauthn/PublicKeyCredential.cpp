/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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
#include "PublicKeyCredential.h"

#if ENABLE(WEB_AUTHN)

#include "AllAcceptedCredentialsOptions.h"
#include "AuthenticatorAssertionResponse.h"
#include "AuthenticatorAttestationResponse.h"
#include "AuthenticatorCoordinator.h"
#include "AuthenticatorResponse.h"
#include "BufferSource.h"
#include "CurrentUserDetailsOptions.h"
#include "Document.h"
#include "DocumentPage.h"
#include "JSAuthenticatorAttachment.h"
#include "JSDOMPromiseDeferred.h"
#include "JSPublicKeyCredentialCreationOptions.h"
#include "JSPublicKeyCredentialCreationOptionsJSON.h"
#include "JSPublicKeyCredentialRequestOptions.h"
#include "JSUserVerificationRequirement.h"
#include "Page.h"
#include "PublicKeyCredentialDescriptorJSON.h"
#include "PublicKeyCredentialRequestOptionsJSON.h"
#include "Settings.h"
#include "UnknownCredentialOptions.h"
#include "WebAuthenticationUtils.h"
#include <wtf/text/Base64.h>

namespace WebCore {

Ref<PublicKeyCredential> PublicKeyCredential::create(Ref<AuthenticatorResponse>&& response)
{
    return adoptRef(*new PublicKeyCredential(WTF::move(response)));
}

ArrayBuffer& PublicKeyCredential::rawId() const
{
    return m_response->rawId();
}

AuthenticationExtensionsClientOutputs PublicKeyCredential::getClientExtensionResults() const
{
    return m_response->extensions();
}

AuthenticatorAttachment PublicKeyCredential::authenticatorAttachment() const
{
    return m_response->attachment();
}

PublicKeyCredential::PublicKeyCredential(Ref<AuthenticatorResponse>&& response)
    : BasicCredential(base64URLEncodeToString(response->rawId().span()), Type::PublicKey, Discovery::Remote)
    , m_response(WTF::move(response))
{
}

void PublicKeyCredential::isUserVerifyingPlatformAuthenticatorAvailable(Document& document, DOMPromiseDeferred<IDLBoolean>&& promise)
{
    if (RefPtr page = document.page())
        page->authenticatorCoordinator().isUserVerifyingPlatformAuthenticatorAvailable(document, WTF::move(promise));
}

void PublicKeyCredential::getClientCapabilities(Document& document, DOMPromiseDeferred<IDLRecord<IDLDOMString, IDLBoolean>>&& promise)
{
    if (RefPtr page = document.page())
        page->authenticatorCoordinator().getClientCapabilities(document, WTF::move(promise));
}

PublicKeyCredentialJSON PublicKeyCredential::toJSON()
{
    if (RefPtr attestationResponse = dynamicDowncast<AuthenticatorAttestationResponse>(m_response)) {
        String encodedId;
        if (RefPtr decodedRawId = rawId())
            encodedId = base64URLEncodeToString(decodedRawId->span());

        return RegistrationResponseJSON {
            .id = encodedId,
            .rawId = encodedId,
            .response = attestationResponse->toJSON(),
            .authenticatorAttachment = convertEnumerationToString(authenticatorAttachment()),
            .clientExtensionResults = getClientExtensionResults().toJSON(),
            .type = type(),
        };
    }
    if (RefPtr assertionResponse = dynamicDowncast<AuthenticatorAssertionResponse>(m_response)) {
        String encodedId;
        if (RefPtr decodedRawId = rawId())
            encodedId = base64URLEncodeToString(decodedRawId->span());

        return AuthenticationResponseJSON {
            .id = encodedId,
            .rawId = encodedId,
            .response = assertionResponse->toJSON(),
            .authenticatorAttachment = convertEnumerationToString(authenticatorAttachment()),
            .clientExtensionResults = getClientExtensionResults().toJSON(),
            .type = type(),
        };
    }

    ASSERT_NOT_REACHED();
    return AuthenticationResponseJSON { };
}

template<typename ErrorStringFunction> static ExceptionOr<BufferSource> fromJSONBase64(String&& value, ErrorStringFunction&& errorStringFunction)
{
    if (auto decodedValue = base64URLDecode(value))
        return toBufferSource(decodedValue->span());
    return Exception { ExceptionCode::EncodingError, errorStringFunction() };
}

static ExceptionOr<PublicKeyCredentialDescriptor> fromJSON(PublicKeyCredentialDescriptorJSON&& jsonOptions)
{
    std::optional<PublicKeyCredentialType> descriptorType;
    if (auto type = parseEnumerationFromString<PublicKeyCredentialType>(jsonOptions.type))
        descriptorType = *type;
    else
        return Exception { ExceptionCode::EncodingError, makeString("Unrecognized credential type: "_s, jsonOptions.type) };

    auto id = fromJSONBase64(WTF::move(jsonOptions.id), [&] { return makeString("Invalid encoding of credential ID: "_s, jsonOptions.id, " (It should be Base64URL encoded.)"_s); });
    if (id.hasException())
        return id.releaseException();

    Vector<AuthenticatorTransport> descriptorTransports;
    for (auto transportString : jsonOptions.transports) {
        if (auto transport = convertStringToAuthenticatorTransport(transportString)) {
            descriptorTransports.append(*transport);
        }
    }

    return PublicKeyCredentialDescriptor {
        .type = WTF::move(*descriptorType),
        .id = id.releaseReturnValue(),
        .transports = WTF::move(descriptorTransports),
    };
}

static ExceptionOr<Vector<PublicKeyCredentialDescriptor>> fromJSON(Vector<PublicKeyCredentialDescriptorJSON>&& jsonDescriptors)
{
    Vector<PublicKeyCredentialDescriptor> descriptors;
    for (auto jsonDescriptor : jsonDescriptors) {
        auto descriptor = fromJSON(WTF::move(jsonDescriptor));
        if (descriptor.hasException())
            return descriptor.releaseException();
        descriptors.append(descriptor.releaseReturnValue());
    }
    return descriptors;
}

static ExceptionOr<AuthenticationExtensionsClientInputs::LargeBlobInputs> fromJSON(AuthenticationExtensionsClientInputsJSON::LargeBlobInputsJSON&& jsonInputs)
{
    auto write = fromJSONBase64(WTF::move(jsonInputs.write), [&] { return makeString("Invalid encoding of largeBlob.write: "_s, jsonInputs.write, " (It should be Base64URL encoded.)"_s); });
    if (write.hasException())
        return write.releaseException();

    return AuthenticationExtensionsClientInputs::LargeBlobInputs {
        .support = WTF::move(jsonInputs.support),
        .read = WTF::move(jsonInputs.read),
        .write = write.releaseReturnValue(),
    };
}

static ExceptionOr<AuthenticationExtensionsClientInputs::PRFValues> fromJSON(AuthenticationExtensionsClientInputsJSON::PRFValuesJSON&& jsonInputs)
{
    auto first = fromJSONBase64(WTF::move(jsonInputs.first), [&] { return makeString("Invalid encoding of prf.first: "_s, jsonInputs.first, " (It should be Base64URL encoded.)"_s); });
    if (first.hasException())
        return first.releaseException();

    std::optional<BufferSource> second;
    if (!jsonInputs.second.isNull()) {
        auto decodedSecond = fromJSONBase64(WTF::move(jsonInputs.second), [&] { return makeString("Invalid encoding of prf.second: "_s, jsonInputs.second, " (It should be Base64URL encoded.)"_s); });
        if (decodedSecond.hasException())
            return decodedSecond.releaseException();
        second = decodedSecond.releaseReturnValue();
    }

    return AuthenticationExtensionsClientInputs::PRFValues {
        .first = first.releaseReturnValue(),
        .second = WTF::move(second),
    };
};

static ExceptionOr<AuthenticationExtensionsClientInputs::PRFInputs> fromJSON(AuthenticationExtensionsClientInputsJSON::PRFInputsJSON&& jsonInputs)
{
    std::optional<AuthenticationExtensionsClientInputs::PRFValues> eval;
    if (jsonInputs.eval) {
        auto decodedEval = fromJSON(WTF::move(*jsonInputs.eval));
        if (decodedEval.hasException())
            return decodedEval.releaseException();
        eval = decodedEval.releaseReturnValue();
    }

    std::optional<Vector<KeyValuePair<String, AuthenticationExtensionsClientInputs::PRFValues>>> evalByCredential;
    if (jsonInputs.evalByCredential) {
        evalByCredential = Vector<KeyValuePair<String, AuthenticationExtensionsClientInputs::PRFValues>> { };
        for (auto credentialEvalJSON : *jsonInputs.evalByCredential) {
            auto value = fromJSON(WTF::move(credentialEvalJSON.value));
            if (value.hasException())
                return value.releaseException();
            evalByCredential->append({ credentialEvalJSON.key, value.releaseReturnValue() });
        }
    }

    return AuthenticationExtensionsClientInputs::PRFInputs {
        .eval = WTF::move(eval),
        .evalByCredential = WTF::move(evalByCredential),
    };
}

static ExceptionOr<AuthenticationExtensionsClientInputs> fromJSON(AuthenticationExtensionsClientInputsJSON&& jsonInputs)
{
    std::optional<bool> credProps;
    if (jsonInputs.credProps)
        credProps = *jsonInputs.credProps;

    std::optional<AuthenticationExtensionsClientInputs::LargeBlobInputs> largeBlob;
    if (jsonInputs.largeBlob) {
        auto decodedLargeBlob = fromJSON(WTF::move(*jsonInputs.largeBlob));
        if (decodedLargeBlob.hasException())
            return decodedLargeBlob.releaseException();
        largeBlob = decodedLargeBlob.releaseReturnValue();
    }

    std::optional<AuthenticationExtensionsClientInputs::PRFInputs> prf;
    if (jsonInputs.prf) {
        auto decodedPrf = fromJSON(WTF::move(*jsonInputs.prf));
        if (decodedPrf.hasException())
            return decodedPrf.releaseException();
        prf = decodedPrf.releaseReturnValue();
    }

    return AuthenticationExtensionsClientInputs {
        .appid = WTF::move(jsonInputs.appid),
        .credProps = WTF::move(credProps),
        .largeBlob = WTF::move(largeBlob),
        .prf = WTF::move(prf),
    };
}

static ExceptionOr<PublicKeyCredentialUserEntity> fromJSON(PublicKeyCredentialUserEntityJSON&& jsonUserEntity)
{
    auto id = fromJSONBase64(WTF::move(jsonUserEntity.id), [&] { return makeString("Invalid encoding of user.id: "_s, jsonUserEntity.id, " (It should be Base64URL encoded.)"_s); });
    if (id.hasException())
        return id.releaseException();

    return PublicKeyCredentialUserEntity {
        PublicKeyCredentialEntity { WTF::move(jsonUserEntity.name), { } },
        id.releaseReturnValue(),
        WTF::move(jsonUserEntity.displayName),
    };
}

ExceptionOr<PublicKeyCredentialCreationOptions> PublicKeyCredential::parseCreationOptionsFromJSON(PublicKeyCredentialCreationOptionsJSON&& jsonOptions)
{
    auto userEntity = fromJSON(WTF::move(jsonOptions.user));
    if (userEntity.hasException())
        return userEntity.releaseException();

    auto challenge = fromJSONBase64(WTF::move(jsonOptions.challenge), [&] { return makeString("Invalid encoding of challenge: "_s, jsonOptions.challenge, " (It should be Base64URL encoded.)"_s); });
    if (challenge.hasException())
        return challenge.releaseException();

    auto excludeCredentials = fromJSON(WTF::move(jsonOptions.excludeCredentials));
    if (excludeCredentials.hasException())
        return excludeCredentials.releaseException();

    std::optional<AuthenticationExtensionsClientInputs> extensions;
    if (jsonOptions.extensions) {
        auto decodedExtensions = fromJSON(WTF::move(*jsonOptions.extensions));
        if (decodedExtensions.hasException())
            return decodedExtensions.releaseException();
        extensions = decodedExtensions.releaseReturnValue();
    }

    return PublicKeyCredentialCreationOptions {
        .rp = WTF::move(jsonOptions.rp),
        .user = userEntity.releaseReturnValue(),
        .challenge = challenge.releaseReturnValue(),
        .pubKeyCredParams = WTF::move(jsonOptions.pubKeyCredParams),
        .timeout = WTF::move(jsonOptions.timeout),
        .excludeCredentials = excludeCredentials.releaseReturnValue(),
        .authenticatorSelection = WTF::move(jsonOptions.authenticatorSelection),
        .attestation = parseEnumerationFromString<AttestationConveyancePreference>(jsonOptions.attestation).value_or(AttestationConveyancePreference::None),
        .extensions = WTF::move(extensions),
    };
}

ExceptionOr<PublicKeyCredentialRequestOptions> PublicKeyCredential::parseRequestOptionsFromJSON(PublicKeyCredentialRequestOptionsJSON&& jsonOptions)
{
    auto challenge = fromJSONBase64(WTF::move(jsonOptions.challenge), [&] { return makeString("Invalid encoding of challenge: "_s, jsonOptions.challenge, " (It should be Base64URL encoded.)"_s); });
    if (challenge.hasException())
        return challenge.releaseException();

    auto allowCredentials = fromJSON(WTF::move(jsonOptions.allowCredentials));
    if (allowCredentials.hasException())
        return allowCredentials.releaseException();

    std::optional<AuthenticationExtensionsClientInputs> extensions;
    if (jsonOptions.extensions) {
        auto decodedExtensions = fromJSON(WTF::move(*jsonOptions.extensions));
        if (decodedExtensions.hasException())
            return decodedExtensions.releaseException();
        extensions = decodedExtensions.releaseReturnValue();
    }

    return PublicKeyCredentialRequestOptions {
        .challenge = challenge.releaseReturnValue(),
        .timeout = WTF::move(jsonOptions.timeout),
        .rpId = WTF::move(jsonOptions.rpId),
        .allowCredentials = allowCredentials.releaseReturnValue(),
        .userVerification = parseEnumerationFromString<UserVerificationRequirement>(jsonOptions.userVerification).value_or(UserVerificationRequirement::Preferred),
        .extensions = WTF::move(extensions),
        .authenticatorAttachment = { },
    };
}

void PublicKeyCredential::signalUnknownCredential(Document& document, UnknownCredentialOptions&& options, DOMPromiseDeferred<void>&& promise)
{
    if (RefPtr page = document.page())
        page->authenticatorCoordinator().signalUnknownCredential(document, WTF::move(options), WTF::move(promise));
}

void PublicKeyCredential::signalAllAcceptedCredentials(Document& document, AllAcceptedCredentialsOptions&& options, DOMPromiseDeferred<void>&& promise)
{
    if (RefPtr page = document.page())
        page->authenticatorCoordinator().signalAllAcceptedCredentials(document, WTF::move(options), WTF::move(promise));
}

void PublicKeyCredential::signalCurrentUserDetails(Document& document, CurrentUserDetailsOptions&& options, DOMPromiseDeferred<void>&& promise)
{
    if (RefPtr page = document.page())
        page->authenticatorCoordinator().signalCurrentUserDetails(document, WTF::move(options), WTF::move(promise));
}

} // namespace WebCore

#endif // ENABLE(WEB_AUTHN)
