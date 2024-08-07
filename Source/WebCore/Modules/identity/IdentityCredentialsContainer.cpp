/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
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
#include "IdentityCredentialsContainer.h"

#if ENABLE(WEB_AUTHN)

#include "CredentialCreationOptions.h"
#include "CredentialRequestOptions.h"
#include "DigitalCredential.h"
#include "DigitalCredentialRequestOptions.h"
#include "Document.h"
#include "ExceptionOr.h"
#include "JSDOMPromiseDeferred.h"
#include "JSDigitalCredential.h"
#include "LocalDOMWindow.h"
#include "Page.h"
#include "VisibilityState.h"

namespace WebCore {
IdentityCredentialsContainer::IdentityCredentialsContainer(WeakPtr<Document, WeakPtrImplWithEventTargetData>&& document)
    : CredentialsContainer(WTFMove(document))
{
}

void IdentityCredentialsContainer::get(CredentialRequestOptions&& options, CredentialPromise&& promise)
{
    if (!performCommonChecks(options, promise))
        return;

    if (!options.digital || options.publicKey) {
        promise.reject(Exception { ExceptionCode::NotSupportedError, "Only digital member is supported."_s });
        return;
    }

    RefPtr document = this->document();
    ASSERT(document);

    if (!PermissionsPolicy::isFeatureEnabled(PermissionsPolicy::Feature::DigitalCredentialsGetRule, *document, PermissionsPolicy::ShouldReportViolation::No)) {
        promise.reject(Exception { ExceptionCode::NotAllowedError, "Third-party iframes are not allowed to call .get() unless explicitly allowed via Permissions Policy (digital-credentials-get)"_s });
        return;
    }

    if (!document->hasFocus()) {
        promise.reject(Exception { ExceptionCode::NotAllowedError, "The document is not focused."_s });
        return;
    }

    if (document->visibilityState() != VisibilityState::Visible) {
        promise.reject(Exception { ExceptionCode::NotAllowedError, "The document is not visible."_s });
        return;
    }

    RefPtr window = document->domWindow();
    if (!window || !window->consumeTransientActivation()) {
        promise.reject(Exception { ExceptionCode::NotAllowedError, "Calling get() needs to be triggered by an activation triggering user event."_s });
        return;
    }

    if (options.digital->providers.isEmpty()) {
        promise.reject(Exception { ExceptionCode::TypeError, "At least one provider must be specified."_s });
        return;
    }

    // FIXME: <https://webkit.org/b/277322> mediation requirement,
    // which is waiting on https://github.com/WICG/digital-credentials/pull/149

    document->page()->credentialRequestCoordinator().discoverFromExternalSource(*document, WTFMove(options), WTFMove(promise));
}

void IdentityCredentialsContainer::isCreate(CredentialCreationOptions&& options, CredentialPromise&& promise)
{
    if (!performCommonChecks(options, promise))
        return;

    // Default as per Cred Man spec is to resolve with null.
    // https://www.w3.org/TR/credential-management-1/#algorithm-create-cred
    promise.resolve(nullptr);
}

} // namespace WebCore

#endif // ENABLE(WEB_AUTHN)
