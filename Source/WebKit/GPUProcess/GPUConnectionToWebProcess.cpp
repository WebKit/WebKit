/*
 * Copyright (C) 2019-2022 Apple Inc. All rights reserved.
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
#include "GPUProcessConnectionInfo.h"
#include "GPUProcessConnectionMessages.h"
#include "GPUProcessConnectionParameters.h"
#include "GPUProcessMessages.h"
#include "GPUProcessProxyMessages.h"
#include "LayerHostingContext.h"
#include "LibWebRTCCodecsProxy.h"
#include "LibWebRTCCodecsProxyMessages.h"
#include "Logging.h"
#include "MediaOverridesForTesting.h"
#include "RemoteAudioHardwareListenerProxy.h"
#include "RemoteAudioMediaStreamTrackRendererInternalUnitManager.h"
#include "RemoteAudioMediaStreamTrackRendererInternalUnitManagerMessages.h"
#include "RemoteMediaPlayerManagerProxy.h"
#include "RemoteMediaPlayerManagerProxyMessages.h"
#include "RemoteMediaPlayerProxy.h"
#include "RemoteMediaPlayerProxyMessages.h"
#include "RemoteMediaRecorderManager.h"
#include "RemoteMediaRecorderManagerMessages.h"
#include "RemoteMediaRecorderMessages.h"
#include "RemoteMediaResourceManager.h"
#include "RemoteMediaResourceManagerMessages.h"
#include "RemoteRemoteCommandListenerProxy.h"
#include "RemoteRemoteCommandListenerProxyMessages.h"
#include "RemoteRenderingBackend.h"
#include "RemoteRenderingBackendCreationParameters.h"
#include "RemoteSampleBufferDisplayLayerManager.h"
#include "RemoteSampleBufferDisplayLayerManagerMessages.h"
#include "RemoteSampleBufferDisplayLayerMessages.h"
#include "RemoteScrollingCoordinatorTransaction.h"
#include "ScopedRenderingResourcesRequest.h"
#include "WebCoreArgumentCoders.h"
#include "WebErrors.h"
#include "WebGPUObjectHeap.h"
#include "WebProcessMessages.h"
#include <WebCore/LogInitialization.h>
#include <WebCore/MockRealtimeMediaSourceCenter.h>
#include <WebCore/NowPlayingManager.h>
#include <wtf/Language.h>

#if PLATFORM(COCOA)
#include "RemoteLayerTreeDrawingAreaProxyMessages.h"
#include <WebCore/MediaSessionManagerCocoa.h>
#include <WebCore/MediaSessionManagerIOS.h>
#endif

#if ENABLE(WEBGL)
#include "RemoteGraphicsContextGL.h"
#include "RemoteGraphicsContextGLMessages.h"
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
#include "RemoteMediaSessionHelperProxyMessages.h"
#endif

#if ENABLE(LEGACY_ENCRYPTED_MEDIA)
#include "RemoteLegacyCDMFactoryProxy.h"
#include "RemoteLegacyCDMFactoryProxyMessages.h"
#include "RemoteLegacyCDMProxyMessages.h"
#include "RemoteLegacyCDMSessionProxyMessages.h"
#endif

#if HAVE(AVASSETREADER)
#include "RemoteImageDecoderAVFProxy.h"
#include "RemoteImageDecoderAVFProxyMessages.h"
#endif

#if ENABLE(GPU_PROCESS)
#include "RemoteMediaEngineConfigurationFactoryProxy.h"
#include "RemoteMediaEngineConfigurationFactoryProxyMessages.h"
#endif

#if PLATFORM(COCOA)
#include <WebCore/SystemBattery.h>
#endif

#if ENABLE(VP9) && PLATFORM(COCOA)
#include <WebCore/VP9UtilitiesCocoa.h>
#endif

#if ENABLE(ROUTING_ARBITRATION) && HAVE(AVAUDIO_ROUTING_ARBITER)
#include "LocalAudioSessionRoutingArbitrator.h"
#endif

#if ENABLE(MEDIA_STREAM)
#include <WebCore/SecurityOrigin.h>
#endif

#if USE(GRAPHICS_LAYER_WC)
#include "RemoteWCLayerTreeHost.h"
#include "WCContentBufferManager.h"
#endif

#if ENABLE(IPC_TESTING_API)
#include "IPCTesterMessages.h"
#endif

#if ENABLE(VIDEO)
#include "RemoteVideoFrameObjectHeap.h"
#endif

#define MESSAGE_CHECK(assertion) MESSAGE_CHECK_BASE(assertion, (&connection()))

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
        case CaptureDevice::DeviceType::SystemAudio:
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
    
    bool setCaptureAttributionString() final
    {
        return m_process.setCaptureAttributionString();
    }

#if ENABLE(APP_PRIVACY_REPORT)
    void setTCCIdentity() final
    {
        m_process.setTCCIdentity();
    }
#endif

    void startProducingData(RealtimeMediaSource::Type type) final
    {
        if (type != RealtimeMediaSource::Type::Audio)
            return;
        m_process.startCapturingAudio();
    }

    const ProcessIdentity& resourceOwner() const final
    {
        return m_process.webProcessIdentity();
    }

    RemoteVideoFrameObjectHeap* remoteVideoFrameObjectHeap() final { return &m_process.videoFrameObjectHeap(); }

    GPUConnectionToWebProcess& m_process;
};

#endif

Ref<GPUConnectionToWebProcess> GPUConnectionToWebProcess::create(GPUProcess& gpuProcess, WebCore::ProcessIdentifier webProcessIdentifier, PAL::SessionID sessionID, IPC::Connection::Handle&& connectionHandle, GPUProcessConnectionParameters&& parameters)
{
    return adoptRef(*new GPUConnectionToWebProcess(gpuProcess, webProcessIdentifier, sessionID, WTFMove(connectionHandle), WTFMove(parameters)));
}

GPUConnectionToWebProcess::GPUConnectionToWebProcess(GPUProcess& gpuProcess, WebCore::ProcessIdentifier webProcessIdentifier, PAL::SessionID sessionID, IPC::Connection::Handle&& connectionHandle, GPUProcessConnectionParameters&& parameters)
    : m_connection(IPC::Connection::createClientConnection(IPC::Connection::Identifier { WTFMove(connectionHandle) }))
    , m_gpuProcess(gpuProcess)
    , m_webProcessIdentifier(webProcessIdentifier)
    , m_webProcessIdentity(WTFMove(parameters.webProcessIdentity))
    , m_remoteMediaPlayerManagerProxy(makeUniqueRef<RemoteMediaPlayerManagerProxy>(*this))
    , m_sessionID(sessionID)
#if PLATFORM(COCOA) && ENABLE(MEDIA_STREAM)
    , m_sampleBufferDisplayLayerManager(RemoteSampleBufferDisplayLayerManager::create(*this))
#endif
#if ENABLE(MEDIA_STREAM)
    , m_captureOrigin(SecurityOrigin::createOpaque())
#endif
#if ENABLE(VIDEO)
    , m_videoFrameObjectHeap(RemoteVideoFrameObjectHeap::create(m_connection.get()))
#endif
#if PLATFORM(COCOA) && USE(LIBWEBRTC)
    , m_libWebRTCCodecsProxy(LibWebRTCCodecsProxy::create(*this))
#endif
#if HAVE(AUDIT_TOKEN)
    , m_presentingApplicationAuditToken(WTFMove(parameters.presentingApplicationAuditToken))
#endif
    , m_isLockdownModeEnabled(parameters.isLockdownModeEnabled)
#if ENABLE(ROUTING_ARBITRATION) && HAVE(AVAUDIO_ROUTING_ARBITER)
    , m_routingArbitrator(LocalAudioSessionRoutingArbitrator::create(*this))
#endif
{
    RELEASE_ASSERT(RunLoop::isMain());

#if ENABLE(ROUTING_ARBITRATION) && HAVE(AVAUDIO_ROUTING_ARBITER)
    gpuProcess.audioSessionManager().session().setRoutingArbitrationClient(m_routingArbitrator.get());
#endif

    if (!parameters.overrideLanguages.isEmpty())
        overrideUserPreferredLanguages(parameters.overrideLanguages);

    // Use this flag to force synchronous messages to be treated as asynchronous messages in the WebProcess.
    // Otherwise, the WebProcess would process incoming synchronous IPC while waiting for a synchronous IPC
    // reply from the GPU process, which would be unsafe.
    m_connection->setOnlySendMessagesAsDispatchWhenWaitingForSyncReplyWhenProcessingSuchAMessage(true);
    m_connection->open(*this);

#if ENABLE(VP9)
    bool hasVP9HardwareDecoder;
    if (parameters.hasVP9HardwareDecoder)
        hasVP9HardwareDecoder = *parameters.hasVP9HardwareDecoder;
    else {
        hasVP9HardwareDecoder = WebCore::vp9HardwareDecoderAvailable();
        gpuProcess.send(Messages::GPUProcessProxy::SetHasVP9HardwareDecoder(hasVP9HardwareDecoder));
    }
#endif

    WebKit::GPUProcessConnectionInfo info {
#if HAVE(AUDIT_TOKEN)
        gpuProcess.parentProcessConnection()->getAuditToken(),
#endif
#if ENABLE(VP9)
        hasVP9HardwareDecoder
#endif
    };
    m_connection->send(Messages::GPUProcessConnection::DidInitialize(info), 0);
    ++gObjectCountForTesting;
}

GPUConnectionToWebProcess::~GPUConnectionToWebProcess()
{
    RELEASE_ASSERT(RunLoop::isMain());

    m_connection->invalidate();

#if PLATFORM(COCOA) && ENABLE(MEDIA_STREAM)
    m_sampleBufferDisplayLayerManager->close();
#endif

    --gObjectCountForTesting;
}

uint64_t GPUConnectionToWebProcess::gObjectCountForTesting = 0;

void GPUConnectionToWebProcess::didClose(IPC::Connection& connection)
{
#if ENABLE(ROUTING_ARBITRATION) && HAVE(AVAUDIO_ROUTING_ARBITER)
    m_routingArbitrator->processDidTerminate();
#endif

#if USE(AUDIO_SESSION)
    if (m_audioSessionProxy) {
        gpuProcess().audioSessionManager().removeProxy(*m_audioSessionProxy);
        m_audioSessionProxy = nullptr;
    }
#endif
#if ENABLE(VIDEO)
    m_videoFrameObjectHeap->close();
#endif
    // RemoteRenderingBackend objects ref their GPUConnectionToWebProcess so we need to make sure
    // to break the reference cycle by destroying them.
    m_remoteRenderingBackendMap.clear();

#if ENABLE(WEBGL)
    // RemoteGraphicsContextsGL objects are unneeded after connection closes.
    m_remoteGraphicsContextGLMap.clear();
#endif
#if USE(GRAPHICS_LAYER_WC)
    remoteGraphicsContextGLStreamWorkQueue().dispatch([webProcessIdentifier = m_webProcessIdentifier] {
        WCContentBufferManager::singleton().removeAllContentBuffersForProcess(webProcessIdentifier);
    });
#endif
#if PLATFORM(COCOA) && USE(LIBWEBRTC)
    m_libWebRTCCodecsProxy = nullptr;
#endif
    gpuProcess().connectionToWebProcessClosed(connection);
    gpuProcess().removeGPUConnectionToWebProcess(*this); // May destroy |this|.
}

#if HAVE(VISIBILITY_PROPAGATION_VIEW)
void GPUConnectionToWebProcess::createVisibilityPropagationContextForPage(WebPageProxyIdentifier pageProxyID, WebCore::PageIdentifier pageID, bool canShowWhileLocked)
{
    auto contextForVisibilityPropagation = LayerHostingContext::createForExternalHostingProcess({ canShowWhileLocked });
    RELEASE_LOG(Process, "GPUConnectionToWebProcess::createVisibilityPropagationContextForPage: pageProxyID=%" PRIu64 ", webPageID=%" PRIu64 ", contextID=%u", pageProxyID.toUInt64(), pageID.toUInt64(), contextForVisibilityPropagation->contextID());
    gpuProcess().send(Messages::GPUProcessProxy::DidCreateContextForVisibilityPropagation(pageProxyID, pageID, contextForVisibilityPropagation->contextID()));
    m_visibilityPropagationContexts.add(std::make_pair(pageProxyID, pageID), WTFMove(contextForVisibilityPropagation));
}

void GPUConnectionToWebProcess::destroyVisibilityPropagationContextForPage(WebPageProxyIdentifier pageProxyID, WebCore::PageIdentifier pageID)
{
    RELEASE_LOG(Process, "GPUConnectionToWebProcess::destroyVisibilityPropagationContextForPage: pageProxyID=%" PRIu64 ", webPageID=%" PRIu64, pageProxyID.toUInt64(), pageID.toUInt64());

    auto key = std::make_pair(pageProxyID, pageID);
    ASSERT(m_visibilityPropagationContexts.contains(key));
    m_visibilityPropagationContexts.remove(key);
}
#endif

void GPUConnectionToWebProcess::configureLoggingChannel(const String& channelName, WTFLogChannelState state, WTFLogLevel level)
{
#if !RELEASE_LOG_DISABLED
    if  (auto* channel = WebCore::getLogChannel(channelName)) {
        channel->state = state;
        channel->level = level;
    }

    auto* channel = getLogChannel(channelName);
    if  (!channel)
        return;

    channel->state = state;
    channel->level = level;
#else
    UNUSED_PARAM(channelName);
    UNUSED_PARAM(state);
    UNUSED_PARAM(level);
#endif
}

#if USE(GRAPHICS_LAYER_WC)
void GPUConnectionToWebProcess::createWCLayerTreeHost(WebKit::WCLayerTreeHostIdentifier identifier, uint64_t nativeWindow, bool usesOffscreenRendering)
{
    auto addResult = m_remoteWCLayerTreeHostMap.add(identifier, RemoteWCLayerTreeHost::create(*this, WTFMove(identifier), nativeWindow, usesOffscreenRendering));
    ASSERT_UNUSED(addResult, addResult.isNewEntry);
}

void GPUConnectionToWebProcess::releaseWCLayerTreeHost(WebKit::WCLayerTreeHostIdentifier identifier)
{
    m_remoteWCLayerTreeHostMap.remove(identifier);
}
#endif

bool GPUConnectionToWebProcess::allowsExitUnderMemoryPressure() const
{
    if (hasOutstandingRenderingResourceUsage())
        return false;
#if ENABLE(WEB_AUDIO)
    if (m_remoteAudioDestinationManager && !m_remoteAudioDestinationManager->allowsExitUnderMemoryPressure())
        return false;
#endif
#if PLATFORM(COCOA) && ENABLE(MEDIA_STREAM)
    if (m_userMediaCaptureManagerProxy && m_userMediaCaptureManagerProxy->hasSourceProxies())
        return false;
    if (m_audioMediaStreamTrackRendererInternalUnitManager && m_audioMediaStreamTrackRendererInternalUnitManager->hasUnits())
        return false;
    if (!m_sampleBufferDisplayLayerManager->allowsExitUnderMemoryPressure())
        return false;
#endif
#if PLATFORM(COCOA) && ENABLE(MEDIA_STREAM)
    if (m_remoteMediaRecorderManager && !m_remoteMediaRecorderManager->allowsExitUnderMemoryPressure())
        return false;
#endif
#if HAVE(AVASSETREADER)
    if (m_imageDecoderAVFProxy && !m_imageDecoderAVFProxy->allowsExitUnderMemoryPressure())
        return false;
#endif
#if ENABLE(ENCRYPTED_MEDIA)
    if (m_cdmFactoryProxy && !m_cdmFactoryProxy->allowsExitUnderMemoryPressure())
        return false;
#endif
#if ENABLE(LEGACY_ENCRYPTED_MEDIA)
    if (m_legacyCdmFactoryProxy && !m_legacyCdmFactoryProxy->allowsExitUnderMemoryPressure())
        return false;
#endif
#if PLATFORM(COCOA) && USE(LIBWEBRTC)
    if (!m_libWebRTCCodecsProxy->allowsExitUnderMemoryPressure())
        return false;
#endif
    return true;
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
#if ENABLE(IPC_TESTING_API)
    if (connection.ignoreInvalidMessageForTesting())
        return;
#endif
    RELEASE_LOG_FAULT(IPC, "Received an invalid message '%" PUBLIC_LOG_STRING "' from WebContent process %" PRIu64 ", requesting for it to be terminated.", description(messageName), m_webProcessIdentifier.toUInt64());
    terminateWebProcess();
}

void GPUConnectionToWebProcess::terminateWebProcess()
{
    gpuProcess().parentProcessConnection()->send(Messages::GPUProcessProxy::TerminateWebProcess(m_webProcessIdentifier), 0);
}

void GPUConnectionToWebProcess::lowMemoryHandler(Critical critical, Synchronous synchronous)
{
    for (auto& remoteRenderingBackend : m_remoteRenderingBackendMap.values())
        remoteRenderingBackend->lowMemoryHandler(critical, synchronous);
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

#if PLATFORM(COCOA) && ENABLE(MEDIA_STREAM)
UserMediaCaptureManagerProxy& GPUConnectionToWebProcess::userMediaCaptureManagerProxy()
{
    if (!m_userMediaCaptureManagerProxy)
        m_userMediaCaptureManagerProxy = makeUnique<UserMediaCaptureManagerProxy>(makeUniqueRef<GPUProxyForCapture>(*this));

    return *m_userMediaCaptureManagerProxy;
}

RemoteAudioMediaStreamTrackRendererInternalUnitManager& GPUConnectionToWebProcess::audioMediaStreamTrackRendererInternalUnitManager()
{
    if (!m_audioMediaStreamTrackRendererInternalUnitManager)
        m_audioMediaStreamTrackRendererInternalUnitManager = makeUnique<RemoteAudioMediaStreamTrackRendererInternalUnitManager>(*this);

    return *m_audioMediaStreamTrackRendererInternalUnitManager;
}
#endif

#if PLATFORM(COCOA) && ENABLE(MEDIA_STREAM)
RemoteMediaRecorderManager& GPUConnectionToWebProcess::mediaRecorderManager()
{
    if (!m_remoteMediaRecorderManager)
        m_remoteMediaRecorderManager = makeUnique<RemoteMediaRecorderManager>(*this);

    return *m_remoteMediaRecorderManager;
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

#if USE(AUDIO_SESSION)
RemoteAudioSessionProxy& GPUConnectionToWebProcess::audioSessionProxy()
{
    if (!m_audioSessionProxy) {
        m_audioSessionProxy = RemoteAudioSessionProxy::create(*this).moveToUniquePtr();

        auto auditToken = gpuProcess().parentProcessConnection()->getAuditToken();
        gpuProcess().audioSessionManager().addProxy(*m_audioSessionProxy, auditToken);
    }
    return *m_audioSessionProxy;
}
#endif

#if HAVE(AVASSETREADER)
RemoteImageDecoderAVFProxy& GPUConnectionToWebProcess::imageDecoderAVFProxy()
{
    if (!m_imageDecoderAVFProxy)
        m_imageDecoderAVFProxy = makeUnique<RemoteImageDecoderAVFProxy>(*this);

    return *m_imageDecoderAVFProxy;
}
#endif

void GPUConnectionToWebProcess::createRenderingBackend(RemoteRenderingBackendCreationParameters&& creationParameters, IPC::Connection::Handle&& connectionIdentifier, IPC::StreamConnectionBuffer&& streamBuffer)
{
    auto addResult = m_remoteRenderingBackendMap.ensure(creationParameters.identifier, [&]() {
        return IPC::ScopedActiveMessageReceiveQueue { RemoteRenderingBackend::create(*this, WTFMove(creationParameters), WTFMove(connectionIdentifier), WTFMove(streamBuffer)) };
    });
    ASSERT_UNUSED(addResult, addResult.isNewEntry);
}

void GPUConnectionToWebProcess::releaseRenderingBackend(RenderingBackendIdentifier renderingBackendIdentifier)
{
    bool found = m_remoteRenderingBackendMap.remove(renderingBackendIdentifier);
    ASSERT_UNUSED(found, found);
    gpuProcess().tryExitIfUnusedAndUnderMemoryPressure();
}

#if ENABLE(WEBGL)
void GPUConnectionToWebProcess::createGraphicsContextGL(WebCore::GraphicsContextGLAttributes attributes, GraphicsContextGLIdentifier graphicsContextGLIdentifier, RenderingBackendIdentifier renderingBackendIdentifier, IPC::StreamConnectionBuffer&& stream)
{
    MESSAGE_CHECK(!isLockdownModeEnabled());

    auto it = m_remoteRenderingBackendMap.find(renderingBackendIdentifier);
    if (it == m_remoteRenderingBackendMap.end())
        return;
    auto* renderingBackend = it->value.get();

    auto addResult = m_remoteGraphicsContextGLMap.ensure(graphicsContextGLIdentifier, [&]() {
        return IPC::ScopedActiveMessageReceiveQueue { RemoteGraphicsContextGL::create(*this, WTFMove(attributes), graphicsContextGLIdentifier, *renderingBackend, WTFMove(stream)) };
    });
    ASSERT_UNUSED(addResult, addResult.isNewEntry);
}

void GPUConnectionToWebProcess::releaseGraphicsContextGL(GraphicsContextGLIdentifier graphicsContextGLIdentifier)
{
    MESSAGE_CHECK(!isLockdownModeEnabled());

    m_remoteGraphicsContextGLMap.remove(graphicsContextGLIdentifier);
    if (m_remoteGraphicsContextGLMap.isEmpty())
        gpuProcess().tryExitIfUnusedAndUnderMemoryPressure();
}

void GPUConnectionToWebProcess::releaseGraphicsContextGLForTesting(GraphicsContextGLIdentifier identifier)
{
    releaseGraphicsContextGL(identifier);
}
#endif

void GPUConnectionToWebProcess::createRemoteGPU(WebGPUIdentifier identifier, RenderingBackendIdentifier renderingBackendIdentifier, IPC::StreamConnectionBuffer&& stream)
{
    auto it = m_remoteRenderingBackendMap.find(renderingBackendIdentifier);
    if (it == m_remoteRenderingBackendMap.end())
        return;
    auto* renderingBackend = it->value.get();

    auto addResult = m_remoteGPUMap.ensure(identifier, [&]() {
        return IPC::ScopedActiveMessageReceiveQueue { RemoteGPU::create(identifier, *this, *renderingBackend, WTFMove(stream)) };
    });
    ASSERT_UNUSED(addResult, addResult.isNewEntry);
}

void GPUConnectionToWebProcess::clearNowPlayingInfo()
{
    m_isActiveNowPlayingProcess = false;
    gpuProcess().nowPlayingManager().removeClient(*this);
}

void GPUConnectionToWebProcess::setNowPlayingInfo(NowPlayingInfo&& nowPlayingInfo)
{
    m_isActiveNowPlayingProcess = true;
    gpuProcess().nowPlayingManager().addClient(*this);
    gpuProcess().nowPlayingManager().setNowPlayingInfo(WTFMove(nowPlayingInfo));
    updateSupportedRemoteCommands();
}

void GPUConnectionToWebProcess::updateSupportedRemoteCommands()
{
    if (!m_isActiveNowPlayingProcess || !m_remoteRemoteCommandListener)
        return;

    gpuProcess().nowPlayingManager().setSupportsSeeking(m_remoteRemoteCommandListener->supportsSeeking());
    gpuProcess().nowPlayingManager().setSupportedRemoteCommands(m_remoteRemoteCommandListener->supportedCommands());
}

void GPUConnectionToWebProcess::didReceiveRemoteControlCommand(PlatformMediaSession::RemoteControlCommandType type, const PlatformMediaSession::RemoteCommandArgument& argument)
{
    connection().send(Messages::GPUProcessConnection::DidReceiveRemoteCommand(type, argument), 0);
}

#if USE(AUDIO_SESSION)
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

#if ENABLE(GPU_PROCESS)
RemoteMediaEngineConfigurationFactoryProxy& GPUConnectionToWebProcess::mediaEngineConfigurationFactoryProxy()
{
    if (!m_mediaEngineConfigurationFactoryProxy)
        m_mediaEngineConfigurationFactoryProxy = makeUnique<RemoteMediaEngineConfigurationFactoryProxy>();
    return *m_mediaEngineConfigurationFactoryProxy;
}
#endif

void GPUConnectionToWebProcess::createAudioHardwareListener(RemoteAudioHardwareListenerIdentifier identifier)
{
    auto addResult = m_remoteAudioHardwareListenerMap.ensure(identifier, [&]() {
        return makeUnique<RemoteAudioHardwareListenerProxy>(*this, WTFMove(identifier));
    });
    ASSERT_UNUSED(addResult, addResult.isNewEntry);
}

void GPUConnectionToWebProcess::releaseAudioHardwareListener(RemoteAudioHardwareListenerIdentifier identifier)
{
    bool found = m_remoteAudioHardwareListenerMap.remove(identifier);
    ASSERT_UNUSED(found, found);
}

void GPUConnectionToWebProcess::createRemoteCommandListener(RemoteRemoteCommandListenerIdentifier identifier)
{
    m_remoteRemoteCommandListener = RemoteRemoteCommandListenerProxy::create(*this, WTFMove(identifier));
}

void GPUConnectionToWebProcess::releaseRemoteCommandListener(RemoteRemoteCommandListenerIdentifier identifier)
{
    if (m_remoteRemoteCommandListener && m_remoteRemoteCommandListener->identifier() == identifier)
        m_remoteRemoteCommandListener = nullptr;
}

void GPUConnectionToWebProcess::setMediaOverridesForTesting(MediaOverridesForTesting overrides)
{
#if ENABLE(VP9) && PLATFORM(COCOA)
    VP9TestingOverrides::singleton().setHardwareDecoderDisabled(WTFMove(overrides.vp9HardwareDecoderDisabled));
    VP9TestingOverrides::singleton().setVP9DecoderDisabled(WTFMove(overrides.vp9DecoderDisabled));
    VP9TestingOverrides::singleton().setVP9ScreenSizeAndScale(WTFMove(overrides.vp9ScreenSizeAndScale));
#endif

#if PLATFORM(COCOA)
    SystemBatteryStatusTestingOverrides::singleton().setHasAC(WTFMove(overrides.systemHasAC));
    SystemBatteryStatusTestingOverrides::singleton().setHasBattery(WTFMove(overrides.systemHasBattery));
#endif
}

void GPUConnectionToWebProcess::setUserPreferredLanguages(const Vector<String>& languages)
{
    overrideUserPreferredLanguages(languages);
}

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
    if (decoder.messageReceiverName() == Messages::RemoteAudioMediaStreamTrackRendererInternalUnitManager::messageReceiverName()) {
        audioMediaStreamTrackRendererInternalUnitManager().didReceiveMessage(connection, decoder);
        return true;
    }
#endif
#if PLATFORM(COCOA) && ENABLE(MEDIA_STREAM)
    if (decoder.messageReceiverName() == Messages::RemoteMediaRecorderManager::messageReceiverName()) {
        mediaRecorderManager().didReceiveMessageFromWebProcess(connection, decoder);
        return true;
    }
    if (decoder.messageReceiverName() == Messages::RemoteMediaRecorder::messageReceiverName()) {
        mediaRecorderManager().didReceiveRemoteMediaRecorderMessage(connection, decoder);
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
#if USE(AUDIO_SESSION)
    if (decoder.messageReceiverName() == Messages::RemoteAudioSessionProxy::messageReceiverName()) {
        audioSessionProxy().didReceiveMessage(connection, decoder);
        return true;
    }
#endif
#if PLATFORM(IOS_FAMILY)
    if (decoder.messageReceiverName() == Messages::RemoteMediaSessionHelperProxy::messageReceiverName()) {
        mediaSessionHelperProxy().didReceiveMessageFromWebProcess(connection, decoder);
        return true;
    }
#endif
#if ENABLE(LEGACY_ENCRYPTED_MEDIA)
    if (decoder.messageReceiverName() == Messages::RemoteLegacyCDMFactoryProxy::messageReceiverName()) {
        legacyCdmFactoryProxy().didReceiveMessageFromWebProcess(connection, decoder);
        return true;
    }

    if (decoder.messageReceiverName() == Messages::RemoteLegacyCDMProxy::messageReceiverName()) {
        legacyCdmFactoryProxy().didReceiveCDMMessage(connection, decoder);
        return true;
    }

    if (decoder.messageReceiverName() == Messages::RemoteLegacyCDMSessionProxy::messageReceiverName()) {
        legacyCdmFactoryProxy().didReceiveCDMSessionMessage(connection, decoder);
        return true;
    }
#endif
    if (decoder.messageReceiverName() == Messages::RemoteMediaEngineConfigurationFactoryProxy::messageReceiverName()) {
        mediaEngineConfigurationFactoryProxy().didReceiveMessageFromWebProcess(connection, decoder);
        return true;
    }
#if HAVE(AVASSETREADER)
    if (decoder.messageReceiverName() == Messages::RemoteImageDecoderAVFProxy::messageReceiverName()) {
        imageDecoderAVFProxy().didReceiveMessage(connection, decoder);
        return true;
    }
#endif
#if ENABLE(WEBGL)
    if (decoder.messageReceiverName() == Messages::RemoteGraphicsContextGL::messageReceiverName()) {
        // Skip messages for already removed receivers.
        return true;
    }
#endif

    if (decoder.messageReceiverName() == Messages::RemoteRemoteCommandListenerProxy::messageReceiverName()) {
        if (m_remoteRemoteCommandListener)
            m_remoteRemoteCommandListener->didReceiveMessage(connection, decoder);
        return true;
    }
#if ENABLE(IPC_TESTING_API)
    if (decoder.messageReceiverName() == Messages::IPCTester::messageReceiverName()) {
        m_ipcTester.didReceiveMessage(connection, decoder);
        return true;
    }
#endif

    return messageReceiverMap().dispatchMessage(connection, decoder);
}

bool GPUConnectionToWebProcess::dispatchSyncMessage(IPC::Connection& connection, IPC::Decoder& decoder, UniqueRef<IPC::Encoder>& replyEncoder)
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
#if USE(AUDIO_SESSION)
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

    if (decoder.messageReceiverName() == Messages::RemoteLegacyCDMProxy::messageReceiverName()) {
        legacyCdmFactoryProxy().didReceiveSyncCDMMessage(connection, decoder, replyEncoder);
        return true;
    }

    if (decoder.messageReceiverName() == Messages::RemoteLegacyCDMSessionProxy::messageReceiverName()) {
        legacyCdmFactoryProxy().didReceiveSyncCDMSessionMessage(connection, decoder, replyEncoder);
        return true;
    }
#endif
#if HAVE(AVASSETREADER)
    if (decoder.messageReceiverName() == Messages::RemoteImageDecoderAVFProxy::messageReceiverName()) {
        imageDecoderAVFProxy().didReceiveSyncMessage(connection, decoder, replyEncoder);
        return true;
    }
#endif
#if ENABLE(WEBGL)
    if (decoder.messageReceiverName() == Messages::RemoteGraphicsContextGL::messageReceiverName())
        // Skip messages for already removed receivers.
        return true;
#endif
#if ENABLE(IPC_TESTING_API)
    if (decoder.messageReceiverName() == Messages::IPCTester::messageReceiverName()) {
        m_ipcTester.didReceiveSyncMessage(connection, decoder, replyEncoder);
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

void GPUConnectionToWebProcess::updateCaptureOrigin(const WebCore::SecurityOriginData& originData)
{
    m_captureOrigin = originData.securityOrigin();
}

#if PLATFORM(COCOA) && ENABLE(MEDIA_STREAM)
void GPUConnectionToWebProcess::startCapturingAudio()
{
    gpuProcess().processIsStartingToCaptureAudio(*this);
}

void GPUConnectionToWebProcess::processIsStartingToCaptureAudio(GPUConnectionToWebProcess& process)
{
    m_isLastToCaptureAudio = this == &process;
    if (m_audioMediaStreamTrackRendererInternalUnitManager)
        m_audioMediaStreamTrackRendererInternalUnitManager->notifyLastToCaptureAudioChanged();
}
#endif

#if !PLATFORM(COCOA)
bool GPUConnectionToWebProcess::setCaptureAttributionString()
{
}
#endif
#endif // ENABLE(MEDIA_STREAM)

#if ENABLE(VIDEO)
RemoteVideoFrameObjectHeap& GPUConnectionToWebProcess::videoFrameObjectHeap() const
{
    return *m_videoFrameObjectHeap.get();
}
#endif

#if PLATFORM(MAC)
void GPUConnectionToWebProcess::displayConfigurationChanged(CGDirectDisplayID, CGDisplayChangeSummaryFlags flags)
{
#if ENABLE(WEBGL)
    if (flags & kCGDisplaySetModeFlag)
        dispatchDisplayWasReconfigured();
#else
    UNUSED_VARIABLE(flags);
#endif
}
#endif

#if PLATFORM(MAC) && ENABLE(WEBGL)
void GPUConnectionToWebProcess::dispatchDisplayWasReconfigured()
{
    for (auto& context : m_remoteGraphicsContextGLMap.values())
        context->displayWasReconfigured();
}
#endif

#if ENABLE(VP9)
void GPUConnectionToWebProcess::enableVP9Decoders(bool shouldEnableVP8Decoder, bool shouldEnableVP9Decoder, bool shouldEnableVP9SWDecoder)
{
    m_gpuProcess->enableVP9Decoders(shouldEnableVP8Decoder, shouldEnableVP9Decoder, shouldEnableVP9SWDecoder);
}
#endif

} // namespace WebKit

#undef MESSAGE_CHECK

#endif // ENABLE(GPU_PROCESS)
