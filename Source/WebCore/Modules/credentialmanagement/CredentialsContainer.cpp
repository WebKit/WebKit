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
#include "Document.h"
#include "ExceptionOr.h"
#include "JSDOMPromiseDeferred.h"
#include "Page.h"
#include "SecurityOrigin.h"

namespace WebCore {

CredentialsContainer::CredentialsContainer(WeakPtr<Document>&& document)
    : m_document(WTFMove(document))
{
}

bool CredentialsContainer::doesHaveSameOriginAsItsAncestors()
{
    // The following implements https://w3c.github.io/webappsec-credential-management/#same-origin-with-its-ancestors
    // as of 14 November 2017.
    if (!m_document)
        return false;

    auto& origin = m_document->securityOrigin();
    for (auto* document = m_document->parentDocument(); document; document = document->parentDocument()) {
        if (!origin.isSameOriginAs(document->securityOrigin()))
            return false;
    }
    return true;
}

void CredentialsContainer::get(CredentialRequestOptions&& options, CredentialPromise&& promise)
{
    // The following implements https://www.w3.org/TR/credential-management-1/#algorithm-request as of 4 August 2017
    // with enhancement from 14 November 2017 Editor's Draft.
    if (!m_document || !m_document->page()) {
        promise.reject(Exception { NotSupportedError });
        return;
    }
    if (options.signal && options.signal->aborted()) {
        promise.reject(Exception { AbortError, "Aborted by AbortSignal."_s });
        return;
    }
    // Step 1-2.
    ASSERT(m_document->isSecureContext());

    // Step 3 is enhanced with doesHaveSameOriginAsItsAncestors.
    // Step 4-6. Shortcut as we only support PublicKeyCredential which can only
    // be requested from [[discoverFromExternalSource]].
    if (!options.publicKey) {
        promise.reject(Exception { NotSupportedError, "Only PublicKeyCredential is supported."_s });
        return;
    }

    // Extra.
    if (!m_document->hasFocus()) {
        promise.reject(Exception { NotAllowedError, "The document is not focused."_s });
        return;
    }

    m_document->page()->authenticatorCoordinator().discoverFromExternalSource(m_document->securityOrigin(), options.publicKey.value(), doesHaveSameOriginAsItsAncestors(), WTFMove(options.signal), WTFMove(promise));
}

void CredentialsContainer::store(const BasicCredential&, CredentialPromise&& promise)
{
    promise.reject(Exception { NotSupportedError, "Not implemented."_s });
}

void CredentialsContainer::isCreate(CredentialCreationOptions&& options, CredentialPromise&& promise)
{
    // The following implements https://www.w3.org/TR/credential-management-1/#algorithm-create as of 4 August 2017
    // with enhancement from 14 November 2017 Editor's Draft.
    if (!m_document || !m_document->page()) {
        promise.reject(Exception { NotSupportedError });
        return;
    }
    if (options.signal && options.signal->aborted()) {
        promise.reject(Exception { AbortError, "Aborted by AbortSignal."_s });
        return;
    }
    // Step 1-2.
    ASSERT(m_document->isSecureContext());

    // Step 3-7. Shortcut as we only support one kind of credentials.
    if (!options.publicKey) {
        promise.reject(Exception { NotSupportedError, "Only PublicKeyCredential is supported."_s });
        return;
    }

    // Extra.
    if (!m_document->hasFocus()) {
        promise.reject(Exception { NotAllowedError, "The document is not focused."_s });
        return;
    }

    m_document->page()->authenticatorCoordinator().create(m_document->securityOrigin(), options.publicKey.value(), doesHaveSameOriginAsItsAncestors(), WTFMove(options.signal), WTFMove(promise));
}

void CredentialsContainer::preventSilentAccess(DOMPromiseDeferred<void>&& promise) const
{
    promise.reject(Exception { NotSupportedError, "Not implemented."_s });
}

} // namespace WebCore

#endif // ENABLE(WEB_AUTHN)
