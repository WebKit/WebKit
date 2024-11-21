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

#include "AuthenticatorAssertionResponse.h"
#include "AuthenticatorAttestationResponse.h"
#include "AuthenticatorCoordinator.h"
#include "AuthenticatorResponse.h"
#include "BufferSource.h"
#include "Document.h"
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
#include "WebAuthenticationUtils.h"
#include <wtf/text/Base64.h>

namespace WebCore {

Ref<PublicKeyCredential> PublicKeyCredential::create(Ref<AuthenticatorResponse>&& response)
{
    return adoptRef(*new PublicKeyCredential(WTFMove(response)));
}

ArrayBuffer* PublicKeyCredential::rawId() const
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
    : BasicCredential(base64URLEncodeToString(response->rawId()->span()), Type::PublicKey, Discovery::Remote)
    , m_response(WTFMove(response))
{
}

void PublicKeyCredential::isUserVerifyingPlatformAuthenticatorAvailable(Document& document, DOMPromiseDeferred<IDLBoolean>&& promise)
{
    if (auto* page = document.page())
        page->authenticatorCoordinator().isUserVerifyingPlatformAuthenticatorAvailable(document, WTFMove(promise));
}

void PublicKeyCredential::getClientCapabilities(Document& document, DOMPromiseDeferred<IDLRecord<IDLDOMString, IDLBoolean>>&& promise)
{
    if (auto* page = document.page())
        page->authenticatorCoordinator().getClientCapabilities(document, WTFMove(promise));
}

PublicKeyCredentialJSON PublicKeyCredential::toJSON()
{
    if (is<AuthenticatorAttestationResponse>(m_response)) {
        RegistrationResponseJSON response;
        if (auto id = rawId()) {
            response.id = base64EncodeToString(id->span());
            response.rawId = base64EncodeToString(id->span());
        }

        response.response = downcast<AuthenticatorAttestationResponse>(m_response)->toJSON();
        response.authenticatorAttachment = convertEnumerationToString(authenticatorAttachment());
        response.clientExtensionResults = getClientExtensionResults().toJSON();
        response.type = type();

        return response;
    }
    if (is<AuthenticatorAssertionResponse>(m_response)) {
        AuthenticationResponseJSON response;
        if (auto id = rawId()) {
            response.id = base64EncodeToString(id->span());
            response.rawId = base64EncodeToString(id->span());
        }
        response.response = downcast<AuthenticatorAssertionResponse>(m_response)->toJSON();
        response.authenticatorAttachment = convertEnumerationToString(authenticatorAttachment());
        response.clientExtensionResults = getClientExtensionResults().toJSON();
        response.type = type();
        return response;
    }
    AuthenticationResponseJSON response;
    ASSERT_NOT_REACHED();
    return response;
}

static ExceptionOr<PublicKeyCredentialDescriptor> fromJSON(PublicKeyCredentialDescriptorJSON jsonOptions)
{
    PublicKeyCredentialDescriptor descriptor;
    if (auto type = parseEnumerationFromString<PublicKeyCredentialType>(jsonOptions.type))
        descriptor.type = *type;
    else
        return Exception { ExceptionCode::EncodingError, makeString("Unrecognized credential type: "_s, jsonOptions.type) };
    if (auto id = base64URLDecode(jsonOptions.id))
        descriptor.id = BufferSource { *id };
    else
        return Exception { ExceptionCode::EncodingError, makeString("Invalid encoding of credential ID: "_s, jsonOptions.id, " (It should be Base64URL encoded.)"_s) };
    for (auto transportString : jsonOptions.transports) {
        if (auto transport = convertStringToAuthenticatorTransport(transportString))
            descriptor.transports.append(*transport);
    }
    return descriptor;
}

static ExceptionOr<Vector<PublicKeyCredentialDescriptor>> fromJSON(Vector<PublicKeyCredentialDescriptorJSON> jsonDescriptors)
{
    Vector<PublicKeyCredentialDescriptor> descriptors;
    for (auto jsonDescriptor : jsonDescriptors) {
        auto descriptor = fromJSON(jsonDescriptor);
        if (descriptor.hasException())
            return descriptor.releaseException();
        descriptors.append(descriptor.releaseReturnValue());
    }
    return descriptors;
}

static ExceptionOr<AuthenticationExtensionsClientInputs::LargeBlobInputs> fromJSON(AuthenticationExtensionsClientInputsJSON::LargeBlobInputsJSON jsonInputs)
{
    AuthenticationExtensionsClientInputs::LargeBlobInputs largeBlob;
    largeBlob.support = jsonInputs.support;
    largeBlob.read = jsonInputs.read;
    if (auto decodedWrite = base64URLDecode(jsonInputs.write))
        largeBlob.write = BufferSource { *decodedWrite };
    else
        return Exception { ExceptionCode::EncodingError, makeString("Invalid encoding of largeBlob.write: "_s, jsonInputs.write, " (It should be Base64URL encoded.)"_s) };
    return largeBlob;
}

static ExceptionOr<AuthenticationExtensionsClientInputs::PRFValues> fromJSON(AuthenticationExtensionsClientInputsJSON::PRFValuesJSON jsonInputs)
{
    AuthenticationExtensionsClientInputs::PRFValues values;
    if (auto decodedFirst = base64URLDecode(jsonInputs.first))
        values.first = BufferSource { *decodedFirst };
    else
        return Exception { ExceptionCode::EncodingError, makeString("Invalid encoding of prf.first: "_s, jsonInputs.first, " (It should be Base64URL encoded.)"_s) };
    if (!jsonInputs.second.isNull()) {
        if (auto decodedSecond = base64URLDecode(jsonInputs.second))
            values.second = BufferSource { *decodedSecond };

        else
            return Exception { ExceptionCode::EncodingError, makeString("Invalid encoding of prf.second: "_s, jsonInputs.second, " (It should be Base64URL encoded.)"_s) };
    }
    return values;
}

static ExceptionOr<AuthenticationExtensionsClientInputs::PRFInputs> fromJSON(AuthenticationExtensionsClientInputsJSON::PRFInputsJSON jsonInputs)
{
    AuthenticationExtensionsClientInputs::PRFInputs inputs;
    if (jsonInputs.eval) {
        auto eval = fromJSON(*jsonInputs.eval);
        if (eval.hasException())
            return eval.releaseException();
        inputs.eval = eval.releaseReturnValue();
    }
    if (jsonInputs.evalByCredential) {
        Vector<KeyValuePair<String, AuthenticationExtensionsClientInputs::PRFValues>> evalByCredential;
        for (auto credentialEvalJSON : *jsonInputs.evalByCredential) {
            auto value = fromJSON(credentialEvalJSON.value);
            if (value.hasException())
                return value.releaseException();
            evalByCredential.append({ credentialEvalJSON.key, value.releaseReturnValue() });
        }
        inputs.evalByCredential = evalByCredential;
    }
    return inputs;
}

static ExceptionOr<AuthenticationExtensionsClientInputs> fromJSON(AuthenticationExtensionsClientInputsJSON&& jsonInputs)
{
    AuthenticationExtensionsClientInputs inputs;
    inputs.appid = jsonInputs.appid;
    if (jsonInputs.credProps)
        inputs.credProps = *jsonInputs.credProps;
    if (jsonInputs.largeBlob) {
        auto largeBlob = fromJSON(*jsonInputs.largeBlob);
        if (largeBlob.hasException())
            return largeBlob.releaseException();
        inputs.largeBlob = largeBlob.releaseReturnValue();
    }
    if (jsonInputs.prf) {
        auto prf = fromJSON(*jsonInputs.prf);
        if (prf.hasException())
            return prf.releaseException();
        inputs.prf = prf.releaseReturnValue();
    }
    return inputs;
}

static ExceptionOr<PublicKeyCredentialUserEntity> fromJSON(PublicKeyCredentialUserEntityJSON&& jsonUserEntity)
{
    PublicKeyCredentialUserEntity userEntity;
    if (auto decodedId = base64URLDecode(jsonUserEntity.id))
        userEntity.id = BufferSource { *decodedId };
    else
        return Exception { ExceptionCode::EncodingError, makeString("Invalid encoding of user.id: "_s, jsonUserEntity.id, " (It should be Base64URL encoded.)"_s) };
    userEntity.name = jsonUserEntity.name;
    userEntity.displayName = jsonUserEntity.displayName;
    return userEntity;
}

ExceptionOr<PublicKeyCredentialCreationOptions> PublicKeyCredential::parseCreationOptionsFromJSON(PublicKeyCredentialCreationOptionsJSON&& jsonOptions)
{
    PublicKeyCredentialCreationOptions options;

    options.rp = jsonOptions.rp;
    auto userEntity = fromJSON(WTFMove(jsonOptions.user));
    if (userEntity.hasException())
        return userEntity.releaseException();
    options.user = userEntity.releaseReturnValue();

    if (auto decodedChallenge = base64URLDecode(jsonOptions.challenge))
        options.challenge = BufferSource { *decodedChallenge };
    else
        return Exception { ExceptionCode::EncodingError, makeString("Invalid encoding of challenge: "_s, jsonOptions.challenge, " (It should be Base64URL encoded.)"_s) };
    options.pubKeyCredParams = jsonOptions.pubKeyCredParams;
    options.timeout = jsonOptions.timeout;
    auto excludeCredentials = fromJSON(jsonOptions.excludeCredentials);
    if (excludeCredentials.hasException())
        return excludeCredentials.releaseException();
    options.excludeCredentials = excludeCredentials.releaseReturnValue();

    options.authenticatorSelection = jsonOptions.authenticatorSelection;
    if (auto attestation = parseEnumerationFromString<AttestationConveyancePreference>(jsonOptions.attestation))
        options.attestation = *attestation;
    if (jsonOptions.extensions) {
        auto extensions = fromJSON(WTFMove(*jsonOptions.extensions));
        if (extensions.hasException())
            return extensions.releaseException();
        options.extensions = extensions.releaseReturnValue();
    }
    return options;
}

ExceptionOr<PublicKeyCredentialRequestOptions> PublicKeyCredential::parseRequestOptionsFromJSON(PublicKeyCredentialRequestOptionsJSON&& jsonOptions)
{
    PublicKeyCredentialRequestOptions options;

    if (auto decodedChallenge = base64URLDecode(jsonOptions.challenge))
        options.challenge = BufferSource { *decodedChallenge };
    else
        return Exception { ExceptionCode::EncodingError, makeString("Invalid encoding of challenge: "_s, jsonOptions.challenge, " (It should be Base64URL encoded.)"_s) };
    options.timeout = jsonOptions.timeout;
    options.rpId = jsonOptions.rpId;
    auto allowCredentials = fromJSON(jsonOptions.allowCredentials);
    if (allowCredentials.hasException())
        return allowCredentials.releaseException();
    options.allowCredentials = allowCredentials.releaseReturnValue();
    if (auto userVerification = parseEnumerationFromString<UserVerificationRequirement>(jsonOptions.userVerification))
        options.userVerification = *userVerification;
    if (jsonOptions.extensions) {
        auto extensions = fromJSON(WTFMove(*jsonOptions.extensions));
        if (extensions.hasException())
            return extensions.releaseException();
        options.extensions = extensions.releaseReturnValue();
    }
    return options;
}

} // namespace WebCore

#endif // ENABLE(WEB_AUTHN)
