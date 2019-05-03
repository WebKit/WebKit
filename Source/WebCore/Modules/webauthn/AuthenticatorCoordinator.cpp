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
#include "RegistrableDomain.h"
#include "SchemeRegistry.h"
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

static bool needsAppIdQuirks(const String& host, const String& appId)
{
    // FIXME(197524): Remove this quirk in 2023. As an early adopter of U2F features, Google has a large number of
    // existing device registrations that authenticate 'google.com' against 'gstatic.com'. Firefox and other browsers
    // have agreed to grant an exception to the AppId rules for a limited time period (5 years from January, 2018) to
    // allow existing Google users to seamlessly transition to proper WebAuthN behavior.
    if (equalLettersIgnoringASCIICase(host, "google.com") || host.endsWithIgnoringASCIICase(".google.com"))
        return (appId == "https://www.gstatic.com/securitykey/origins.json"_s) || (appId == "https://www.gstatic.com/securitykey/a/google.com/origins.json"_s);
    return false;
}

// The following roughly implements Step 1-3 of the spec to avoid the complexity of making unnecessary network requests:
// https://fidoalliance.org/specs/fido-v2.0-id-20180227/fido-appid-and-facets-v2.0-id-20180227.html#determining-if-a-caller-s-facetid-is-authorized-for-an-appid
// It follows what Chrome and Firefox do, see:
// https://bugzilla.mozilla.org/show_bug.cgi?id=1244959#c8
// https://bugs.chromium.org/p/chromium/issues/detail?id=818303
static String processAppIdExtension(const SecurityOrigin& facetId, const String& appId)
{
    // Step 1. Skipped since facetId should always be secure origins.
    ASSERT(SchemeRegistry::shouldTreatURLSchemeAsSecure(facetId.protocol()));

    // Step 2. Follow Chrome and Firefox to use the origin directly without adding a trailing slash.
    if (appId.isEmpty())
        return facetId.toString();

    // Step 3. Relax the comparison to same site.
    URL appIdURL(URL(), appId);
    if (!appIdURL.isValid() || facetId.protocol() != appIdURL.protocol() || (RegistrableDomain(appIdURL) != RegistrableDomain::uncheckedCreateFromHost(facetId.host()) && !needsAppIdQuirks(facetId.host(), appId)))
        return String();
    return appId;
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
    // Extensions are not supported. Skip Step 11-12.
    // Step 1, 3, 16 are handled by the caller.
    // Step 2.
    if (!sameOriginWithAncestors) {
        promise.reject(Exception { NotAllowedError, "The origin of the document is not the same as its ancestors."_s });
        return;
    }

    // Step 5. Skipped since SecurityOrigin doesn't have the concept of "opaque origin".
    // Step 6. The effective domain may be represented in various manners, such as a domain or an ip address.
    // Only the domain format of host is permitted in WebAuthN.
    if (URL::hostIsIPAddress(callerOrigin.domain())) {
        promise.reject(Exception { SecurityError, "The effective domain of the document is not a valid domain."_s });
        return;
    }

    // Step 7.
    if (!options.rp.id.isEmpty() && !callerOrigin.isMatchingRegistrableDomainSuffix(options.rp.id)) {
        promise.reject(Exception { SecurityError, "The provided RP ID is not a registrable domain suffix of the effective domain of the document."_s });
        return;
    }
    if (options.rp.id.isEmpty())
        options.rp.id = callerOrigin.domain();

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
    if (!m_client) {
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
    // Step 1, 3, 13 are handled by the caller.
    // Step 2.
    if (!sameOriginWithAncestors) {
        promise.reject(Exception { NotAllowedError, "The origin of the document is not the same as its ancestors."_s });
        return;
    }

    // Step 5. Skipped since SecurityOrigin doesn't have the concept of "opaque origin".
    // Step 6. The effective domain may be represented in various manners, such as a domain or an ip address.
    // Only the domain format of host is permitted in WebAuthN.
    if (URL::hostIsIPAddress(callerOrigin.domain())) {
        promise.reject(Exception { SecurityError, "The effective domain of the document is not a valid domain."_s });
        return;
    }

    // Step 7.
    if (!options.rpId.isEmpty() && !callerOrigin.isMatchingRegistrableDomainSuffix(options.rpId)) {
        promise.reject(Exception { SecurityError, "The provided RP ID is not a registrable domain suffix of the effective domain of the document."_s });
        return;
    }
    if (options.rpId.isEmpty())
        options.rpId = callerOrigin.domain();

    // Step 8-9.
    // Only FIDO AppID Extension is supported.
    if (options.extensions && !options.extensions->appid.isNull()) {
        // The following implements https://www.w3.org/TR/webauthn/#sctn-appid-extension as of 4 March 2019.
        auto appid = processAppIdExtension(callerOrigin, options.extensions->appid);
        if (!appid) {
            promise.reject(Exception { SecurityError, "The origin of the document is not authorized for the provided App ID."_s });
            return;
        }
        options.extensions->appid = appid;
    }

    // Step 10-12.
    auto clientDataJson = produceClientDataJson(ClientDataType::Get, options.challenge, callerOrigin);
    auto clientDataJsonHash = produceClientDataJsonHash(clientDataJson);

    // Step 4, 14-19.
    if (!m_client) {
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
