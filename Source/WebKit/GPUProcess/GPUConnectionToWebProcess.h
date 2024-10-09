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

#pragma once

#if ENABLE(GPU_PROCESS)

#include "Connection.h"
#include "GPUConnectionToWebProcessMessages.h"
#include "MessageReceiverMap.h"
#include "RemoteAudioHardwareListenerIdentifier.h"
#include "RemoteAudioSessionIdentifier.h"
#include "RemoteGPU.h"
#include "RemoteRemoteCommandListenerIdentifier.h"
#include "RenderingBackendIdentifier.h"
#include "ScopedActiveMessageReceiveQueue.h"
#include "SharedPreferencesForWebProcess.h"
#include "WebGPUIdentifier.h"
#include <WebCore/ImageBuffer.h>
#include <WebCore/IntDegrees.h>
#include <WebCore/NowPlayingManager.h>
#include <WebCore/PageIdentifier.h>
#include <WebCore/ProcessIdentifier.h>
#include <WebCore/ProcessIdentity.h>
#include <WebCore/RenderingResourceIdentifier.h>
#include <pal/SessionID.h>
#include <wtf/Logger.h>
#include <wtf/MachSendRight.h>
#include <wtf/ThreadSafeRefCounted.h>
#include <wtf/ThreadSafeWeakPtr.h>

#if PLATFORM(COCOA) && ENABLE(MEDIA_STREAM)
#include "SampleBufferDisplayLayerIdentifier.h"
#endif

#if ENABLE(WEBGL)
#include "GraphicsContextGLIdentifier.h"
#include <WebCore/GraphicsContextGLAttributes.h>
#endif

#if PLATFORM(MAC)
#include <CoreGraphics/CGDisplayConfiguration.h>
#endif

#if PLATFORM(COCOA)
#include <pal/spi/cocoa/TCCSPI.h>
#endif

#if USE(GRAPHICS_LAYER_WC)
#include "WCLayerTreeHostIdentifier.h"
#endif

#if ENABLE(IPC_TESTING_API)
#include "IPCTester.h"
#endif

namespace WTF {
enum class Critical : bool;
enum class Synchronous : bool;
}

namespace WebCore {
class SecurityOrigin;
class SecurityOriginData;
}

