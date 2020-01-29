/*
 * Copyright (C) 2019-2020 Apple Inc. All rights reserved.
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
#include "GPUProcessSessionParameters.h"
#include "Logging.h"
#include "WebPageMessages.h"
#include "WebPageProxy.h"
#include "WebProcessMessages.h"
#include "WebProcessPool.h"
#include "WebProcessProxy.h"
#include "WebProcessProxyMessages.h"
#include <WebCore/MockRealtimeMediaSourceCenter.h>
#include <WebCore/RuntimeApplicationChecks.h>
#include <wtf/CompletionHandler.h>

#if PLATFORM(IOS_FAMILY)
#include <wtf/spi/darwin/XPCSPI.h>
#endif

#define MESSAGE_CHECK(assertion) MESSAGE_CHECK_BASE(assertion, this->connection())

namespace WebKit {
using namespace WebCore;

static inline bool isSafari()
{
    bool isSafari = false;
#if PLATFORM(IOS_FAMILY)
    if (IOSApplication::isMobileSafari())
        isSafari = true;
#elif PLATFORM(MAC)
    if (MacApplication::isSafari())
        isSafari = true;
#endif
    return isSafari;
}

static inline bool shouldCreateCameraSandboxExtension()
{
    // FIXME: We should check for "com.apple.security.device.camera" entitlement.
    if (!isSafari())
        return false;
    return true;
}

static inline bool shouldCreateMicrophoneSandboxExtension()
{
    // FIXME: We should check for "com.apple.security.device.microphone" entitlement.
    if (!isSafari())
        return false;
    return true;
}

GPUProcessProxy* GPUProcessProxy::m_singleton = nullptr;

GPUProcessProxy& GPUProcessProxy::singleton()
{
    ASSERT(RunLoop::isMain());

    static std::once_flag onceFlag;
    static LazyNeverDestroyed<GPUProcessProxy> gpuProcess;

    std::call_once(onceFlag, [] {
        gpuProcess.construct();

        GPUProcessCreationParameters parameters;
#if ENABLE(MEDIA_STREAM)
        parameters.useMockCaptureDevices = gpuProcess->m_useMockCaptureDevices;

        bool needsCameraSandboxExtension = shouldCreateCameraSandboxExtension();
        bool needsMicrophoneSandboxExtension = shouldCreateMicrophoneSandboxExtension();
        if (needsCameraSandboxExtension)
            SandboxExtension::createHandleForGenericExtension("com.apple.webkit.camera", parameters.cameraSandboxExtensionHandle);
        if (needsMicrophoneSandboxExtension)
            SandboxExtension::createHandleForGenericExtension("com.apple.webkit.microphone", parameters.microphoneSandboxExtensionHandle);
#if PLATFORM(IOS)
        if (needsCameraSandboxExtension || needsMicrophoneSandboxExtension)
            SandboxExtension::createHandleForGenericExtension("com.apple.tccd", parameters.tccSandboxExtensionHandle);
#endif
#endif
        // Initialize the GPU process.
        gpuProcess->send(Messages::GPUProcess::InitializeGPUProcess(parameters), 0);
        gpuProcess->updateProcessAssertion();

        m_singleton = &gpuProcess.get();
    });

    return gpuProcess.get();
}

GPUProcessProxy::GPUProcessProxy()
    : AuxiliaryProcessProxy()
    , m_throttler(*this, false)
#if ENABLE(MEDIA_STREAM)
    , m_useMockCaptureDevices(MockRealtimeMediaSourceCenter::mockRealtimeMediaSourceCenterEnabled())
#endif
{
    connect();
}

GPUProcessProxy::~GPUProcessProxy()
{
    for (auto& request : m_connectionRequests.values())
        request.reply({ });
}

#if ENABLE(MEDIA_STREAM)
void GPUProcessProxy::setUseMockCaptureDevices(bool value)
{
    if (value == m_useMockCaptureDevices)
        return;
    m_useMockCaptureDevices = value;
    send(Messages::GPUProcess::SetMockCaptureDevicesEnabled { m_useMockCaptureDevices }, 0);
}
#endif

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
    addSession(webProcessProxy.websiteDataStore());

    auto& connection = *this->connection();

    connection.sendWithAsyncReply(Messages::GPUProcess::CreateGPUConnectionToWebProcess { webProcessProxy.coreProcessIdentifier(), webProcessProxy.sessionID() }, [this, weakThis = makeWeakPtr(this), webProcessProxy = makeWeakPtr(webProcessProxy), connectionRequestIdentifier](auto&& connectionIdentifier) mutable {
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
        request.reply(GPUProcessConnectionInfo { IPC::Attachment { connectionIdentifier->port(), MACH_MSG_TYPE_MOVE_SEND }, this->connection()->getAuditToken() });
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
    logInvalidMessage(connection, messageReceiverName, messageName);

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

static inline GPUProcessSessionParameters gpuProcessSessionParameters(const WebsiteDataStore& store)
{
    GPUProcessSessionParameters parameters;

    parameters.mediaCacheDirectory = store.resolvedMediaCacheDirectory();
    SandboxExtension::Handle mediaCacheDirectoryExtensionHandle;
    if (!parameters.mediaCacheDirectory.isEmpty())
        SandboxExtension::createHandleWithoutResolvingPath(parameters.mediaCacheDirectory, SandboxExtension::Type::ReadWrite, parameters.mediaCacheDirectorySandboxExtensionHandle);

#if ENABLE(LEGACY_ENCRYPTED_MEDIA)
    parameters.mediaKeysStorageDirectory = store.resolvedMediaKeysDirectory();
    SandboxExtension::Handle mediaKeysStorageDirectorySandboxExtensionHandle;
    if (!parameters.mediaKeysStorageDirectory.isEmpty())
        SandboxExtension::createHandleWithoutResolvingPath(parameters.mediaKeysStorageDirectory, SandboxExtension::Type::ReadWrite, parameters.mediaKeysStorageDirectorySandboxExtensionHandle);
#endif

    return parameters;
}

void GPUProcessProxy::addSession(const WebsiteDataStore& store)
{
    if (!canSendMessage())
        return;

    if (m_sessionIDs.contains(store.sessionID()))
        return;

    send(Messages::GPUProcess::AddSession { store.sessionID(), gpuProcessSessionParameters(store) }, 0);
    m_sessionIDs.add(store.sessionID());
}

void GPUProcessProxy::removeSession(PAL::SessionID sessionID)
{
    if (!canSendMessage())
        return;

    if (m_sessionIDs.remove(sessionID))
        send(Messages::GPUProcess::RemoveSession { sessionID }, 0);
}

} // namespace WebKit

#undef MESSAGE_CHECK

#endif // ENABLE(GPU_PROCESS)
