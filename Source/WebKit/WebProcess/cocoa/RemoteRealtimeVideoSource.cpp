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

#include "config.h"
#include "RemoteRealtimeVideoSource.h"

#if PLATFORM(COCOA) && ENABLE(MEDIA_STREAM)

#include "UserMediaCaptureManager.h"
#include "UserMediaCaptureManagerMessages.h"
#include "UserMediaCaptureManagerProxyMessages.h"
#include "WebCoreArgumentCoders.h"
#include "WebProcess.h"
#include <WebCore/MediaConstraints.h>

namespace WebKit {
using namespace WebCore;

Ref<RealtimeMediaSource> RemoteRealtimeVideoSource::create(const CaptureDevice& device, const MediaConstraints* constraints, String&& hashSalt, UserMediaCaptureManager& manager, bool shouldCaptureInGPUProcess, PageIdentifier pageIdentifier)
{
    auto source = adoptRef(*new RemoteRealtimeVideoSource(RealtimeMediaSourceIdentifier::generate(), device, constraints, WTFMove(hashSalt), manager, shouldCaptureInGPUProcess, pageIdentifier));
    manager.addSource(source.copyRef());
    manager.remoteCaptureSampleManager().addSource(source.copyRef());
    source->createRemoteMediaSource();
    return source;
}

RemoteRealtimeVideoSource::RemoteRealtimeVideoSource(RealtimeMediaSourceIdentifier identifier, const CaptureDevice& device, const MediaConstraints* constraints, String&& hashSalt, UserMediaCaptureManager& manager, bool shouldCaptureInGPUProcess, PageIdentifier pageIdentifier)
    : RealtimeMediaSource(RealtimeMediaSource::Type::Video, String { device.label() }, String { device.persistentId() }, WTFMove(hashSalt), pageIdentifier)
    , m_proxy(identifier, device, shouldCaptureInGPUProcess, constraints)
    , m_manager(manager)
{
}

RemoteRealtimeVideoSource::RemoteRealtimeVideoSource(RemoteRealtimeMediaSourceProxy&& proxy, String&& hashSalt, UserMediaCaptureManager& manager, PageIdentifier pageIdentifier)
    : RealtimeMediaSource(RealtimeMediaSource::Type::Video, String { proxy.device().label() }, String { proxy.device().persistentId() }, WTFMove(hashSalt), pageIdentifier)
    , m_proxy(WTFMove(proxy))
    , m_manager(manager)
{
}

void RemoteRealtimeVideoSource::createRemoteMediaSource()
{
    m_proxy.createRemoteMediaSource(deviceIDHashSalt(), pageIdentifier(), [this, protectedThis = Ref { *this }](bool succeeded, auto&& errorMessage, auto&& settings, auto&& capabilities, auto&&, auto, auto) {
        if (!succeeded) {
            m_proxy.didFail(WTFMove(errorMessage));
            return;
        }

        setSettings(WTFMove(settings));
        setCapabilities(WTFMove(capabilities));

        m_proxy.setAsReady();
        if (m_proxy.shouldCaptureInGPUProcess())
            WebProcess::singleton().ensureGPUProcessConnection().addClient(*this);
    }, m_proxy.shouldCaptureInGPUProcess() && m_manager.shouldUseGPUProcessRemoteFrames());
}

RemoteRealtimeVideoSource::~RemoteRealtimeVideoSource()
{
    if (m_proxy.shouldCaptureInGPUProcess()) {
        if (auto* connection = WebProcess::singleton().existingGPUProcessConnection())
            connection->removeClient(*this);
    }
}

void RemoteRealtimeVideoSource::endProducingData()
{
    m_proxy.endProducingData();
}

void RemoteRealtimeVideoSource::setCapabilities(RealtimeMediaSourceCapabilities&& capabilities)
{
    m_capabilities = WTFMove(capabilities);
}

void RemoteRealtimeVideoSource::setSettings(RealtimeMediaSourceSettings&& settings)
{
    auto changed = m_settings.difference(settings);
    m_settings = WTFMove(settings);
    notifySettingsDidChangeObservers(changed);
}

bool RemoteRealtimeVideoSource::setShouldApplyRotation(bool shouldApplyRotation)
{
    connection()->send(Messages::UserMediaCaptureManagerProxy::SetShouldApplyRotation { identifier(), shouldApplyRotation }, 0);
    return true;
}

void RemoteRealtimeVideoSource::applyConstraintsSucceeded(WebCore::RealtimeMediaSourceSettings&& settings)
{
    setSettings(WTFMove(settings));
    m_proxy.applyConstraintsSucceeded();
}

void RemoteRealtimeVideoSource::hasEnded()
{
    if (m_proxy.isEnded())
        return;

    m_proxy.end();
    m_manager.removeSource(identifier());
    m_manager.remoteCaptureSampleManager().removeSource(identifier());
}

Ref<RealtimeMediaSource> RemoteRealtimeVideoSource::clone()
{
    if (isEnded() || m_proxy.isEnded())
        return *this;

    auto source = adoptRef(*new RemoteRealtimeVideoSource(m_proxy.clone(), deviceIDHashSalt(), m_manager, pageIdentifier()));

    source->setSettings(RealtimeMediaSourceSettings { settings() });
    source->setCapabilities(RealtimeMediaSourceCapabilities { capabilities() });

    m_manager.addSource(source.copyRef());
    m_manager.remoteCaptureSampleManager().addSource(source.copyRef());

    m_proxy.createRemoteCloneSource(source->identifier());

    return source;
}

void RemoteRealtimeVideoSource::captureStopped(bool didFail)
{
    if (didFail)
        captureFailed();
    else
        end();
    hasEnded();
}

void RemoteRealtimeVideoSource::applyConstraints(const MediaConstraints& constraints, ApplyConstraintsHandler&& callback)
{
    m_constraints = constraints;
    m_proxy.applyConstraints(constraints, WTFMove(callback));
}

#if ENABLE(GPU_PROCESS)
void RemoteRealtimeVideoSource::gpuProcessConnectionDidClose(GPUProcessConnection&)
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
