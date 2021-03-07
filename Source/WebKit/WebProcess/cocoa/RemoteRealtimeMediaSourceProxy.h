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

#include "UserMediaCaptureManagerProxyMessages.h"
#include <WebCore/RealtimeMediaSource.h>

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

class RemoteRealtimeMediaSourceProxy {
public:
    RemoteRealtimeMediaSourceProxy(WebCore::RealtimeMediaSourceIdentifier, const WebCore::CaptureDevice&, bool shouldCaptureInGPUProcess, const WebCore::MediaConstraints*);
    ~RemoteRealtimeMediaSourceProxy();

    IPC::Connection* connection();
    WebCore::RealtimeMediaSourceIdentifier identifier() const { return m_identifier; }
    WebCore::CaptureDevice::DeviceType deviceType() const { return m_device.type(); }
    bool shouldCaptureInGPUProcess() const { return m_shouldCaptureInGPUProcess; }

    using CreateCallback = CompletionHandler<void(bool, String&&, WebCore::RealtimeMediaSourceSettings&&, WebCore::RealtimeMediaSourceCapabilities&&, Vector<WebCore::VideoPresetData>&&, WebCore::IntSize, double)>;
    void createRemoteMediaSource(const String&, CreateCallback&&);

    void applyConstraintsSucceeded();
    void applyConstraintsFailed(String&& failedConstraint, String&& errorMessage);
    void failApplyConstraintCallbacks(const String& errorMessage);
    
    void hasEnded();
    void startProducingData();
    void stopProducingData();
    void applyConstraints(const WebCore::MediaConstraints&, WebCore::RealtimeMediaSource::ApplyConstraintsHandler&&);

    void whenReady(CompletionHandler<void(String)>&&);
    void setAsReady();
    void resetReady() { m_isReady = false; }
    bool isReady() const { return m_isReady; }

    void didFail(String&& errorMessage);

private:
    WebCore::RealtimeMediaSourceIdentifier m_identifier;
    WebCore::CaptureDevice m_device;
    bool m_shouldCaptureInGPUProcess { false };

    WebCore::MediaConstraints m_constraints;
    Deque<WebCore::RealtimeMediaSource::ApplyConstraintsHandler> m_pendingApplyConstraintsCallbacks;
    bool m_isReady { false };
    CompletionHandler<void(String)> m_callback;
    String m_errorMessage;
};

inline RemoteRealtimeMediaSourceProxy::RemoteRealtimeMediaSourceProxy(WebCore::RealtimeMediaSourceIdentifier identifier, const WebCore::CaptureDevice& device, bool shouldCaptureInGPUProcess, const WebCore::MediaConstraints* constraints)
    : m_identifier(identifier)
    , m_device(device)
    , m_shouldCaptureInGPUProcess(shouldCaptureInGPUProcess)
{
    if (constraints)
        m_constraints = *constraints;
}

} // namespace WebKit

#endif
