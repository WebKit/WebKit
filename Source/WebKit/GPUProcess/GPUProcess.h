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

#include "AuxiliaryProcess.h"
#include "SandboxExtension.h"
#include "ShareableBitmap.h"
#include "WebPageProxyIdentifier.h"
#include <WebCore/LibWebRTCEnumTraits.h>
#include <WebCore/MediaPlayerIdentifier.h>
#include <WebCore/Timer.h>
#include <pal/SessionID.h>
#include <wtf/Function.h>
#include <wtf/MemoryPressureHandler.h>
#include <wtf/MonotonicTime.h>
#include <wtf/WeakPtr.h>

#if PLATFORM(MAC)
#include <CoreGraphics/CGDisplayConfiguration.h>
#endif

#if USE(GRAPHICS_LAYER_WC)
#include "WCSharedSceneContextHolder.h"
#endif

namespace WebCore {
class CaptureDevice;
class NowPlayingManager;
struct MockMediaDevice;
struct ScreenProperties;
struct SecurityOriginData;
}

namespace WebKit {

class GPUConnectionToWebProcess;
struct GPUProcessConnectionInitializationParameters;
struct GPUProcessConnectionParameters;
struct GPUProcessCreationParameters;
struct GPUProcessSessionParameters;
class RemoteAudioSessionProxyManager;

class GPUProcess : public AuxiliaryProcess, public ThreadSafeRefCounted<GPUProcess> {
    WTF_MAKE_NONCOPYABLE(GPUProcess);
public:
    explicit GPUProcess(AuxiliaryProcessInitializationParameters&&);
    ~GPUProcess();
    static constexpr WebCore::AuxiliaryProcessType processType = WebCore::AuxiliaryProcessType::GPU;

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
    WorkQueue& videoMediaStreamTrackRendererQueue();
#endif
#if USE(LIBWEBRTC) && PLATFORM(COCOA)
    WorkQueue& libWebRTCCodecsQueue();
#endif

#if USE(GRAPHICS_LAYER_WC)
    WCSharedSceneContextHolder& sharedSceneContext() { return m_sharedSceneContext; }
#endif

#if ENABLE(VP9)
    void enableVP9Decoders(bool shouldEnableVP8Decoder, bool shouldEnableVP9Decoder, bool shouldEnableVP9SWDecoder);
#endif

    void tryExitIfUnusedAndUnderMemoryPressure();

    const String& applicationVisibleName() const { return m_applicationVisibleName; }

    void webProcessConnectionCountForTesting(CompletionHandler<void(uint64_t)>&&);

#if PLATFORM(COCOA) && ENABLE(MEDIA_STREAM)
    void processIsStartingToCaptureAudio(GPUConnectionToWebProcess&);
#endif

    void requestBitmapImageForCurrentTime(WebCore::ProcessIdentifier, WebCore::MediaPlayerIdentifier, CompletionHandler<void(const ShareableBitmap::Handle&)>&&);

private:
    void lowMemoryHandler(Critical, Synchronous);

    // AuxiliaryProcess
    void initializeProcess(const AuxiliaryProcessInitializationParameters&) override;
    void initializeProcessName(const AuxiliaryProcessInitializationParameters&) override;
    void initializeSandbox(const AuxiliaryProcessInitializationParameters&, SandboxInitializationParameters&) override;
    bool shouldTerminate() override;

    void tryExitIfUnused();
    bool canExitUnderMemoryPressure() const;

    // IPC::Connection::Client
    void didReceiveMessage(IPC::Connection&, IPC::Decoder&) override;
    void didReceiveGPUProcessMessage(IPC::Connection&, IPC::Decoder&);

