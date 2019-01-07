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
#include "AuthenticatorCoordinator.h"

#if ENABLE(WEB_AUTHN)

#include "AbortSignal.h"
#include "AuthenticatorAssertionResponse.h"
#include "AuthenticatorAttestationResponse.h"
#include "AuthenticatorCoordinatorClient.h"
#include "JSBasicCredential.h"
#include "PublicKeyCredential.h"
#include "PublicKeyCredentialCreationOptions.h"
#include "PublicKeyCredentialData.h"
#include "PublicKeyCredentialRequestOptions.h"
#include "SecurityOrigin.h"
#include <pal/crypto/CryptoDigest.h>
#include <wtf/JSONValues.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/text/Base64.h>

namespace WebCore {

namespace AuthenticatorCoordinatorInternal {

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
        object->setString("type"_s, "webauthn.create"_s);
        break;
    case ClientDataType::Get:
        object->setString("type"_s, "webauthn.get"_s);
        break;
    }
    object->setString("challenge"_s, WTF::base64URLEncode(challenge.data(), challenge.length()));
    object->setString("origin"_s, origin.toRawString());

    auto utf8JSONString = object->toJSONString().utf8();
    return ArrayBuffer::create(utf8JSONString.data(), utf8JSONString.length());
}

static Vector<uint8_t> produceClientDataJsonHash(const ArrayBuffer& clientDataJson)
{
    auto crypto = PAL::CryptoDigest::create(PAL::CryptoDigest::Algorithm::SHA_256);
    crypto->addBytes(clientDataJson.data(), clientDataJson.byteLength());
    return crypto->computeHash();
}

} // namespace AuthenticatorCoordinatorInternal

AuthenticatorCoordinator::AuthenticatorCoordinator(std::unique_ptr<AuthenticatorCoordinatorClient>&& client)
    : m_client(WTFMove(client))
{
}

void AuthenticatorCoordinator::setClient(std::unique_ptr<AuthenticatorCoordinatorClient>&& client)
{
    m_client = WTFMove(client);
}

void AuthenticatorCoordinator::create(const SecurityOrigin& callerOrigin, const PublicKeyCredentialCreationOptions& options, bool sameOriginWithAncestors, RefPtr<AbortSignal>&& abortSignal, CredentialPromise&& promise) const
{
    using namespace AuthenticatorCoordinatorInternal;

    // The following implements https://www.w3.org/TR/webauthn/#createCredential as of 5 December 2017.
    // FIXME: Extensions are not supported yet. Skip Step 11-12.
    // Step 1, 3, 16 are handled by the caller.
    // Step 2.
    if (!sameOriginWithAncestors) {
        promise.reject(Exception { NotAllowedError, "The origin of the document is not the same as its ancestors."_s });
        return;
    }

    // Step 5-7.
    // FIXME(181950): We lack fundamental support from SecurityOrigin to determine if a host is a valid domain or not.
    // Step 6 is therefore skipped. Also, we lack the support to determine whether a domain is a registrable
    // domain suffix of another domain. Hence restrict the comparison to equal in Step 7.
    if (!options.rp.id.isEmpty() && callerOrigin.host() != options.rp.id) {
        promise.reject(Exception { SecurityError, "The origin of the document is not a registrable domain suffix of the provided RP ID."_s });
        return;
    }
    if (options.rp.id.isEmpty())
        options.rp.id = callerOrigin.host();

    // Step 8-10.
    // Most of the jobs are done by bindings. However, we can't know if the JSValue of options.pubKeyCredParams
    // is empty or not. Return NotSupportedError as long as it is empty.
    if (options.pubKeyCredParams.isEmpty()) {
        promise.reject(Exception { NotSupportedError, "No desired properties of the to be created credential are provided."_s });
        return;
    }

    // Step 13-15.
    auto clientDataJson = produceClientDataJson(ClientDataType::Create, options.challenge, callerOrigin);
    auto clientDataJsonHash = produceClientDataJsonHash(clientDataJson);

    // Step 4, 17-21.
    // Only platform attachments will be supported at this stage. Assuming one authenticator per device.
    // Also, resident keys, user verifications and direct attestation are enforced at this tage.
    // For better performance, transports of options.excludeCredentials are checked in LocalAuthenticator.
    if (!m_client)  {
        promise.reject(Exception { UnknownError, "Unknown internal error."_s });
        return;
    }

    auto completionHandler = [clientDataJson = WTFMove(clientDataJson), promise = WTFMove(promise), abortSignal = WTFMove(abortSignal)] (const WebCore::PublicKeyCredentialData& data, const WebCore::ExceptionData& exception) mutable {
        if (abortSignal && abortSignal->aborted()) {
            promise.reject(Exception { AbortError, "Aborted by AbortSignal."_s });
            return;
        }

        data.clientDataJSON = WTFMove(clientDataJson);
        if (auto publicKeyCredential = PublicKeyCredential::tryCreate(data)) {
            promise.resolve(publicKeyCredential.get());
            return;
        }
        ASSERT(!exception.message.isNull());
        promise.reject(exception.toException());
    };
    // Async operations are dispatched and handled in the messenger.
    m_client->makeCredential(clientDataJsonHash, options, WTFMove(completionHandler));
}

