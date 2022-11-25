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
#include "GPUProcessConnection.h"

#if ENABLE(GPU_PROCESS)

#include "AudioMediaStreamTrackRendererInternalUnitManager.h"
#include "DataReference.h"
#include "GPUConnectionToWebProcessMessages.h"
#include "GPUProcessConnectionInfo.h"
#include "GPUProcessConnectionMessages.h"
#include "GPUProcessConnectionParameters.h"
#include "LibWebRTCCodecs.h"
#include "LibWebRTCCodecsMessages.h"
#include "Logging.h"
#include "MediaOverridesForTesting.h"
#include "MediaPlayerPrivateRemoteMessages.h"
#include "MediaSourcePrivateRemoteMessages.h"
#include "RemoteAudioHardwareListenerMessages.h"
#include "RemoteAudioSourceProviderManager.h"
#include "RemoteCDMFactory.h"
#include "RemoteCDMProxy.h"
#include "RemoteMediaEngineConfigurationFactory.h"
#include "RemoteMediaPlayerManager.h"
#include "RemoteRemoteCommandListenerMessages.h"
#include "SampleBufferDisplayLayerManager.h"
#include "SampleBufferDisplayLayerMessages.h"
#include "SourceBufferPrivateRemoteMessages.h"
#include "WebCoreArgumentCoders.h"
#include "WebPage.h"
#include "WebPageCreationParameters.h"
#include "WebPageMessages.h"
#include "WebProcess.h"
#include "WebProcessProxyMessages.h"
#include <WebCore/PlatformMediaSessionManager.h>
#include <WebCore/SharedBuffer.h>
#include <wtf/Language.h>

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

static void languagesChanged(void* context)
{
    static_cast<GPUProcessConnection*>(context)->connection().send(Messages::GPUConnectionToWebProcess::SetUserPreferredLanguages(userPreferredLanguages()), { });
}

static GPUProcessConnectionParameters getGPUProcessConnectionParameters()
{
    GPUProcessConnectionParameters parameters;
#if PLATFORM(COCOA)
    parameters.webProcessIdentity = ProcessIdentity { ProcessIdentity::CurrentProcess };
    parameters.overrideLanguages = userPreferredLanguagesOverride();
#endif
    return parameters;
}

RefPtr<GPUProcessConnection> GPUProcessConnection::create(IPC::Connection& parentConnection)
{
    auto connectionIdentifiers = IPC::Connection::createConnectionIdentifierPair();
    if (!connectionIdentifiers)
        return nullptr;

    parentConnection.send(Messages::WebProcessProxy::CreateGPUProcessConnection(connectionIdentifiers->client, getGPUProcessConnectionParameters()), 0, IPC::SendOption::DispatchMessageEvenWhenWaitingForSyncReply);

    auto instance = adoptRef(*new GPUProcessConnection(WTFMove(connectionIdentifiers->server)));
#if ENABLE(IPC_TESTING_API)
    if (parentConnection.ignoreInvalidMessageForTesting())
        instance->connection().setIgnoreInvalidMessageForTesting();
#endif
    return instance;
}

GPUProcessConnection::GPUProcessConnection(IPC::Connection::Identifier&& connectionIdentifier)
    : m_connection(IPC::Connection::createServerConnection(connectionIdentifier))
{
    m_connection->open(*this);

    addLanguageChangeObserver(this, languagesChanged);

    if (WebProcess::singleton().shouldUseRemoteRenderingFor(RenderingPurpose::MediaPainting)) {
#if ENABLE(VP9)
        enableVP9Decoders(PlatformMediaSessionManager::shouldEnableVP8Decoder(), PlatformMediaSessionManager::shouldEnableVP9Decoder(), PlatformMediaSessionManager::shouldEnableVP9SWDecoder());
#endif
    }
}

GPUProcessConnection::~GPUProcessConnection()
{
    m_connection->invalidate();
#if PLATFORM(COCOA) && ENABLE(WEB_AUDIO)
    if (m_audioSourceProviderManager)
        m_audioSourceProviderManager->stopListeningForIPC();
#endif
    removeLanguageChangeObserver(this);
}

#if HAVE(AUDIT_TOKEN)
std::optional<audit_token_t> GPUProcessConnection::auditToken()
{
    if (!waitForDidInitialize())
        return std::nullopt;
    return m_auditToken;
}
#endif

void GPUProcessConnection::invalidate()
{
    m_connection->invalidate();
    m_hasInitialized = true;
}

void GPUProcessConnection::didClose(IPC::Connection&)
{
    auto protector = Ref { *this };
    WebProcess::singleton().gpuProcessConnectionClosed(*this);

#if ENABLE(ROUTING_ARBITRATION)
    if (auto* arbitrator = WebProcess::singleton().supplement<AudioSessionRoutingArbitrator>())
        arbitrator->leaveRoutingAbritration();
#endif

    auto clients = m_clients;
    for (auto& client : clients)
        client.gpuProcessConnectionDidClose(*this);
}

void GPUProcessConnection::didReceiveInvalidMessage(IPC::Connection&, IPC::MessageName)
{
}

