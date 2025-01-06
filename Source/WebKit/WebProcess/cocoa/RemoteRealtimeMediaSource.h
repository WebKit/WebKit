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
#include <wtf/CheckedRef.h>

namespace WebKit {

class UserMediaCaptureManager;

class RemoteRealtimeMediaSource : public WebCore::RealtimeMediaSource
#if ENABLE(GPU_PROCESS)
    , public GPUProcessConnection::Client
#endif
    , public ThreadSafeRefCountedAndCanMakeThreadSafeWeakPtr<RemoteRealtimeMediaSource, WTF::DestructionThread::MainRunLoop>
{
public:
    ~RemoteRealtimeMediaSource();

    WebCore::RealtimeMediaSourceIdentifier identifier() const { return m_proxy.identifier(); }
    IPC::Connection& connection() { return m_proxy.connection(); }

    void setSettings(WebCore::RealtimeMediaSourceSettings&&);

    void applyConstraintsSucceeded(WebCore::RealtimeMediaSourceSettings&&);
    void applyConstraintsFailed(WebCore::MediaConstraintType invalidConstraint, String&& errorMessage) { m_proxy.applyConstraintsFailed(invalidConstraint, WTFMove(errorMessage)); }

    void captureStopped(bool didFail);
    void sourceMutedChanged(bool value, bool interrupted);

    void configurationChanged(String&& persistentID, WebCore::RealtimeMediaSourceSettings&&, WebCore::RealtimeMediaSourceCapabilities&&);

#if ENABLE(GPU_PROCESS)
    WTF_ABSTRACT_THREAD_SAFE_REF_COUNTED_AND_CAN_MAKE_WEAK_PTR_IMPL;
#endif

protected:
    RemoteRealtimeMediaSource(WebCore::RealtimeMediaSourceIdentifier, const WebCore::CaptureDevice&, const WebCore::MediaConstraints*, WebCore::MediaDeviceHashSalts&&, UserMediaCaptureManager&, bool shouldCaptureInGPUProcess, std::optional<WebCore::PageIdentifier>);
    RemoteRealtimeMediaSource(RemoteRealtimeMediaSourceProxy&&, WebCore::MediaDeviceHashSalts&&, UserMediaCaptureManager&, std::optional<WebCore::PageIdentifier>);
    void createRemoteMediaSource();

    RemoteRealtimeMediaSourceProxy& proxy() { return m_proxy; }
    UserMediaCaptureManager& manager();

    void setCapabilities(WebCore::RealtimeMediaSourceCapabilities&&);

    const WebCore::RealtimeMediaSourceSettings& settings() final { return m_settings; }
    const WebCore::RealtimeMediaSourceCapabilities& capabilities() final { return m_capabilities; }

    Ref<TakePhotoNativePromise> takePhoto(WebCore::PhotoSettings&&) final;
    Ref<PhotoCapabilitiesNativePromise> getPhotoCapabilities() final;
    Ref<PhotoSettingsNativePromise> getPhotoSettings() final;

private:
    // RealtimeMediaSource
    void startProducingData() final { m_proxy.startProducingData(*pageIdentifier()); }
    void stopProducingData() final { m_proxy.stopProducingData(); }
    void endProducingData() final { m_proxy.endProducingData(); }
    bool isCaptureSource() const final { return true; }
    void applyConstraints(const WebCore::MediaConstraints&, ApplyConstraintsHandler&&) final;
    void didEnd() final;
    void whenReady(CompletionHandler<void(WebCore::CaptureSourceError&&)>&& callback) final { m_proxy.whenReady(WTFMove(callback)); }
    WebCore::CaptureDevice::DeviceType deviceType() const final { return m_proxy.deviceType(); }
    bool interrupted() const final { return m_proxy.interrupted(); }
    bool isPowerEfficient() const final { return m_proxy.isPowerEfficient(); }

#if ENABLE(GPU_PROCESS)
    // GPUProcessConnection::Client
    void gpuProcessConnectionDidClose(GPUProcessConnection&) final;
#endif

    RemoteRealtimeMediaSourceProxy m_proxy;
    CheckedRef<UserMediaCaptureManager> m_manager;
    std::optional<WebCore::MediaConstraints> m_constraints;
    WebCore::RealtimeMediaSourceCapabilities m_capabilities;
    std::optional<WebCore::PhotoCapabilities> m_photoCapabilities;
    WebCore::RealtimeMediaSourceSettings m_settings;
};

inline void RemoteRealtimeMediaSource::sourceMutedChanged(bool muted, bool interrupted)
{
    m_proxy.setInterrupted(interrupted);
    notifyMutedChange(muted);
}

} // namespace WebKit

#endif
