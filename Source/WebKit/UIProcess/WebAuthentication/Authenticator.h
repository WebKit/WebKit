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
#include <wtf/Forward.h>
#include <wtf/RefCounted.h>
#include <wtf/WeakPtr.h>

namespace WebCore {
class AuthenticatorAssertionResponse;
}

namespace WebKit {

class Authenticator : public RefCounted<Authenticator>, public CanMakeWeakPtr<Authenticator> {
public:
    using Respond = Variant<Ref<WebCore::AuthenticatorResponse>, WebCore::ExceptionData>;

    class Observer : public CanMakeWeakPtr<Observer> {
    public:
        virtual ~Observer() = default;
        virtual void respondReceived(Respond&&) = 0;
        virtual void downgrade(Authenticator* id, Ref<Authenticator>&& downgradedAuthenticator) = 0;
        virtual void authenticatorStatusUpdated(WebAuthenticationStatus) = 0;
        virtual void requestPin(uint64_t retries, CompletionHandler<void(const WTF::String&)>&&) = 0;
        virtual void selectAssertionResponse(const HashSet<Ref<WebCore::AuthenticatorAssertionResponse>>&, CompletionHandler<void(const WebCore::AuthenticatorAssertionResponse&)>&&) = 0;
    };

    virtual ~Authenticator() = default;

    void setObserver(Observer& observer) { m_observer = makeWeakPtr(observer); }

    // This operation is guaranteed to execute asynchronously.
    void handleRequest(const WebAuthenticationRequestData&);

protected:
    Authenticator() = default;

    Observer* observer() const { return m_observer.get(); }
    const WebAuthenticationRequestData& requestData() const { return m_pendingRequestData; }

    void receiveRespond(Respond&&) const;

private:
    virtual void makeCredential() = 0;
    virtual void getAssertion() = 0;

    WeakPtr<Observer> m_observer;
    WebAuthenticationRequestData m_pendingRequestData;
};

} // namespace WebKit

#endif // ENABLE(WEB_AUTHN)
