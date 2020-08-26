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
#include "GPUConnectionToWebProcess.h"

#if ENABLE(GPU_PROCESS)

#include "DataReference.h"
#include "GPUConnectionToWebProcessMessages.h"
#include "GPUProcess.h"
#include "GPUProcessConnectionMessages.h"
#include "GPUProcessMessages.h"
#include "GPUProcessProxyMessages.h"
#include "LibWebRTCCodecsProxy.h"
#include "LibWebRTCCodecsProxyMessages.h"
#include "Logging.h"
#include "RemoteAudioMediaStreamTrackRendererManager.h"
#include "RemoteAudioMediaStreamTrackRendererManagerMessages.h"
#include "RemoteAudioMediaStreamTrackRendererMessages.h"
#include "RemoteMediaPlayerManagerProxy.h"
#include "RemoteMediaPlayerManagerProxyMessages.h"
#include "RemoteMediaPlayerProxy.h"
#include "RemoteMediaPlayerProxyMessages.h"
#include "RemoteMediaRecorderManager.h"
#include "RemoteMediaRecorderManagerMessages.h"
#include "RemoteMediaRecorderMessages.h"
#include "RemoteMediaResourceManager.h"
#include "RemoteMediaResourceManagerMessages.h"
#include "RemoteSampleBufferDisplayLayerManager.h"
#include "RemoteSampleBufferDisplayLayerManagerMessages.h"
#include "RemoteSampleBufferDisplayLayerMessages.h"
#include "RemoteScrollingCoordinatorTransaction.h"
#include "WebCoreArgumentCoders.h"
#include "WebErrors.h"
#include "WebProcessMessages.h"
#include <WebCore/MockRealtimeMediaSourceCenter.h>
#include <WebCore/NowPlayingManager.h>

#if PLATFORM(COCOA)
#include "RemoteLayerTreeDrawingAreaProxyMessages.h"
#include <WebCore/MediaSessionManagerCocoa.h>
#include <WebCore/MediaSessionManagerIOS.h>
#endif

#if ENABLE(ENCRYPTED_MEDIA)
#include "RemoteCDMFactoryProxy.h"
#include "RemoteCDMFactoryProxyMessages.h"
#include "RemoteCDMInstanceProxyMessages.h"
#include "RemoteCDMInstanceSessionProxyMessages.h"
#include "RemoteCDMProxyMessages.h"
#endif

// FIXME: <https://bugs.webkit.org/show_bug.cgi?id=211085>
// UserMediaCaptureManagerProxy should not be platform specific
#if PLATFORM(COCOA) && ENABLE(MEDIA_STREAM)
#include "UserMediaCaptureManagerProxy.h"
#include "UserMediaCaptureManagerProxyMessages.h"
#endif

#if ENABLE(WEB_AUDIO)
#include "RemoteAudioDestinationManager.h"
#include "RemoteAudioDestinationManagerMessages.h"
#endif

#if USE(AUDIO_SESSION)
#include "RemoteAudioSessionProxy.h"
#include "RemoteAudioSessionProxyManager.h"
#include "RemoteAudioSessionProxyMessages.h"
#endif

#if PLATFORM(IOS_FAMILY)
#include "RemoteMediaSessionHelperProxy.h"
#endif

#if ENABLE(LEGACY_ENCRYPTED_MEDIA)
#include "RemoteLegacyCDMFactoryProxy.h"
#include "RemoteLegacyCDMFactoryProxyMessages.h"
#include "RemoteLegacyCDMProxyMessages.h"
#include "RemoteLegacyCDMSessionProxyMessages.h"
#endif

namespace WebKit {
using namespace WebCore;

#if PLATFORM(COCOA) && ENABLE(MEDIA_STREAM)
class GPUProxyForCapture final : public UserMediaCaptureManagerProxy::ConnectionProxy {
    WTF_MAKE_FAST_ALLOCATED;
public:
    explicit GPUProxyForCapture(GPUConnectionToWebProcess& process)
        : m_process(process)
    {
    }

private:
    Logger& logger() final { return m_process.logger(); }
    void addMessageReceiver(IPC::ReceiverName, IPC::MessageReceiver&) final { }
    void removeMessageReceiver(IPC::ReceiverName messageReceiverName) final { }
    IPC::Connection& connection() final { return m_process.connection(); }
    bool willStartCapture(CaptureDevice::DeviceType type) const final
    {
        switch (type) {
        case CaptureDevice::DeviceType::Unknown:
        case CaptureDevice::DeviceType::Speaker:
            return false;
        case CaptureDevice::DeviceType::Microphone:
            return m_process.allowsAudioCapture();
        case CaptureDevice::DeviceType::Camera:
            if (!m_process.allowsVideoCapture())
                return false;
#if PLATFORM(IOS)
            MediaSessionManageriOS::providePresentingApplicationPID();
#endif
            return true;
            break;
        case CaptureDevice::DeviceType::Screen:
            return m_process.allowsDisplayCapture();
        case CaptureDevice::DeviceType::Window:
            return m_process.allowsDisplayCapture();
        }
    }

