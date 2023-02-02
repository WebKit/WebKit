/*
 * Copyright (C) 2018-2021 Apple Inc. All rights reserved.
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

#include "AuthenticatorCoordinator.h"
#include "ExceptionData.h"
#include <wtf/CompletionHandler.h>
#include <wtf/WeakPtr.h>

namespace WebAuthn {
enum class Scope;
}

namespace WebCore {

class DeferredPromise;
class Frame;
class SecurityOrigin;

enum class AuthenticatorAttachment : uint8_t;
enum class MediationRequirement : uint8_t;

struct AuthenticatorResponseData;
struct PublicKeyCredentialCreationOptions;
struct PublicKeyCredentialRequestOptions;
struct SecurityOriginData;

using RequestCompletionHandler = CompletionHandler<void(WebCore::AuthenticatorResponseData&&, WebCore::AuthenticatorAttachment, WebCore::ExceptionData&&)>;
using QueryCompletionHandler = CompletionHandler<void(bool)>;

class AuthenticatorCoordinatorClient : public CanMakeWeakPtr<AuthenticatorCoordinatorClient> {
    WTF_MAKE_FAST_ALLOCATED;
    WTF_MAKE_NONCOPYABLE(AuthenticatorCoordinatorClient);
public:
    AuthenticatorCoordinatorClient() = default;
    virtual ~AuthenticatorCoordinatorClient() = default;

    virtual void makeCredential(const Frame&, const SecurityOrigin&, const Vector<uint8_t>&, const PublicKeyCredentialCreationOptions&, RequestCompletionHandler&&) = 0;
    virtual void getAssertion(const Frame&, const SecurityOrigin&, const Vector<uint8_t>&, const PublicKeyCredentialRequestOptions&, MediationRequirement, const ScopeAndCrossOriginParent&, RequestCompletionHandler&&) = 0;
    virtual void isConditionalMediationAvailable(const SecurityOrigin&, QueryCompletionHandler&&) = 0;
    virtual void isUserVerifyingPlatformAuthenticatorAvailable(const SecurityOrigin&, QueryCompletionHandler&&) = 0;
    virtual void cancel() = 0;

    virtual void resetUserGestureRequirement() { }
};

} // namespace WebCore

#endif // ENABLE(WEB_AUTHN)
