/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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
#include "RemoteRealtimeDisplaySource.h"

#if PLATFORM(COCOA) && ENABLE(MEDIA_STREAM)

#include "UserMediaCaptureManager.h"
#include "UserMediaCaptureManagerMessages.h"
#include "UserMediaCaptureManagerProxyMessages.h"
#include "WebCoreArgumentCoders.h"
#include "WebProcess.h"
#include <WebCore/MediaConstraints.h>

namespace WebKit {
using namespace WebCore;

Ref<RealtimeMediaSource> RemoteRealtimeDisplaySource::create(const CaptureDevice& device, const MediaConstraints* constraints, String&& name, String&& hashSalt, UserMediaCaptureManager& manager, bool shouldCaptureInGPUProcess)
{
    auto source = adoptRef(*new RemoteRealtimeDisplaySource(RealtimeMediaSourceIdentifier::generate(), device, constraints, WTFMove(name), WTFMove(hashSalt), manager, shouldCaptureInGPUProcess));
    manager.addSource(source.copyRef());
    manager.remoteCaptureSampleManager().addSource(source.copyRef());
    source->createRemoteMediaSource();
    return source;
}

RemoteRealtimeDisplaySource::RemoteRealtimeDisplaySource(RealtimeMediaSourceIdentifier identifier, const CaptureDevice& device, const MediaConstraints* constraints, String&& name, String&& hashSalt, UserMediaCaptureManager& manager, bool shouldCaptureInGPUProcess)
    : RealtimeMediaSource(RealtimeMediaSource::Type::Video, WTFMove(name), String { device.persistentId() }, WTFMove(hashSalt))
    , m_proxy(identifier, device, shouldCaptureInGPUProcess, constraints)
    , m_manager(manager)
{
    ASSERT(device.type() == CaptureDevice::DeviceType::Screen || device.type() == CaptureDevice::DeviceType::Window);
}

void RemoteRealtimeDisplaySource::createRemoteMediaSource()
{
    m_proxy.createRemoteMediaSource(deviceIDHashSalt(), [this, protectedThis = Ref { *this }](bool succeeded, auto&& errorMessage, auto&& settings, auto&& capabilities, auto&&, auto, auto) {
        if (!succeeded) {
            m_proxy.didFail(WTFMove(errorMessage));
            return;
        }

        setSettings(WTFMove(settings));
        setCapabilities(WTFMove(capabilities));
        setName(String { m_settings.label().string() });

        m_proxy.setAsReady();
        if (m_proxy.shouldCaptureInGPUProcess())
            WebProcess::singleton().ensureGPUProcessConnection().addClient(*this);
    });
}

RemoteRealtimeDisplaySource::~RemoteRealtimeDisplaySource()
{
    if (m_proxy.shouldCaptureInGPUProcess()) {
        if (auto* connection = WebProcess::singleton().existingGPUProcessConnection())
            connection->removeClient(*this);
    }
}

void RemoteRealtimeDisplaySource::setCapabilities(RealtimeMediaSourceCapabilities&& capabilities)
{
    m_capabilities = WTFMove(capabilities);
}

void RemoteRealtimeDisplaySource::setSettings(RealtimeMediaSourceSettings&& settings)
{
    auto changed = m_settings.difference(settings);
    m_settings = WTFMove(settings);
    notifySettingsDidChangeObservers(changed);
}

void RemoteRealtimeDisplaySource::applyConstraintsSucceeded(WebCore::RealtimeMediaSourceSettings&& settings)
{
    setSettings(WTFMove(settings));
    m_proxy.applyConstraintsSucceeded();
}

void RemoteRealtimeDisplaySource::hasEnded()
{
    m_proxy.hasEnded();
    m_manager.removeSource(identifier());
    m_manager.remoteCaptureSampleManager().removeSource(identifier());
}

void RemoteRealtimeDisplaySource::captureStopped()
{
    stop();
    hasEnded();
}

void RemoteRealtimeDisplaySource::captureFailed()
{
    RealtimeMediaSource::captureFailed();
    hasEnded();
}

void RemoteRealtimeDisplaySource::applyConstraints(const MediaConstraints& constraints, ApplyConstraintsHandler&& callback)
{
    m_constraints = constraints;
    m_proxy.applyConstraints(constraints, WTFMove(callback));
}

#if ENABLE(GPU_PROCESS)
void RemoteRealtimeDisplaySource::gpuProcessConnectionDidClose(GPUProcessConnection&)
{
    ASSERT(m_proxy.shouldCaptureInGPUProcess());
    if (isEnded())
        return;

    m_manager.remoteCaptureSampleManager().didUpdateSourceConnection(connection());
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
