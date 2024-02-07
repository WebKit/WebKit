/*
 * Copyright (C) 2017 Google Inc. All rights reserved.
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
#include "CredentialsContainer.h"

#if ENABLE(WEB_AUTHN)

#include "AbortSignal.h"
#include "CredentialCreationOptions.h"
#include "CredentialRequestOptions.h"
#include "DigitalCredentialRequestOptions.h"
#include "DigitalIdentity.h"
#include "Document.h"
#include "ExceptionOr.h"
#include "JSDOMPromiseDeferred.h"
#include "JSDigitalIdentity.h"
#include "Page.h"
#include "SecurityOrigin.h"
#include "WebAuthenticationConstants.h"

namespace WebCore {

CredentialsContainer::CredentialsContainer(WeakPtr<Document, WeakPtrImplWithEventTargetData>&& document)
    : m_document(WTFMove(document))
{
}

ScopeAndCrossOriginParent CredentialsContainer::scopeAndCrossOriginParent() const
{
    if (!m_document)
        return std::pair { WebAuthn::Scope::CrossOrigin, std::nullopt };

    bool isSameSite = true;
    auto& origin = m_document->securityOrigin();
    auto& url = m_document->url();
    std::optional<SecurityOriginData> crossOriginParent;
    for (RefPtr document = m_document->parentDocument(); document; document = document->parentDocument()) {
        if (!origin.isSameOriginDomain(document->securityOrigin()) && !areRegistrableDomainsEqual(url, document->url()))
            isSameSite = false;
        if (!crossOriginParent && !origin.isSameOriginAs(document->securityOrigin()))
            crossOriginParent = document->securityOrigin().data();
    }

    if (!crossOriginParent)
        return std::pair { WebAuthn::Scope::SameOrigin, std::nullopt };
    if (isSameSite)
        return std::pair { WebAuthn::Scope::SameSite, crossOriginParent };
    return std::pair { WebAuthn::Scope::CrossOrigin, crossOriginParent };
}

void CredentialsContainer::get(CredentialRequestOptions&& options, CredentialPromise&& promise)
{
    // The following implements https://www.w3.org/TR/credential-management-1/#algorithm-request as of 4 August 2017
    // with enhancement from 14 November 2017 Editor's Draft.
    if (!m_document || !m_document->page()) {
        promise.reject(Exception { ExceptionCode::NotSupportedError });
        return;
    }
    if (options.signal && options.signal->aborted()) {
        promise.reject(Exception { ExceptionCode::AbortError, "Aborted by AbortSignal."_s });
        return;
    }
    // Step 1-2.
    ASSERT(m_document->isSecureContext());

    // Step 3 is enhanced with doesHaveSameOriginAsItsAncestors.
    // Step 4-6. Shortcut as we only support PublicKeyCredential which can only
    // be requested from [[discoverFromExternalSource]].
    if (!options.publicKey) {
        promise.reject(Exception { ExceptionCode::NotSupportedError, "Only PublicKeyCredential is supported."_s });
        return;
    }

    // The request will be aborted in WebAuthenticatorCoordinatorProxy if conditional mediation is not available.
    if (options.mediation != MediationRequirement::Conditional && !m_document->hasFocus()) {
        promise.reject(Exception { ExceptionCode::NotAllowedError, "The document is not focused."_s });
        return;
    }

    m_document->page()->authenticatorCoordinator().discoverFromExternalSource(*m_document, WTFMove(options), scopeAndCrossOriginParent(), WTFMove(promise));
}

void CredentialsContainer::store(const BasicCredential&, CredentialPromise&& promise)
{
    promise.reject(Exception { ExceptionCode::NotSupportedError, "Not implemented."_s });
}

void CredentialsContainer::isCreate(CredentialCreationOptions&& options, CredentialPromise&& promise)
{
    // The following implements https://www.w3.org/TR/credential-management-1/#algorithm-create as of 4 August 2017
    // with enhancement from 14 November 2017 Editor's Draft.
    if (!m_document || !m_document->page()) {
        promise.reject(Exception { ExceptionCode::NotSupportedError });
        return;
    }
    if (options.signal && options.signal->aborted()) {
        promise.reject(Exception { ExceptionCode::AbortError, "Aborted by AbortSignal."_s });
        return;
    }
    // Step 1-2.
    ASSERT(m_document->isSecureContext());

    // Step 3-7. Shortcut as we only support one kind of credentials.
    if (!options.publicKey) {
        promise.reject(Exception { ExceptionCode::NotSupportedError, "Only PublicKeyCredential is supported."_s });
        return;
    }

    // Extra.
    if (!m_document->hasFocus()) {
        promise.reject(Exception { ExceptionCode::NotAllowedError, "The document is not focused."_s });
        return;
    }

    m_document->page()->authenticatorCoordinator().create(*m_document, WTFMove(options), scopeAndCrossOriginParent().first, WTFMove(options.signal), WTFMove(promise));
}

void CredentialsContainer::preventSilentAccess(DOMPromiseDeferred<void>&& promise) const
{
    promise.resolve();
}

void CredentialsContainer::requestIdentity(DigitalCredentialRequestOptions&& options, DigitalIdentityPromise&& promise)
{
    if (options.signal && options.signal->aborted()) {
        promise.reject(Exception { ExceptionCode::AbortError, "Aborted by AbortSignal."_s });
        return;
    }
    std::span<uint8_t> emptySpan;
    Ref<ArrayBuffer> emptyArrayBuffer = ArrayBuffer::create(emptySpan);
    promise.resolve(DigitalIdentity::create(WTFMove(emptyArrayBuffer)));
}

} // namespace WebCore

#endif // ENABLE(WEB_AUTHN)
