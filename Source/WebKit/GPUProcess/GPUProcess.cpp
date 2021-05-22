/*
 * Copyright (C) 2019-2021 Apple Inc. All rights reserved.
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
#include "GPUProcessConnectionParameters.h"
#include "GPUProcessCreationParameters.h"
#include "GPUProcessSessionParameters.h"
#include "Logging.h"
#include "SandboxExtension.h"
#include "WebPageProxyMessages.h"
#include "WebProcessPoolMessages.h"
#include <WebCore/DeprecatedGlobalSettings.h>
#include <WebCore/LogInitialization.h>
#include <WebCore/MemoryRelease.h>
#include <WebCore/NowPlayingManager.h>
#include <WebCore/RuntimeApplicationChecks.h>
#include <WebCore/RuntimeEnabledFeatures.h>
#include <wtf/Algorithms.h>
#include <wtf/CallbackAggregator.h>
#include <wtf/MemoryPressureHandler.h>
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

#if HAVE(CGIMAGESOURCE_WITH_SET_ALLOWABLE_TYPES)
#include <pal/spi/cg/ImageIOSPI.h>
#endif

namespace WebKit {
using namespace WebCore;

// We wouldn't want the GPUProcess to repeatedly exit then relaunch when under memory pressure. In particular, we need to make sure the
// WebProcess has a change to schedule work after the GPUProcess get launched. For this reason, we make sure that the GPUProcess never
// idle-exits less than 5 seconds after getting launched. This amount of time should be sufficient for the WebProcess to schedule work
// work in the GPUProcess.
constexpr Seconds minimumLifetimeBeforeIdleExit { 5_s };

GPUProcess::GPUProcess(AuxiliaryProcessInitializationParameters&& parameters)
    : m_idleExitTimer(*this, &GPUProcess::tryExitIfUnused)
{
    initialize(WTFMove(parameters));
    RELEASE_LOG(Process, "%p - GPUProcess::GPUProcess:", this);
}

GPUProcess::~GPUProcess()
{
}

void GPUProcess::createGPUConnectionToWebProcess(ProcessIdentifier identifier, PAL::SessionID sessionID, GPUProcessConnectionParameters&& parameters, CompletionHandler<void(Optional<IPC::Attachment>&&)>&& completionHandler)
{
    RELEASE_LOG(Process, "%p - GPUProcess::createGPUConnectionToWebProcess: processIdentifier=%" PRIu64, this, identifier.toUInt64());
    auto ipcConnection = createIPCConnectionPair();
    if (!ipcConnection) {
        completionHandler({ });
        return;
    }

    auto newConnection = GPUConnectionToWebProcess::create(*this, identifier, ipcConnection->first, sessionID, WTFMove(parameters));

#if ENABLE(MEDIA_STREAM)
    // FIXME: We should refactor code to go from WebProcess -> GPUProcess -> UIProcess when getUserMedia is called instead of going from WebProcess -> UIProcess directly.
    auto access = m_mediaCaptureAccessMap.take(identifier);
    newConnection->updateCaptureAccess(access.allowAudioCapture, access.allowVideoCapture, access.allowDisplayCapture);
    newConnection->setOrientationForMediaCapture(m_orientation);
#endif

#if ENABLE(IPC_TESTING_API)
    if (parameters.ignoreInvalidMessageForTesting)
        newConnection->connection().setIgnoreInvalidMessageForTesting();
#endif

    ASSERT(!m_webProcessConnections.contains(identifier));
    m_webProcessConnections.add(identifier, WTFMove(newConnection));

    completionHandler(WTFMove(ipcConnection->second));
}

void GPUProcess::removeGPUConnectionToWebProcess(GPUConnectionToWebProcess& connection)
{
    RELEASE_LOG(Process, "%p - GPUProcess::removeGPUConnectionToWebProcess: processIdentifier=%" PRIu64, this, connection.webProcessIdentifier().toUInt64());
    ASSERT(m_webProcessConnections.contains(connection.webProcessIdentifier()));
    m_webProcessConnections.remove(connection.webProcessIdentifier());
    tryExitIfUnusedAndUnderMemoryPressure();
}

void GPUProcess::connectionToWebProcessClosed(IPC::Connection& connection)
{
}

bool GPUProcess::shouldTerminate()
{
    return m_webProcessConnections.isEmpty();
}

bool GPUProcess::canExitUnderMemoryPressure() const
{
    ASSERT(isMainRunLoop());
    for (auto& webProcessConnection : m_webProcessConnections.values()) {
        if (!webProcessConnection->allowsExitUnderMemoryPressure())
            return false;
    }
    return true;
}

void GPUProcess::tryExitIfUnusedAndUnderMemoryPressure()
{
    ASSERT(isMainRunLoop());
    if (!MemoryPressureHandler::singleton().isUnderMemoryPressure())
        return;

    tryExitIfUnused();
}

void GPUProcess::tryExitIfUnused()
{
    ASSERT(isMainRunLoop());
    if (!canExitUnderMemoryPressure()) {
        m_idleExitTimer.stop();
        return;
    }

    // To avoid exiting the GPUProcess too aggressively while under memory pressure and make sure the WebProcess gets a
    // change to schedule work, we don't exit if we've been running for less than |minimumLifetimeBeforeIdleExit|.
    // In case of simulated memory pressure, we ignore this rule to avoid flakiness in our benchmarks and tests.
    auto lifetime = MonotonicTime::now() - m_creationTime;
    if (lifetime < minimumLifetimeBeforeIdleExit && !MemoryPressureHandler::singleton().isSimulatingMemoryPressure()) {
        RELEASE_LOG(Process, "GPUProcess::tryExitIfUnused: GPUProcess is idle and under memory pressure but it is not exiting because it has just launched");
        // Check again after the process have lived long enough (minimumLifetimeBeforeIdleExit) to see if the GPUProcess
        // can idle-exit then.
        if (!m_idleExitTimer.isActive())
            m_idleExitTimer.startOneShot(minimumLifetimeBeforeIdleExit - lifetime);
        return;
    }
    m_idleExitTimer.stop();

    RELEASE_LOG(Process, "GPUProcess::tryExitIfUnused: GPUProcess is exiting because we are under memory pressure and the process is no longer useful.");
    parentProcessConnection()->send(Messages::GPUProcessProxy::ProcessIsReadyToExit(), 0);
}

void GPUProcess::lowMemoryHandler(Critical critical, Synchronous synchronous)
{
    RELEASE_LOG(Process, "GPUProcess::lowMemoryHandler: critical=%d, synchronous=%d", critical == Critical::Yes, synchronous == Synchronous::Yes);
    tryExitIfUnused();

    WebCore::releaseGraphicsMemory(critical, synchronous);
}

void GPUProcess::initializeGPUProcess(GPUProcessCreationParameters&& parameters)
{
    RELEASE_LOG(Process, "%p - GPUProcess::initializeGPUProcess:", this);
    WTF::Thread::setCurrentThreadIsUserInitiated();
    AtomString::init();

    auto& memoryPressureHandler = MemoryPressureHandler::singleton();
    memoryPressureHandler.setLowMemoryHandler([this] (Critical critical, Synchronous synchronous) {
        lowMemoryHandler(critical, synchronous);
    });
    memoryPressureHandler.install();

#if PLATFORM(IOS_FAMILY) || ENABLE(ROUTING_ARBITRATION)
    DeprecatedGlobalSettings::setShouldManageAudioSessionCategory(true);
#endif

#if ENABLE(MEDIA_STREAM)
    setMockCaptureDevicesEnabled(parameters.useMockCaptureDevices);
#if PLATFORM(MAC)
    SandboxExtension::consumePermanently(parameters.microphoneSandboxExtensionHandle);
#endif
#endif // ENABLE(MEDIA_STREAM)

#if USE(SANDBOX_EXTENSIONS_FOR_CACHE_AND_TEMP_DIRECTORY_ACCESS)
    SandboxExtension::consumePermanently(parameters.containerCachesDirectoryExtensionHandle);
    SandboxExtension::consumePermanently(parameters.containerTemporaryDirectoryExtensionHandle);
#endif
#if PLATFORM(IOS_FAMILY)
    SandboxExtension::consumePermanently(parameters.compilerServiceExtensionHandles);
    SandboxExtension::consumePermanently(parameters.dynamicIOKitExtensionHandles);
    SandboxExtension::consumePermanently(parameters.dynamicMachExtensionHandles);
#endif

#if HAVE(CGIMAGESOURCE_WITH_SET_ALLOWABLE_TYPES)
    auto emptyArray = adoptCF(CFArrayCreate(kCFAllocatorDefault, nullptr, 0, &kCFTypeArrayCallBacks));
    CGImageSourceSetAllowableTypes(emptyArray.get());
#endif

    // Match the QoS of the UIProcess since the GPU process is doing rendering on its behalf.
    WTF::Thread::setCurrentThreadIsUserInteractive(0);

    WebCore::setPresentingApplicationPID(parameters.parentPID);
}

void GPUProcess::prepareToSuspend(bool isSuspensionImminent, CompletionHandler<void()>&& completionHandler)
{
    RELEASE_LOG(ProcessSuspension, "%p - GPUProcess::prepareToSuspend(), isSuspensionImminent: %d", this, isSuspensionImminent);

    lowMemoryHandler(Critical::Yes, Synchronous::Yes);
    completionHandler();
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
    m_orientation = orientation;
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

void GPUProcess::updateSandboxAccess(const Vector<SandboxExtension::Handle>& extensions)
{
    for (auto& extension : extensions)
        SandboxExtension::consumePermanently(extension);
}

void GPUProcess::addMockMediaDevice(const WebCore::MockMediaDevice& device)
{
    MockRealtimeMediaSourceCenter::addDevice(device);
}

void GPUProcess::clearMockMediaDevices()
{
    MockRealtimeMediaSourceCenter::setDevices({ });
}

void GPUProcess::removeMockMediaDevice(const String& persistentId)
{
    MockRealtimeMediaSourceCenter::removeDevice(persistentId);
}

void GPUProcess::resetMockMediaDevices()
{
    MockRealtimeMediaSourceCenter::resetDevices();
}
#endif

#if PLATFORM(MAC)
void GPUProcess::displayConfigurationChanged(CGDirectDisplayID displayID, CGDisplayChangeSummaryFlags flags)
{
    for (auto& connection : m_webProcessConnections.values())
        connection->displayConfigurationChanged(displayID, flags);
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
WorkQueue& GPUProcess::videoMediaStreamTrackRendererQueue()
{
    if (!m_videoMediaStreamTrackRendererQueue)
        m_videoMediaStreamTrackRendererQueue = WorkQueue::create("RemoteVideoMediaStreamTrackRenderer", WorkQueue::Type::Serial, WorkQueue::QOS::UserInitiated);
    return *m_videoMediaStreamTrackRendererQueue;
}
#endif

#if USE(LIBWEBRTC) && PLATFORM(COCOA)
WorkQueue& GPUProcess::libWebRTCCodecsQueue()
{
    if (!m_libWebRTCCodecsQueue)
        m_libWebRTCCodecsQueue = WorkQueue::create("LibWebRTCCodecsQueue", WorkQueue::Type::Serial, WorkQueue::QOS::UserInitiated);
    return *m_libWebRTCCodecsQueue;
}
#endif

#if ENABLE(VP9)
void GPUProcess::enableVP9Decoders(bool shouldEnableVP8Decoder, bool shouldEnableVP9Decoder, bool shouldEnableVP9SWDecoder)
{
    if (shouldEnableVP9Decoder && !m_enableVP9Decoder) {
        m_enableVP9Decoder = true;
#if PLATFORM(COCOA)
        WebCore::registerSupplementalVP9Decoder();
#endif
    }
    if (shouldEnableVP8Decoder && !m_enableVP8Decoder) {
        m_enableVP8Decoder = true;
#if PLATFORM(COCOA)
        WebCore::registerWebKitVP8Decoder();
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

#if ENABLE(MEDIA_SOURCE) && ENABLE(VP9)
void GPUProcess::setWebMParserEnabled(bool enabled)
{
    if (m_webMParserEnabled == enabled)
        return;
    m_webMParserEnabled = enabled;
    WebCore::RuntimeEnabledFeatures::sharedFeatures().setWebMParserEnabled(m_webMParserEnabled);
}
#endif

#if ENABLE(WEBM_FORMAT_READER)
void GPUProcess::setWebMFormatReaderEnabled(bool enabled)
{
    if (m_webMFormatReaderEnabled == enabled)
        return;
    m_webMFormatReaderEnabled = enabled;
    PlatformMediaSessionManager::setWebMFormatReaderEnabled(m_webMParserEnabled);
}
#endif

#if ENABLE(OPUS)
void GPUProcess::setOpusDecoderEnabled(bool enabled)
{
    if (m_opusEnabled == enabled)
        return;
    m_opusEnabled = enabled;
    PlatformMediaSessionManager::setOpusDecoderEnabled(m_opusEnabled);
}
#endif

#if ENABLE(VORBIS)
void GPUProcess::setVorbisDecoderEnabled(bool enabled)
{
    if (m_vorbisEnabled == enabled)
        return;
    m_vorbisEnabled = enabled;
    PlatformMediaSessionManager::setVorbisDecoderEnabled(m_vorbisEnabled);
}
#endif

} // namespace WebKit

#endif // ENABLE(GPU_PROCESS)
