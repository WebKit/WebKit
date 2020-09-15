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
#include "GPUProcess.h"

#if ENABLE(GPU_PROCESS)

#include "ArgumentCoders.h"
#include "Attachment.h"
#include "AuxiliaryProcessMessages.h"
#include "DataReference.h"
#include "GPUConnectionToWebProcess.h"
#include "GPUProcessCreationParameters.h"
#include "GPUProcessSessionParameters.h"
#include "Logging.h"
#include "SandboxExtension.h"
#include "WebPageProxyMessages.h"
#include "WebProcessPoolMessages.h"
#include <WebCore/DeprecatedGlobalSettings.h>
#include <WebCore/LogInitialization.h>
#include <WebCore/NowPlayingManager.h>
#include <WebCore/RuntimeApplicationChecks.h>
#include <wtf/Algorithms.h>
#include <wtf/CallbackAggregator.h>
#include <wtf/OptionSet.h>
#include <wtf/ProcessPrivilege.h>
#include <wtf/RunLoop.h>
#include <wtf/UniqueRef.h>
#include <wtf/text/AtomString.h>

#if USE(AUDIO_SESSION)
#include "RemoteAudioSessionProxyManager.h"
#endif

#if ENABLE(MEDIA_STREAM)
#include <WebCore/MockRealtimeMediaSourceCenter.h>
#endif

#if PLATFORM(COCOA)
#include <WebCore/VP9UtilitiesCocoa.h>
#endif

