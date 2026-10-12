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
#include "ProofOfPossessionCredential.h"

#if ENABLE(PROOF_OF_POSSESSION)

#include "AbortSignal.h"
#include "DocumentPage.h"
#include "JSDOMPromiseDeferred.h"
#include "MediationRequirement.h"
#include "Page.h"
#include "ProofOfPossessionCoordinator.h"
#include "ProofOfPossessionRequest.h"
#include "ProofOfPossessionResponse.h"
#include <wtf/UUID.h>

namespace WebCore {

Ref<ProofOfPossessionCredential> ProofOfPossessionCredential::create(ProofOfPossessionResponse&& response)
{
    return adoptRef(*new ProofOfPossessionCredential(WTF::move(response)));
}

ProofOfPossessionCredential::ProofOfPossessionCredential(ProofOfPossessionResponse&& response)
    : BasicCredential(createVersion4UUIDString(), Type::ProofOfPossession, Discovery::Remote)
    , m_encryptedIdentifierWithProof(WTF::move(response.encryptedIdentifierWithProof))
    , m_providerData(response.providerData ? RefPtr { ProofOfPossessionProviderInfo::create(WTF::move(*response.providerData)) } : nullptr)
    , m_issuerPublicKey(WTF::move(response.issuerPublicKey))
    , m_encryptedSubject(WTF::move(response.encryptedSubject))
    , m_clientDataJSON(WTF::move(response.clientDataJSON))
    , m_transferProof(WTF::move(response.transferProof))
{
}

ProofOfPossessionCredential::~ProofOfPossessionCredential() = default;

void ProofOfPossessionCredential::discoverFromExternalSource(const Document& document, ProofOfPossessionRequest&& request, MediationRequirement mediation, RefPtr<AbortSignal>&& signal, CredentialPromise&& promise)
{
    RefPtr page = document.page();
    if (!page) {
        promise.reject(Exception { ExceptionCode::InvalidStateError, "Preconditions for calling .get() are not met."_s });
        return;
    }
    page->proofOfPossessionCoordinator().discoverFromExternalSource(document, WTF::move(request), mediation, WTF::move(signal), WTF::move(promise));
}

void ProofOfPossessionCredential::isAvailable(Document& document, DOMPromiseDeferred<IDLBoolean>&& promise)
{
    RefPtr page = document.page();
    if (!page) {
        promise.reject(Exception { ExceptionCode::InvalidStateError });
        return;
    }
    page->proofOfPossessionCoordinator().isAvailable(document, WTF::move(promise));
}

void ProofOfPossessionCredential::isConditionalMediationAvailable(DOMPromiseDeferred<IDLBoolean>&& promise)
{
    promise.resolve(false);
}

} // namespace WebCore

#endif // ENABLE(PROOF_OF_POSSESSION)
