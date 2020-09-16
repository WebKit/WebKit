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

#include "AuxiliaryProcess.h"
#include "WebPageProxyIdentifier.h"
#include <WebCore/LibWebRTCEnumTraits.h>
#include <pal/SessionID.h>
#include <wtf/Function.h>
#include <wtf/MemoryPressureHandler.h>
#include <wtf/WeakPtr.h>

namespace WebCore {
class NowPlayingManager;
}

namespace WebKit {

class GPUConnectionToWebProcess;
struct GPUProcessCreationParameters;
struct GPUProcessSessionParameters;
class LayerHostingContext;
class RemoteAudioSessionProxyManager;

class GPUProcess : public AuxiliaryProcess, public ThreadSafeRefCounted<GPUProcess>, public CanMakeWeakPtr<GPUProcess> {
    WTF_MAKE_NONCOPYABLE(GPUProcess);
public:
    explicit GPUProcess(AuxiliaryProcessInitializationParameters&&);
    ~GPUProcess();
    static constexpr ProcessType processType = ProcessType::GPU;

    void removeGPUConnectionToWebProcess(GPUConnectionToWebProcess&);

    void prepareToSuspend(bool isSuspensionImminent, CompletionHandler<void()>&&);
    void processDidResume();
    void resume();

    void connectionToWebProcessClosed(IPC::Connection&);

    GPUConnectionToWebProcess* webProcessConnection(WebCore::ProcessIdentifier) const;

    const String& mediaCacheDirectory(PAL::SessionID) const;
#if ENABLE(LEGACY_ENCRYPTED_MEDIA)
    const String& mediaKeysStorageDirectory(PAL::SessionID) const;
#endif

#if ENABLE(GPU_PROCESS) && USE(AUDIO_SESSION)
    RemoteAudioSessionProxyManager& audioSessionManager() const;
#endif

    WebCore::NowPlayingManager& nowPlayingManager();

#if ENABLE(MEDIA_STREAM) && PLATFORM(COCOA)
    WorkQueue& audioMediaStreamTrackRendererQueue();
    WorkQueue& videoMediaStreamTrackRendererQueue();
#endif
#if USE(LIBWEBRTC) && PLATFORM(COCOA)
    WorkQueue& libWebRTCCodecsQueue();
#endif

#if ENABLE(VP9)
    void enableVP9Decoders(bool shouldEnableVP9Decoder, bool shouldEnableVP9SWDecoder);
#endif

private:
    void lowMemoryHandler(Critical);

    // AuxiliaryProcess
    void initializeProcess(const AuxiliaryProcessInitializationParameters&) override;
    void initializeProcessName(const AuxiliaryProcessInitializationParameters&) override;
    void initializeSandbox(const AuxiliaryProcessInitializationParameters&, SandboxInitializationParameters&) override;
    bool shouldTerminate() override;

    // IPC::Connection::Client
    void didReceiveMessage(IPC::Connection&, IPC::Decoder&) override;
    void didClose(IPC::Connection&) override;

    // Message Handlers
    void initializeGPUProcess(GPUProcessCreationParameters&&);
    void createGPUConnectionToWebProcess(WebCore::ProcessIdentifier, PAL::SessionID, CompletionHandler<void(Optional<IPC::Attachment>&&)>&&);
    void addSession(PAL::SessionID, GPUProcessSessionParameters&&);
    void removeSession(PAL::SessionID);

    void processDidTransitionToForeground();
    void processDidTransitionToBackground();
#if ENABLE(MEDIA_STREAM)
    void setMockCaptureDevicesEnabled(bool);
    void setOrientationForMediaCapture(uint64_t orientation);
    void updateCaptureAccess(bool allowAudioCapture, bool allowVideoCapture, bool allowDisplayCapture, WebCore::ProcessIdentifier, CompletionHandler<void()>&&);
#endif

    // Connections to WebProcesses.
    HashMap<WebCore::ProcessIdentifier, Ref<GPUConnectionToWebProcess>> m_webProcessConnections;

#if ENABLE(MEDIA_STREAM)
    struct MediaCaptureAccess {
        bool allowAudioCapture { false };
        bool allowVideoCapture { false };
        bool allowDisplayCapture { false };
    };
    HashMap<WebCore::ProcessIdentifier, MediaCaptureAccess> m_mediaCaptureAccessMap;
#if ENABLE(MEDIA_STREAM) && PLATFORM(COCOA)
    RefPtr<WorkQueue> m_audioMediaStreamTrackRendererQueue;
    RefPtr<WorkQueue> m_videoMediaStreamTrackRendererQueue;
#endif
#endif
#if USE(LIBWEBRTC) && PLATFORM(COCOA)
    RefPtr<WorkQueue> m_libWebRTCCodecsQueue;
#endif

    struct GPUSession {
        String mediaCacheDirectory;
#if ENABLE(LEGACY_ENCRYPTED_MEDIA)
        String mediaKeysStorageDirectory;
#endif
    };
    HashMap<PAL::SessionID, GPUSession> m_sessions;
#if HAVE(VISIBILITY_PROPAGATION_VIEW)
    std::unique_ptr<LayerHostingContext> m_contextForVisibilityPropagation;
    bool m_canShowWhileLocked { false };
#endif
    std::unique_ptr<WebCore::NowPlayingManager> m_nowPlayingManager;
#if ENABLE(GPU_PROCESS) && USE(AUDIO_SESSION)
    mutable std::unique_ptr<RemoteAudioSessionProxyManager> m_audioSessionManager;
#endif
#if ENABLE(VP9)
    bool m_enableVP9Decoder { false };
    bool m_enableVP9SWDecoder { false };
#endif
};

} // namespace WebKit

#endif // ENABLE(GPU_PROCESS)
