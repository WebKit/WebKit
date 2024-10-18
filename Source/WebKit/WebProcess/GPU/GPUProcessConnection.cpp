/*
 * Copyright (C) 2019-2024 Apple Inc. All rights reserved.
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
#include "GPUProcessConnection.h"

#if ENABLE(GPU_PROCESS)

#include "AudioMediaStreamTrackRendererInternalUnitManager.h"
#include "GPUConnectionToWebProcessMessages.h"
#include "GPUProcessConnectionInfo.h"
#include "GPUProcessConnectionMessages.h"
#include "LibWebRTCCodecs.h"
#include "LibWebRTCCodecsMessages.h"
#include "Logging.h"
#include "MediaOverridesForTesting.h"
#include "MediaPlayerPrivateRemoteMessages.h"
#include "MediaSourcePrivateRemoteMessageReceiverMessages.h"
#include "RemoteAudioHardwareListenerMessages.h"
#include "RemoteAudioSourceProviderManager.h"
#include "RemoteCDMFactory.h"
#include "RemoteCDMProxy.h"
#include "RemoteMediaEngineConfigurationFactory.h"
#include "RemoteMediaPlayerManager.h"
#include "RemoteRemoteCommandListenerMessages.h"
#include "RemoteSharedResourceCacheProxy.h"
#include "SampleBufferDisplayLayerManager.h"
#include "SampleBufferDisplayLayerMessages.h"
#include "SourceBufferPrivateRemoteMessageReceiverMessages.h"
#include "WebCoreArgumentCoders.h"
#include "WebPage.h"
#include "WebPageCreationParameters.h"
#include "WebPageMessages.h"
#include "WebProcess.h"
#include "WebProcessProxyMessages.h"
#include <WebCore/PlatformMediaSessionManager.h>
#include <WebCore/SharedBuffer.h>

#if ENABLE(ENCRYPTED_MEDIA)
#include "RemoteCDMInstanceSessionMessages.h"
#endif

#if USE(AUDIO_SESSION)
#include "RemoteAudioSession.h"
#include "RemoteAudioSessionMessages.h"
#endif

#if PLATFORM(IOS_FAMILY)
#include "RemoteMediaSessionHelper.h"
#include "RemoteMediaSessionHelperMessages.h"
#endif

#if PLATFORM(COCOA) && ENABLE(MEDIA_STREAM)
#include "UserMediaCaptureManager.h"
#include "UserMediaCaptureManagerMessages.h"
#endif

#if ENABLE(VIDEO)
#include "RemoteVideoFrameObjectHeapProxy.h"
#include "RemoteVideoFrameProxy.h"
#endif

#if ENABLE(WEBGL)
#include "RemoteGraphicsContextGLProxy.h"
#include "RemoteGraphicsContextGLProxyMessages.h"
#endif

#if PLATFORM(COCOA)
#include <WebCore/SystemBattery.h>
#endif

#if ENABLE(VP9) && PLATFORM(COCOA)
#include <WebCore/VP9UtilitiesCocoa.h>
#endif

#if ENABLE(ROUTING_ARBITRATION)
#include "AudioSessionRoutingArbitrator.h"
#endif

namespace WebKit {
using namespace WebCore;

Ref<GPUProcessConnection> GPUProcessConnection::create(Ref<IPC::Connection>&& connection)
{
    Ref instance = adoptRef(*new GPUProcessConnection(WTFMove(connection)));
    RELEASE_LOG(Process, "GPUProcessConnection::create - %p", instance.ptr());
    return instance;
}

GPUProcessConnection::GPUProcessConnection(Ref<IPC::Connection>&& connection)
    : m_connection(connection)
{
    connection->open(*this);

    if (WebProcess::singleton().shouldUseRemoteRenderingFor(RenderingPurpose::MediaPainting)) {
    }
}

GPUProcessConnection::~GPUProcessConnection()
{
    protectedConnection()->invalidate();
#if PLATFORM(COCOA) && ENABLE(WEB_AUDIO)
    if (RefPtr audioSourceProviderManager = m_audioSourceProviderManager)
        audioSourceProviderManager->stopListeningForIPC();
#endif
}


void GPUProcessConnection::didBecomeUnresponsive()
{
    Ref webProcess = WebProcess::singleton();
    // The function call might have been posted asynchronously from other thread.
    // Guard against notifying a problem for a GPUProcessConnection that has already been
    // switched away.
    if (webProcess->existingGPUProcessConnection() != this)
        return;
    webProcess->gpuProcessConnectionDidBecomeUnresponsive();
}

#if HAVE(AUDIT_TOKEN)
std::optional<audit_token_t> GPUProcessConnection::auditToken()
{
    if (!waitForDidInitialize())
        return std::nullopt;
    return m_auditToken;
}
#endif

Ref<RemoteSharedResourceCacheProxy> GPUProcessConnection::sharedResourceCache()
{
    if (!m_sharedResourceCache)
        m_sharedResourceCache = RemoteSharedResourceCacheProxy::create();
    return *m_sharedResourceCache;
}

void GPUProcessConnection::invalidate()
{
    protectedConnection()->invalidate();
    m_hasInitialized = true;
}

void GPUProcessConnection::didClose(IPC::Connection&)
{
    RELEASE_LOG_ERROR(Process, "%p - GPUProcessConnection::didClose", this);
    auto protector = Ref { *this };
    Ref webProcess = WebProcess::singleton();
    ASSERT(webProcess->existingGPUProcessConnection() == this);
    webProcess->gpuProcessConnectionClosed();

#if ENABLE(ROUTING_ARBITRATION)
    if (auto* arbitrator = webProcess->audioSessionRoutingArbitrator())
        arbitrator->leaveRoutingAbritration();
#endif

    m_clients.forEach([this] (auto& client) {
        client.gpuProcessConnectionDidClose(*this);
    });
    m_clients.clear();
}

void GPUProcessConnection::didReceiveInvalidMessage(IPC::Connection&, IPC::MessageName, int32_t)
{
}

#if PLATFORM(COCOA) && ENABLE(MEDIA_STREAM)
SampleBufferDisplayLayerManager& GPUProcessConnection::sampleBufferDisplayLayerManager()
{
    if (!m_sampleBufferDisplayLayerManager)
        m_sampleBufferDisplayLayerManager = makeUniqueWithoutRefCountedCheck<SampleBufferDisplayLayerManager>(*this);
    return *m_sampleBufferDisplayLayerManager;
}

Ref<SampleBufferDisplayLayerManager> GPUProcessConnection::protectedSampleBufferDisplayLayerManager()
{
    return sampleBufferDisplayLayerManager();
}

void GPUProcessConnection::resetAudioMediaStreamTrackRendererInternalUnit(AudioMediaStreamTrackRendererInternalUnitIdentifier identifier)
{
    WebProcess::singleton().audioMediaStreamTrackRendererInternalUnitManager().reset(identifier);
}
#endif

#if ENABLE(VIDEO)
RemoteVideoFrameObjectHeapProxy& GPUProcessConnection::videoFrameObjectHeapProxy()
{
    if (!m_videoFrameObjectHeapProxy)
        m_videoFrameObjectHeapProxy = RemoteVideoFrameObjectHeapProxy::create(*this);
    return *m_videoFrameObjectHeapProxy;
}

Ref<RemoteVideoFrameObjectHeapProxy> GPUProcessConnection::protectedVideoFrameObjectHeapProxy()
{
    return videoFrameObjectHeapProxy();
}

RemoteMediaPlayerManager& GPUProcessConnection::mediaPlayerManager()
{
    return WebProcess::singleton().remoteMediaPlayerManager();
}
#endif

#if PLATFORM(COCOA) && ENABLE(WEB_AUDIO)
RemoteAudioSourceProviderManager& GPUProcessConnection::audioSourceProviderManager()
{
    if (!m_audioSourceProviderManager)
        m_audioSourceProviderManager = RemoteAudioSourceProviderManager::create();
    return *m_audioSourceProviderManager;
}
#endif

bool GPUProcessConnection::dispatchMessage(IPC::Connection& connection, IPC::Decoder& decoder)
{
#if ENABLE(VIDEO)
    if (decoder.messageReceiverName() == Messages::MediaPlayerPrivateRemote::messageReceiverName()) {
        WebProcess::singleton().protectedRemoteMediaPlayerManager()->didReceivePlayerMessage(connection, decoder);
        return true;
    }
#endif

#if PLATFORM(COCOA) && ENABLE(MEDIA_STREAM)
    if (decoder.messageReceiverName() == Messages::UserMediaCaptureManager::messageReceiverName()) {
        if (auto* captureManager = WebProcess::singleton().supplement<UserMediaCaptureManager>())
            captureManager->didReceiveMessageFromGPUProcess(connection, decoder);
        return true;
    }

    if (decoder.messageReceiverName() == Messages::SampleBufferDisplayLayer::messageReceiverName()) {
        protectedSampleBufferDisplayLayerManager()->didReceiveLayerMessage(connection, decoder);
        return true;
    }
#endif // PLATFORM(COCOA) && ENABLE(MEDIA_STREAM)

#if ENABLE(ENCRYPTED_MEDIA)
    if (decoder.messageReceiverName() == Messages::RemoteCDMInstanceSession::messageReceiverName()) {
        WebProcess::singleton().protectedCDMFactory()->didReceiveSessionMessage(connection, decoder);
        return true;
    }
#endif
    if (messageReceiverMap().dispatchMessage(connection, decoder))
        return true;

#if USE(AUDIO_SESSION)
    if (decoder.messageReceiverName() == Messages::RemoteAudioSession::messageReceiverName()) {
        RELEASE_LOG_ERROR(Media, "The RemoteAudioSession object has beed destroyed");
        return true;
    }
#endif

#if ENABLE(MEDIA_SOURCE)
    if (decoder.messageReceiverName() == Messages::MediaSourcePrivateRemoteMessageReceiver::messageReceiverName()) {
        RELEASE_LOG_ERROR(Media, "The MediaSourcePrivateRemote object has beed destroyed");
        return true;
    }

    if (decoder.messageReceiverName() == Messages::SourceBufferPrivateRemoteMessageReceiver::messageReceiverName()) {
        RELEASE_LOG_ERROR(Media, "The SourceBufferPrivateRemote object has beed destroyed");
        return true;
    }
#endif

    if (decoder.messageReceiverName() == Messages::RemoteAudioHardwareListener::messageReceiverName()) {
        RELEASE_LOG_ERROR(Media, "The RemoteAudioHardwareListener object has beed destroyed");
        return true;
    }

    if (decoder.messageReceiverName() == Messages::RemoteRemoteCommandListener::messageReceiverName()) {
        RELEASE_LOG_ERROR(Media, "The RemoteRemoteCommandListener object has beed destroyed");
        return true;
    }

    return false;
}

bool GPUProcessConnection::dispatchSyncMessage(IPC::Connection& connection, IPC::Decoder& decoder, UniqueRef<IPC::Encoder>& replyEncoder)
{
    return messageReceiverMap().dispatchSyncMessage(connection, decoder, replyEncoder);
}

void GPUProcessConnection::didInitialize(std::optional<GPUProcessConnectionInfo>&& info)
{
    if (!info) {
        RELEASE_LOG_ERROR(Process, "%p - GPUProcessConnection::didInitialize - failed", this);
        invalidate();
        return;
    }
    m_hasInitialized = true;
    RELEASE_LOG(Process, "%p - GPUProcessConnection::didInitialize", this);

#if USE(LIBWEBRTC) && PLATFORM(COCOA)
#if ENABLE(VP9)
    WebProcess::singleton().libWebRTCCodecs().setVP9VTBSupport(info->hasVP9HardwareDecoder);
#endif
#if ENABLE(AV1)
    WebProcess::singleton().protectedLibWebRTCCodecs()->setHasAV1HardwareDecoder(info->hasAV1HardwareDecoder);
#endif
#endif
}

bool GPUProcessConnection::waitForDidInitialize()
{
    Ref connection = m_connection;

    if (!m_hasInitialized) {
        auto result = connection->waitForAndDispatchImmediately<Messages::GPUProcessConnection::DidInitialize>(0, defaultTimeout);
        if (result != IPC::Error::NoError) {
            RELEASE_LOG_ERROR(Process, "%p - GPUProcessConnection::waitForDidInitialize - failed, error:%" PUBLIC_LOG_STRING, this, IPC::errorAsString(result).characters());
            invalidate();
            return false;
        }
    }
    return connection->isValid();
}

void GPUProcessConnection::didReceiveRemoteCommand(PlatformMediaSession::RemoteControlCommandType type, const PlatformMediaSession::RemoteCommandArgument& argument)
{
#if ENABLE(VIDEO) || ENABLE(WEB_AUDIO)
    PlatformMediaSessionManager::sharedManager().processDidReceiveRemoteControlCommand(type, argument);
#endif
}

#if ENABLE(ROUTING_ARBITRATION)
void GPUProcessConnection::beginRoutingArbitrationWithCategory(AudioSession::CategoryType category, AudioSessionRoutingArbitrationClient::ArbitrationCallback&& callback)
{
    if (auto* arbitrator = WebProcess::singleton().audioSessionRoutingArbitrator()) {
        arbitrator->beginRoutingArbitrationWithCategory(category, WTFMove(callback));
        return;
    }

    ASSERT_NOT_REACHED();
    callback(AudioSessionRoutingArbitrationClient::RoutingArbitrationError::Failed, AudioSessionRoutingArbitrationClient::DefaultRouteChanged::No);
}

void GPUProcessConnection::endRoutingArbitration()
{
    if (auto* arbitrator = WebProcess::singleton().audioSessionRoutingArbitrator()) {
        arbitrator->leaveRoutingAbritration();
        return;
    }

    ASSERT_NOT_REACHED();
}
#endif

#if HAVE(VISIBILITY_PROPAGATION_VIEW)
void GPUProcessConnection::createVisibilityPropagationContextForPage(WebPage& page)
{
    protectedConnection()->send(Messages::GPUConnectionToWebProcess::CreateVisibilityPropagationContextForPage(page.webPageProxyIdentifier(), page.identifier(), page.canShowWhileLocked()), { });
}

void GPUProcessConnection::destroyVisibilityPropagationContextForPage(WebPage& page)
{
    protectedConnection()->send(Messages::GPUConnectionToWebProcess::DestroyVisibilityPropagationContextForPage(page.webPageProxyIdentifier(), page.identifier()), { });
}
#endif

void GPUProcessConnection::configureLoggingChannel(const String& channelName, WTFLogChannelState state, WTFLogLevel level)
{
    protectedConnection()->send(Messages::GPUConnectionToWebProcess::ConfigureLoggingChannel(channelName, state, level), { });
}

void GPUProcessConnection::updateMediaConfiguration(bool forceUpdate)
{
#if PLATFORM(COCOA)
    bool settingsChanged = forceUpdate;

    if (m_mediaOverridesForTesting.systemHasAC != SystemBatteryStatusTestingOverrides::singleton().hasAC() || m_mediaOverridesForTesting.systemHasBattery != SystemBatteryStatusTestingOverrides::singleton().hasBattery())
        settingsChanged = true;

#if ENABLE(VP9)
    if (m_mediaOverridesForTesting.vp9HardwareDecoderDisabled != VP9TestingOverrides::singleton().hardwareDecoderDisabled()
        || m_mediaOverridesForTesting.vp9DecoderDisabled != VP9TestingOverrides::singleton().vp9DecoderDisabled()
        || m_mediaOverridesForTesting.vp9ScreenSizeAndScale != VP9TestingOverrides::singleton().vp9ScreenSizeAndScale())
        settingsChanged = true;
#endif

    if (!settingsChanged)
        return;

    m_mediaOverridesForTesting = {
        .systemHasAC = SystemBatteryStatusTestingOverrides::singleton().hasAC(),
        .systemHasBattery = SystemBatteryStatusTestingOverrides::singleton().hasBattery(),

#if ENABLE(VP9)
        .vp9HardwareDecoderDisabled = VP9TestingOverrides::singleton().hardwareDecoderDisabled(),
        .vp9DecoderDisabled = VP9TestingOverrides::singleton().vp9DecoderDisabled(),
        .vp9ScreenSizeAndScale = VP9TestingOverrides::singleton().vp9ScreenSizeAndScale(),
#endif
    };

    protectedConnection()->send(Messages::GPUConnectionToWebProcess::SetMediaOverridesForTesting(m_mediaOverridesForTesting), { });
#endif
}

#if ENABLE(EXTENSION_CAPABILITIES)
void GPUProcessConnection::setMediaEnvironment(WebCore::PageIdentifier pageIdentifier, const String& mediaEnvironment)
{
    protectedConnection()->send(Messages::GPUConnectionToWebProcess::SetMediaEnvironment(pageIdentifier, mediaEnvironment), { });
}
#endif

void GPUProcessConnection::createRenderingBackend(RenderingBackendIdentifier identifier, IPC::StreamServerConnection::Handle&& serverHandle)
{
    protectedConnection()->send(Messages::GPUConnectionToWebProcess::CreateRenderingBackend(identifier, WTFMove(serverHandle)), 0, IPC::SendOption::DispatchMessageEvenWhenWaitingForSyncReply);
}

void GPUProcessConnection::releaseRenderingBackend(RenderingBackendIdentifier identifier)
{
    protectedConnection()->send(Messages::GPUConnectionToWebProcess::ReleaseRenderingBackend(identifier), 0, IPC::SendOption::DispatchMessageEvenWhenWaitingForSyncReply);
}

#if ENABLE(WEBGL)
void GPUProcessConnection::createGraphicsContextGL(GraphicsContextGLIdentifier identifier, const GraphicsContextGLAttributes& contextAttributes, RenderingBackendIdentifier renderingBackendIdentifier, IPC::StreamServerConnection::Handle&& serverHandle)
{
    protectedConnection()->send(Messages::GPUConnectionToWebProcess::CreateGraphicsContextGL(identifier, contextAttributes, renderingBackendIdentifier, WTFMove(serverHandle)), 0, IPC::SendOption::DispatchMessageEvenWhenWaitingForSyncReply);
}

void GPUProcessConnection::releaseGraphicsContextGL(GraphicsContextGLIdentifier identifier)
{
    protectedConnection()->send(Messages::GPUConnectionToWebProcess::ReleaseGraphicsContextGL(identifier), 0, IPC::SendOption::DispatchMessageEvenWhenWaitingForSyncReply);
}
#endif

void GPUProcessConnection::createGPU(WebGPUIdentifier identifier, RenderingBackendIdentifier renderingBackendIdentifier, IPC::StreamServerConnection::Handle&& serverHandle)
{
    protectedConnection()->send(Messages::GPUConnectionToWebProcess::CreateGPU(identifier, renderingBackendIdentifier, WTFMove(serverHandle)), 0, IPC::SendOption::DispatchMessageEvenWhenWaitingForSyncReply);
}

void GPUProcessConnection::releaseGPU(WebGPUIdentifier identifier)
{
    protectedConnection()->send(Messages::GPUConnectionToWebProcess::ReleaseGPU(identifier), 0, IPC::SendOption::DispatchMessageEvenWhenWaitingForSyncReply);
}

} // namespace WebKit

#endif // ENABLE(GPU_PROCESS)
