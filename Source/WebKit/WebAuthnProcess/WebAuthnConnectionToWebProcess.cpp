/*
 * Copyright (C) 2020-2021 Apple Inc. All rights reserved.
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

#include "config.h"
#include "WebAuthnConnectionToWebProcess.h"

#if ENABLE(WEB_AUTHN)

#include "AuthenticatorManager.h"
#include "LocalService.h"
#include "WebAuthnProcess.h"
#include <WebCore/AuthenticatorResponseData.h>

#if ENABLE(IPC_TESTING_API)
#include "IPCTesterMessages.h"
#endif

namespace WebKit {
using namespace WebCore;

Ref<WebAuthnConnectionToWebProcess> WebAuthnConnectionToWebProcess::create(WebAuthnProcess& WebAuthnProcess, WebCore::ProcessIdentifier webProcessIdentifier, IPC::Connection::Identifier connectionIdentifier)
{
    return adoptRef(*new WebAuthnConnectionToWebProcess(WebAuthnProcess, webProcessIdentifier, connectionIdentifier));
}

WebAuthnConnectionToWebProcess::WebAuthnConnectionToWebProcess(WebAuthnProcess& WebAuthnProcess, WebCore::ProcessIdentifier webProcessIdentifier, IPC::Connection::Identifier connectionIdentifier)
    : m_connection(IPC::Connection::createServerConnection(connectionIdentifier, *this))
    , m_WebAuthnProcess(WebAuthnProcess)
    , m_webProcessIdentifier(webProcessIdentifier)
{
    RELEASE_ASSERT(RunLoop::isMain());
    m_connection->open();
}

WebAuthnConnectionToWebProcess::~WebAuthnConnectionToWebProcess()
{
    RELEASE_ASSERT(RunLoop::isMain());

    m_connection->invalidate();
}

void WebAuthnConnectionToWebProcess::didClose(IPC::Connection&)
{
}

void WebAuthnConnectionToWebProcess::didReceiveInvalidMessage(IPC::Connection& connection, IPC::MessageName messageName)
{
    WTFLogAlways("Received an invalid message \"%s\" from the web process.\n", description(messageName));
    CRASH();
}

bool WebAuthnConnectionToWebProcess::dispatchMessage(IPC::Connection& connection, IPC::Decoder& decoder)
{
#if ENABLE(IPC_TESTING_API)
    if (decoder.messageReceiverName() == Messages::IPCTester::messageReceiverName()) {
        m_ipcTester.didReceiveMessage(connection, decoder);
        return true;
    }
#endif
    return false;
}

bool WebAuthnConnectionToWebProcess::dispatchSyncMessage(IPC::Connection& connection, IPC::Decoder& decoder, UniqueRef<IPC::Encoder>& replyEncoder)
{
#if ENABLE(IPC_TESTING_API)
    if (decoder.messageReceiverName() == Messages::IPCTester::messageReceiverName()) {
        m_ipcTester.didReceiveSyncMessage(connection, decoder, replyEncoder);
        return true;
    }
#endif
    return false;
}


void WebAuthnConnectionToWebProcess::makeCredential(Vector<uint8_t>&& hash, PublicKeyCredentialCreationOptions&& options, bool processingUserGesture, RequestCompletionHandler&& handler)
{
    handleRequest({ WTFMove(hash), WTFMove(options), nullptr, WebAuthenticationPanelResult::Unavailable, nullptr, std::nullopt, { }, processingUserGesture, String(), nullptr, std::nullopt }, WTFMove(handler));
}

void WebAuthnConnectionToWebProcess::getAssertion(Vector<uint8_t>&& hash, PublicKeyCredentialRequestOptions&& options, bool processingUserGesture, RequestCompletionHandler&& handler)
{
    handleRequest({ WTFMove(hash), WTFMove(options), nullptr, WebAuthenticationPanelResult::Unavailable, nullptr, std::nullopt, { }, processingUserGesture, String(), nullptr, std::nullopt }, WTFMove(handler));
}

void WebAuthnConnectionToWebProcess::handleRequest(WebAuthenticationRequestData&& data, RequestCompletionHandler&& handler)
{
    auto callback = [handler = WTFMove(handler)] (std::variant<Ref<AuthenticatorResponse>, ExceptionData>&& result) mutable {
        ASSERT(RunLoop::isMain());
        WTF::switchOn(result, [&](const Ref<AuthenticatorResponse>& response) {
            handler(response->data(), response->attachment(), { });
        }, [&](const ExceptionData& exception) {
            handler({ }, static_cast<AuthenticatorAttachment>(0), exception);
        });
    };
    m_WebAuthnProcess->authenticatorManager().handleRequest(WTFMove(data), WTFMove(callback));
}

void WebAuthnConnectionToWebProcess::isUserVerifyingPlatformAuthenticatorAvailable(QueryCompletionHandler&& handler)
{
    handler(LocalService::isAvailable());
}

void WebAuthnConnectionToWebProcess::setMockWebAuthenticationConfiguration(WebCore::MockWebAuthenticationConfiguration&& configuration)
{
    m_WebAuthnProcess->setMockWebAuthenticationConfiguration(WTFMove(configuration));
}

} // namespace WebKit

#endif // ENABLE(WEB_AUTHN)
