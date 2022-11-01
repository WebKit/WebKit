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

RemoteRealtimeMediaSource::RemoteRealtimeMediaSource(RealtimeMediaSourceIdentifier identifier, const CaptureDevice& device, const MediaConstraints* constraints, MediaDeviceHashSalts&& hashSalts, UserMediaCaptureManager& manager, bool shouldCaptureInGPUProcess, PageIdentifier pageIdentifier)
    : RealtimeMediaSource(device, WTFMove(hashSalts), pageIdentifier)
    , m_proxy(identifier, device, shouldCaptureInGPUProcess, constraints)
    , m_manager(manager)
{
}

RemoteRealtimeMediaSource::RemoteRealtimeMediaSource(RemoteRealtimeMediaSourceProxy&& proxy, MediaDeviceHashSalts&& hashSalts, UserMediaCaptureManager& manager, PageIdentifier pageIdentifier)
    : RealtimeMediaSource(proxy.device(), WTFMove(hashSalts), pageIdentifier)
    , m_proxy(WTFMove(proxy))
    , m_manager(manager)
{
}

void RemoteRealtimeMediaSource::createRemoteMediaSource()
{
    m_proxy.createRemoteMediaSource(deviceIDHashSalts(), pageIdentifier(), [this, protectedThis = Ref { *this }](bool succeeded, auto&& errorMessage, auto&& settings, auto&& capabilities, auto&&, auto, auto) {
        if (!succeeded) {
            m_proxy.didFail(WTFMove(errorMessage));
            return;
        }

        setSettings(WTFMove(settings));
        setCapabilities(WTFMove(capabilities));
        setName(m_settings.label());

        m_proxy.setAsReady();
        if (m_proxy.shouldCaptureInGPUProcess())
            WebProcess::singleton().ensureGPUProcessConnection().addClient(*this);
    }, m_proxy.shouldCaptureInGPUProcess() && m_manager.shouldUseGPUProcessRemoteFrames());
}

void RemoteRealtimeMediaSource::removeAsClient()
{
    if (m_proxy.shouldCaptureInGPUProcess()) {
        if (auto* connection = WebProcess::singleton().existingGPUProcessConnection())
            connection->removeClient(*this);
    }
}

void RemoteRealtimeMediaSource::setCapabilities(RealtimeMediaSourceCapabilities&& capabilities)
{
    m_capabilities = WTFMove(capabilities);
}

void RemoteRealtimeMediaSource::setSettings(RealtimeMediaSourceSettings&& settings)
{
    auto changed = m_settings.difference(settings);
    m_settings = WTFMove(settings);
    notifySettingsDidChangeObservers(changed);
}

void RemoteRealtimeMediaSource::configurationChanged(String&& persistentID, WebCore::RealtimeMediaSourceSettings&& settings, WebCore::RealtimeMediaSourceCapabilities&& capabilities)
{
    setPersistentId(WTFMove(persistentID));
    setSettings(WTFMove(settings));
    setCapabilities(WTFMove(capabilities));
    setName(m_settings.label());
    
    forEachObserver([](auto& observer) {
        observer.sourceConfigurationChanged();
    });
}

void RemoteRealtimeMediaSource::applyConstraintsSucceeded(WebCore::RealtimeMediaSourceSettings&& settings)
{
    setSettings(WTFMove(settings));
    m_proxy.applyConstraintsSucceeded();
}

void RemoteRealtimeMediaSource::didEnd()
{
    if (m_proxy.isEnded())
        return;

    m_proxy.end();
    m_manager.removeSource(identifier());
    m_manager.remoteCaptureSampleManager().removeSource(identifier());
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
    m_proxy.applyConstraints(constraints, WTFMove(callback));
}

#if ENABLE(GPU_PROCESS)
void RemoteRealtimeMediaSource::gpuProcessConnectionDidClose(GPUProcessConnection&)
{
    ASSERT(m_proxy.shouldCaptureInGPUProcess());
    if (isEnded())
        return;

    m_proxy.updateConnection();
    m_manager.remoteCaptureSampleManager().didUpdateSourceConnection(m_proxy.connection());
    m_proxy.resetReady();
    createRemoteMediaSource();

    m_proxy.failApplyConstraintCallbacks("GPU Process terminated"_s);
    if (m_constraints)
        m_proxy.applyConstraints(*m_constraints, [](auto) { });

    if (isProducingData())
        startProducingData();

}
#endif

}

#endif