namespace WebKit {
using namespace WebCore;


GPUProcess::GPUProcess(AuxiliaryProcessInitializationParameters&& parameters)
{
    initialize(WTFMove(parameters));
}

GPUProcess::~GPUProcess()
{
}

void GPUProcess::createGPUConnectionToWebProcess(ProcessIdentifier identifier, PAL::SessionID sessionID, CompletionHandler<void(Optional<IPC::Attachment>&&)>&& completionHandler)
{
    auto ipcConnection = createIPCConnectionPair();
    if (!ipcConnection) {
        completionHandler({ });
        return;
    }

    auto newConnection = GPUConnectionToWebProcess::create(*this, identifier, ipcConnection->first, sessionID);

#if ENABLE(MEDIA_STREAM)
    // FIXME: We should refactor code to go from WebProcess -> GPUProcess -> UIProcess when getUserMedia is called instead of going from WebProcess -> UIProcess directly.
    auto access = m_mediaCaptureAccessMap.take(identifier);
    newConnection->updateCaptureAccess(access.allowAudioCapture, access.allowVideoCapture, access.allowDisplayCapture);
#endif

    ASSERT(!m_webProcessConnections.contains(identifier));
    m_webProcessConnections.add(identifier, WTFMove(newConnection));

    completionHandler(WTFMove(ipcConnection->second));
}

void GPUProcess::removeGPUConnectionToWebProcess(GPUConnectionToWebProcess& connection)
{
    ASSERT(m_webProcessConnections.contains(connection.webProcessIdentifier()));
    m_webProcessConnections.remove(connection.webProcessIdentifier());
}

void GPUProcess::connectionToWebProcessClosed(IPC::Connection& connection)
{
}

bool GPUProcess::shouldTerminate()
{
    return m_webProcessConnections.isEmpty();
}

void GPUProcess::didClose(IPC::Connection&)
{
    ASSERT(RunLoop::isMain());
}

void GPUProcess::lowMemoryHandler(Critical critical)
{
    WTF::releaseFastMallocFreeMemory();
}

void GPUProcess::initializeGPUProcess(GPUProcessCreationParameters&& parameters)
{
    WTF::Thread::setCurrentThreadIsUserInitiated();
    AtomString::init();

#if PLATFORM(IOS_FAMILY) || ENABLE(ROUTING_ARBITRATION)
    DeprecatedGlobalSettings::setShouldManageAudioSessionCategory(true);
#endif

#if ENABLE(MEDIA_STREAM)
    setMockCaptureDevicesEnabled(parameters.useMockCaptureDevices);
    SandboxExtension::consumePermanently(parameters.cameraSandboxExtensionHandle);
    SandboxExtension::consumePermanently(parameters.microphoneSandboxExtensionHandle);
#if PLATFORM(IOS)
    SandboxExtension::consumePermanently(parameters.tccSandboxExtensionHandle);
#endif
#endif

#if HAVE(VISIBILITY_PROPAGATION_VIEW)
    m_contextForVisibilityPropagation = LayerHostingContext::createForExternalHostingProcess({
        m_canShowWhileLocked
    });
    send(Messages::GPUProcessProxy::DidCreateContextForVisibilityPropagation(m_contextForVisibilityPropagation->contextID()));
#endif

    WebCore::setPresentingApplicationPID(parameters.parentPID);
}

void GPUProcess::prepareToSuspend(bool isSuspensionImminent, CompletionHandler<void()>&& completionHandler)
{
    RELEASE_LOG(ProcessSuspension, "%p - GPUProcess::prepareToSuspend(), isSuspensionImminent: %d", this, isSuspensionImminent);

    lowMemoryHandler(Critical::Yes);
}

void GPUProcess::processDidResume()
{
    RELEASE_LOG(ProcessSuspension, "%p - GPUProcess::processDidResume()", this);
    resume();
}

void GPUProcess::resume()
{
}

void GPUProcess::processDidTransitionToForeground()
{
}

void GPUProcess::processDidTransitionToBackground()
{
}

GPUConnectionToWebProcess* GPUProcess::webProcessConnection(ProcessIdentifier identifier) const
{
    return m_webProcessConnections.get(identifier);
}

#if ENABLE(MEDIA_STREAM)
void GPUProcess::setMockCaptureDevicesEnabled(bool isEnabled)
{
    MockRealtimeMediaSourceCenter::setMockRealtimeMediaSourceCenterEnabled(isEnabled);
}

void GPUProcess::setOrientationForMediaCapture(uint64_t orientation)
{
    for (auto& connection : m_webProcessConnections.values())
        connection->setOrientationForMediaCapture(orientation);
}

void GPUProcess::updateCaptureAccess(bool allowAudioCapture, bool allowVideoCapture, bool allowDisplayCapture, ProcessIdentifier processID, CompletionHandler<void()>&& completionHandler)
{
    if (auto* connection = webProcessConnection(processID)) {
        connection->updateCaptureAccess(allowAudioCapture, allowVideoCapture, allowDisplayCapture);
        return completionHandler();
    }

    auto& access = m_mediaCaptureAccessMap.add(processID, MediaCaptureAccess { allowAudioCapture, allowVideoCapture, allowDisplayCapture }).iterator->value;
    access.allowAudioCapture |= allowAudioCapture;
    access.allowVideoCapture |= allowVideoCapture;
    access.allowDisplayCapture |= allowDisplayCapture;

    completionHandler();
}
#endif

void GPUProcess::addSession(PAL::SessionID sessionID, GPUProcessSessionParameters&& parameters)
{
    ASSERT(!m_sessions.contains(sessionID));
    SandboxExtension::consumePermanently(parameters.mediaCacheDirectorySandboxExtensionHandle);
#if ENABLE(LEGACY_ENCRYPTED_MEDIA)
    SandboxExtension::consumePermanently(parameters.mediaKeysStorageDirectorySandboxExtensionHandle);
#endif

    m_sessions.add(sessionID, GPUSession {
        WTFMove(parameters.mediaCacheDirectory)
#if ENABLE(LEGACY_ENCRYPTED_MEDIA)
        , WTFMove(parameters.mediaKeysStorageDirectory)
#endif
    });
}

void GPUProcess::removeSession(PAL::SessionID sessionID)
{
    ASSERT(m_sessions.contains(sessionID));
    m_sessions.remove(sessionID);
}

const String& GPUProcess::mediaCacheDirectory(PAL::SessionID sessionID) const
{
    ASSERT(m_sessions.contains(sessionID));
    return m_sessions.find(sessionID)->value.mediaCacheDirectory;
}

#if ENABLE(LEGACY_ENCRYPTED_MEDIA)
const String& GPUProcess::mediaKeysStorageDirectory(PAL::SessionID sessionID) const
{
    ASSERT(m_sessions.contains(sessionID));
    return m_sessions.find(sessionID)->value.mediaKeysStorageDirectory;
}
#endif

NowPlayingManager& GPUProcess::nowPlayingManager()
{
    if (!m_nowPlayingManager)
        m_nowPlayingManager = makeUnique<NowPlayingManager>();
    return *m_nowPlayingManager;
}

#if ENABLE(GPU_PROCESS) && USE(AUDIO_SESSION)
RemoteAudioSessionProxyManager& GPUProcess::audioSessionManager() const
{
    if (!m_audioSessionManager)
        m_audioSessionManager = WTF::makeUnique<RemoteAudioSessionProxyManager>();
    return *m_audioSessionManager;
}
#endif

#if ENABLE(MEDIA_STREAM) && PLATFORM(COCOA)
WorkQueue& GPUProcess::audioMediaStreamTrackRendererQueue()
{
    if (!m_audioMediaStreamTrackRendererQueue)
        m_audioMediaStreamTrackRendererQueue = WorkQueue::create("RemoteAudioMediaStreamTrackRenderer", WorkQueue::Type::Serial, WorkQueue::QOS::UserInteractive);
    return *m_audioMediaStreamTrackRendererQueue;
}
#endif

#if ENABLE(VP9)
void GPUProcess::enableVP9Decoders(bool shouldEnableVP9Decoder, bool shouldEnableVP9SWDecoder)
{
    if (shouldEnableVP9Decoder && !m_enableVP9Decoder) {
        m_enableVP9Decoder = true;
#if PLATFORM(COCOA)
        WebCore::registerSupplementalVP9Decoder();
#endif
    }
    if (shouldEnableVP9SWDecoder && !m_enableVP9SWDecoder) {
        m_enableVP9SWDecoder = true;
#if PLATFORM(COCOA)
        WebCore::registerWebKitVP9Decoder();
#endif
    }
}
#endif

} // namespace WebKit

#endif // ENABLE(GPU_PROCESS)
