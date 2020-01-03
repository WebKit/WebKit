/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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
#include "GPUProcessProxy.h"

#if ENABLE(GPU_PROCESS)

#include "DrawingAreaProxy.h"
#include "GPUProcessConnectionInfo.h"
#include "GPUProcessCreationParameters.h"
#include "GPUProcessMessages.h"
#include "GPUProcessProxyMessages.h"
#include "Logging.h"
#include "WebPageMessages.h"
#include "WebPageProxy.h"
#include "WebProcessMessages.h"
#include "WebProcessPool.h"
#include "WebProcessProxy.h"
#include "WebProcessProxyMessages.h"
#include <WebCore/MockRealtimeMediaSourceCenter.h>
#include <wtf/CompletionHandler.h>

#if PLATFORM(IOS_FAMILY)
#include <wtf/spi/darwin/XPCSPI.h>
#endif

#define MESSAGE_CHECK(assertion) MESSAGE_CHECK_BASE(assertion, connection())

namespace WebKit {
using namespace WebCore;

GPUProcessProxy& GPUProcessProxy::singleton()
{
    ASSERT(RunLoop::isMain());

    static std::once_flag onceFlag;
    static LazyNeverDestroyed<GPUProcessProxy> gpuProcess;

    std::call_once(onceFlag, [] {
        gpuProcess.construct();

        GPUProcessCreationParameters parameters;
#if ENABLE(MEDIA_STREAM)
        parameters.useMockCaptureDevices = MockRealtimeMediaSourceCenter::mockRealtimeMediaSourceCenterEnabled();
#endif
        // Initialize the GPU process.
        gpuProcess->send(Messages::GPUProcess::InitializeGPUProcess(parameters), 0);
        gpuProcess->updateProcessAssertion();
    });

    return gpuProcess.get();
}

GPUProcessProxy::GPUProcessProxy()
    : AuxiliaryProcessProxy()
    , m_throttler(*this, false)
{
    connect();
}

GPUProcessProxy::~GPUProcessProxy()
{
    for (auto& request : m_connectionRequests.values())
        request.reply({ });
}

void GPUProcessProxy::getLaunchOptions(ProcessLauncher::LaunchOptions& launchOptions)
{
    launchOptions.processType = ProcessLauncher::ProcessType::GPU;
    AuxiliaryProcessProxy::getLaunchOptions(launchOptions);
}

void GPUProcessProxy::connectionWillOpen(IPC::Connection&)
{
}

void GPUProcessProxy::processWillShutDown(IPC::Connection& connection)
{
    ASSERT_UNUSED(connection, this->connection() == &connection);
}

void GPUProcessProxy::getGPUProcessConnection(WebProcessProxy& webProcessProxy, Messages::WebProcessProxy::GetGPUProcessConnection::DelayedReply&& reply)
{
    m_connectionRequests.add(++m_connectionRequestIdentifier, ConnectionRequest { makeWeakPtr(webProcessProxy), WTFMove(reply) });
    if (state() == State::Launching)
        return;
    openGPUProcessConnection(m_connectionRequestIdentifier, webProcessProxy);
}

void GPUProcessProxy::openGPUProcessConnection(ConnectionRequestIdentifier connectionRequestIdentifier, WebProcessProxy& webProcessProxy)
{
    connection()->sendWithAsyncReply(Messages::GPUProcess::CreateGPUConnectionToWebProcess { webProcessProxy.coreProcessIdentifier(), webProcessProxy.sessionID() }, [this, weakThis = makeWeakPtr(this), webProcessProxy = makeWeakPtr(webProcessProxy), connectionRequestIdentifier](auto&& connectionIdentifier) mutable {
        if (!weakThis)
            return;

        if (!connectionIdentifier) {
            // GPU process probably crashed, the connection request should have been moved.
            ASSERT(m_connectionRequests.isEmpty());
            return;
        }

        ASSERT(m_connectionRequests.contains(connectionRequestIdentifier));
        auto request = m_connectionRequests.take(connectionRequestIdentifier);

#if USE(UNIX_DOMAIN_SOCKETS) || OS(WINDOWS)
        request.reply(GPUProcessConnectionInfo { WTFMove(*connectionIdentifier) });
#elif OS(DARWIN)
        MESSAGE_CHECK(MACH_PORT_VALID(connectionIdentifier->port()));
        request.reply(GPUProcessConnectionInfo { IPC::Attachment { connectionIdentifier->port(), MACH_MSG_TYPE_MOVE_SEND } });
#else
        notImplemented();
#endif
    }, 0, IPC::SendOption::DispatchMessageEvenWhenWaitingForSyncReply);
}

void GPUProcessProxy::gpuProcessCrashed()
{
    for (auto& processPool : WebProcessPool::allProcessPools())
        processPool->gpuProcessCrashed(processIdentifier());
}

void GPUProcessProxy::didClose(IPC::Connection&)
{
    // This will cause us to be deleted.
    gpuProcessCrashed();
}

void GPUProcessProxy::didReceiveInvalidMessage(IPC::Connection& connection, IPC::StringReference messageReceiverName, IPC::StringReference messageName)
{
    WTFLogAlways("Received an invalid message \"%s.%s\" from the GPU process.\n", messageReceiverName.toString().data(), messageName.toString().data());

    WebProcessPool::didReceiveInvalidMessage(messageReceiverName, messageName);

    // Terminate the GPU process.
    terminate();

    // Since we've invalidated the connection we'll never get a IPC::Connection::Client::didClose
    // callback so we'll explicitly call it here instead.
    didClose(connection);
}

void GPUProcessProxy::didFinishLaunching(ProcessLauncher* launcher, IPC::Connection::Identifier connectionIdentifier)
{
    AuxiliaryProcessProxy::didFinishLaunching(launcher, connectionIdentifier);

    if (!IPC::Connection::identifierIsValid(connectionIdentifier)) {
        gpuProcessCrashed();
        return;
    }

    auto connectionRequests = WTFMove(m_connectionRequests);
    for (auto& connectionRequest : connectionRequests.values()) {
        if (connectionRequest.webProcess)
            getGPUProcessConnection(*connectionRequest.webProcess, WTFMove(connectionRequest.reply));
        else
            connectionRequest.reply({ });
    }
    
#if PLATFORM(IOS_FAMILY)
    if (xpc_connection_t connection = this->connection()->xpcConnection())
        m_throttler.didConnectToProcess(xpc_connection_get_pid(connection));
#endif
}

void GPUProcessProxy::updateProcessAssertion()
{
    bool hasAnyForegroundWebProcesses = false;
    bool hasAnyBackgroundWebProcesses = false;

    for (auto& processPool : WebProcessPool::allProcessPools()) {
        hasAnyForegroundWebProcesses |= processPool->hasForegroundWebProcesses();
        hasAnyBackgroundWebProcesses |= processPool->hasBackgroundWebProcesses();
    }

    if (hasAnyForegroundWebProcesses) {
        if (!ProcessThrottler::isValidForegroundActivity(m_activityFromWebProcesses)) {
            m_activityFromWebProcesses = throttler().foregroundActivity("GPU for foreground view(s)"_s);
            send(Messages::GPUProcess::ProcessDidTransitionToForeground(), 0);
        }
        return;
    }
    if (hasAnyBackgroundWebProcesses) {
        if (!ProcessThrottler::isValidBackgroundActivity(m_activityFromWebProcesses)) {
            m_activityFromWebProcesses = throttler().backgroundActivity("GPU for background view(s)"_s);
            send(Messages::GPUProcess::ProcessDidTransitionToBackground(), 0);
        }
        return;
    }
    m_activityFromWebProcesses = nullptr;
}

} // namespace WebKit

#undef MESSAGE_CHECK

#endif // ENABLE(GPU_PROCESS)