namespace WebKit {

class GPUProcess;
class LayerHostingContext;
class LibWebRTCCodecsProxy;
class LocalAudioSessionRoutingArbitrator;
class RemoteAudioDestinationManager;
class RemoteAudioHardwareListenerProxy;
class RemoteAudioMediaStreamTrackRendererInternalUnitManager;
class RemoteAudioSessionProxy;
class RemoteAudioSessionProxyManager;
class RemoteCDMFactoryProxy;
class RemoteImageDecoderAVFProxy;
class RemoteLegacyCDMFactoryProxy;
class RemoteMediaEngineConfigurationFactoryProxy;
class RemoteMediaSessionHelperProxy;
class RemoteRemoteCommandListenerProxy;
class RemoteRenderingBackend;
class RemoteSampleBufferDisplayLayerManager;
class RemoteSharedResourceCache;
class UserMediaCaptureManagerProxy;
struct GPUProcessConnectionParameters;
struct MediaOverridesForTesting;
struct RemoteAudioSessionConfiguration;

#if USE(GRAPHICS_LAYER_WC)
class RemoteWCLayerTreeHost;
#endif

#if ENABLE(VIDEO)
class RemoteMediaPlayerManagerProxy;
class RemoteMediaResourceManager;
class RemoteVideoFrameObjectHeap;
#endif

#if PLATFORM(COCOA) && ENABLE(MEDIA_RECORDER)
class RemoteMediaRecorderManager;
#endif

#if ENABLE(WEBGL)
class RemoteGraphicsContextGL;
#endif

class GPUConnectionToWebProcess
    : public ThreadSafeRefCountedAndCanMakeThreadSafeWeakPtr<GPUConnectionToWebProcess, WTF::DestructionThread::Main>
    , public WebCore::NowPlayingManagerClient
    , IPC::Connection::Client {
    WTF_MAKE_FAST_ALLOCATED;
    WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(GPUConnectionToWebProcess);
public:
    static Ref<GPUConnectionToWebProcess> create(GPUProcess&, WebCore::ProcessIdentifier, PAL::SessionID, IPC::Connection::Handle&&, GPUProcessConnectionParameters&&);
    virtual ~GPUConnectionToWebProcess();

    std::optional<SharedPreferencesForWebProcess> sharedPreferencesForWebProcess() const { return m_sharedPreferencesForWebProcess; }
    const SharedPreferencesForWebProcess& sharedPreferencesForWebProcessValue() const { return m_sharedPreferencesForWebProcess; }
    void updateSharedPreferencesForWebProcess(SharedPreferencesForWebProcess&&);

#if ENABLE(WEBXR)
    bool isWebXREnabled() const { return m_sharedPreferencesForWebProcess.webXREnabled; }
#else
    bool isWebXREnabled() const { return false; }
#endif

#if ENABLE(RE_DYNAMIC_CONTENT_SCALING)
    bool isDynamicContentScalingEnabled() const { return m_sharedPreferencesForWebProcess.useCGDisplayListsForDOMRendering; }
#endif

    USING_CAN_MAKE_WEAKPTR(WebCore::NowPlayingManagerClient);

    IPC::Connection& connection() { return m_connection.get(); }
    Ref<IPC::Connection> protectedConnection() { return m_connection; }
    IPC::MessageReceiverMap& messageReceiverMap() { return m_messageReceiverMap; }
    GPUProcess& gpuProcess() { return m_gpuProcess.get(); }
    Ref<GPUProcess> protectedGPUProcess() const;
    WebCore::ProcessIdentifier webProcessIdentifier() const { return m_webProcessIdentifier; }
    Ref<RemoteSharedResourceCache> sharedResourceCache();

#if ENABLE(VIDEO)
    RemoteMediaResourceManager& remoteMediaResourceManager();
    Ref<RemoteMediaResourceManager> protectedRemoteMediaResourceManager();
#endif

    PAL::SessionID sessionID() const { return m_sessionID; }

    bool isLockdownModeEnabled() const { return m_isLockdownModeEnabled; }
    bool isLockdownSafeFontParserEnabled() const { return sharedPreferencesForWebProcess() ? sharedPreferencesForWebProcess()->lockdownFontParserEnabled : false; }

    bool allowTestOnlyIPC() const { return sharedPreferencesForWebProcess() ? sharedPreferencesForWebProcess()->allowTestOnlyIPC : false; }

    Logger& logger();

    const String& mediaCacheDirectory() const;
#if ENABLE(LEGACY_ENCRYPTED_MEDIA) || ENABLE(ENCRYPTED_MEDIA)
    const String& mediaKeysStorageDirectory() const;
#endif

#if ENABLE(MEDIA_STREAM)
    void setOrientationForMediaCapture(WebCore::IntDegrees);
    void rotationAngleForCaptureDeviceChanged(const String&, WebCore::VideoFrameRotation);
    void startMonitoringCaptureDeviceRotation(WebCore::PageIdentifier, const String&);
    void stopMonitoringCaptureDeviceRotation(WebCore::PageIdentifier, const String&);
    void updateCaptureAccess(bool allowAudioCapture, bool allowVideoCapture, bool allowDisplayCapture);
    void updateCaptureOrigin(const WebCore::SecurityOriginData&);
    bool setCaptureAttributionString();
    bool allowsAudioCapture() const { return m_allowsAudioCapture; }
    bool allowsVideoCapture() const { return m_allowsVideoCapture; }
    bool allowsDisplayCapture() const { return m_allowsDisplayCapture; }
#endif
#if ENABLE(VIDEO)
    RemoteVideoFrameObjectHeap& videoFrameObjectHeap() const;
#endif
#if PLATFORM(COCOA) && ENABLE(MEDIA_STREAM)
    void startCapturingAudio();
    void processIsStartingToCaptureAudio(GPUConnectionToWebProcess&);
    bool isLastToCaptureAudio() const { return m_isLastToCaptureAudio; }
#endif

#if ENABLE(APP_PRIVACY_REPORT)
    void setTCCIdentity();
#endif

    const WebCore::ProcessIdentity& webProcessIdentity() const { return m_webProcessIdentity; }
#if ENABLE(ENCRYPTED_MEDIA)
    RemoteCDMFactoryProxy& cdmFactoryProxy();
    Ref<RemoteCDMFactoryProxy> protectedCdmFactoryProxy();
#endif
#if ENABLE(LEGACY_ENCRYPTED_MEDIA)
    RemoteLegacyCDMFactoryProxy& legacyCdmFactoryProxy();
    Ref<RemoteLegacyCDMFactoryProxy> protectedLegacyCdmFactoryProxy();
#endif
    RemoteMediaEngineConfigurationFactoryProxy& mediaEngineConfigurationFactoryProxy();
#if ENABLE(VIDEO)
    RemoteMediaPlayerManagerProxy& remoteMediaPlayerManagerProxy() { return m_remoteMediaPlayerManagerProxy.get(); }
    Ref<RemoteMediaPlayerManagerProxy> protectedRemoteMediaPlayerManagerProxy();
#endif
#if USE(AUDIO_SESSION)
    RemoteAudioSessionProxyManager& audioSessionManager();
#endif

#if HAVE(AVASSETREADER)
    RemoteImageDecoderAVFProxy& imageDecoderAVFProxy();
#endif

    void updateSupportedRemoteCommands();

    bool allowsExitUnderMemoryPressure() const;
    void terminateWebProcess();

    void lowMemoryHandler(WTF::Critical, WTF::Synchronous);

#if ENABLE(WEBGL)
    void releaseGraphicsContextGLForTesting(GraphicsContextGLIdentifier);
#endif

    static uint64_t objectCountForTesting() { return gObjectCountForTesting; }

    using RemoteRenderingBackendMap = HashMap<RenderingBackendIdentifier, IPC::ScopedActiveMessageReceiveQueue<RemoteRenderingBackend>>;
    const RemoteRenderingBackendMap& remoteRenderingBackendMap() const { return m_remoteRenderingBackendMap; }

    RemoteRenderingBackend* remoteRenderingBackend(RenderingBackendIdentifier);

#if HAVE(AUDIT_TOKEN)
    const std::optional<audit_token_t>& presentingApplicationAuditToken() const { return m_presentingApplicationAuditToken; }
#endif

#if ENABLE(VIDEO)
    Ref<RemoteVideoFrameObjectHeap> protectedVideoFrameObjectHeap();
    void performWithMediaPlayerOnMainThread(WebCore::MediaPlayerIdentifier, Function<void(WebCore::MediaPlayer&)>&&);
#endif

#if PLATFORM(IOS_FAMILY)
    void overridePresentingApplicationPIDIfNeeded();
#endif

#if ENABLE(EXTENSION_CAPABILITIES)
    String mediaEnvironment(WebCore::PageIdentifier);
    void setMediaEnvironment(WebCore::PageIdentifier, const String&);
#endif

private:
    GPUConnectionToWebProcess(GPUProcess&, WebCore::ProcessIdentifier, PAL::SessionID, IPC::Connection::Handle&&, GPUProcessConnectionParameters&&);

#if PLATFORM(COCOA) && USE(LIBWEBRTC)
    Ref<LibWebRTCCodecsProxy> protectedLibWebRTCCodecsProxy() const;
#endif

#if ENABLE(WEB_AUDIO)
    RemoteAudioDestinationManager& remoteAudioDestinationManager();
#endif
#if PLATFORM(COCOA) && ENABLE(MEDIA_STREAM)
    Ref<RemoteSampleBufferDisplayLayerManager> protectedSampleBufferDisplayLayerManager() const;
    UserMediaCaptureManagerProxy& userMediaCaptureManagerProxy();
    RemoteAudioMediaStreamTrackRendererInternalUnitManager& audioMediaStreamTrackRendererInternalUnitManager();
#endif
#if PLATFORM(COCOA) && ENABLE(MEDIA_RECORDER)
    RemoteMediaRecorderManager& mediaRecorderManager();
#endif

    void createRenderingBackend(RenderingBackendIdentifier, IPC::StreamServerConnection::Handle&&);
    void releaseRenderingBackend(RenderingBackendIdentifier);

#if ENABLE(WEBGL)
    void createGraphicsContextGL(GraphicsContextGLIdentifier, WebCore::GraphicsContextGLAttributes, RenderingBackendIdentifier, IPC::StreamServerConnection::Handle&&);
    void releaseGraphicsContextGL(GraphicsContextGLIdentifier);
#endif

    void createGPU(WebGPUIdentifier, RenderingBackendIdentifier, IPC::StreamServerConnection::Handle&&);
    void releaseGPU(WebGPUIdentifier);

    void clearNowPlayingInfo();
    void setNowPlayingInfo(WebCore::NowPlayingInfo&&);

#if ENABLE(MEDIA_SOURCE)
    void enableMockMediaSource();
#endif
#if PLATFORM(COCOA) && ENABLE(MEDIA_STREAM)
    void updateSampleBufferDisplayLayerBoundsAndPosition(WebKit::SampleBufferDisplayLayerIdentifier, WebCore::FloatRect, std::optional<MachSendRight>&&);
#endif

#if HAVE(VISIBILITY_PROPAGATION_VIEW)
    void createVisibilityPropagationContextForPage(WebPageProxyIdentifier, WebCore::PageIdentifier, bool canShowWhileLocked);
    void destroyVisibilityPropagationContextForPage(WebPageProxyIdentifier, WebCore::PageIdentifier);
#endif

#if USE(AUDIO_SESSION)
    RemoteAudioSessionProxy& audioSessionProxy();
    Ref<RemoteAudioSessionProxy> protectedAudioSessionProxy();
    using EnsureAudioSessionCompletion = CompletionHandler<void(const RemoteAudioSessionConfiguration&)>;
    void ensureAudioSession(EnsureAudioSessionCompletion&&);
#endif

#if PLATFORM(IOS_FAMILY)
    RemoteMediaSessionHelperProxy& mediaSessionHelperProxy();
    void ensureMediaSessionHelper();
#endif

    void createAudioHardwareListener(RemoteAudioHardwareListenerIdentifier);
    void releaseAudioHardwareListener(RemoteAudioHardwareListenerIdentifier);
    void createRemoteCommandListener(RemoteRemoteCommandListenerIdentifier);
    void releaseRemoteCommandListener(RemoteRemoteCommandListenerIdentifier);
    void setMediaOverridesForTesting(MediaOverridesForTesting);
    void configureLoggingChannel(const String&, WTFLogChannelState, WTFLogLevel);

#if USE(GRAPHICS_LAYER_WC)
    void createWCLayerTreeHost(WebKit::WCLayerTreeHostIdentifier, uint64_t nativeWindow, bool usesOffscreenRendering);
    void releaseWCLayerTreeHost(WebKit::WCLayerTreeHostIdentifier);
#endif

    // IPC::Connection::Client
    void didClose(IPC::Connection&) final;
    void didReceiveInvalidMessage(IPC::Connection&, IPC::MessageName, int32_t indexOfObjectFailingDecoding) final;
    void didReceiveMessage(IPC::Connection&, IPC::Decoder&) final;
    bool didReceiveSyncMessage(IPC::Connection&, IPC::Decoder&, UniqueRef<IPC::Encoder>&) final;

    bool dispatchMessage(IPC::Connection&, IPC::Decoder&);
    bool dispatchSyncMessage(IPC::Connection&, IPC::Decoder&, UniqueRef<IPC::Encoder>&);

    // NowPlayingManagerClient
    void didReceiveRemoteControlCommand(WebCore::PlatformMediaSession::RemoteControlCommandType, const WebCore::PlatformMediaSession::RemoteCommandArgument&) final;

#if PLATFORM(MAC) && ENABLE(WEBGL)
    void dispatchDisplayWasReconfigured();
#endif
    void enableMediaPlaybackIfNecessary();

    static uint64_t gObjectCountForTesting;

    RefPtr<Logger> m_logger;

    Ref<IPC::Connection> m_connection;
    IPC::MessageReceiverMap m_messageReceiverMap;
    Ref<GPUProcess> m_gpuProcess;
    const WebCore::ProcessIdentifier m_webProcessIdentifier;
    const WebCore::ProcessIdentity m_webProcessIdentity;
#if ENABLE(WEB_AUDIO)
    std::unique_ptr<RemoteAudioDestinationManager> m_remoteAudioDestinationManager;
#endif
    RefPtr<RemoteSharedResourceCache> m_sharedResourceCache;
#if ENABLE(VIDEO)
    RefPtr<RemoteMediaResourceManager> m_remoteMediaResourceManager WTF_GUARDED_BY_CAPABILITY(mainThread);
    Ref<RemoteMediaPlayerManagerProxy> m_remoteMediaPlayerManagerProxy;
#endif
    PAL::SessionID m_sessionID;
#if PLATFORM(COCOA) && ENABLE(MEDIA_STREAM)
    std::unique_ptr<UserMediaCaptureManagerProxy> m_userMediaCaptureManagerProxy;
    std::unique_ptr<RemoteAudioMediaStreamTrackRendererInternalUnitManager> m_audioMediaStreamTrackRendererInternalUnitManager;
    bool m_isLastToCaptureAudio { false };

    Ref<RemoteSampleBufferDisplayLayerManager> m_sampleBufferDisplayLayerManager;
#endif

#if PLATFORM(COCOA) && ENABLE(MEDIA_RECORDER)
    std::unique_ptr<RemoteMediaRecorderManager> m_remoteMediaRecorderManager;
#endif
#if ENABLE(MEDIA_STREAM)
    Ref<WebCore::SecurityOrigin> m_captureOrigin;
    bool m_allowsAudioCapture { false };
    bool m_allowsVideoCapture { false };
    bool m_allowsDisplayCapture { false };
#endif
#if ENABLE(VIDEO)
    IPC::ScopedActiveMessageReceiveQueue<RemoteVideoFrameObjectHeap> m_videoFrameObjectHeap;
#endif
#if PLATFORM(COCOA) && USE(LIBWEBRTC)
    IPC::ScopedActiveMessageReceiveQueue<LibWebRTCCodecsProxy> m_libWebRTCCodecsProxy;
#endif
#if HAVE(AUDIT_TOKEN)
    const std::optional<audit_token_t> m_presentingApplicationAuditToken;
#endif

    RemoteRenderingBackendMap m_remoteRenderingBackendMap;
#if ENABLE(WEBGL)
    using RemoteGraphicsContextGLMap = HashMap<GraphicsContextGLIdentifier, IPC::ScopedActiveMessageReceiveQueue<RemoteGraphicsContextGL>>;
    RemoteGraphicsContextGLMap m_remoteGraphicsContextGLMap;
#endif
    using RemoteGPUMap = HashMap<WebGPUIdentifier, IPC::ScopedActiveMessageReceiveQueue<RemoteGPU>>;
    RemoteGPUMap m_remoteGPUMap;
#if ENABLE(ENCRYPTED_MEDIA)
    RefPtr<RemoteCDMFactoryProxy> m_cdmFactoryProxy;
#endif
#if USE(AUDIO_SESSION)
    RefPtr<RemoteAudioSessionProxy> m_audioSessionProxy;
#endif
#if PLATFORM(IOS_FAMILY)
    std::unique_ptr<RemoteMediaSessionHelperProxy> m_mediaSessionHelperProxy;
#endif
#if ENABLE(LEGACY_ENCRYPTED_MEDIA)
    RefPtr<RemoteLegacyCDMFactoryProxy> m_legacyCdmFactoryProxy;
#endif
#if HAVE(AVASSETREADER)
    std::unique_ptr<RemoteImageDecoderAVFProxy> m_imageDecoderAVFProxy;
#endif

    std::unique_ptr<RemoteMediaEngineConfigurationFactoryProxy> m_mediaEngineConfigurationFactoryProxy;

#if HAVE(VISIBILITY_PROPAGATION_VIEW)
    HashMap<std::pair<WebPageProxyIdentifier, WebCore::PageIdentifier>, std::unique_ptr<LayerHostingContext>> m_visibilityPropagationContexts;
#endif

    using RemoteAudioHardwareListenerMap = HashMap<RemoteAudioHardwareListenerIdentifier, std::unique_ptr<RemoteAudioHardwareListenerProxy>>;
    RemoteAudioHardwareListenerMap m_remoteAudioHardwareListenerMap;

#if USE(GRAPHICS_LAYER_WC)
    using RemoteWCLayerTreeHostMap = HashMap<WCLayerTreeHostIdentifier, std::unique_ptr<RemoteWCLayerTreeHost>>;
    RemoteWCLayerTreeHostMap m_remoteWCLayerTreeHostMap;
#endif

    RefPtr<RemoteRemoteCommandListenerProxy> m_remoteRemoteCommandListener;
    bool m_isActiveNowPlayingProcess { false };
    const bool m_isLockdownModeEnabled { false };
#if ENABLE(MEDIA_SOURCE)
    bool m_mockMediaSourceEnabled { false };
#endif

#if ENABLE(EXTENSION_CAPABILITIES)
    HashMap<WebCore::PageIdentifier, String> m_mediaEnvironments;
#endif

#if ENABLE(ROUTING_ARBITRATION) && HAVE(AVAUDIO_ROUTING_ARBITER)
    std::unique_ptr<LocalAudioSessionRoutingArbitrator> m_routingArbitrator;
#endif
#if ENABLE(IPC_TESTING_API)
    IPCTester m_ipcTester;
#endif
    SharedPreferencesForWebProcess m_sharedPreferencesForWebProcess;
};

} // namespace WebKit

#endif // ENABLE(GPU_PROCESS)
