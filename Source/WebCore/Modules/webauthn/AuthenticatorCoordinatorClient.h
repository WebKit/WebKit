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

#include "ExceptionData.h"
#include <wtf/CompletionHandler.h>
#include <wtf/HashMap.h>
#include <wtf/WeakPtr.h>

namespace WebCore {

class DeferredPromise;
class Frame;
class SecurityOrigin;

struct AuthenticatorResponseData;
struct PublicKeyCredentialCreationOptions;
struct PublicKeyCredentialRequestOptions;

using RequestCompletionHandler = CompletionHandler<void(WebCore::AuthenticatorResponseData&&, WebCore::ExceptionData&&)>;
using QueryCompletionHandler = CompletionHandler<void(bool)>;

class WEBCORE_EXPORT AuthenticatorCoordinatorClient : public CanMakeWeakPtr<AuthenticatorCoordinatorClient> {
    WTF_MAKE_FAST_ALLOCATED;
    WTF_MAKE_NONCOPYABLE(AuthenticatorCoordinatorClient);
public:
    AuthenticatorCoordinatorClient() = default;
    virtual ~AuthenticatorCoordinatorClient() = default;

    virtual void makeCredential(const Frame&, const SecurityOrigin&, const Vector<uint8_t>&, const PublicKeyCredentialCreationOptions&, RequestCompletionHandler&&) { };
    virtual void getAssertion(const Frame&, const SecurityOrigin&, const Vector<uint8_t>&, const PublicKeyCredentialRequestOptions&, RequestCompletionHandler&&) { };
    virtual void isUserVerifyingPlatformAuthenticatorAvailable(QueryCompletionHandler&&) { };
};

} // namespace WebCore

#endif // ENABLE(WEB_AUTHN)
