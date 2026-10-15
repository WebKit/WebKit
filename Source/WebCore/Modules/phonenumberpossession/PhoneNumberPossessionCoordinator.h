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

#include <WebCore/IDLTypes.h>
#include <wtf/Noncopyable.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/WeakPtr.h>

namespace WebCore {

class AbortSignal;
class BasicCredential;
class Document;
class Page;
class PhoneNumberPossessionCoordinatorClient;
enum class MediationRequirement : uint8_t;
struct PhoneNumberPossessionRequest;
template<typename IDLType> class DOMPromiseDeferred;

using CredentialPromise = DOMPromiseDeferred<IDLNullable<IDLInterface<BasicCredential>>>;

class PhoneNumberPossessionCoordinator final : public CanMakeWeakPtr<PhoneNumberPossessionCoordinator> {
    WTF_MAKE_TZONE_ALLOCATED(PhoneNumberPossessionCoordinator);
    WTF_MAKE_NONCOPYABLE(PhoneNumberPossessionCoordinator);
public:
    PhoneNumberPossessionCoordinator(Page&, std::unique_ptr<PhoneNumberPossessionCoordinatorClient>&&);
    ~PhoneNumberPossessionCoordinator();

    void LEGACY_NODELETE ref() const;
    void deref() const;

    void discoverFromExternalSource(const Document&, PhoneNumberPossessionRequest&&, MediationRequirement, RefPtr<AbortSignal>&&, CredentialPromise&&);
    void isAvailable(const Document&, DOMPromiseDeferred<IDLBoolean>&&);

private:
    WeakRef<Page> m_page;
    std::unique_ptr<PhoneNumberPossessionCoordinatorClient> m_client;
    bool m_hasPendingRequest { false };
};

} // namespace WebCore

#endif // ENABLE(PHONE_NUMBER_POSSESSION)
