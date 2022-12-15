/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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

#if ENABLE(WEB_AUTHN)

#include "IDLTypes.h"
#include <wtf/Forward.h>
#include <wtf/Noncopyable.h>
#include <wtf/WeakPtr.h>

namespace WebAuthn {
enum class Scope;
}

namespace WebCore {

class AbortSignal;
class AuthenticatorCoordinatorClient;
class BasicCredential;
class Document;

struct PublicKeyCredentialCreationOptions;
struct PublicKeyCredentialRequestOptions;
struct CredentialRequestOptions;
struct SecurityOriginData;

template<typename IDLType> class DOMPromiseDeferred;

using CredentialPromise = DOMPromiseDeferred<IDLNullable<IDLInterface<BasicCredential>>>;
using ScopeAndCrossOriginParent = std::pair<WebAuthn::Scope, std::optional<SecurityOriginData>>;

class AuthenticatorCoordinator final : public CanMakeWeakPtr<AuthenticatorCoordinator> {
    WTF_MAKE_FAST_ALLOCATED;
    WTF_MAKE_NONCOPYABLE(AuthenticatorCoordinator);
public:
    WEBCORE_EXPORT explicit AuthenticatorCoordinator(std::unique_ptr<AuthenticatorCoordinatorClient>&&);
    WEBCORE_EXPORT void setClient(std::unique_ptr<AuthenticatorCoordinatorClient>&&);

    // The following methods implement static methods of PublicKeyCredential.
    void create(const Document&, const PublicKeyCredentialCreationOptions&, WebAuthn::Scope, RefPtr<AbortSignal>&&, CredentialPromise&&);
    void discoverFromExternalSource(const Document&, CredentialRequestOptions&&, const ScopeAndCrossOriginParent&, CredentialPromise&&);
    void isUserVerifyingPlatformAuthenticatorAvailable(const Document&, DOMPromiseDeferred<IDLBoolean>&&) const;
    void isConditionalMediationAvailable(DOMPromiseDeferred<IDLBoolean>&&) const;

    void resetUserGestureRequirement();

private:
    AuthenticatorCoordinator() = default;

    std::unique_ptr<AuthenticatorCoordinatorClient> m_client;
};

} // namespace WebCore

#endif // ENABLE(WEB_AUTHN)
