/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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
#include "ProofOfPossessionCoordinator.h"

#if ENABLE(PROOF_OF_POSSESSION)

#include "AbortSignal.h"
#include "DocumentPage.h"
#include "DocumentSecurityOrigin.h"
#include "DocumentWindow.h"
#include "Exception.h"
#include "FrameDestructionObserverInlines.h"
#include "JSBasicCredential.h"
#include "JSDOMConvertBoolean.h"
#include "JSDOMConvertInterface.h"
#include "JSDOMConvertNullable.h"
#include "JSDOMPromiseDeferred.h"
#include "JSValueInWrappedObjectInlines.h"
#include "LocalDOMWindow.h"
#include "LocalFrame.h"
#include "MediationRequirement.h"
#include "Page.h"
#include "ProofOfPossessionCoordinatorClient.h"
#include "ProofOfPossessionCredential.h"
#include "ProofOfPossessionRequest.h"
#include "ProofOfPossessionResponse.h"
#include "SecurityOrigin.h"
#include <wtf/TZoneMallocInlines.h>
#include <wtf/URL.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(ProofOfPossessionCoordinatorClient);
WTF_MAKE_TZONE_ALLOCATED_IMPL(ProofOfPossessionCoordinator);

ProofOfPossessionCoordinator::ProofOfPossessionCoordinator(Page& page, std::unique_ptr<ProofOfPossessionCoordinatorClient>&& client)
    : m_page(page)
    , m_client(WTF::move(client))
{
}

ProofOfPossessionCoordinator::~ProofOfPossessionCoordinator() = default;

void ProofOfPossessionCoordinator::ref() const
{
    m_page->ref();
}

void ProofOfPossessionCoordinator::deref() const
{
    m_page->deref();
}

void ProofOfPossessionCoordinator::discoverFromExternalSource(const Document& document, ProofOfPossessionRequest&& request, MediationRequirement mediation, RefPtr<AbortSignal>&& signal, CredentialPromise&& promise)
{
    if (mediation == MediationRequirement::Conditional) {
        promise.reject(Exception { ExceptionCode::TypeError, "Conditional mediation is not supported for proof of possession credentials."_s });
        return;
    }

    if (mediation == MediationRequirement::Silent) {
        promise.resolve(nullptr);
        return;
    }

    if (m_hasPendingRequest) {
        promise.reject(Exception { ExceptionCode::NotAllowedError, "A proof of possession request is already in progress."_s });
        return;
    }

    Ref callerOrigin = document.securityOrigin();
    if (callerOrigin->isOpaque()) {
        promise.reject(Exception { ExceptionCode::SecurityError, "The credential operation is not allowed in an opaque origin."_s });
        return;
    }

    if (!m_client) {
        promise.reject(Exception { ExceptionCode::NotSupportedError, "Proof of possession is not supported."_s });
        return;
    }

    if (!document.isSameOriginWithAncestors()) {
        promise.reject(Exception { ExceptionCode::NotAllowedError, "The origin of the document is not the same as its ancestors."_s });
        return;
    }

    RefPtr frame = document.frame();
    RefPtr window = document.window();
    if (!frame || !window || !document.page()) {
        promise.reject(Exception { ExceptionCode::InvalidStateError, "Preconditions for calling .get() are not met."_s });
        return;
    }

    if (!document.isFullyActiveAndHasUserAttention()) {
        promise.reject(Exception { ExceptionCode::NotAllowedError, "The document must be focused and visible."_s });
        return;
    }

    if (auto exception = validateProofOfPossessionRequest(request)) {
        promise.reject(WTF::move(*exception));
        return;
    }

    if (URL::hostIsIPAddress(callerOrigin->domain())) {
        promise.reject(Exception { ExceptionCode::SecurityError, "The effective domain of the document is not a valid domain."_s });
        return;
    }

    if (request.relyingPartyIdentifier.isEmpty())
        request.relyingPartyIdentifier = callerOrigin->domain();

    if (!callerOrigin->isMatchingRegistrableDomainSuffix(request.relyingPartyIdentifier)) {
        promise.reject(Exception { ExceptionCode::SecurityError, "The origin of the document is not authorized for the provided RP ID."_s });
        return;
    }

    // Consume the activation last, so that a request rejected for invalid input doesn't use it up.
    if (!window->consumeTransientActivation()) {
        promise.reject(Exception { ExceptionCode::NotAllowedError, "Calling get() needs to be triggered by an activation triggering user event."_s });
        return;
    }

    m_hasPendingRequest = true;

    std::optional<uint32_t> abortAlgorithmIdentifier;
    if (signal) {
        abortAlgorithmIdentifier = signal->addAlgorithm([weakThis = WeakPtr { *this }](JSC::JSValue) {
            RefPtr protectedThis = weakThis.get();
            if (!protectedThis || !protectedThis->m_hasPendingRequest)
                return;
            // The UI process answers the pending request once it is cancelled, and that answer rejects the promise.
            protectedThis->m_client->cancel([] { });
        });
    }

    m_client->requestProofOfPossession(*frame, request, [weakThis = WeakPtr { *this }, promise = WTF::move(promise), signal = WTF::move(signal), abortAlgorithmIdentifier](std::expected<ProofOfPossessionResponse, ExceptionData>&& result) mutable {
        if (RefPtr protectedThis = weakThis.get())
            protectedThis->m_hasPendingRequest = false;

        if (abortAlgorithmIdentifier)
            signal->removeAlgorithm(*abortAlgorithmIdentifier);

        if (signal && signal->aborted()) {
            promise.rejectType<IDLAny>(signal->reason().getValue());
            return;
        }

        if (!result) {
            promise.reject(WTF::move(result.error()).toException());
            return;
        }

        promise.resolve(ProofOfPossessionCredential::create(WTF::move(*result)).ptr());
    });
}

void ProofOfPossessionCoordinator::isAvailable(const Document& document, DOMPromiseDeferred<IDLBoolean>&& promise)
{
    if (!m_client || !document.isFullyActive() || document.securityOrigin().isOpaque() || !document.isSameOriginWithAncestors()) {
        promise.resolve(false);
        return;
    }

    m_client->isAvailable([promise = WTF::move(promise)](bool isAvailable) mutable {
        promise.resolve(isAvailable);
    });
}

} // namespace WebCore

#endif // ENABLE(PROOF_OF_POSSESSION)
