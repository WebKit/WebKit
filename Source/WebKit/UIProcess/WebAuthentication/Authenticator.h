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

#include "WebAuthenticationFlags.h"
#include "WebAuthenticationRequestData.h"
#include <WebCore/AuthenticatorResponse.h>
#include <WebCore/ExceptionData.h>
#include <wtf/AbstractRefCountedAndCanMakeWeakPtr.h>
#include <wtf/Forward.h>
#include <wtf/RefCounted.h>
#include <wtf/WeakPtr.h>
#include <wtf/spi/cocoa/SecuritySPI.h>

OBJC_CLASS LAContext;

namespace WebCore {
class AuthenticatorAssertionResponse;
}

namespace WebKit {

class Authenticator;
using AuthenticatorObserverRespond = std::variant<Ref<WebCore::AuthenticatorResponse>, WebCore::ExceptionData>;

class AuthenticatorObserver : public AbstractRefCountedAndCanMakeWeakPtr<AuthenticatorObserver> {
public:
    virtual ~AuthenticatorObserver() = default;
    virtual void respondReceived(AuthenticatorObserverRespond&&) = 0;
    virtual void downgrade(Authenticator* id, Ref<Authenticator>&& downgradedAuthenticator) = 0;
    virtual void authenticatorStatusUpdated(WebAuthenticationStatus) = 0;
    virtual void requestPin(uint64_t retries, CompletionHandler<void(const WTF::String&)>&&) = 0;
    virtual void selectAssertionResponse(Vector<Ref<WebCore::AuthenticatorAssertionResponse>>&&, WebAuthenticationSource, CompletionHandler<void(WebCore::AuthenticatorAssertionResponse*)>&&) = 0;
    virtual void decidePolicyForLocalAuthenticator(CompletionHandler<void(LocalAuthenticatorPolicy)>&&) = 0;
    virtual void requestLAContextForUserVerification(CompletionHandler<void(LAContext *)>&&) = 0;
    virtual void cancelRequest() = 0;
};

class Authenticator : public RefCountedAndCanMakeWeakPtr<Authenticator> {
public:
    virtual ~Authenticator() = default;

    void setObserver(AuthenticatorObserver& observer) { m_observer = observer; }

    // This operation is guaranteed to execute asynchronously.
    void handleRequest(const WebAuthenticationRequestData&);

protected:
    Authenticator() = default;

    AuthenticatorObserver* observer() const { return m_observer.get(); }
    const WebAuthenticationRequestData& requestData() const { return m_pendingRequestData; }

    void receiveRespond(AuthenticatorObserverRespond&&) const;

private:
    virtual void makeCredential() = 0;
    virtual void getAssertion() = 0;

    WeakPtr<AuthenticatorObserver> m_observer;
    WebAuthenticationRequestData m_pendingRequestData;
};

} // namespace WebKit

#endif // ENABLE(WEB_AUTHN)
