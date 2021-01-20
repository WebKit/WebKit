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

#include "config.h"
#include "WebAuthnProcessProxy.h"

#if ENABLE(WEB_AUTHN)

#include "Logging.h"
#include "WebAuthnProcessConnectionInfo.h"
#include "WebAuthnProcessCreationParameters.h"
#include "WebAuthnProcessMessages.h"
#include "WebAuthnProcessProxyMessages.h"
#include "WebPageMessages.h"
#include "WebPageProxy.h"
#include "WebProcessMessages.h"
#include "WebProcessPool.h"
#include "WebProcessProxy.h"
#include "WebProcessProxyMessages.h"
#include <WebCore/RuntimeApplicationChecks.h>
#include <wtf/CompletionHandler.h>

#if PLATFORM(IOS_FAMILY)
#include <wtf/spi/darwin/XPCSPI.h>
#endif

#define MESSAGE_CHECK(assertion) MESSAGE_CHECK_BASE(assertion, this->connection())

namespace WebKit {
using namespace WebCore;

WebAuthnProcessProxy& WebAuthnProcessProxy::singleton()
{
    ASSERT(RunLoop::isMain());

    static std::once_flag onceFlag;
    static LazyNeverDestroyed<WebAuthnProcessProxy> webAuthnProcess;

    std::call_once(onceFlag, [] {
        webAuthnProcess.construct();

        WebAuthnProcessCreationParameters parameters;

        // Initialize the WebAuthn process.
        webAuthnProcess->send(Messages::WebAuthnProcess::InitializeWebAuthnProcess(parameters), 0);
        webAuthnProcess->updateProcessAssertion();
    });

    return webAuthnProcess.get();
}

WebAuthnProcessProxy::WebAuthnProcessProxy()
    : AuxiliaryProcessProxy()
    , m_throttler(*this, false)
{
    connect();
}

WebAuthnProcessProxy::~WebAuthnProcessProxy() = default;

void WebAuthnProcessProxy::getLaunchOptions(ProcessLauncher::LaunchOptions& launchOptions)
{
    launchOptions.processType = ProcessLauncher::ProcessType::WebAuthn;
    AuxiliaryProcessProxy::getLaunchOptions(launchOptions);
}

void WebAuthnProcessProxy::connectionWillOpen(IPC::Connection&)
{
}

void WebAuthnProcessProxy::processWillShutDown(IPC::Connection& connection)
{
    ASSERT_UNUSED(connection, this->connection() == &connection);
}

void WebAuthnProcessProxy::getWebAuthnProcessConnection(WebProcessProxy& webProcessProxy, Messages::WebProcessProxy::GetWebAuthnProcessConnection::DelayedReply&& reply)
{
    RELEASE_LOG(ProcessSuspension, "%p - WebAuthnProcessProxy is taking a background assertion because a web process is requesting a connection", this);
    sendWithAsyncReply(Messages::WebAuthnProcess::CreateWebAuthnConnectionToWebProcess { webProcessProxy.coreProcessIdentifier() }, [this, weakThis = makeWeakPtr(*this), reply = WTFMove(reply)](auto&& identifier) mutable {
        if (!weakThis) {
            RELEASE_LOG_ERROR(Process, "WebAuthnProcessProxy::getWebAuthnProcessConnection: WebAuthnProcessProxy deallocated during connection establishment");
            return reply({ });
        }

        if (!identifier) {
            RELEASE_LOG_ERROR(Process, "WebAuthnProcessProxy::getWebAuthnProcessConnection: connection identifier is empty");
            return reply({ });
        }

        MESSAGE_CHECK(MACH_PORT_VALID(identifier->port()));
        reply(WebAuthnProcessConnectionInfo { IPC::Attachment { identifier->port(), MACH_MSG_TYPE_MOVE_SEND } });
    }, 0, IPC::SendOption::DispatchMessageEvenWhenWaitingForSyncReply);
}

void WebAuthnProcessProxy::webAuthnProcessCrashed()
{
    for (auto& processPool : WebProcessPool::allProcessPools())
        processPool->terminateAllWebContentProcesses();
}

void WebAuthnProcessProxy::didClose(IPC::Connection&)
{
    // This will cause us to be deleted.
    webAuthnProcessCrashed();
}

void WebAuthnProcessProxy::didReceiveInvalidMessage(IPC::Connection& connection, IPC::MessageName messageName)
{
    logInvalidMessage(connection, messageName);

    WebProcessPool::didReceiveInvalidMessage(messageName);

    // Terminate the WebAuthn process.
    terminate();

    // Since we've invalidated the connection we'll never get a IPC::Connection::Client::didClose
    // callback so we'll explicitly call it here instead.
    didClose(connection);
}

void WebAuthnProcessProxy::didFinishLaunching(ProcessLauncher* launcher, IPC::Connection::Identifier connectionIdentifier)
{
    AuxiliaryProcessProxy::didFinishLaunching(launcher, connectionIdentifier);

    if (!IPC::Connection::identifierIsValid(connectionIdentifier)) {
        webAuthnProcessCrashed();
        return;
    }

#if PLATFORM(IOS_FAMILY)
    if (xpc_connection_t connection = this->connection()->xpcConnection())
        m_throttler.didConnectToProcess(xpc_connection_get_pid(connection));
#endif
}

void WebAuthnProcessProxy::updateProcessAssertion()
{
    bool hasAnyForegroundWebProcesses = false;
    bool hasAnyBackgroundWebProcesses = false;

    for (auto& processPool : WebProcessPool::allProcessPools()) {
        hasAnyForegroundWebProcesses |= processPool->hasForegroundWebProcesses();
        hasAnyBackgroundWebProcesses |= processPool->hasBackgroundWebProcesses();
    }

    if (hasAnyForegroundWebProcesses) {
        if (!ProcessThrottler::isValidForegroundActivity(m_activityFromWebProcesses)) {
            m_activityFromWebProcesses = throttler().foregroundActivity("WebAuthn for foreground view(s)"_s);
            send(Messages::WebAuthnProcess::ProcessDidTransitionToForeground(), 0);
        }
        return;
    }
    if (hasAnyBackgroundWebProcesses) {
        if (!ProcessThrottler::isValidBackgroundActivity(m_activityFromWebProcesses)) {
            m_activityFromWebProcesses = throttler().backgroundActivity("WebAuthn for background view(s)"_s);
            send(Messages::WebAuthnProcess::ProcessDidTransitionToBackground(), 0);
        }
        return;
    }
    m_activityFromWebProcesses = nullptr;
}

} // namespace WebKit

#undef MESSAGE_CHECK

#endif // ENABLE(WEB_AUTHN)
