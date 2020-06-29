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
            SandboxExtension::createHandleForGenericExtension("com.apple.webkit.camera"_s, parameters.cameraSandboxExtensionHandle);
        if (needsMicrophoneSandboxExtension)
            SandboxExtension::createHandleForGenericExtension("com.apple.webkit.microphone"_s, parameters.microphoneSandboxExtensionHandle);
#if PLATFORM(IOS)
        if (needsCameraSandboxExtension || needsMicrophoneSandboxExtension)
            SandboxExtension::createHandleForMachLookup("com.apple.tccd"_s, WTF::nullopt, parameters.tccSandboxExtensionHandle);
#endif
#endif
        parameters.parentPID = getCurrentProcessID();

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

GPUProcessProxy::~GPUProcessProxy() = default;

#if ENABLE(MEDIA_STREAM)
void GPUProcessProxy::setUseMockCaptureDevices(bool value)
{
    if (value == m_useMockCaptureDevices)
        return;
    m_useMockCaptureDevices = value;
    send(Messages::GPUProcess::SetMockCaptureDevicesEnabled { m_useMockCaptureDevices }, 0);
}

void GPUProcessProxy::setOrientationForMediaCapture(uint64_t orientation)
{
    if (m_orientation == orientation)
        return;
    m_orientation = orientation;
    send(Messages::GPUProcess::SetOrientationForMediaCapture { orientation }, 0);
}

void GPUProcessProxy::updateCaptureAccess(bool allowAudioCapture, bool allowVideoCapture, bool allowDisplayCapture, WebCore::ProcessIdentifier processID, CompletionHandler<void()>&& completionHandler)
{
    sendWithAsyncReply(Messages::GPUProcess::UpdateCaptureAccess { allowAudioCapture, allowVideoCapture, allowDisplayCapture, processID }, WTFMove(completionHandler));
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
    addSession(webProcessProxy.websiteDataStore());
#if HAVE(VISIBILITY_PROPAGATION_VIEW)
    if (m_contextIDForVisibilityPropagation)
        webProcessProxy.didCreateContextInGPUProcessForVisibilityPropagation(m_contextIDForVisibilityPropagation);
    else
        m_processesPendingVisibilityPropagationNotification.append(makeWeakPtr(webProcessProxy));
#endif

    RELEASE_LOG(ProcessSuspension, "%p - GPUProcessProxy is taking a background assertion because a web process is requesting a connection", this);
    sendWithAsyncReply(Messages::GPUProcess::CreateGPUConnectionToWebProcess { webProcessProxy.coreProcessIdentifier(), webProcessProxy.sessionID() }, [this, weakThis = makeWeakPtr(*this), reply = WTFMove(reply)](auto&& identifier) mutable {
        if (!weakThis) {
            RELEASE_LOG_ERROR(Process, "GPUProcessProxy::getGPUProcessConnection: GPUProcessProxy deallocated during connection establishment");
            return reply({ });
        }

        if (!identifier) {
            RELEASE_LOG_ERROR(Process, "GPUProcessProxy::getGPUProcessConnection: connection identifier is empty");
            return reply({ });
        }

#if USE(UNIX_DOMAIN_SOCKETS) || OS(WINDOWS)
        reply(GPUProcessConnectionInfo { WTFMove(*identifier) });
        UNUSED_VARIABLE(this);
#elif OS(DARWIN)
        MESSAGE_CHECK(MACH_PORT_VALID(identifier->port()));
        reply(GPUProcessConnectionInfo { IPC::Attachment { identifier->port(), MACH_MSG_TYPE_MOVE_SEND }, this->connection()->getAuditToken() });
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

void GPUProcessProxy::didReceiveInvalidMessage(IPC::Connection& connection, IPC::MessageName messageName)
{
    logInvalidMessage(connection, messageName);

    WebProcessPool::didReceiveInvalidMessage(messageName);

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
    if (!parameters.mediaCacheDirectory.isEmpty()) {
        String parentFolder = parameters.mediaCacheDirectory;
        auto position = parentFolder.reverseFind("/");
        if (position != notFound)
            parentFolder = parentFolder.substring(0, position);
        SandboxExtension::createHandleWithoutResolvingPath(parentFolder, SandboxExtension::Type::ReadWrite, parameters.mediaCacheDirectorySandboxExtensionHandle);
    }

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

#if HAVE(VISIBILITY_PROPAGATION_VIEW)
void GPUProcessProxy::didCreateContextForVisibilityPropagation(LayerHostingContextID contextID)
{
    m_contextIDForVisibilityPropagation = contextID;

    auto processes = WTFMove(m_processesPendingVisibilityPropagationNotification);
    for (auto& process : processes) {
        if (process)
            process->didCreateContextInGPUProcessForVisibilityPropagation(contextID);
    }
}

LayerHostingContextID GPUProcessProxy::contextIDForVisibilityPropagation() const
{
    return m_contextIDForVisibilityPropagation;
}
#endif

} // namespace WebKit

#undef MESSAGE_CHECK

#endif // ENABLE(GPU_PROCESS)
