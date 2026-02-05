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

#include "config.h"
#include "RemoteRealtimeMediaSource.h"

#if PLATFORM(COCOA) && ENABLE(MEDIA_STREAM)

#include "GPUProcessConnection.h"
#include "UserMediaCaptureManager.h"
#include "WebProcess.h"
#include <WebCore/MediaConstraints.h>

namespace WebKit {
using namespace WebCore;

RemoteRealtimeMediaSource::RemoteRealtimeMediaSource(RealtimeMediaSourceIdentifier identifier, const CaptureDevice& device, const MediaConstraints* constraints, MediaDeviceHashSalts&& hashSalts, UserMediaCaptureManager& manager, bool shouldCaptureInGPUProcess, std::optional<PageIdentifier> pageIdentifier)
    : RealtimeMediaSource(device, WTF::move(hashSalts), pageIdentifier)
    , m_proxy(identifier, device, shouldCaptureInGPUProcess, constraints)
    , m_manager(manager)
{
}

RemoteRealtimeMediaSource::RemoteRealtimeMediaSource(RemoteRealtimeMediaSourceProxy&& proxy, MediaDeviceHashSalts&& hashSalts, UserMediaCaptureManager& manager, std::optional<PageIdentifier> pageIdentifier)
    : RealtimeMediaSource(proxy.device(), WTF::move(hashSalts), pageIdentifier)
    , m_proxy(WTF::move(proxy))
    , m_manager(manager)
{
}

RemoteRealtimeMediaSource::~RemoteRealtimeMediaSource() = default;

void RemoteRealtimeMediaSource::createRemoteMediaSource()
{
    m_proxy.createRemoteMediaSource(deviceIDHashSalts(), *pageIdentifier(), [this, protectedThis = Ref { *this }](WebCore::CaptureSourceError&& error, WebCore::RealtimeMediaSourceSettings&& settings, WebCore::RealtimeMediaSourceCapabilities&& capabilities) {
        if (error) {
            m_proxy.didFail(WTF::move(error));
            return;
        }

        setSettings(WTF::move(settings));
        setCapabilities(WTF::move(capabilities));
        setName(m_settings.label());

        m_proxy.setAsReady();
        if (m_proxy.shouldCaptureInGPUProcess())
            WebProcess::singleton().ensureProtectedGPUProcessConnection()->addClient(*this);
    }, m_proxy.shouldCaptureInGPUProcess() && m_manager->shouldUseGPUProcessRemoteFrames());
}

void RemoteRealtimeMediaSource::setCapabilities(RealtimeMediaSourceCapabilities&& capabilities)
{
    m_capabilities = WTF::move(capabilities);
}

void RemoteRealtimeMediaSource::setSettings(RealtimeMediaSourceSettings&& settings)
{
    auto changed = m_settings.difference(settings);
    m_settings = WTF::move(settings);
    notifySettingsDidChangeObservers(changed);
}

Ref<RealtimeMediaSource::TakePhotoNativePromise> RemoteRealtimeMediaSource::takePhoto(PhotoSettings&& settings)
{
    return m_proxy.takePhoto(WTF::move(settings));
}

Ref<RealtimeMediaSource::PhotoCapabilitiesNativePromise> RemoteRealtimeMediaSource::getPhotoCapabilities()
{
    return m_proxy.getPhotoCapabilities();
}

Ref<RealtimeMediaSource::PhotoSettingsNativePromise> RemoteRealtimeMediaSource::getPhotoSettings()
{
    return m_proxy.getPhotoSettings();
}

void RemoteRealtimeMediaSource::configurationChanged(String&& persistentID, WebCore::RealtimeMediaSourceSettings&& settings, WebCore::RealtimeMediaSourceCapabilities&& capabilities)
{
    setPersistentId(WTF::move(persistentID));
    setSettings(WTF::move(settings));
    setCapabilities(WTF::move(capabilities));
    setName(m_settings.label());

    RealtimeMediaSource::configurationChanged();
}

void RemoteRealtimeMediaSource::applyConstraintsSucceeded(WebCore::RealtimeMediaSourceSettings&& settings)
{
    setSettings(WTF::move(settings));
    m_proxy.applyConstraintsSucceeded();
}

void RemoteRealtimeMediaSource::stopProducingData()
{
    if (isAudio())
        protect(m_manager->remoteCaptureSampleManager())->audioSourceWillBeStopped(identifier());
    m_proxy.stopProducingData();
}

void RemoteRealtimeMediaSource::didEnd()
{
    if (m_proxy.isEnded())
        return;

    m_proxy.end();
    m_manager->removeSource(identifier());
    protect(m_manager->remoteCaptureSampleManager())->removeSource(identifier());
}

void RemoteRealtimeMediaSource::captureStopped(bool didFail)
{
    if (didFail) {
        captureFailed();
        return;
    }
    end();
}

void RemoteRealtimeMediaSource::applyConstraints(const MediaConstraints& constraints, ApplyConstraintsHandler&& callback)
{
    m_constraints = constraints;
    m_proxy.applyConstraints(constraints, WTF::move(callback));
}

#if ENABLE(GPU_PROCESS)
void RemoteRealtimeMediaSource::gpuProcessConnectionDidClose(GPUProcessConnection&)
{
    ASSERT(m_proxy.shouldCaptureInGPUProcess());
    if (isEnded())
        return;

    m_proxy.updateConnection();
    protect(m_manager->remoteCaptureSampleManager())->didUpdateSourceConnection(Ref { m_proxy.connection() });
    m_proxy.resetReady();
    createRemoteMediaSource();

    m_proxy.failApplyConstraintCallbacks("GPU Process terminated"_s);
    if (m_constraints)
        m_proxy.applyConstraints(*m_constraints, [](auto) { });

    if (isProducingData())
        startProducingData();
    else if (isAudio() && !interrupted()) {
        // To be able to reenable voice detection, we have to restart the source.
        startProducingData();
        stopProducingData();
    }
}
#endif

}

#endif
