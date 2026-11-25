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

#include <JavaScriptCore/ArrayBuffer.h>
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
    NODELETE static Ref<PhoneNumberPossessionCredential> create(PhoneNumberPossessionResponse&&);
    virtual ~PhoneNumberPossessionCredential();

    static void discoverFromExternalSource(const Document&, PhoneNumberPossessionRequest&&, MediationRequirement, RefPtr<AbortSignal>&&, CredentialPromise&&);
    static void isAvailable(Document&, DOMPromiseDeferred<IDLBoolean>&&);
    static void isConditionalMediationAvailable(DOMPromiseDeferred<IDLBoolean>&&);

    Ref<ArrayBuffer> encryptedResult() const { return m_encryptedResult; }
    const RefPtr<StringImpl>& carrierDomain() const LIFETIME_BOUND { return m_carrierDomain; }
    uint16_t carrierConfigurationID() const { return m_carrierConfigurationID; }
    RefPtr<ArrayBuffer> encryptedDecryptionKey() const { return m_encryptedDecryptionKey; }
    RefPtr<ArrayBuffer> carrierPubKey() const { return m_carrierPubKey; }
    Ref<ArrayBuffer> clientDataJSON() const { return m_clientDataJSON; }
    RefPtr<ArrayBuffer> portingZKP() const { return m_portingZKP; }

private:
    explicit PhoneNumberPossessionCredential(PhoneNumberPossessionResponse&&);

    Type credentialType() const final { return Type::PhoneNumberPossession; }

    const Ref<ArrayBuffer> m_encryptedResult;
    const RefPtr<StringImpl> m_carrierDomain;
    const uint16_t m_carrierConfigurationID;
    const RefPtr<ArrayBuffer> m_encryptedDecryptionKey;
    const RefPtr<ArrayBuffer> m_carrierPubKey;
    const Ref<ArrayBuffer> m_clientDataJSON;
    const RefPtr<ArrayBuffer> m_portingZKP;
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_BASIC_CREDENTIAL(PhoneNumberPossessionCredential, BasicCredential::Type::PhoneNumberPossession)

#endif // ENABLE(PHONE_NUMBER_POSSESSION)
