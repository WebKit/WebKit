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
#include "WebAuthnProcess.h"

#if ENABLE(WEB_AUTHN)

#include "Logging.h"
#include "MockAuthenticatorManager.h"
#include "WebAuthnConnectionToWebProcess.h"
#include "WebAuthnProcessCreationParameters.h"
#include <wtf/text/AtomString.h>

namespace WebKit {
using namespace WebCore;


WebAuthnProcess::WebAuthnProcess(AuxiliaryProcessInitializationParameters&& parameters)
    : m_authenticatorManager(makeUniqueRef<AuthenticatorManager>())
{
    initialize(WTFMove(parameters));
    m_authenticatorManager->enableModernWebAuthentication();
}

WebAuthnProcess::~WebAuthnProcess()
{
}

void WebAuthnProcess::createWebAuthnConnectionToWebProcess(ProcessIdentifier identifier, CompletionHandler<void(std::optional<IPC::Attachment>&&)>&& completionHandler)
{
    auto ipcConnection = createIPCConnectionPair();
    if (!ipcConnection) {
        completionHandler({ });
        return;
    }

    auto newConnection = WebAuthnConnectionToWebProcess::create(*this, identifier, ipcConnection->first);

    ASSERT(!m_webProcessConnections.contains(identifier));
    m_webProcessConnections.add(identifier, WTFMove(newConnection));

    completionHandler(WTFMove(ipcConnection->second));
}

void WebAuthnProcess::removeWebAuthnConnectionToWebProcess(WebAuthnConnectionToWebProcess& connection)
{
    ASSERT(m_webProcessConnections.contains(connection.webProcessIdentifier()));
    m_webProcessConnections.remove(connection.webProcessIdentifier());
}

void WebAuthnProcess::connectionToWebProcessClosed(IPC::Connection& connection)
{
}

bool WebAuthnProcess::shouldTerminate()
{
    return m_webProcessConnections.isEmpty();
}

void WebAuthnProcess::didClose(IPC::Connection&)
{
    ASSERT(RunLoop::isMain());
}

void WebAuthnProcess::lowMemoryHandler(Critical critical)
{
    WTF::releaseFastMallocFreeMemory();
}

void WebAuthnProcess::initializeWebAuthnProcess(WebAuthnProcessCreationParameters&& parameters)
{
    WTF::Thread::setCurrentThreadIsUserInitiated();
    AtomString::init();
}

void WebAuthnProcess::prepareToSuspend(bool isSuspensionImminent, CompletionHandler<void()>&& completionHandler)
{
    RELEASE_LOG(ProcessSuspension, "%p - WebAuthnProcess::prepareToSuspend(), isSuspensionImminent=%d", this, isSuspensionImminent);

    lowMemoryHandler(Critical::Yes);
}

void WebAuthnProcess::processDidResume()
{
    RELEASE_LOG(ProcessSuspension, "%p - WebAuthnProcess::processDidResume()", this);
    resume();
}

void WebAuthnProcess::resume()
{
}

WebAuthnConnectionToWebProcess* WebAuthnProcess::webProcessConnection(ProcessIdentifier identifier) const
{
    return m_webProcessConnections.get(identifier);
}

void WebAuthnProcess::setMockWebAuthenticationConfiguration(WebCore::MockWebAuthenticationConfiguration&& configuration)
{
    if (!m_authenticatorManager->isMock()) {
        m_authenticatorManager = makeUniqueRef<MockAuthenticatorManager>(WTFMove(configuration));
        m_authenticatorManager->enableModernWebAuthentication();
        return;
    }
    static_cast<MockAuthenticatorManager*>(&m_authenticatorManager)->setTestConfiguration(WTFMove(configuration));
}

#if !PLATFORM(COCOA)
void WebAuthnProcess::platformInitializeWebAuthnProcess(const WebAuthnProcessCreationParameters&)
{
}
#endif

} // namespace WebKit

#endif // ENABLE(WEB_AUTHN)
