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

#include "config.h"
#include "RemoteRealtimeAudioSource.h"

#if PLATFORM(COCOA) && ENABLE(MEDIA_STREAM)

#include "GPUProcessConnection.h"
#include "SharedRingBufferStorage.h"
#include "UserMediaCaptureManager.h"
#include "UserMediaCaptureManagerMessages.h"
#include "UserMediaCaptureManagerProxyMessages.h"
#include "WebCoreArgumentCoders.h"
#include "WebProcess.h"
#include <WebCore/MediaConstraints.h>
#include <WebCore/RealtimeMediaSource.h>
#include <WebCore/RealtimeMediaSourceCenter.h>
#include <WebCore/WebAudioBufferList.h>

namespace WebKit {
using namespace WebCore;

Ref<RealtimeMediaSource> RemoteRealtimeAudioSource::create(const CaptureDevice& device, const MediaConstraints* constraints, String&& name, String&& hashSalt, UserMediaCaptureManager& manager, bool shouldCaptureInGPUProcess)
{
    auto source = adoptRef(*new RemoteRealtimeAudioSource(RealtimeMediaSourceIdentifier::generate(), device, constraints, WTFMove(name), WTFMove(hashSalt), manager, shouldCaptureInGPUProcess));
    manager.addSource(source.copyRef());
    manager.remoteCaptureSampleManager().addSource(source.copyRef());
    source->createRemoteMediaSource();
    return source;
}

RemoteRealtimeAudioSource::RemoteRealtimeAudioSource(RealtimeMediaSourceIdentifier identifier, const CaptureDevice& device, const MediaConstraints* constraints, String&& name, String&& hashSalt, UserMediaCaptureManager& manager, bool shouldCaptureInGPUProcess)
    : RealtimeMediaSource(RealtimeMediaSource::Type::Audio, WTFMove(name), String { device.persistentId() }, WTFMove(hashSalt))
    , m_proxy(identifier, device, shouldCaptureInGPUProcess, constraints)
    , m_manager(manager)
{
    ASSERT(device.type() == CaptureDevice::DeviceType::Microphone);
#if PLATFORM(IOS_FAMILY)
    RealtimeMediaSourceCenter::singleton().audioCaptureFactory().setActiveSource(*this);
#endif
}

void RemoteRealtimeAudioSource::createRemoteMediaSource()
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

RemoteRealtimeAudioSource::~RemoteRealtimeAudioSource()
{
    if (m_proxy.shouldCaptureInGPUProcess()) {
        if (auto* connection = WebProcess::singleton().existingGPUProcessConnection())
            connection->removeClient(*this);
    }

#if PLATFORM(IOS_FAMILY)
    RealtimeMediaSourceCenter::singleton().audioCaptureFactory().unsetActiveSource(*this);
#endif
}

void RemoteRealtimeAudioSource::setCapabilities(RealtimeMediaSourceCapabilities&& capabilities)
{
    m_capabilities = WTFMove(capabilities);
}

void RemoteRealtimeAudioSource::setSettings(RealtimeMediaSourceSettings&& settings)
{
    auto changed = m_settings.difference(settings);
    m_settings = WTFMove(settings);
    notifySettingsDidChangeObservers(changed);
}

void RemoteRealtimeAudioSource::applyConstraintsSucceeded(WebCore::RealtimeMediaSourceSettings&& settings)
{
    setSettings(WTFMove(settings));
    m_proxy.applyConstraintsSucceeded();
}

void RemoteRealtimeAudioSource::remoteAudioSamplesAvailable(const MediaTime& time, const PlatformAudioData& data, const AudioStreamDescription& description, size_t size)
{
    ASSERT(!isMainRunLoop());
    audioSamplesAvailable(time, data, description, size);
}

void RemoteRealtimeAudioSource::hasEnded()
{
    m_proxy.hasEnded();
    m_manager.removeSource(identifier());
    m_manager.remoteCaptureSampleManager().removeSource(identifier());
}

void RemoteRealtimeAudioSource::captureStopped()
{
    stop();
    hasEnded();
}

void RemoteRealtimeAudioSource::captureFailed()
{
    RealtimeMediaSource::captureFailed();
    hasEnded();
}

void RemoteRealtimeAudioSource::applyConstraints(const MediaConstraints& constraints, ApplyConstraintsHandler&& callback)
{
    m_constraints = constraints;
    m_proxy.applyConstraints(constraints, WTFMove(callback));
}

#if ENABLE(GPU_PROCESS)
void RemoteRealtimeAudioSource::gpuProcessConnectionDidClose(GPUProcessConnection&)
{
    ASSERT(m_proxy.shouldCaptureInGPUProcess());
    if (isEnded())
        return;

#if PLATFORM(IOS_FAMILY)
    if (this != RealtimeMediaSourceCenter::singleton().audioCaptureFactory().activeSource()) {
        // Track is muted and has no chance of being unmuted, let's end it.
        captureFailed();
        return;
    }
#endif

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
