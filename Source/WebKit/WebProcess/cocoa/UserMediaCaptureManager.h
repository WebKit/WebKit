/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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

#if PLATFORM(COCOA) && ENABLE(MEDIA_STREAM)

#include "MessageReceiver.h"
#include "SharedMemory.h"
#include "WebProcessSupplement.h"
#include <WebCore/CaptureDeviceManager.h>
#include <WebCore/RealtimeMediaSource.h>
#include <WebCore/RealtimeMediaSourceFactory.h>
#include <wtf/HashMap.h>

namespace WebCore {
class CAAudioStreamDescription;
class RemoteVideoSample;
}

namespace WebKit {

class CrossProcessRealtimeAudioSource;
class WebProcess;

class UserMediaCaptureManager : public WebProcessSupplement, public IPC::MessageReceiver {
    WTF_MAKE_FAST_ALLOCATED;
public:
    explicit UserMediaCaptureManager(WebProcess&);
    ~UserMediaCaptureManager();

    static const char* supplementName();

    void didReceiveMessageFromGPUProcess(IPC::Connection& connection, IPC::Decoder& decoder) { didReceiveMessage(connection, decoder); }

private:
    // WebProcessSupplement
    void initialize(const WebProcessCreationParameters&) final;

    // WebCore::RealtimeMediaSource factories
    class AudioFactory : public WebCore::AudioCaptureFactory {
    public:
        explicit AudioFactory(UserMediaCaptureManager& manager) : m_manager(manager) { }
        void setShouldCaptureInGPUProcess(bool value) { m_shouldCaptureInGPUProcess = value; }
    private:
        WebCore::CaptureSourceOrError createAudioCaptureSource(const WebCore::CaptureDevice&, String&& hashSalt, const WebCore::MediaConstraints*) final;
        WebCore::CaptureDeviceManager& audioCaptureDeviceManager() final { return m_manager.m_noOpCaptureDeviceManager; }
#if PLATFORM(IOS_FAMILY)
        void setAudioCapturePageState(bool interrupted, bool pageMuted) final;
#endif

        UserMediaCaptureManager& m_manager;
        bool m_shouldCaptureInGPUProcess { false };
    };
    class VideoFactory : public WebCore::VideoCaptureFactory {
    public:
        explicit VideoFactory(UserMediaCaptureManager& manager) : m_manager(manager) { }

    private:
        WebCore::CaptureSourceOrError createVideoCaptureSource(const WebCore::CaptureDevice& device, String&& hashSalt, const WebCore::MediaConstraints* constraints) final { return m_manager.createCaptureSource(device, WTFMove(hashSalt), constraints); }
        WebCore::CaptureDeviceManager& videoCaptureDeviceManager() final { return m_manager.m_noOpCaptureDeviceManager; }
#if PLATFORM(IOS_FAMILY)
        void setVideoCapturePageState(bool interrupted, bool pageMuted) final;
#endif

        UserMediaCaptureManager& m_manager;
    };
    class DisplayFactory : public WebCore::DisplayCaptureFactory {
    public:
        explicit DisplayFactory(UserMediaCaptureManager& manager) : m_manager(manager) { }

    private:
        WebCore::CaptureSourceOrError createDisplayCaptureSource(const WebCore::CaptureDevice& device, const WebCore::MediaConstraints* constraints) final  { return m_manager.createCaptureSource(device, { }, constraints); }
        WebCore::CaptureDeviceManager& displayCaptureDeviceManager() final { return m_manager.m_noOpCaptureDeviceManager; }

        UserMediaCaptureManager& m_manager;
    };

    WebCore::CaptureSourceOrError createCaptureSource(const WebCore::CaptureDevice&, String&&, const WebCore::MediaConstraints*, bool shouldCaptureInGPUProcess = false);

    class NoOpCaptureDeviceManager : public WebCore::CaptureDeviceManager {
    public:
        NoOpCaptureDeviceManager() = default;

    private:
        const Vector<WebCore::CaptureDevice>& captureDevices() final
        {
            ASSERT_NOT_REACHED();
            return m_emptyDevices;
        }
        Vector<WebCore::CaptureDevice> m_emptyDevices;
    };

    // IPC::MessageReceiver
    void didReceiveMessage(IPC::Connection&, IPC::Decoder&) final;

    // Messages::UserMediaCaptureManager
    void captureFailed(uint64_t id);
    void sourceStopped(uint64_t id);
    void sourceEnded(uint64_t id);
    void sourceMutedChanged(uint64_t id, bool muted);
    void sourceSettingsChanged(uint64_t id, const WebCore::RealtimeMediaSourceSettings&);
    void storageChanged(uint64_t id, const SharedMemory::Handle&, const WebCore::CAAudioStreamDescription&, uint64_t numberOfFrames);
    void ringBufferFrameBoundsChanged(uint64_t id, uint64_t startFrame, uint64_t endFrame);
    void audioSamplesAvailable(uint64_t id, MediaTime, uint64_t numberOfFrames, uint64_t startFrame, uint64_t endFrame);
    void remoteVideoSampleAvailable(uint64_t id, WebCore::RemoteVideoSample&&);

    void startProducingData(uint64_t);
    void stopProducingData(uint64_t);
    WebCore::RealtimeMediaSourceCapabilities capabilities(uint64_t);
    void applyConstraints(uint64_t, const WebCore::MediaConstraints&);
    void applyConstraintsSucceeded(uint64_t, const WebCore::RealtimeMediaSourceSettings&);
    void applyConstraintsFailed(uint64_t, String&&, String&&);

    class Source;
    friend class Source;

    void requestToEnd(uint64_t sourceID);
    Ref<WebCore::RealtimeMediaSource> cloneSource(Source&);
    Ref<WebCore::RealtimeMediaSource> cloneVideoSource(Source&);

    HashMap<uint64_t, RefPtr<Source>> m_sources;
    WebProcess& m_process;
    NoOpCaptureDeviceManager m_noOpCaptureDeviceManager;
    AudioFactory m_audioFactory;
    VideoFactory m_videoFactory;
    DisplayFactory m_displayFactory;
};

} // namespace WebKit

#endif