#if PLATFORM(COCOA) && ENABLE(MEDIA_STREAM)
SampleBufferDisplayLayerManager& GPUProcessConnection::sampleBufferDisplayLayerManager()
{
    if (!m_sampleBufferDisplayLayerManager)
        m_sampleBufferDisplayLayerManager = makeUnique<SampleBufferDisplayLayerManager>();
    return *m_sampleBufferDisplayLayerManager;
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
#endif

RemoteMediaPlayerManager& GPUProcessConnection::mediaPlayerManager()
{
    return *WebProcess::singleton().supplement<RemoteMediaPlayerManager>();
}

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
    if (decoder.messageReceiverName() == Messages::MediaPlayerPrivateRemote::messageReceiverName()) {
        WebProcess::singleton().supplement<RemoteMediaPlayerManager>()->didReceivePlayerMessage(connection, decoder);
        return true;
    }

#if PLATFORM(COCOA) && ENABLE(MEDIA_STREAM)
    if (decoder.messageReceiverName() == Messages::UserMediaCaptureManager::messageReceiverName()) {
        if (auto* captureManager = WebProcess::singleton().supplement<UserMediaCaptureManager>())
            captureManager->didReceiveMessageFromGPUProcess(connection, decoder);
        return true;
    }

    if (decoder.messageReceiverName() == Messages::SampleBufferDisplayLayer::messageReceiverName()) {
        sampleBufferDisplayLayerManager().didReceiveLayerMessage(connection, decoder);
        return true;
    }
#endif // PLATFORM(COCOA) && ENABLE(MEDIA_STREAM)

#if ENABLE(ENCRYPTED_MEDIA)
    if (decoder.messageReceiverName() == Messages::RemoteCDMInstanceSession::messageReceiverName()) {
        WebProcess::singleton().cdmFactory().didReceiveSessionMessage(connection, decoder);
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
    if (decoder.messageReceiverName() == Messages::MediaSourcePrivateRemote::messageReceiverName()) {
        RELEASE_LOG_ERROR(Media, "The MediaSourcePrivateRemote object has beed destroyed");
        return true;
    }

    if (decoder.messageReceiverName() == Messages::SourceBufferPrivateRemote::messageReceiverName()) {
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
        invalidate();
        return;
    }
    m_hasInitialized = true;
#if ENABLE(VP9)
    m_hasVP9HardwareDecoder = info->hasVP9HardwareDecoder;
#endif
}

bool GPUProcessConnection::waitForDidInitialize()
{
    if (!m_hasInitialized) {
        bool result = m_connection->waitForAndDispatchImmediately<Messages::GPUProcessConnection::DidInitialize>(0, defaultTimeout);
        if (!result) {
            invalidate();
            return false;
        }
    }
    return m_connection->isValid();
}

void GPUProcessConnection::didReceiveRemoteCommand(PlatformMediaSession::RemoteControlCommandType type, const PlatformMediaSession::RemoteCommandArgument& argument)
{
    PlatformMediaSessionManager::sharedManager().processDidReceiveRemoteControlCommand(type, argument);
}

#if ENABLE(ROUTING_ARBITRATION)
void GPUProcessConnection::beginRoutingArbitrationWithCategory(AudioSession::CategoryType category, AudioSessionRoutingArbitrationClient::ArbitrationCallback&& callback)
{
    if (auto* arbitrator = WebProcess::singleton().supplement<AudioSessionRoutingArbitrator>()) {
        arbitrator->beginRoutingArbitrationWithCategory(category, WTFMove(callback));
        return;
    }

    ASSERT_NOT_REACHED();
    callback(AudioSessionRoutingArbitrationClient::RoutingArbitrationError::Failed, AudioSessionRoutingArbitrationClient::DefaultRouteChanged::No);
}

void GPUProcessConnection::endRoutingArbitration()
{
    if (auto* arbitrator = WebProcess::singleton().supplement<AudioSessionRoutingArbitrator>()) {
        arbitrator->leaveRoutingAbritration();
        return;
    }

    ASSERT_NOT_REACHED();
}
#endif

#if HAVE(VISIBILITY_PROPAGATION_VIEW)
void GPUProcessConnection::createVisibilityPropagationContextForPage(WebPage& page)
{
    connection().send(Messages::GPUConnectionToWebProcess::CreateVisibilityPropagationContextForPage(page.webPageProxyIdentifier(), page.identifier(), page.canShowWhileLocked()), { });
}

void GPUProcessConnection::destroyVisibilityPropagationContextForPage(WebPage& page)
{
    connection().send(Messages::GPUConnectionToWebProcess::DestroyVisibilityPropagationContextForPage(page.webPageProxyIdentifier(), page.identifier()), { });
}
#endif

void GPUProcessConnection::configureLoggingChannel(const String& channelName, WTFLogChannelState state, WTFLogLevel level)
{
    connection().send(Messages::GPUConnectionToWebProcess::ConfigureLoggingChannel(channelName, state, level), { });
}

#if ENABLE(VP9)
void GPUProcessConnection::enableVP9Decoders(bool enableVP8Decoder, bool enableVP9Decoder, bool enableVP9SWDecoder)
{
    if (m_enableVP8Decoder == enableVP8Decoder && m_enableVP9Decoder == enableVP9Decoder && m_enableVP9SWDecoder == enableVP9SWDecoder)
        return;

    m_enableVP8Decoder = enableVP8Decoder;
    m_enableVP9Decoder = enableVP9Decoder;
    m_enableVP9SWDecoder = enableVP9SWDecoder;
    connection().send(Messages::GPUConnectionToWebProcess::EnableVP9Decoders(enableVP8Decoder, enableVP9Decoder, enableVP9SWDecoder), { });
}

bool GPUProcessConnection::hasVP9HardwareDecoder()
{
    if (!waitForDidInitialize())
        return false;
    return m_hasVP9HardwareDecoder;
}

#endif

void GPUProcessConnection::updateMediaConfiguration()
{
#if PLATFORM(COCOA)
    bool settingsChanged = false;

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

    connection().send(Messages::GPUConnectionToWebProcess::SetMediaOverridesForTesting(m_mediaOverridesForTesting), { });
#endif
}

} // namespace WebKit

#endif // ENABLE(GPU_PROCESS)
