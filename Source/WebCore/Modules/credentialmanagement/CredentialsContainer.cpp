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
#include "DigitalCredential.h"
#include "DigitalCredentialRequestOptions.h"
#include "Document.h"
#include "ExceptionOr.h"
#include "JSDOMPromiseDeferred.h"
#include "JSDigitalCredential.h"
#include "Page.h"
#include "SecurityOrigin.h"

namespace WebCore {

CredentialsContainer::CredentialsContainer(WeakPtr<Document, WeakPtrImplWithEventTargetData>&& document)
    : m_document(WTFMove(document))
{
}

void CredentialsContainer::get(CredentialRequestOptions&& options, CredentialPromise&& promise)
{
    // The following implements https://www.w3.org/TR/credential-management-1/#algorithm-request as of 4 August 2017
    // with enhancement from 14 November 2017 Editor's Draft.
    if (!performCommonChecks(options, promise)) {
        return;
    }

    document()->page()->authenticatorCoordinator().discoverFromExternalSource(*document(), WTFMove(options), WTFMove(promise));
}

void CredentialsContainer::store(const BasicCredential&, CredentialPromise&& promise)
{
    promise.reject(Exception { ExceptionCode::NotSupportedError, "Not implemented."_s });
}

void CredentialsContainer::isCreate(CredentialCreationOptions&& options, CredentialPromise&& promise)
{
    // The following implements https://www.w3.org/TR/credential-management-1/#algorithm-create as of 4 August 2017
    // with enhancement from 14 November 2017 Editor's Draft.
    if (!performCommonChecks(options, promise))
        return;

    // Extra.
    if (!document()->hasFocus()) {
        promise.reject(Exception { ExceptionCode::NotAllowedError, "The document is not focused."_s });
        return;
    }

    document()->page()->authenticatorCoordinator().create(*document(), WTFMove(options), WTFMove(options.signal), WTFMove(promise));
}

void CredentialsContainer::preventSilentAccess(DOMPromiseDeferred<void>&& promise) const
{
    if (document() && !document()->isFullyActive()) {
        promise.reject(Exception { ExceptionCode::InvalidStateError, "The document is not fully active."_s });
        return;
    }
    promise.resolve();
}

template<typename Options>
bool CredentialsContainer::performCommonChecks(const Options& options, CredentialPromise& promise)
{
    RefPtr document = this->document();
    if (!document) {
        promise.reject(Exception { ExceptionCode::NotSupportedError });
        return false;
    }

    if (!document->isFullyActive()) {
        promise.reject(Exception { ExceptionCode::InvalidStateError, "The document is not fully active."_s });
        return false;
    }

    if (!document->page()) {
        promise.reject(Exception { ExceptionCode::InvalidStateError, "No browsing context"_s });
        return false;
    }

    if (options.signal && options.signal->aborted()) {
        promise.rejectType<IDLAny>(options.signal->reason().getValue());
        return false;
    }

    if constexpr (std::is_same_v<Options, CredentialRequestOptions>) {
        if (!options.publicKey && !options.digital) {
            promise.reject(Exception { ExceptionCode::NotSupportedError, "Missing request type."_s });
            return false;
        }

        if (options.publicKey && options.digital) {
            promise.reject(Exception { ExceptionCode::NotSupportedError, "Only one request type is supported at a time."_s });
            return false;
        }
    }

    ASSERT(document->isSecureContext());
    return true;
}

} // namespace WebCore

#endif // ENABLE(WEB_AUTHN)
