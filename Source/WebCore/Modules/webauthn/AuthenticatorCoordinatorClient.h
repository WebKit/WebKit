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

struct PublicKeyCredentialCreationOptions;
struct PublicKeyCredentialData;
struct PublicKeyCredentialRequestOptions;

using RequestCompletionHandler = CompletionHandler<void(const WebCore::PublicKeyCredentialData&, const WebCore::ExceptionData&)>;
using QueryCompletionHandler = CompletionHandler<void(bool)>;

class WEBCORE_EXPORT AuthenticatorCoordinatorClient : public CanMakeWeakPtr<AuthenticatorCoordinatorClient> {
    WTF_MAKE_FAST_ALLOCATED;
    WTF_MAKE_NONCOPYABLE(AuthenticatorCoordinatorClient);
public:
    AuthenticatorCoordinatorClient() = default;
    virtual ~AuthenticatorCoordinatorClient() = default;

    // Senders.
    virtual void makeCredential(const Vector<uint8_t>& hash, const PublicKeyCredentialCreationOptions&, RequestCompletionHandler&&) = 0;
    virtual void getAssertion(const Vector<uint8_t>& hash, const PublicKeyCredentialRequestOptions&, RequestCompletionHandler&&) = 0;
    virtual void isUserVerifyingPlatformAuthenticatorAvailable(QueryCompletionHandler&&) = 0;

    // Receivers.
    void requestReply(const WebCore::PublicKeyCredentialData&, const WebCore::ExceptionData&);
    void isUserVerifyingPlatformAuthenticatorAvailableReply(uint64_t messageId, bool);

protected:
    // Only one request is allowed at one time. It returns false whenever there is an existing pending request.
    // And invokes the provided handler with NotAllowedError.
    bool setRequestCompletionHandler(RequestCompletionHandler&&);
    uint64_t addQueryCompletionHandler(QueryCompletionHandler&&);

private:
    RequestCompletionHandler m_pendingCompletionHandler;
    uint64_t m_accumulatedMessageId { 1 };
    HashMap<uint64_t, QueryCompletionHandler> m_pendingQueryCompletionHandlers;
};

} // namespace WebCore

#endif // ENABLE(WEB_AUTHN)
