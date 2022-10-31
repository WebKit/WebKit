/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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

#include "GPUProcessConnection.h"
#include "RemoteRealtimeMediaSourceProxy.h"
#include <WebCore/RealtimeMediaSource.h>

namespace WebKit {

class UserMediaCaptureManager;

class RemoteRealtimeMediaSource : public WebCore::RealtimeMediaSource
#if ENABLE(GPU_PROCESS)
    , public GPUProcessConnection::Client
#endif
{
public:
    RemoteRealtimeMediaSource(WebCore::RealtimeMediaSourceIdentifier, const WebCore::CaptureDevice&, const WebCore::MediaConstraints*, WebCore::MediaDeviceHashSalts&&, UserMediaCaptureManager&, bool shouldCaptureInGPUProcess, WebCore::PageIdentifier);
    RemoteRealtimeMediaSource(RemoteRealtimeMediaSourceProxy&&, WebCore::MediaDeviceHashSalts&&, UserMediaCaptureManager&, WebCore::PageIdentifier);

    WebCore::RealtimeMediaSourceIdentifier identifier() const { return m_proxy.identifier(); }
    IPC::Connection& connection() { return m_proxy.connection(); }

    void setSettings(WebCore::RealtimeMediaSourceSettings&&);

    void applyConstraintsSucceeded(WebCore::RealtimeMediaSourceSettings&&);
    void applyConstraintsFailed(String&& failedConstraint, String&& errorMessage) { m_proxy.applyConstraintsFailed(WTFMove(failedConstraint), WTFMove(errorMessage)); }

    void captureStopped(bool didFail);
    void sourceMutedChanged(bool value, bool interrupted);

    void configurationChanged(String&& persistentID, WebCore::RealtimeMediaSourceSettings&&, WebCore::RealtimeMediaSourceCapabilities&&);

protected:
    void createRemoteMediaSource();
    void removeAsClient();

    RemoteRealtimeMediaSourceProxy& proxy() { return m_proxy; }
    UserMediaCaptureManager& manager() { return m_manager; }

    void setCapabilities(WebCore::RealtimeMediaSourceCapabilities&&);

    const WebCore::RealtimeMediaSourceSettings& settings() final { return m_settings; }
    const WebCore::RealtimeMediaSourceCapabilities& capabilities() final { return m_capabilities; }

private:
    // RealtimeMediaSource
    void startProducingData() final { m_proxy.startProducingData(); }
    void stopProducingData() final { m_proxy.stopProducingData(); }
    bool isCaptureSource() const final { return true; }
    void applyConstraints(const WebCore::MediaConstraints&, ApplyConstraintsHandler&&) final;
    void didEnd() final;
    void whenReady(CompletionHandler<void(String)>&& callback) final { m_proxy.whenReady(WTFMove(callback)); }
    WebCore::CaptureDevice::DeviceType deviceType() const final { return m_proxy.deviceType(); }
    bool interrupted() const final { return m_proxy.interrupted(); }

#if ENABLE(GPU_PROCESS)
    // GPUProcessConnection::Client
    void gpuProcessConnectionDidClose(GPUProcessConnection&) final;
#endif

    RemoteRealtimeMediaSourceProxy m_proxy;
    UserMediaCaptureManager& m_manager;
    std::optional<WebCore::MediaConstraints> m_constraints;
    WebCore::RealtimeMediaSourceCapabilities m_capabilities;
    WebCore::RealtimeMediaSourceSettings m_settings;
};

inline void RemoteRealtimeMediaSource::sourceMutedChanged(bool muted, bool interrupted)
{
    m_proxy.setInterrupted(interrupted);
    notifyMutedChange(muted);
}

} // namespace WebKit

#endif
