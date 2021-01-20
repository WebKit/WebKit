/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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

#include <WebCore/CaptureDevice.h>
#include <WebCore/RealtimeMediaSource.h>
#include <WebCore/RealtimeMediaSourceIdentifier.h>
#include <wtf/Deque.h>

namespace IPC {
class Connection;
}

namespace WebCore {
class CAAudioStreamDescription;
class ImageTransferSessionVT;
struct MediaConstraints;
class RemoteVideoSample;
}

namespace WebKit {

class UserMediaCaptureManager;

class RemoteRealtimeMediaSource : public WebCore::RealtimeMediaSource {
public:
    static Ref<WebCore::RealtimeMediaSource> create(const WebCore::CaptureDevice&, const WebCore::MediaConstraints&, String&& name, String&& hashSalt, UserMediaCaptureManager&, bool shouldCaptureInGPUProcess = false);
    ~RemoteRealtimeMediaSource();

    WebCore::RealtimeMediaSourceIdentifier identifier() const { return m_identifier; }
    IPC::Connection* connection();

    void setSettings(WebCore::RealtimeMediaSourceSettings&&);

    void applyConstraintsSucceeded(const WebCore::RealtimeMediaSourceSettings&);
    void applyConstraintsFailed(String&& failedConstraint, String&& errorMessage);

    void captureStopped();
    void captureFailed() final;

    void remoteVideoSampleAvailable(WebCore::RemoteVideoSample&&);
    void remoteAudioSamplesAvailable(const WTF::MediaTime&, const WebCore::PlatformAudioData&, const WebCore::AudioStreamDescription&, size_t);

private:
    RemoteRealtimeMediaSource(WebCore::RealtimeMediaSourceIdentifier, WebCore::CaptureDevice::DeviceType, String&& name, String&& hashSalt, UserMediaCaptureManager&, bool shouldCaptureInGPUProcess);

    // RealtimeMediaSource
    void startProducingData() final;
    void stopProducingData() final;
    bool isCaptureSource() const final { return true; }
    void beginConfiguration() final { }
    void commitConfiguration() final { }
    bool setShouldApplyRotation(bool /* shouldApplyRotation */) final;
    void applyConstraints(const WebCore::MediaConstraints&, ApplyConstraintsHandler&&) final;
    void requestToEnd(Observer&) final;
    void stopBeingObserved() final;
    void hasEnded() final;
    const WebCore::RealtimeMediaSourceSettings& settings() final { return m_settings; }
    const WebCore::RealtimeMediaSourceCapabilities& capabilities() final;
    void whenReady(CompletionHandler<void(String)>&&) final;
    WebCore::CaptureDevice::DeviceType deviceType() const final { return m_deviceType; }
    Ref<RealtimeMediaSource> clone() final;

    void didFail(String&& errorMessage);
    void setAsReady();
    void setCapabilities(WebCore::RealtimeMediaSourceCapabilities&&);
    Ref<RealtimeMediaSource> cloneVideoSource();

    WebCore::RealtimeMediaSourceIdentifier m_identifier;
    UserMediaCaptureManager& m_manager;
    WebCore::RealtimeMediaSourceCapabilities m_capabilities;
    WebCore::RealtimeMediaSourceSettings m_settings;

    std::unique_ptr<WebCore::ImageTransferSessionVT> m_imageTransferSession;
    WebCore::CaptureDevice::DeviceType m_deviceType { WebCore::CaptureDevice::DeviceType::Unknown };

    Deque<ApplyConstraintsHandler> m_pendingApplyConstraintsCallbacks;
    bool m_shouldCaptureInGPUProcess { false };
    bool m_isReady { false };
    String m_errorMessage;
    CompletionHandler<void(String)> m_callback;
};

} // namespace WebKit

#endif
