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
using namespace PAL;
using namespace WebCore;

Ref<RealtimeMediaSource> RemoteRealtimeAudioSource::create(const CaptureDevice& device, const MediaConstraints* constraints, String&& name, String&& hashSalt, UserMediaCaptureManager& manager, bool shouldCaptureInGPUProcess)
{
    auto source = adoptRef(*new RemoteRealtimeAudioSource(RealtimeMediaSourceIdentifier::generate(), device, constraints, WTFMove(name), WTFMove(hashSalt), manager, shouldCaptureInGPUProcess));
    manager.addAudioSource(source.copyRef());
    manager.remoteCaptureSampleManager().addSource(source.copyRef());
    source->createRemoteMediaSource();
    return source;
}

RemoteRealtimeAudioSource::RemoteRealtimeAudioSource(RealtimeMediaSourceIdentifier identifier, const CaptureDevice& device, const MediaConstraints* constraints, String&& name, String&& hashSalt, UserMediaCaptureManager& manager, bool shouldCaptureInGPUProcess)
    : RealtimeMediaSource(RealtimeMediaSource::Type::Audio, WTFMove(name), String::number(identifier.toUInt64()), WTFMove(hashSalt))
    , m_identifier(identifier)
    , m_manager(manager)
    , m_shouldCaptureInGPUProcess(shouldCaptureInGPUProcess)
    , m_device(device)
{
    if (constraints)
        m_constraints = *constraints;

    ASSERT(m_device.type() == CaptureDevice::DeviceType::Microphone);
#if PLATFORM(IOS_FAMILY)
    RealtimeMediaSourceCenter::singleton().audioCaptureFactory().setActiveSource(*this);
#endif
}

void RemoteRealtimeAudioSource::createRemoteMediaSource()
{
    connection()->sendWithAsyncReply(Messages::UserMediaCaptureManagerProxy::CreateMediaSourceForCaptureDeviceWithConstraints(identifier(), m_device, deviceIDHashSalt(), m_constraints), [this, protectedThis = makeRef(*this)](bool succeeded, auto&& errorMessage, auto&& settings, auto&& capabilities, auto&&, auto, auto) {
        if (!succeeded) {
            didFail(WTFMove(errorMessage));
            return;
        }
        setSettings(WTFMove(settings));
        setCapabilities(WTFMove(capabilities));
        setAsReady();
        if (m_shouldCaptureInGPUProcess)
            WebProcess::singleton().ensureGPUProcessConnection().addClient(*this);
    });
}

RemoteRealtimeAudioSource::~RemoteRealtimeAudioSource()
{
    if (m_shouldCaptureInGPUProcess)
        WebProcess::singleton().ensureGPUProcessConnection().removeClient(*this);

#if PLATFORM(IOS_FAMILY)
    RealtimeMediaSourceCenter::singleton().audioCaptureFactory().unsetActiveSource(*this);
#endif
}

void RemoteRealtimeAudioSource::whenReady(CompletionHandler<void(String)>&& callback)
{
    if (m_isReady)
        return callback(WTFMove(m_errorMessage));
    m_callback = WTFMove(callback);
}

void RemoteRealtimeAudioSource::didFail(String&& errorMessage)
{
    m_isReady = true;
    m_errorMessage = WTFMove(errorMessage);
    if (m_callback)
        m_callback(m_errorMessage);
}

void RemoteRealtimeAudioSource::setAsReady()
{
    ASSERT(!m_isReady);
    m_isReady = true;

    setName(String { m_settings.label().string() });
    if (m_callback)
        m_callback({ });
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

void RemoteRealtimeAudioSource::remoteAudioSamplesAvailable(const WTF::MediaTime& time, const PlatformAudioData& data, const AudioStreamDescription& description, size_t size)
{
    ASSERT(!isMainThread());
    audioSamplesAvailable(time, data, description, size);
}

IPC::Connection* RemoteRealtimeAudioSource::connection()
{
    ASSERT(isMainThread());
#if ENABLE(GPU_PROCESS)
    if (m_shouldCaptureInGPUProcess)
        return &WebProcess::singleton().ensureGPUProcessConnection().connection();
#endif
    return WebProcess::singleton().parentProcessConnection();
}

void RemoteRealtimeAudioSource::startProducingData()
{
    connection()->send(Messages::UserMediaCaptureManagerProxy::StartProducingData { m_identifier }, 0);
}

void RemoteRealtimeAudioSource::stopProducingData()
{
    connection()->send(Messages::UserMediaCaptureManagerProxy::StopProducingData { m_identifier }, 0);
}

const WebCore::RealtimeMediaSourceCapabilities& RemoteRealtimeAudioSource::capabilities()
{
    return m_capabilities;
}

void RemoteRealtimeAudioSource::applyConstraints(const MediaConstraints& constraints, ApplyConstraintsHandler&& completionHandler)
{
    m_pendingApplyConstraintsCallbacks.append(WTFMove(completionHandler));
    // FIXME: Use sendAsyncWithReply.
    connection()->send(Messages::UserMediaCaptureManagerProxy::ApplyConstraints { m_identifier, constraints }, 0);
}

void RemoteRealtimeAudioSource::applyConstraintsSucceeded(RealtimeMediaSourceSettings&& settings)
{
    setSettings(WTFMove(settings));

    auto callback = m_pendingApplyConstraintsCallbacks.takeFirst();
    callback({ });
}

void RemoteRealtimeAudioSource::applyConstraintsFailed(String&& failedConstraint, String&& errorMessage)
{
    auto callback = m_pendingApplyConstraintsCallbacks.takeFirst();
    callback(ApplyConstraintsError { WTFMove(failedConstraint), WTFMove(errorMessage) });
}

void RemoteRealtimeAudioSource::hasEnded()
{
    connection()->send(Messages::UserMediaCaptureManagerProxy::End { m_identifier }, 0);
    m_manager.removeAudioSource(m_identifier);
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

#if ENABLE(GPU_PROCESS)
void RemoteRealtimeAudioSource::gpuProcessConnectionDidClose(GPUProcessConnection&)
{
    ASSERT(m_shouldCaptureInGPUProcess);
    if (isEnded())
        return;

    m_manager.remoteCaptureSampleManager().didUpdateSourceConnection(*this);
    m_isReady = false;
    createRemoteMediaSource();
    // FIXME: We should update the track according current settings.
    if (isProducingData())
        startProducingData();
}
#endif

}

#endif
