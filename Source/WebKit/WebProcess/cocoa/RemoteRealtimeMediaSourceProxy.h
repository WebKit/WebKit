/*
 * Copyright (C) 2020-2022 Apple Inc. All rights reserved.
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

#include <WebCore/RealtimeMediaSource.h>
#include <wtf/Deque.h>

namespace IPC {
class Connection;
}

namespace WebCore {
class CAAudioStreamDescription;
class ImageTransferSessionVT;
struct MediaConstraints;
enum class MediaAccessDenialReason : uint8_t;
}

namespace WebKit {

class RemoteRealtimeMediaSourceProxy {
public:
    RemoteRealtimeMediaSourceProxy(WebCore::RealtimeMediaSourceIdentifier, const WebCore::CaptureDevice&, bool shouldCaptureInGPUProcess, const WebCore::MediaConstraints*);
    ~RemoteRealtimeMediaSourceProxy();

    RemoteRealtimeMediaSourceProxy(RemoteRealtimeMediaSourceProxy&&) = default;
    RemoteRealtimeMediaSourceProxy& operator=(RemoteRealtimeMediaSourceProxy&&) = default;

    IPC::Connection& connection() { return m_connection.get(); }
    WebCore::RealtimeMediaSourceIdentifier identifier() const { return m_identifier; }
    WebCore::CaptureDevice::DeviceType deviceType() const { return m_device.type(); }
    const WebCore::CaptureDevice& device() const { return m_device; }
    bool shouldCaptureInGPUProcess() const { return m_shouldCaptureInGPUProcess; }

    using CreateCallback = CompletionHandler<void(WebCore::CaptureSourceError&&, WebCore::RealtimeMediaSourceSettings&&, WebCore::RealtimeMediaSourceCapabilities&&)>;
    void createRemoteMediaSource(const WebCore::MediaDeviceHashSalts&, WebCore::PageIdentifier, CreateCallback&&, bool shouldUseRemoteFrame = false);

    RemoteRealtimeMediaSourceProxy clone();
    void createRemoteCloneSource(WebCore::RealtimeMediaSourceIdentifier, WebCore::PageIdentifier);

    void applyConstraintsSucceeded();
    void applyConstraintsFailed(WebCore::MediaConstraintType, String&& errorMessage);
    void failApplyConstraintCallbacks(const String& errorMessage);

    bool isEnded() const { return m_isEnded; }
    void end();
    void startProducingData(WebCore::PageIdentifier);
    void stopProducingData();
    void endProducingData();
    void applyConstraints(const WebCore::MediaConstraints&, WebCore::RealtimeMediaSource::ApplyConstraintsHandler&&);

    Ref<WebCore::RealtimeMediaSource::TakePhotoNativePromise> takePhoto(WebCore::PhotoSettings&&);
    Ref<WebCore::RealtimeMediaSource::PhotoCapabilitiesNativePromise> getPhotoCapabilities();
    Ref<WebCore::RealtimeMediaSource::PhotoSettingsNativePromise> getPhotoSettings();

    void whenReady(CompletionHandler<void(WebCore::CaptureSourceError&&)>&&);
    void setAsReady();
    void resetReady() { m_isReady = false; }
    bool isReady() const { return m_isReady; }

    void didFail(WebCore::CaptureSourceError&&);

    bool interrupted() const { return m_interrupted; }
    void setInterrupted(bool interrupted) { m_interrupted = interrupted; }

    void updateConnection();

    bool isPowerEfficient() const;

private:
    struct PromiseConverter;

    WebCore::RealtimeMediaSourceIdentifier m_identifier;
    Ref<IPC::Connection> m_connection;
    WebCore::CaptureDevice m_device;
    bool m_shouldCaptureInGPUProcess { false };

    WebCore::MediaConstraints m_constraints;
    Deque<std::pair<WebCore::RealtimeMediaSource::ApplyConstraintsHandler, WebCore::MediaConstraints>> m_pendingApplyConstraintsRequests;
    bool m_isReady { false };
    CompletionHandler<void(WebCore::CaptureSourceError&&)> m_callback;
    WebCore::CaptureSourceError m_failureReason;
    bool m_interrupted { false };
    bool m_isEnded { false };
};

} // namespace WebKit

#endif
