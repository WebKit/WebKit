/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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

#include "Connection.h"
#include "WebAuthnConnectionToWebProcessMessages.h"
#include <WebCore/ProcessIdentifier.h>
#include <wtf/RefCounted.h>

namespace WebCore {
struct AuthenticatorResponseData;
struct ExceptionData;
struct MockWebAuthenticationConfiguration;
struct PublicKeyCredentialCreationOptions;
struct PublicKeyCredentialRequestOptions;
}

namespace WebKit {

class WebAuthnProcess;
struct WebAuthenticationRequestData;

class WebAuthnConnectionToWebProcess
    : public RefCounted<WebAuthnConnectionToWebProcess>
    , IPC::Connection::Client {
public:
    static Ref<WebAuthnConnectionToWebProcess> create(WebAuthnProcess&, WebCore::ProcessIdentifier, IPC::Connection::Identifier);
    virtual ~WebAuthnConnectionToWebProcess();

    IPC::Connection& connection() { return m_connection.get(); }
    WebAuthnProcess& WebAuthnProcessProcess() { return m_WebAuthnProcess.get(); }
    WebCore::ProcessIdentifier webProcessIdentifier() const { return m_webProcessIdentifier; }

private:
    using RequestCompletionHandler = CompletionHandler<void(const WebCore::AuthenticatorResponseData&, const WebCore::ExceptionData&)>;
    using QueryCompletionHandler = CompletionHandler<void(bool)>;

    WebAuthnConnectionToWebProcess(WebAuthnProcess&, WebCore::ProcessIdentifier, IPC::Connection::Identifier);

    // IPC::Connection::Client
    void didClose(IPC::Connection&) final;
    void didReceiveInvalidMessage(IPC::Connection&, IPC::MessageName) final;
    void didReceiveMessage(IPC::Connection&, IPC::Decoder&) final;

    // Receivers.
    void makeCredential(Vector<uint8_t>&& hash, WebCore::PublicKeyCredentialCreationOptions&&, bool processingUserGesture, RequestCompletionHandler&&);
    void getAssertion(Vector<uint8_t>&& hash, WebCore::PublicKeyCredentialRequestOptions&&, bool processingUserGesture, RequestCompletionHandler&&);
    void isUserVerifyingPlatformAuthenticatorAvailable(QueryCompletionHandler&&);
    void setMockWebAuthenticationConfiguration(WebCore::MockWebAuthenticationConfiguration&&);

    void handleRequest(WebAuthenticationRequestData&&, RequestCompletionHandler&&);

    Ref<IPC::Connection> m_connection;
    Ref<WebAuthnProcess> m_WebAuthnProcess;
    const WebCore::ProcessIdentifier m_webProcessIdentifier;
};

} // namespace WebKit

#endif // ENABLE(WEB_AUTHN)
