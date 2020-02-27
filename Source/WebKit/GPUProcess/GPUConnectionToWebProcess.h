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

#pragma once

#if ENABLE(GPU_PROCESS)

#include "Connection.h"
#include "GPUConnectionToWebProcessMessages.h"
#include "MessageReceiverMap.h"
#include "RemoteRenderingBackendProxy.h"
#include "RenderingBackendIdentifier.h"
#include <WebCore/ProcessIdentifier.h>
#include <pal/SessionID.h>
#include <wtf/Logger.h>
#include <wtf/RefCounted.h>
#include <wtf/WeakPtr.h>

namespace WebCore {
class PlatformMediaSessionManager;
}

namespace WebKit {

class GPUProcess;
class LibWebRTCCodecsProxy;
class RemoteAudioMediaStreamTrackRendererManager;
class RemoteMediaPlayerManagerProxy;
class RemoteMediaRecorderManager;
class RemoteMediaResourceManager;
class RemoteSampleBufferDisplayLayerManager;
class UserMediaCaptureManagerProxy;

class GPUConnectionToWebProcess
    : public RefCounted<GPUConnectionToWebProcess>
    , public CanMakeWeakPtr<GPUConnectionToWebProcess>
    , IPC::Connection::Client {
public:
    static Ref<GPUConnectionToWebProcess> create(GPUProcess&, WebCore::ProcessIdentifier, IPC::Connection::Identifier, PAL::SessionID);
    virtual ~GPUConnectionToWebProcess();

    IPC::Connection& connection() { return m_connection.get(); }
    IPC::MessageReceiverMap& messageReceiverMap() { return m_messageReceiverMap; }
    GPUProcess& gpuProcess() { return m_gpuProcess.get(); }
    WebCore::ProcessIdentifier webProcessIdentifier() const { return m_webProcessIdentifier; }

    void cleanupForSuspension(Function<void()>&&);
    void endSuspension();

    RemoteMediaResourceManager& remoteMediaResourceManager();

    Logger& logger();

    const String& mediaCacheDirectory() const;
#if ENABLE(LEGACY_ENCRYPTED_MEDIA)
    const String& mediaKeysStorageDirectory() const;
#endif

#if ENABLE(MEDIA_STREAM)
    void setOrientationForMediaCapture(uint64_t orientation);
#endif

    WebCore::PlatformMediaSessionManager& sessionManager();

private:
    GPUConnectionToWebProcess(GPUProcess&, WebCore::ProcessIdentifier, IPC::Connection::Identifier, PAL::SessionID);

    RemoteMediaPlayerManagerProxy& remoteMediaPlayerManagerProxy();
#if PLATFORM(COCOA) && USE(LIBWEBRTC)
    LibWebRTCCodecsProxy& libWebRTCCodecsProxy();
#endif
#if PLATFORM(COCOA) && ENABLE(MEDIA_STREAM)
    UserMediaCaptureManagerProxy& userMediaCaptureManagerProxy();
    RemoteMediaRecorderManager& mediaRecorderManager();
#if ENABLE(VIDEO_TRACK)
    RemoteAudioMediaStreamTrackRendererManager& audioTrackRendererManager();
    RemoteSampleBufferDisplayLayerManager& sampleBufferDisplayLayerManager();
#endif
#endif
    void createRenderingBackend(RenderingBackendIdentifier);
    void releaseRenderingBackend(RenderingBackendIdentifier);

    // IPC::Connection::Client
    void didClose(IPC::Connection&) final;
    void didReceiveInvalidMessage(IPC::Connection&, IPC::StringReference messageReceiverName, IPC::StringReference messageName) final;
    void didReceiveMessage(IPC::Connection&, IPC::Decoder&) final;
    void didReceiveSyncMessage(IPC::Connection&, IPC::Decoder&, std::unique_ptr<IPC::Encoder>&) final;

    bool dispatchMessage(IPC::Connection&, IPC::Decoder&);
    bool dispatchSyncMessage(IPC::Connection&, IPC::Decoder&, std::unique_ptr<IPC::Encoder>&);

    RefPtr<Logger> m_logger;

    Ref<IPC::Connection> m_connection;
    IPC::MessageReceiverMap m_messageReceiverMap;
    Ref<GPUProcess> m_gpuProcess;
    const WebCore::ProcessIdentifier m_webProcessIdentifier;
    std::unique_ptr<RemoteMediaResourceManager> m_remoteMediaResourceManager;
    std::unique_ptr<RemoteMediaPlayerManagerProxy> m_remoteMediaPlayerManagerProxy;
    PAL::SessionID m_sessionID;
#if PLATFORM(COCOA) && ENABLE(MEDIA_STREAM)
    std::unique_ptr<UserMediaCaptureManagerProxy> m_userMediaCaptureManagerProxy;
    std::unique_ptr<RemoteMediaRecorderManager> m_remoteMediaRecorderManager;
#if ENABLE(VIDEO_TRACK)
    std::unique_ptr<RemoteAudioMediaStreamTrackRendererManager> m_audioTrackRendererManager;
    std::unique_ptr<RemoteSampleBufferDisplayLayerManager> m_sampleBufferDisplayLayerManager;
#endif
#endif
#if PLATFORM(COCOA) && USE(LIBWEBRTC)
    std::unique_ptr<LibWebRTCCodecsProxy> m_libWebRTCCodecsProxy;
#endif
    std::unique_ptr<WebCore::PlatformMediaSessionManager> m_sessionManager;
    
    using RemoteRenderingBackendProxyMap = HashMap<RenderingBackendIdentifier, std::unique_ptr<RemoteRenderingBackendProxy>>;
    RemoteRenderingBackendProxyMap m_remoteRenderingBackendProxyMap;
};

} // namespace WebKit

#endif // ENABLE(GPU_PROCESS)