    // Message Handlers
    void initializeGPUProcess(GPUProcessCreationParameters&&);
    void createGPUConnectionToWebProcess(WebCore::ProcessIdentifier, PAL::SessionID, GPUProcessConnectionParameters&&, CompletionHandler<void(std::optional<IPC::Attachment>&&, GPUProcessConnectionInitializationParameters&&)>&&);
    void addSession(PAL::SessionID, GPUProcessSessionParameters&&);
    void removeSession(PAL::SessionID);

#if ENABLE(MEDIA_STREAM)
    void setMockCaptureDevicesEnabled(bool);
    void setOrientationForMediaCapture(uint64_t orientation);
    void updateCaptureAccess(bool allowAudioCapture, bool allowVideoCapture, bool allowDisplayCapture, WebCore::ProcessIdentifier, CompletionHandler<void()>&&);
    void updateCaptureOrigin(const WebCore::SecurityOriginData&, WebCore::ProcessIdentifier);
    void updateSandboxAccess(const Vector<SandboxExtension::Handle>&);
    void addMockMediaDevice(const WebCore::MockMediaDevice&);
    void clearMockMediaDevices();
    void removeMockMediaDevice(const String& persistentId);
    void resetMockMediaDevices();
    void setMockCameraIsInterrupted(bool);
    bool setCaptureAttributionString(const String&);
#endif
#if HAVE(SC_CONTENT_SHARING_SESSION)
    void showWindowPicker(CompletionHandler<void(std::optional<WebCore::CaptureDevice>)>&&);
    void showScreenPicker(CompletionHandler<void(std::optional<WebCore::CaptureDevice>)>&&);
#endif
#if PLATFORM(MAC)
    void displayConfigurationChanged(CGDirectDisplayID, CGDisplayChangeSummaryFlags);
    void setScreenProperties(const WebCore::ScreenProperties&);
#endif

#if USE(OS_STATE)
    RetainPtr<NSDictionary> additionalStateForDiagnosticReport() const final;
#endif

#if ENABLE(MEDIA_SOURCE)
    void setWebMParserEnabled(bool);
#endif

#if ENABLE(WEBM_FORMAT_READER)
    void setWebMFormatReaderEnabled(bool);
#endif

#if ENABLE(OPUS)
    void setOpusDecoderEnabled(bool);
#endif

#if ENABLE(VORBIS)
    void setVorbisDecoderEnabled(bool);
#endif

#if ENABLE(MEDIA_SOURCE) && HAVE(AVSAMPLEBUFFERVIDEOOUTPUT)
    void setMediaSourceInlinePaintingEnabled(bool);
#endif

#if HAVE(SCREEN_CAPTURE_KIT)
    void setUseScreenCaptureKit(bool);
#endif

#if ENABLE(CFPREFS_DIRECT_MODE)
    void notifyPreferencesChanged(const String& domain, const String& key, const std::optional<String>& encodedValue);
    void dispatchSimulatedNotificationsForPreferenceChange(const String& key) final;
#endif

    // Connections to WebProcesses.
    HashMap<WebCore::ProcessIdentifier, Ref<GPUConnectionToWebProcess>> m_webProcessConnections;
    MonotonicTime m_creationTime { MonotonicTime::now() };

#if ENABLE(MEDIA_STREAM)
    struct MediaCaptureAccess {
        bool allowAudioCapture { false };
        bool allowVideoCapture { false };
        bool allowDisplayCapture { false };
    };
    HashMap<WebCore::ProcessIdentifier, MediaCaptureAccess> m_mediaCaptureAccessMap;
#if ENABLE(MEDIA_STREAM) && PLATFORM(COCOA)
    RefPtr<WorkQueue> m_videoMediaStreamTrackRendererQueue;
#endif
    uint64_t m_orientation { 0 };
#endif
#if USE(LIBWEBRTC) && PLATFORM(COCOA)
    RefPtr<WorkQueue> m_libWebRTCCodecsQueue;
#endif

#if USE(GRAPHICS_LAYER_WC)
    WCSharedSceneContextHolder m_sharedSceneContext;
#endif

    struct GPUSession {
        String mediaCacheDirectory;
#if ENABLE(LEGACY_ENCRYPTED_MEDIA)
        String mediaKeysStorageDirectory;
#endif
    };
    HashMap<PAL::SessionID, GPUSession> m_sessions;
    WebCore::Timer m_idleExitTimer;
    std::unique_ptr<WebCore::NowPlayingManager> m_nowPlayingManager;
    String m_applicationVisibleName;
#if ENABLE(GPU_PROCESS) && USE(AUDIO_SESSION)
    mutable std::unique_ptr<RemoteAudioSessionProxyManager> m_audioSessionManager;
#endif
#if ENABLE(VP9)
    bool m_enableVP8Decoder { false };
    bool m_enableVP9Decoder { false };
    bool m_enableVP9SWDecoder { false };
#endif
#if ENABLE(MEDIA_SOURCE)
    bool m_webMParserEnabled { false };
#endif
#if ENABLE(WEBM_FORMAT_READER)
    bool m_webMFormatReaderEnabled { false };
#endif
#if ENABLE(OPUS)
    bool m_opusEnabled { false };
#endif
#if ENABLE(VORBIS)
    bool m_vorbisEnabled { false };
#endif
#if ENABLE(MEDIA_SOURCE) && HAVE(AVSAMPLEBUFFERVIDEOOUTPUT)
    bool m_mediaSourceInlinePaintingEnabled { false };
#endif
#if HAVE(SCREEN_CAPTURE_KIT)
    bool m_useScreenCaptureKit { false };
#endif
};

} // namespace WebKit

#endif // ENABLE(GPU_PROCESS)