void AuthenticatorCoordinator::discoverFromExternalSource(const SecurityOrigin& callerOrigin, const PublicKeyCredentialRequestOptions& options, bool sameOriginWithAncestors, RefPtr<AbortSignal>&& abortSignal, CredentialPromise&& promise) const
{
    using namespace AuthenticatorCoordinatorInternal;

    // The following implements https://www.w3.org/TR/webauthn/#createCredential as of 5 December 2017.
    // FIXME: Extensions are not supported yet. Skip Step 8-9.
    // Step 1, 3, 13 are handled by the caller.
    // Step 2.
    if (!sameOriginWithAncestors) {
        promise.reject(Exception { NotAllowedError, "The origin of the document is not the same as its ancestors."_s });
        return;
    }

    // Step 5-7.
    // FIXME(181950): We lack fundamental support from SecurityOrigin to determine if a host is a valid domain or not.
    // Step 6 is therefore skipped. Also, we lack the support to determine whether a domain is a registrable
    // domain suffix of another domain. Hence restrict the comparison to equal in Step 7.
    if (!options.rpId.isEmpty() && callerOrigin.host() != options.rpId) {
        promise.reject(Exception { SecurityError, "The origin of the document is not a registrable domain suffix of the provided RP ID."_s });
        return;
    }
    if (options.rpId.isEmpty())
        options.rpId = callerOrigin.host();

    // Step 10-12.
    auto clientDataJson = produceClientDataJson(ClientDataType::Get, options.challenge, callerOrigin);
    auto clientDataJsonHash = produceClientDataJsonHash(clientDataJson);

    // Step 4, 14-19.
    // Only platform attachments will be supported at this stage. Assuming one authenticator per device.
    // Also, resident keys, user verifications and direct attestation are enforced at this tage.
    // For better performance, filtering of options.allowCredentials is done in LocalAuthenticator.
    if (!m_client)  {
        promise.reject(Exception { UnknownError, "Unknown internal error."_s });
        return;
    }

    auto completionHandler = [clientDataJson = WTFMove(clientDataJson), promise = WTFMove(promise), abortSignal = WTFMove(abortSignal)] (const WebCore::PublicKeyCredentialData& data, const WebCore::ExceptionData& exception) mutable {
        if (abortSignal && abortSignal->aborted()) {
            promise.reject(Exception { AbortError, "Aborted by AbortSignal."_s });
            return;
        }

        data.clientDataJSON = WTFMove(clientDataJson);
        if (auto publicKeyCredential = PublicKeyCredential::tryCreate(data)) {
            promise.resolve(publicKeyCredential.get());
            return;
        }
        ASSERT(!exception.message.isNull());
        promise.reject(exception.toException());
    };
    // Async operations are dispatched and handled in the messenger.
    m_client->getAssertion(clientDataJsonHash, options, WTFMove(completionHandler));
}

void AuthenticatorCoordinator::isUserVerifyingPlatformAuthenticatorAvailable(DOMPromiseDeferred<IDLBoolean>&& promise) const
{
    // The following implements https://www.w3.org/TR/webauthn/#isUserVerifyingPlatformAuthenticatorAvailable
    // as of 5 December 2017.
    if (!m_client)  {
        promise.reject(Exception { UnknownError, "Unknown internal error."_s });
        return;
    }

    // FIXME(182767): We should consider more on the assessment of the return value. Right now, we return true/false
    // immediately according to platform specific procedures.
    auto completionHandler = [promise = WTFMove(promise)] (bool result) mutable {
        promise.resolve(result);
    };
    // Async operation are dispatched and handled in the messenger.
    m_client->isUserVerifyingPlatformAuthenticatorAvailable(WTFMove(completionHandler));
}

} // namespace WebCore

#endif // ENABLE(WEB_AUTHN)
