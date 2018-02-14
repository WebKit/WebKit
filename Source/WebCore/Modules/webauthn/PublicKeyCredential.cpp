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

#include "Authenticator.h"
#include "AuthenticatorResponse.h"
#include "JSDOMPromiseDeferred.h"
#include "PublicKeyCredentialCreationOptions.h"
#include "PublicKeyCredentialRequestOptions.h"
#include "SecurityOrigin.h"
#include <pal/crypto/CryptoDigest.h>
#include <wtf/CurrentTime.h>
#include <wtf/JSONValues.h>
#include <wtf/text/Base64.h>

namespace WebCore {

namespace PublicKeyCredentialInternal {

// The layout of attestation object: https://www.w3.org/TR/webauthn/#attestation-object as of 5 December 2017.
// Here is a summary before CredentialID in the layout. All lengths are fixed.
// RP ID hash (32) || FLAGS (1) || COUNTER (4) || AAGUID (16) || L (2) || CREDENTIAL ID (?) || ...
static constexpr size_t CredentialIdLengthOffset = 43;

enum class ClientDataType {
    Create,
    Get
};

// FIXME(181948): Add token binding ID and extensions.
static Ref<ArrayBuffer> produceClientDataJson(ClientDataType type, const BufferSource& challenge, const SecurityOrigin& origin)
{
    auto object = JSON::Object::create();
    switch (type) {
    case ClientDataType::Create:
        object->setString(ASCIILiteral("type"), ASCIILiteral("webauthn.create"));
        break;
    case ClientDataType::Get:
        object->setString(ASCIILiteral("type"), ASCIILiteral("webauthn.get"));
        break;
    }
    object->setString(ASCIILiteral("challenge"), WTF::base64URLEncode(challenge.data(), challenge.length()));
    object->setString(ASCIILiteral("origin"), origin.toRawString());
    // FIXME: This might be platform dependent.
    object->setString(ASCIILiteral("hashAlgorithm"), ASCIILiteral("SHA-256"));

    auto utf8JSONString = object->toJSONString().utf8();
    return ArrayBuffer::create(utf8JSONString.data(), utf8JSONString.length());
}

static Vector<uint8_t> produceClientDataJsonHash(const Ref<ArrayBuffer>& clientDataJson)
{
    auto crypto = PAL::CryptoDigest::create(PAL::CryptoDigest::Algorithm::SHA_256);
    crypto->addBytes(clientDataJson->data(), clientDataJson->byteLength());
    return crypto->computeHash();
}

static RefPtr<ArrayBuffer> getIdFromAttestationObject(const Vector<uint8_t>& attestationObject)
{
    // The byte length of L is 2.
    if (attestationObject.size() < CredentialIdLengthOffset + 2)
        return nullptr;
    size_t length = (attestationObject[CredentialIdLengthOffset] << 8) + attestationObject[CredentialIdLengthOffset + 1];
    if (attestationObject.size() < CredentialIdLengthOffset + 2 + length)
        return nullptr;
    return ArrayBuffer::create(attestationObject.data() + CredentialIdLengthOffset + 2, length);
}

} // namespace PublicKeyCredentialInternal

PublicKeyCredential::PublicKeyCredential(RefPtr<ArrayBuffer>&& id, RefPtr<AuthenticatorResponse>&& response)
    : BasicCredential(WTF::base64URLEncode(id->data(), id->byteLength()), Type::PublicKey, Discovery::Remote)
    , m_rawId(WTFMove(id))
    , m_response(WTFMove(response))
{
}

Vector<Ref<BasicCredential>> PublicKeyCredential::collectFromCredentialStore(PublicKeyCredentialRequestOptions&&, bool)
{
    return { };
}

ExceptionOr<RefPtr<BasicCredential>> PublicKeyCredential::discoverFromExternalSource(const SecurityOrigin& callerOrigin, const PublicKeyCredentialRequestOptions& options, bool sameOriginWithAncestors)
{
    using namespace PublicKeyCredentialInternal;

    // The following implements https://www.w3.org/TR/webauthn/#createCredential as of 5 December 2017.
    // FIXME: Extensions are not supported yet. Skip Step 8-9.
    // Step 1, 3-4, 13, 16 are handled by the caller, including options sanitizing, timer and abort signal.
    // Step 2.
    if (!sameOriginWithAncestors)
        return Exception { NotAllowedError };

    // Step 5-7.
    // FIXME(181950): We lack fundamental support from SecurityOrigin to determine if a host is a valid domain or not.
    // Step 6 is therefore skipped. Also, we lack the support to determine whether a domain is a registrable
    // domain suffix of another domain. Hence restrict the comparison to equal in Step 7.
    if (!options.rpId.isEmpty() && !(callerOrigin.host() == options.rpId))
        return Exception { SecurityError };
    if (options.rpId.isEmpty())
        options.rpId = callerOrigin.host();

    // Step 10-12.
    auto clientDataJson = produceClientDataJson(ClientDataType::Get, options.challenge, callerOrigin);
    auto clientDataJsonHash = produceClientDataJsonHash(clientDataJson);

    // Step 14-15, 17-19.
    // Only platform attachments will be supported at this stage. Assuming one authenticator per device.
    // Also, resident keys, user verifications and direct attestation are enforced at this tage.
    // For better performance, no filtering is done here regarding to options.excludeCredentials.
    // What's more, user cancellations effectively means NotAllowedError. Therefore, the below call
    // will only returns either an exception or a PublicKeyCredential ref.
    // FIXME(181946): The following operation might need to perform async.
    auto result = Authenticator::singleton().getAssertion(options.rpId, clientDataJsonHash, options.allowCredentials);
    if (result.hasException())
        return result.releaseException();

    auto bundle = result.releaseReturnValue();
    return ExceptionOr<RefPtr<BasicCredential>>(PublicKeyCredential::create(WTFMove(bundle.credentialID), AuthenticatorAssertionResponse::create(WTFMove(clientDataJson), WTFMove(bundle.authenticatorData), WTFMove(bundle.signature), WTFMove(bundle.userHandle))));
}

RefPtr<BasicCredential> PublicKeyCredential::store(RefPtr<BasicCredential>&&, bool)
{
    return nullptr;
}

ExceptionOr<RefPtr<BasicCredential>> PublicKeyCredential::create(const SecurityOrigin& callerOrigin, const PublicKeyCredentialCreationOptions& options, bool sameOriginWithAncestors)
{
    using namespace PublicKeyCredentialInternal;

    // The following implements https://www.w3.org/TR/webauthn/#createCredential as of 5 December 2017.
    // FIXME: Extensions are not supported yet. Skip Step 11-12.
    // Step 1, 3-4, 16-17 are handled by the caller, including options sanitizing, timer and abort signal.
    // Step 2.
    if (!sameOriginWithAncestors)
        return Exception { NotAllowedError };

    // Step 5-7.
    // FIXME(181950): We lack fundamental support from SecurityOrigin to determine if a host is a valid domain or not.
    // Step 6 is therefore skipped. Also, we lack the support to determine whether a domain is a registrable
    // domain suffix of another domain. Hence restrict the comparison to equal in Step 7.
    if (!options.rp.id.isEmpty() && !(callerOrigin.host() == options.rp.id))
        return Exception { SecurityError };
    if (options.rp.id.isEmpty())
        options.rp.id = callerOrigin.host();

    // Step 8-10.
    // Most of the jobs are done by bindings. However, we can't know if the JSValue of options.pubKeyCredParams
    // is empty or not. Return NotSupportedError as long as it is empty.
    if (options.pubKeyCredParams.isEmpty())
        return Exception { NotSupportedError };

    // Step 13-15.
    auto clientDataJson = produceClientDataJson(ClientDataType::Create, options.challenge, callerOrigin);
    auto clientDataJsonHash = produceClientDataJsonHash(clientDataJson);

    // Step 18-21.
    // Only platform attachments will be supported at this stage. Assuming one authenticator per device.
    // Also, resident keys, user verifications and direct attestation are enforced at this tage.
    // For better performance, no filtering is done here regarding to options.excludeCredentials.
    // What's more, user cancellations effectively means NotAllowedError. Therefore, the below call
    // will only returns either an exception or a PublicKeyCredential ref.
    // FIXME(181946): The following operation might need to perform async.
    auto result = Authenticator::singleton().makeCredential(clientDataJsonHash, options.rp, options.user, options.pubKeyCredParams, options.excludeCredentials);
    if (result.hasException())
        return result.releaseException();

    auto attestationObject = result.releaseReturnValue();
    return ExceptionOr<RefPtr<BasicCredential>>(PublicKeyCredential::create(getIdFromAttestationObject(attestationObject), AuthenticatorAttestationResponse::create(WTFMove(clientDataJson), ArrayBuffer::create(attestationObject.data(), attestationObject.size()))));
}

ArrayBuffer* PublicKeyCredential::rawId() const
{
    return m_rawId.get();
}

AuthenticatorResponse* PublicKeyCredential::response() const
{
    return m_response.get();
}

ExceptionOr<bool> PublicKeyCredential::getClientExtensionResults() const
{
    return Exception { NotSupportedError };
}

void PublicKeyCredential::isUserVerifyingPlatformAuthenticatorAvailable(Ref<DeferredPromise>&& promise)
{
    promise->reject(Exception { NotSupportedError });
}

} // namespace WebCore

#endif // ENABLE(WEB_AUTHN)
