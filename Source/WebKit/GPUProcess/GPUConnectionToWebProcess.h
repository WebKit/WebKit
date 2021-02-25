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

#pragma once

#if ENABLE(GPU_PROCESS)

#include "Connection.h"
#include "GPUConnectionToWebProcessMessages.h"
#include "MessageReceiverMap.h"
#include "RemoteAudioHardwareListenerIdentifier.h"
#include "RemoteAudioSessionIdentifier.h"
#include "RemoteRemoteCommandListenerIdentifier.h"
#include "RenderingBackendIdentifier.h"
#include "ScopedActiveMessageReceiveQueue.h"
#include <WebCore/LibWebRTCEnumTraits.h>
#include <WebCore/NowPlayingManager.h>
#include <WebCore/ProcessIdentifier.h>
#include <pal/SessionID.h>
#include <wtf/Logger.h>
#include <wtf/MachSendRight.h>
#include <wtf/ThreadSafeRefCounted.h>

#if ENABLE(WEBGL)
#include "GraphicsContextGLIdentifier.h"
#include <WebCore/GraphicsContextGLAttributes.h>
#endif

namespace WebKit {

class GPUProcess;
class LibWebRTCCodecsProxy;
class RemoteAudioDestinationManager;
class RemoteAudioHardwareListenerProxy;
class RemoteAudioMediaStreamTrackRendererManager;
class RemoteAudioSessionProxy;
class RemoteAudioSessionProxyManager;
class RemoteCDMFactoryProxy;
class RemoteImageDecoderAVFProxy;
class RemoteLegacyCDMFactoryProxy;
class RemoteMediaEngineConfigurationFactoryProxy;
class RemoteMediaPlayerManagerProxy;
class RemoteMediaRecorderManager;
class RemoteMediaResourceManager;
class RemoteMediaSessionHelperProxy;
class RemoteRemoteCommandListenerProxy;
class RemoteRenderingBackend;
class RemoteGraphicsContextGL;
class RemoteSampleBufferDisplayLayerManager;
class UserMediaCaptureManagerProxy;
struct GPUProcessConnectionParameters;
struct MediaOverridesForTesting;
struct RemoteAudioSessionConfiguration;

class GPUConnectionToWebProcess
    : public ThreadSafeRefCounted<GPUConnectionToWebProcess, WTF::DestructionThread::Main>
    , public WebCore::NowPlayingManager::Client
    , IPC::Connection::Client {
public:
    static Ref<GPUConnectionToWebProcess> create(GPUProcess&, WebCore::ProcessIdentifier, IPC::Connection::Identifier, PAL::SessionID, GPUProcessConnectionParameters&&);
    virtual ~GPUConnectionToWebProcess();

    IPC::Connection& connection() { return m_connection.get(); }
    IPC::MessageReceiverMap& messageReceiverMap() { return m_messageReceiverMap; }
    GPUProcess& gpuProcess() { return m_gpuProcess.get(); }
    WebCore::ProcessIdentifier webProcessIdentifier() const { return m_webProcessIdentifier; }

    RemoteMediaResourceManager& remoteMediaResourceManager();

    Logger& logger();
    bool isAlwaysOnLoggingAllowed() const;

    const String& mediaCacheDirectory() const;
#if ENABLE(LEGACY_ENCRYPTED_MEDIA)
    const String& mediaKeysStorageDirectory() const;
#endif

#if ENABLE(MEDIA_STREAM)
    void setOrientationForMediaCapture(uint64_t orientation);
    void updateCaptureAccess(bool allowAudioCapture, bool allowVideoCapture, bool allowDisplayCapture);
    bool allowsAudioCapture() const { return m_allowsAudioCapture; }
    bool allowsVideoCapture() const { return m_allowsVideoCapture; }
    bool allowsDisplayCapture() const { return m_allowsDisplayCapture; }
#endif

#if HAVE(TASK_IDENTITY_TOKEN)
    task_id_token_t webProcessIdentityToken() const { return static_cast<task_id_token_t>(m_webProcessIdentityToken.sendRight()); }
#endif

#if ENABLE(ENCRYPTED_MEDIA)
    RemoteCDMFactoryProxy& cdmFactoryProxy();
#endif
#if ENABLE(LEGACY_ENCRYPTED_MEDIA)
    RemoteLegacyCDMFactoryProxy& legacyCdmFactoryProxy();
#endif

    RemoteMediaEngineConfigurationFactoryProxy& mediaEngineConfigurationFactoryProxy();
    RemoteMediaPlayerManagerProxy& remoteMediaPlayerManagerProxy() { return *m_remoteMediaPlayerManagerProxy; }

#if USE(AUDIO_SESSION)
    RemoteAudioSessionProxyManager& audioSessionManager();
#endif

#if HAVE(AVASSETREADER)
    RemoteImageDecoderAVFProxy& imageDecoderAVFProxy();
#endif

    void terminateWebProcess();

private:
    GPUConnectionToWebProcess(GPUProcess&, WebCore::ProcessIdentifier, IPC::Connection::Identifier, PAL::SessionID, GPUProcessConnectionParameters&&);

#if ENABLE(WEB_AUDIO)
    RemoteAudioDestinationManager& remoteAudioDestinationManager();
#endif
#if PLATFORM(COCOA) && ENABLE(MEDIA_STREAM)
    UserMediaCaptureManagerProxy& userMediaCaptureManagerProxy();
#if HAVE(AVASSETWRITERDELEGATE)
    RemoteMediaRecorderManager& mediaRecorderManager();
#endif
#endif

    void createRenderingBackend(RenderingBackendIdentifier, IPC::Semaphore&& resumeDisplayListSemaphore);
    void releaseRenderingBackend(RenderingBackendIdentifier);

#if ENABLE(WEBGL)
    void createGraphicsContextGL(WebCore::GraphicsContextGLAttributes, GraphicsContextGLIdentifier, RenderingBackendIdentifier, IPC::StreamConnectionBuffer&&);
    void releaseGraphicsContextGL(GraphicsContextGLIdentifier);
#endif

    void clearNowPlayingInfo();
    void setNowPlayingInfo(bool setAsNowPlayingApplication, WebCore::NowPlayingInfo&&);

#if ENABLE(VP9)
    void enableVP9Decoders(bool shouldEnableVP8Decoder, bool shouldEnableVP9Decoder, bool shouldEnableVP9SWDecoder);
#endif

#if USE(AUDIO_SESSION)
    RemoteAudioSessionProxy& audioSessionProxy();
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

    // IPC::Connection::Client
    void didClose(IPC::Connection&) final;
    void didReceiveInvalidMessage(IPC::Connection&, IPC::MessageName) final;
    void didReceiveMessage(IPC::Connection&, IPC::Decoder&) final;
    void didReceiveSyncMessage(IPC::Connection&, IPC::Decoder&, std::unique_ptr<IPC::Encoder>&) final;

    bool dispatchMessage(IPC::Connection&, IPC::Decoder&);
    bool dispatchSyncMessage(IPC::Connection&, IPC::Decoder&, std::unique_ptr<IPC::Encoder>&);

    // NowPlayingManager::Client
    void didReceiveRemoteControlCommand(WebCore::PlatformMediaSession::RemoteControlCommandType, const WebCore::PlatformMediaSession::RemoteCommandArgument&) final;

    RefPtr<Logger> m_logger;

    Ref<IPC::Connection> m_connection;
    IPC::MessageReceiverMap m_messageReceiverMap;
    Ref<GPUProcess> m_gpuProcess;
    const WebCore::ProcessIdentifier m_webProcessIdentifier;
#if HAVE(TASK_IDENTITY_TOKEN)
    MachSendRight m_webProcessIdentityToken;
#endif
#if ENABLE(WEB_AUDIO)
    std::unique_ptr<RemoteAudioDestinationManager> m_remoteAudioDestinationManager;
#endif
    std::unique_ptr<RemoteMediaResourceManager> m_remoteMediaResourceManager;
    std::unique_ptr<RemoteMediaPlayerManagerProxy> m_remoteMediaPlayerManagerProxy;
    PAL::SessionID m_sessionID;
#if PLATFORM(COCOA) && USE(LIBWEBRTC)
    Ref<LibWebRTCCodecsProxy> m_libWebRTCCodecsProxy;
#endif
#if PLATFORM(COCOA) && ENABLE(MEDIA_STREAM)
    std::unique_ptr<UserMediaCaptureManagerProxy> m_userMediaCaptureManagerProxy;
    Ref<RemoteAudioMediaStreamTrackRendererManager> m_audioTrackRendererManager;
    Ref<RemoteSampleBufferDisplayLayerManager> m_sampleBufferDisplayLayerManager;
#if HAVE(AVASSETWRITERDELEGATE)
    std::unique_ptr<RemoteMediaRecorderManager> m_remoteMediaRecorderManager;
#endif
#endif
#if ENABLE(MEDIA_STREAM)
    bool m_allowsAudioCapture { false };
    bool m_allowsVideoCapture { false };
    bool m_allowsDisplayCapture { false };
#endif

    using RemoteRenderingBackendMap = HashMap<RenderingBackendIdentifier, IPC::ScopedActiveMessageReceiveQueue<RemoteRenderingBackend>>;
    RemoteRenderingBackendMap m_remoteRenderingBackendMap;
#if ENABLE(WEBGL)
    using RemoteGraphicsContextGLMap = HashMap<GraphicsContextGLIdentifier, IPC::ScopedActiveMessageReceiveQueue<RemoteGraphicsContextGL>>;
    RemoteGraphicsContextGLMap m_remoteGraphicsContextGLMap;
#endif
#if ENABLE(ENCRYPTED_MEDIA)
    std::unique_ptr<RemoteCDMFactoryProxy> m_cdmFactoryProxy;
#endif
#if USE(AUDIO_SESSION)
    std::unique_ptr<RemoteAudioSessionProxy> m_audioSessionProxy;
#endif
#if PLATFORM(IOS_FAMILY)
    std::unique_ptr<RemoteMediaSessionHelperProxy> m_mediaSessionHelperProxy;
#endif
#if ENABLE(LEGACY_ENCRYPTED_MEDIA)
    std::unique_ptr<RemoteLegacyCDMFactoryProxy> m_legacyCdmFactoryProxy;
#endif
#if HAVE(AVASSETREADER)
    std::unique_ptr<RemoteImageDecoderAVFProxy> m_imageDecoderAVFProxy;
#endif

    std::unique_ptr<RemoteMediaEngineConfigurationFactoryProxy> m_mediaEngineConfigurationFactoryProxy;

    using RemoteAudioHardwareListenerMap = HashMap<RemoteAudioHardwareListenerIdentifier, std::unique_ptr<RemoteAudioHardwareListenerProxy>>;
    RemoteAudioHardwareListenerMap m_remoteAudioHardwareListenerMap;
    using RemoteRemoteCommandListenerMap = HashMap<RemoteRemoteCommandListenerIdentifier, std::unique_ptr<RemoteRemoteCommandListenerProxy>>;
    RemoteRemoteCommandListenerMap m_remoteRemoteCommandListenerMap;
};

} // namespace WebKit

#endif // ENABLE(GPU_PROCESS)