    GPUConnectionToWebProcess& m_process;
};
#endif

Ref<GPUConnectionToWebProcess> GPUConnectionToWebProcess::create(GPUProcess& gpuProcess, WebCore::ProcessIdentifier webProcessIdentifier, IPC::Connection::Identifier connectionIdentifier, PAL::SessionID sessionID)
{
    return adoptRef(*new GPUConnectionToWebProcess(gpuProcess, webProcessIdentifier, connectionIdentifier, sessionID));
}

GPUConnectionToWebProcess::GPUConnectionToWebProcess(GPUProcess& gpuProcess, WebCore::ProcessIdentifier webProcessIdentifier, IPC::Connection::Identifier connectionIdentifier, PAL::SessionID sessionID)
    : m_connection(IPC::Connection::createServerConnection(connectionIdentifier, *this))
    , m_gpuProcess(gpuProcess)
    , m_webProcessIdentifier(webProcessIdentifier)
    , m_sessionID(sessionID)
{
    RELEASE_ASSERT(RunLoop::isMain());
    m_connection->open();
}

GPUConnectionToWebProcess::~GPUConnectionToWebProcess()
{
    RELEASE_ASSERT(RunLoop::isMain());

    m_connection->invalidate();
}

void GPUConnectionToWebProcess::didClose(IPC::Connection&)
{
#if USE(AUDIO_SESSION)
    if (m_audioSessionProxy) {
        gpuProcess().audioSessionManager().removeProxy(*m_audioSessionProxy);
        m_audioSessionProxy = nullptr;
    }
#endif
}

Logger& GPUConnectionToWebProcess::logger()
{
    if (!m_logger) {
        m_logger = Logger::create(this);
        m_logger->setEnabled(this, m_sessionID.isAlwaysOnLoggingAllowed());
    }

    return *m_logger;
}

void GPUConnectionToWebProcess::didReceiveInvalidMessage(IPC::Connection& connection, IPC::MessageName messageName)
{
    WTFLogAlways("Received an invalid message \"%s\" from the web process.\n", description(messageName));
    CRASH();
}

#if ENABLE(WEB_AUDIO)
RemoteAudioDestinationManager& GPUConnectionToWebProcess::remoteAudioDestinationManager()
{
    if (!m_remoteAudioDestinationManager)
        m_remoteAudioDestinationManager = makeUnique<RemoteAudioDestinationManager>(*this);

    return *m_remoteAudioDestinationManager;
}
#endif

RemoteMediaResourceManager& GPUConnectionToWebProcess::remoteMediaResourceManager()
{
    if (!m_remoteMediaResourceManager)
        m_remoteMediaResourceManager = makeUnique<RemoteMediaResourceManager>();

    return *m_remoteMediaResourceManager;
}

RemoteMediaPlayerManagerProxy& GPUConnectionToWebProcess::remoteMediaPlayerManagerProxy()
{
    if (!m_remoteMediaPlayerManagerProxy)
        m_remoteMediaPlayerManagerProxy = makeUnique<RemoteMediaPlayerManagerProxy>(*this);

    return *m_remoteMediaPlayerManagerProxy;
}

#if PLATFORM(COCOA) && ENABLE(MEDIA_STREAM)
UserMediaCaptureManagerProxy& GPUConnectionToWebProcess::userMediaCaptureManagerProxy()
{
    if (!m_userMediaCaptureManagerProxy)
        m_userMediaCaptureManagerProxy = makeUnique<UserMediaCaptureManagerProxy>(makeUniqueRef<GPUProxyForCapture>(*this));

    return *m_userMediaCaptureManagerProxy;
}

#if HAVE(AVASSETWRITERDELEGATE)
RemoteMediaRecorderManager& GPUConnectionToWebProcess::mediaRecorderManager()
{
    if (!m_remoteMediaRecorderManager)
        m_remoteMediaRecorderManager = makeUnique<RemoteMediaRecorderManager>(*this);

    return *m_remoteMediaRecorderManager;
}
#endif

RemoteAudioMediaStreamTrackRendererManager& GPUConnectionToWebProcess::audioTrackRendererManager()
{
    if (!m_audioTrackRendererManager)
        m_audioTrackRendererManager = makeUnique<RemoteAudioMediaStreamTrackRendererManager>();

    return *m_audioTrackRendererManager;
}

RemoteSampleBufferDisplayLayerManager& GPUConnectionToWebProcess::sampleBufferDisplayLayerManager()
{
    if (!m_sampleBufferDisplayLayerManager)
        m_sampleBufferDisplayLayerManager = makeUnique<RemoteSampleBufferDisplayLayerManager>(m_connection.copyRef());

    return *m_sampleBufferDisplayLayerManager;
}
#endif //  PLATFORM(COCOA) && ENABLE(MEDIA_STREAM)

#if PLATFORM(COCOA) && USE(LIBWEBRTC)
LibWebRTCCodecsProxy& GPUConnectionToWebProcess::libWebRTCCodecsProxy()
{
    if (!m_libWebRTCCodecsProxy)
        m_libWebRTCCodecsProxy = makeUnique<LibWebRTCCodecsProxy>(*this);

    return *m_libWebRTCCodecsProxy;
}
#endif

#if ENABLE(ENCRYPTED_MEDIA)
RemoteCDMFactoryProxy& GPUConnectionToWebProcess::cdmFactoryProxy()
{
    if (!m_cdmFactoryProxy)
        m_cdmFactoryProxy = makeUnique<RemoteCDMFactoryProxy>(*this);

    return *m_cdmFactoryProxy;
}
#endif

#if ENABLE(GPU_PROCESS) && USE(AUDIO_SESSION)
RemoteAudioSessionProxy& GPUConnectionToWebProcess::audioSessionProxy()
{
    if (!m_audioSessionProxy) {
        m_audioSessionProxy = RemoteAudioSessionProxy::create(*this).moveToUniquePtr();
        gpuProcess().audioSessionManager().addProxy(*m_audioSessionProxy);
    }
    return *m_audioSessionProxy;
}
#endif

void GPUConnectionToWebProcess::createRenderingBackend(RenderingBackendIdentifier renderingBackendIdentifier)
{
    auto addResult = m_remoteRenderingBackendProxyMap.ensure(renderingBackendIdentifier, [&]() {
        return RemoteRenderingBackendProxy::create(*this, renderingBackendIdentifier);
    });
    ASSERT_UNUSED(addResult, addResult.isNewEntry);
}

void GPUConnectionToWebProcess::releaseRenderingBackend(RenderingBackendIdentifier renderingBackendIdentifier)
{
    bool found = m_remoteRenderingBackendProxyMap.remove(renderingBackendIdentifier);
    ASSERT_UNUSED(found, found);
}

void GPUConnectionToWebProcess::clearNowPlayingInfo()
{
    gpuProcess().nowPlayingManager().clearNowPlayingInfoClient(*this);
}

void GPUConnectionToWebProcess::setNowPlayingInfo(bool setAsNowPlayingApplication, NowPlayingInfo&& nowPlayingInfo)
{
    gpuProcess().nowPlayingManager().setNowPlayingInfo(*this, WTFMove(nowPlayingInfo));
}

void GPUConnectionToWebProcess::didReceiveRemoteControlCommand(PlatformMediaSession::RemoteControlCommandType type, Optional<double> argument)
{
    connection().send(Messages::GPUProcessConnection::DidReceiveRemoteCommand { type, argument }, 0);
}

#if ENABLE(GPU_PROCESS) && USE(AUDIO_SESSION)
void GPUConnectionToWebProcess::ensureAudioSession(EnsureAudioSessionCompletion&& completion)
{
    completion(audioSessionProxy().configuration());
}
#endif

#if PLATFORM(IOS_FAMILY)
RemoteMediaSessionHelperProxy& GPUConnectionToWebProcess::mediaSessionHelperProxy()
{
    if (!m_mediaSessionHelperProxy)
        m_mediaSessionHelperProxy = makeUnique<RemoteMediaSessionHelperProxy>(*this);
    return *m_mediaSessionHelperProxy;
}

void GPUConnectionToWebProcess::ensureMediaSessionHelper()
{
    mediaSessionHelperProxy();
}
#endif

#if ENABLE(LEGACY_ENCRYPTED_MEDIA)
RemoteLegacyCDMFactoryProxy& GPUConnectionToWebProcess::legacyCdmFactoryProxy()
{
    if (!m_legacyCdmFactoryProxy)
        m_legacyCdmFactoryProxy = makeUnique<RemoteLegacyCDMFactoryProxy>(*this);

    return *m_legacyCdmFactoryProxy;
}
#endif

bool GPUConnectionToWebProcess::dispatchMessage(IPC::Connection& connection, IPC::Decoder& decoder)
{
#if ENABLE(WEB_AUDIO)
    if (decoder.messageReceiverName() == Messages::RemoteAudioDestinationManager::messageReceiverName()) {
        remoteAudioDestinationManager().didReceiveMessageFromWebProcess(connection, decoder);
        return true;
    }
#endif
    if (decoder.messageReceiverName() == Messages::RemoteMediaPlayerManagerProxy::messageReceiverName()) {
        remoteMediaPlayerManagerProxy().didReceiveMessageFromWebProcess(connection, decoder);
        return true;
    }
    if (decoder.messageReceiverName() == Messages::RemoteMediaPlayerProxy::messageReceiverName()) {
        remoteMediaPlayerManagerProxy().didReceivePlayerMessage(connection, decoder);
        return true;
    }
    if (decoder.messageReceiverName() == Messages::RemoteMediaResourceManager::messageReceiverName()) {
        remoteMediaResourceManager().didReceiveMessage(connection, decoder);
        return true;
    }
#if PLATFORM(COCOA) && ENABLE(MEDIA_STREAM)
    if (decoder.messageReceiverName() == Messages::UserMediaCaptureManagerProxy::messageReceiverName()) {
        userMediaCaptureManagerProxy().didReceiveMessageFromGPUProcess(connection, decoder);
        return true;
    }
#if HAVE(AVASSETWRITERDELEGATE)
    if (decoder.messageReceiverName() == Messages::RemoteMediaRecorderManager::messageReceiverName()) {
        mediaRecorderManager().didReceiveMessageFromWebProcess(connection, decoder);
        return true;
    }
    if (decoder.messageReceiverName() == Messages::RemoteMediaRecorder::messageReceiverName()) {
        mediaRecorderManager().didReceiveRemoteMediaRecorderMessage(connection, decoder);
        return true;
    }
#endif // HAVE(AVASSETWRITERDELEGATE)
    if (decoder.messageReceiverName() == Messages::RemoteAudioMediaStreamTrackRendererManager::messageReceiverName()) {
        audioTrackRendererManager().didReceiveMessageFromWebProcess(connection, decoder);
        return true;
    }
    if (decoder.messageReceiverName() == Messages::RemoteAudioMediaStreamTrackRenderer::messageReceiverName()) {
        audioTrackRendererManager().didReceiveRendererMessage(connection, decoder);
        return true;
    }
    if (decoder.messageReceiverName() == Messages::RemoteSampleBufferDisplayLayerManager::messageReceiverName()) {
        sampleBufferDisplayLayerManager().didReceiveMessageFromWebProcess(connection, decoder);
        return true;
    }
    if (decoder.messageReceiverName() == Messages::RemoteSampleBufferDisplayLayer::messageReceiverName()) {
        sampleBufferDisplayLayerManager().didReceiveLayerMessage(connection, decoder);
        return true;
    }
#endif // PLATFORM(COCOA) && ENABLE(MEDIA_STREAM)
#if PLATFORM(COCOA) && USE(LIBWEBRTC)
    if (decoder.messageReceiverName() == Messages::LibWebRTCCodecsProxy::messageReceiverName()) {
        libWebRTCCodecsProxy().didReceiveMessageFromWebProcess(connection, decoder);
        return true;
    }
#endif
#if ENABLE(ENCRYPTED_MEDIA)
    if (decoder.messageReceiverName() == Messages::RemoteCDMFactoryProxy::messageReceiverName()) {
        cdmFactoryProxy().didReceiveMessageFromWebProcess(connection, decoder);
        return true;
    }

    if (decoder.messageReceiverName() == Messages::RemoteCDMProxy::messageReceiverName()) {
        cdmFactoryProxy().didReceiveCDMMessage(connection, decoder);
        return true;
    }

    if (decoder.messageReceiverName() == Messages::RemoteCDMInstanceProxy::messageReceiverName()) {
        cdmFactoryProxy().didReceiveCDMInstanceMessage(connection, decoder);
        return true;
    }

    if (decoder.messageReceiverName() == Messages::RemoteCDMInstanceSessionProxy::messageReceiverName()) {
        cdmFactoryProxy().didReceiveCDMInstanceSessionMessage(connection, decoder);
        return true;
    }
#endif
#if ENABLE(GPU_PROCESS) && USE(AUDIO_SESSION)
    if (decoder.messageReceiverName() == Messages::RemoteAudioSessionProxy::messageReceiverName()) {
        audioSessionProxy().didReceiveMessage(connection, decoder);
        return true;
    }
#endif
#if ENABLE(LEGACY_ENCRYPTED_MEDIA)
    if (decoder.messageReceiverName() == Messages::RemoteLegacyCDMFactoryProxy::messageReceiverName()) {
        legacyCdmFactoryProxy().didReceiveMessageFromWebProcess(connection, decoder);
        return true;
    }
#endif

    return messageReceiverMap().dispatchMessage(connection, decoder);
}

bool GPUConnectionToWebProcess::dispatchSyncMessage(IPC::Connection& connection, IPC::Decoder& decoder, std::unique_ptr<IPC::Encoder>& replyEncoder)
{
#if ENABLE(WEB_AUDIO)
    if (decoder.messageReceiverName() == Messages::RemoteAudioDestinationManager::messageReceiverName()) {
        remoteAudioDestinationManager().didReceiveSyncMessageFromWebProcess(connection, decoder, replyEncoder);
        return true;
    }
#endif
    if (decoder.messageReceiverName() == Messages::RemoteMediaPlayerManagerProxy::messageReceiverName()) {
        remoteMediaPlayerManagerProxy().didReceiveSyncMessageFromWebProcess(connection, decoder, replyEncoder);
        return true;
    }
    if (decoder.messageReceiverName() == Messages::RemoteMediaPlayerProxy::messageReceiverName()) {
        remoteMediaPlayerManagerProxy().didReceiveSyncPlayerMessage(connection, decoder, replyEncoder);
        return true;
    }
#if ENABLE(ENCRYPTED_MEDIA)
    if (decoder.messageReceiverName() == Messages::RemoteCDMFactoryProxy::messageReceiverName()) {
        cdmFactoryProxy().didReceiveSyncMessageFromWebProcess(connection, decoder, replyEncoder);
        return true;
    }

    if (decoder.messageReceiverName() == Messages::RemoteCDMProxy::messageReceiverName()) {
        cdmFactoryProxy().didReceiveSyncCDMMessage(connection, decoder, replyEncoder);
        return true;
    }

    if (decoder.messageReceiverName() == Messages::RemoteCDMInstanceProxy::messageReceiverName()) {
        cdmFactoryProxy().didReceiveSyncCDMInstanceMessage(connection, decoder, replyEncoder);
        return true;
    }

    if (decoder.messageReceiverName() == Messages::RemoteCDMInstanceSessionProxy::messageReceiverName()) {
        cdmFactoryProxy().didReceiveSyncCDMInstanceSessionMessage(connection, decoder, replyEncoder);
        return true;
    }
#endif
#if ENABLE(GPU_PROCESS) && USE(AUDIO_SESSION)
    if (decoder.messageReceiverName() == Messages::RemoteAudioSessionProxy::messageReceiverName()) {
        audioSessionProxy().didReceiveSyncMessage(connection, decoder, replyEncoder);
        return true;
    }
#endif
#if ENABLE(LEGACY_ENCRYPTED_MEDIA)
    if (decoder.messageReceiverName() == Messages::RemoteLegacyCDMFactoryProxy::messageReceiverName()) {
        legacyCdmFactoryProxy().didReceiveSyncMessageFromWebProcess(connection, decoder, replyEncoder);
        return true;
    }
#endif

    return messageReceiverMap().dispatchSyncMessage(connection, decoder, replyEncoder);
}

const String& GPUConnectionToWebProcess::mediaCacheDirectory() const
{
    return m_gpuProcess->mediaCacheDirectory(m_sessionID);
}

#if ENABLE(LEGACY_ENCRYPTED_MEDIA)
const String& GPUConnectionToWebProcess::mediaKeysStorageDirectory() const
{
    return m_gpuProcess->mediaKeysStorageDirectory(m_sessionID);
}
#endif

#if ENABLE(MEDIA_STREAM)
void GPUConnectionToWebProcess::setOrientationForMediaCapture(uint64_t orientation)
{
// FIXME: <https://bugs.webkit.org/show_bug.cgi?id=211085>
#if PLATFORM(COCOA)
    userMediaCaptureManagerProxy().setOrientation(orientation);
#endif
}

void GPUConnectionToWebProcess::updateCaptureAccess(bool allowAudioCapture, bool allowVideoCapture, bool allowDisplayCapture)
{
    m_allowsAudioCapture |= allowAudioCapture;
    m_allowsVideoCapture |= allowVideoCapture;
    m_allowsDisplayCapture |= allowDisplayCapture;
}
#endif

} // namespace WebKit

#endif // ENABLE(GPU_PROCESS)
