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

#if ENABLE(PHONE_NUMBER_POSSESSION)

#include "AggregatorResponseData.h"
#include <WebCore/BasicCredential.h>
#include <WebCore/IDLTypes.h>

namespace WebCore {

class AbortSignal;
class Document;
enum class MediationRequirement : uint8_t;
struct PhoneNumberPossessionRequest;
struct PhoneNumberPossessionResponse;
template<typename IDLType> class DOMPromiseDeferred;

using CredentialPromise = DOMPromiseDeferred<IDLNullable<IDLInterface<BasicCredential>>>;

class PhoneNumberPossessionCredential final : public BasicCredential {
public:
    static Ref<PhoneNumberPossessionCredential> LEGACY_NODELETE create(PhoneNumberPossessionResponse&&);
    virtual ~PhoneNumberPossessionCredential();

    static void discoverFromExternalSource(const Document&, PhoneNumberPossessionRequest&&, MediationRequirement, RefPtr<AbortSignal>&&, CredentialPromise&&);
    static void isAvailable(Document&, DOMPromiseDeferred<IDLBoolean>&&);
    static void isConditionalMediationAvailable(DOMPromiseDeferred<IDLBoolean>&&);

    Ref<ArrayBuffer> encryptedPNPIWithProof() const { return m_encryptedPNPIWithProof; }
    RefPtr<AggregatorResponseData> aggregatorData() const { return m_aggregatorData; }
    RefPtr<ArrayBuffer> carrierPubKey() const { return m_carrierPubKey; }
    RefPtr<ArrayBuffer> encryptedMSISDN() const { return m_encryptedMSISDN; }
    Ref<ArrayBuffer> clientDataJSON() const { return m_clientDataJSON; }
    RefPtr<ArrayBuffer> portingZKP() const { return m_portingZKP; }

private:
    explicit PhoneNumberPossessionCredential(PhoneNumberPossessionResponse&&);

    Type credentialType() const final { return Type::PhoneNumberPossession; }

    const Ref<ArrayBuffer> m_encryptedPNPIWithProof;
    const RefPtr<AggregatorResponseData> m_aggregatorData;
    const RefPtr<ArrayBuffer> m_carrierPubKey;
    const RefPtr<ArrayBuffer> m_encryptedMSISDN;
    const Ref<ArrayBuffer> m_clientDataJSON;
    const RefPtr<ArrayBuffer> m_portingZKP;
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_BASIC_CREDENTIAL(PhoneNumberPossessionCredential, BasicCredential::Type::PhoneNumberPossession)

#endif // ENABLE(PHONE_NUMBER_POSSESSION)
