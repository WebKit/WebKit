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

#pragma once

#if ENABLE(PROOF_OF_POSSESSION)

#include <WebCore/BasicCredential.h>
#include <WebCore/IDLTypes.h>
#include <WebCore/ProofOfPossessionProviderInfo.h>

namespace WebCore {

class AbortSignal;
class Document;
enum class MediationRequirement : uint8_t;
struct ProofOfPossessionRequest;
struct ProofOfPossessionResponse;
template<typename IDLType> class DOMPromiseDeferred;

using CredentialPromise = DOMPromiseDeferred<IDLNullable<IDLInterface<BasicCredential>>>;

class ProofOfPossessionCredential : public BasicCredential {
public:
    static Ref<ProofOfPossessionCredential> create(ProofOfPossessionResponse&&);
    virtual ~ProofOfPossessionCredential();

    static void discoverFromExternalSource(const Document&, ProofOfPossessionRequest&&, MediationRequirement, RefPtr<AbortSignal>&&, CredentialPromise&&);
    static void isAvailable(Document&, DOMPromiseDeferred<IDLBoolean>&&);
    static void isConditionalMediationAvailable(DOMPromiseDeferred<IDLBoolean>&&);

    Ref<ArrayBuffer> encryptedIdentifierWithProof() const { return m_encryptedIdentifierWithProof; }
    RefPtr<ProofOfPossessionProviderInfo> providerData() const { return m_providerData; }
    RefPtr<ArrayBuffer> issuerPublicKey() const { return m_issuerPublicKey; }
    RefPtr<ArrayBuffer> encryptedSubject() const { return m_encryptedSubject; }
    Ref<ArrayBuffer> clientDataJSON() const { return m_clientDataJSON; }
    RefPtr<ArrayBuffer> transferProof() const { return m_transferProof; }

protected:
    explicit ProofOfPossessionCredential(ProofOfPossessionResponse&&);

private:
    Type credentialType() const final { return Type::ProofOfPossession; }

    const Ref<ArrayBuffer> m_encryptedIdentifierWithProof;
    const RefPtr<ProofOfPossessionProviderInfo> m_providerData;
    const RefPtr<ArrayBuffer> m_issuerPublicKey;
    const RefPtr<ArrayBuffer> m_encryptedSubject;
    const Ref<ArrayBuffer> m_clientDataJSON;
    const RefPtr<ArrayBuffer> m_transferProof;
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_BASIC_CREDENTIAL(ProofOfPossessionCredential, BasicCredential::Type::ProofOfPossession)

#endif // ENABLE(PROOF_OF_POSSESSION)
