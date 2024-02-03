/*
 * Copyright (C) 2018-2021 Apple Inc. All rights reserved.
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
#include "AuthenticatorResponseData.h"
#include "Document.h"
#include "FeaturePolicy.h"
#include "FrameDestructionObserverInlines.h"
#include "JSBasicCredential.h"
#include "JSCredentialCreationOptions.h"
#include "JSCredentialRequestOptions.h"
#include "JSDOMPromiseDeferred.h"
#include "PublicKeyCredential.h"
#include "PublicKeyCredentialCreationOptions.h"
#include "PublicKeyCredentialRequestOptions.h"
#include "RegistrableDomain.h"
#include "LegacySchemeRegistry.h"
#include "WebAuthenticationConstants.h"
#include "WebAuthenticationUtils.h"
#include <pal/crypto/CryptoDigest.h>
#include <wtf/NeverDestroyed.h>

namespace WebCore {

namespace AuthenticatorCoordinatorInternal {

static bool needsAppIdQuirks(const String& host, const String& appId)
{
    // FIXME(197524): Remove this quirk in 2023. As an early adopter of U2F features, Google has a large number of
    // existing device registrations that authenticate 'google.com' against 'gstatic.com'. Firefox and other browsers
    // have agreed to grant an exception to the AppId rules for a limited time period (5 years from January, 2018) to
    // allow existing Google users to seamlessly transition to proper WebAuthN behavior.
    if (equalLettersIgnoringASCIICase(host, "google.com"_s) || host.endsWithIgnoringASCIICase(".google.com"_s))
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
    ASSERT(LegacySchemeRegistry::shouldTreatURLSchemeAsSecure(facetId.protocol()));

    // Step 2. Follow Chrome and Firefox to use the origin directly without adding a trailing slash.
    if (appId.isEmpty())
        return facetId.toString();

    // Step 3. Relax the comparison to same site.
    URL appIdURL { appId };
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

void AuthenticatorCoordinator::create(const Document& document, CredentialCreationOptions&& createOptions, WebAuthn::Scope scope, RefPtr<AbortSignal>&& abortSignal, CredentialPromise&& promise)
{
    using namespace AuthenticatorCoordinatorInternal;

    const auto& callerOrigin = document.securityOrigin();
    auto* frame = document.frame();
    const auto& options = createOptions.publicKey.value();
    ASSERT(frame);
    // The following implements https://www.w3.org/TR/webauthn-2/#createCredential as of 28 June 2022.
    // Step 1, 3, 16 are handled by the caller.
    // Step 2.
    if (scope != WebAuthn::Scope::SameOrigin) {
        promise.reject(Exception { ExceptionCode::NotAllowedError, "The origin of the document is not the same as its ancestors."_s });
        return;
    }

    // Step 5.
    if (options.user.id.length() < 1 || options.user.id.length() > 64) {
        promise.reject(Exception { ExceptionCode::TypeError, "The length options.user.id must be between 1-64 bytes."_s });
        return;
    }

    // Step 6. Skipped since SecurityOrigin doesn't have the concept of "opaque origin".
    // Step 7. The effective domain may be represented in various manners, such as a domain or an ip address.
    // Only the domain format of host is permitted in WebAuthN.
    if (URL::hostIsIPAddress(callerOrigin.domain())) {
        promise.reject(Exception { ExceptionCode::SecurityError, "The effective domain of the document is not a valid domain."_s });
        return;
    }

    // Step 8.
    if (!options.rp.id)
        options.rp.id = callerOrigin.domain();
    else if (!callerOrigin.isMatchingRegistrableDomainSuffix(*options.rp.id)) {
        promise.reject(Exception { ExceptionCode::SecurityError, "The provided RP ID is not a registrable domain suffix of the effective domain of the document."_s });
        return;
    }

    // Step 9-11.
    // Most of the jobs are done by bindings.
    if (options.pubKeyCredParams.isEmpty()) {
        options.pubKeyCredParams.append({ PublicKeyCredentialType::PublicKey, COSE::ES256 });
        options.pubKeyCredParams.append({ PublicKeyCredentialType::PublicKey, COSE::RS256 });
    } else {
        if (notFound != options.pubKeyCredParams.findIf([] (auto& pubKeyCredParam) {
            return pubKeyCredParam.type != PublicKeyCredentialType::PublicKey;
        })) {
            
            promise.reject(Exception { ExceptionCode::NotSupportedError, "options.pubKeyCredParams contains unsupported PublicKeyCredentialType value."_s });
            return;
        }
    }

    // Step 12-13.
    ASSERT(options.rp.id);

    AuthenticationExtensionsClientInputs extensionInputs = {
        String(),
        false,
        std::nullopt
    };

    if (auto extensions = options.extensions) {
        extensionInputs.credProps = extensions->credProps;
        extensionInputs.largeBlob = extensions->largeBlob;
    }

    options.extensions = extensionInputs;

    // Step 4, 18-22.
    if (!m_client) {
        promise.reject(Exception { ExceptionCode::UnknownError, "Unknown internal error."_s });
        return;
    }

    if (createOptions.signal) {
        createOptions.signal->addAlgorithm([weakThis = WeakPtr { *this }](JSC::JSValue) mutable {
            if (!weakThis)
                return;
            weakThis->m_isCancelling = true;
            weakThis->m_client->cancel([weakThis = WTFMove(weakThis)] () mutable {
                if (!weakThis)
                    return;
                if (auto queuedRequest = WTFMove(weakThis->m_queuedRequest)) {
                    weakThis->m_isCancelling = false;
                    queuedRequest();
                }
            });
        });
    }

    auto callback = [weakThis = WeakPtr { *this }, promise = WTFMove(promise), abortSignal = WTFMove(abortSignal)] (AuthenticatorResponseData&& data, AuthenticatorAttachment attachment, ExceptionData&& exception) mutable {
        if (abortSignal && abortSignal->aborted()) {
            promise.reject(Exception { ExceptionCode::AbortError, "Aborted by AbortSignal."_s });
            return;
        }

        if (auto response = AuthenticatorResponse::tryCreate(WTFMove(data), attachment)) {
            promise.resolve(PublicKeyCredential::create(response.releaseNonNull()).ptr());
            return;
        }
        ASSERT(!exception.message.isNull());
        promise.reject(exception.toException());
    };

    if (m_isCancelling) {
        m_queuedRequest = [weakThis = WeakPtr { *this }, weakFrame = WeakPtr { *frame }, createOptions = WTFMove(createOptions), callback = WTFMove(callback)]() mutable {
            if (!weakThis || !weakFrame)
                return;
            const auto options = createOptions.publicKey.value();
            RefPtr frame = weakFrame.get();
            if (!frame)
                return;
            weakThis->m_client->makeCredential(*weakFrame, options, createOptions.mediation, WTFMove(callback));
        };
        return;
    }
    // Async operations are dispatched and handled in the messenger.
    m_client->makeCredential(*frame, options, createOptions.mediation, WTFMove(callback));
}

void AuthenticatorCoordinator::discoverFromExternalSource(const Document& document, CredentialRequestOptions&& requestOptions, const ScopeAndCrossOriginParent& scopeAndCrossOriginParent, CredentialPromise&& promise)
{
    using namespace AuthenticatorCoordinatorInternal;

    auto& callerOrigin = document.securityOrigin();
    RefPtr frame = document.frame();
    const auto& options = requestOptions.publicKey.value();
    ASSERT(frame);
    // The following implements https://www.w3.org/TR/webauthn/#createCredential as of 5 December 2017.
    // Step 1, 3, 13 are handled by the caller.
    // Step 2.
    // This implements https://www.w3.org/TR/webauthn-2/#sctn-permissions-policy
    if (scopeAndCrossOriginParent.first != WebAuthn::Scope::SameOrigin && !isFeaturePolicyAllowedByDocumentAndAllOwners(FeaturePolicy::Type::PublickeyCredentialsGetRule, document, LogFeaturePolicyFailure::No)) {
        promise.reject(Exception { ExceptionCode::NotAllowedError, "The origin of the document is not the same as its ancestors."_s });
        return;
    }

    // Step 5. Skipped since SecurityOrigin doesn't have the concept of "opaque origin".
    // Step 6. The effective domain may be represented in various manners, such as a domain or an ip address.
    // Only the domain format of host is permitted in WebAuthN.
    if (URL::hostIsIPAddress(callerOrigin.domain())) {
        promise.reject(Exception { ExceptionCode::SecurityError, "The effective domain of the document is not a valid domain."_s });
        return;
    }

    // Step 7.
    if (!options.rpId.isEmpty() && !callerOrigin.isMatchingRegistrableDomainSuffix(options.rpId)) {
        promise.reject(Exception { ExceptionCode::SecurityError, "The provided RP ID is not a registrable domain suffix of the effective domain of the document."_s });
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
            promise.reject(Exception { ExceptionCode::SecurityError, "The origin of the document is not authorized for the provided App ID."_s });
            return;
        }
        options.extensions->appid = appid;
    }

    // Step 4, 14-19.
    if (!m_client) {
        promise.reject(Exception { ExceptionCode::UnknownError, "Unknown internal error."_s });
        return;
    }

    if (requestOptions.signal) {
        requestOptions.signal->addAlgorithm([weakThis = WeakPtr { *this }](JSC::JSValue) mutable {
            if (!weakThis)
                return;
            weakThis->m_isCancelling = true;
            weakThis->m_client->cancel([weakThis = WTFMove(weakThis)] () mutable {
                if (!weakThis)
                    return;
                if (auto queuedRequest = WTFMove(weakThis->m_queuedRequest)) {
                    weakThis->m_isCancelling = false;
                    queuedRequest();
                }
            });
        });
    }

    auto callback = [weakThis = WeakPtr { *this }, promise = WTFMove(promise), abortSignal = WTFMove(requestOptions.signal)] (AuthenticatorResponseData&& data, AuthenticatorAttachment attachment, ExceptionData&& exception) mutable {
        if (abortSignal && abortSignal->aborted()) {
            promise.reject(Exception { ExceptionCode::AbortError, "Aborted by AbortSignal."_s });
            return;
        }

        if (auto response = AuthenticatorResponse::tryCreate(WTFMove(data), attachment)) {
            promise.resolve(PublicKeyCredential::create(response.releaseNonNull()).ptr());
            return;
        }
        ASSERT(!exception.message.isNull());
        promise.reject(exception.toException());
    };

    if (m_isCancelling) {
        m_queuedRequest = [weakThis = WeakPtr { *this }, weakFrame = WeakPtr { *frame }, requestOptions = WTFMove(requestOptions), scopeAndCrossOriginParent, callback = WTFMove(callback)]() mutable {
            if (!weakThis || !weakFrame)
                return;
            const auto options = requestOptions.publicKey.value();
            RefPtr frame = weakFrame.get();
            if (!frame)
                return;
            weakThis->m_client->getAssertion(*weakFrame, options, requestOptions.mediation, scopeAndCrossOriginParent, WTFMove(callback));
        };
        return;
    }
    // Async operations are dispatched and handled in the messenger.
    m_client->getAssertion(*frame, options, requestOptions.mediation, scopeAndCrossOriginParent, WTFMove(callback));
}

void AuthenticatorCoordinator::isUserVerifyingPlatformAuthenticatorAvailable(const Document& document, DOMPromiseDeferred<IDLBoolean>&& promise) const
{
    // The following implements https://www.w3.org/TR/webauthn/#isUserVerifyingPlatformAuthenticatorAvailable
    // as of 5 December 2017.
    if (!m_client)  {
        promise.reject(Exception { ExceptionCode::UnknownError, "Unknown internal error."_s });
        return;
    }

    // FIXME(182767): We should consider more on the assessment of the return value. Right now, we return true/false
    // immediately according to platform specific procedures.
    auto completionHandler = [promise = WTFMove(promise)] (bool result) mutable {
        promise.resolve(result);
    };

    // Async operation are dispatched and handled in the messenger.
    m_client->isUserVerifyingPlatformAuthenticatorAvailable(document.securityOrigin(), WTFMove(completionHandler));
}


void AuthenticatorCoordinator::isConditionalMediationAvailable(const Document& document, DOMPromiseDeferred<IDLBoolean>&& promise) const
{
    if (!m_client)  {
        promise.reject(Exception { ExceptionCode::UnknownError, "Unknown internal error."_s });
        return;
    }

    auto completionHandler = [promise = WTFMove(promise)] (bool result) mutable {
        promise.resolve(result);
    };
    // Async operations are dispatched and handled in the messenger.
    m_client->isConditionalMediationAvailable(document.securityOrigin(), WTFMove(completionHandler));
}

void AuthenticatorCoordinator::getClientCapabilities(const Document& document, DOMPromiseDeferred<PublicKeyCredentialClientCapabilities>&& promise) const
{
    if (!m_client)  {
        promise.reject(Exception { ExceptionCode::UnknownError, "Unknown internal error."_s });
        return;
    }

    auto completionHandler = [promise = WTFMove(promise)] (const Vector<KeyValuePair<String, bool>> result) mutable {
        promise.resolve(result);
    };

    m_client->getClientCapabilities(document.securityOrigin(), WTFMove(completionHandler));
}

} // namespace WebCore

#endif // ENABLE(WEB_AUTHN)
