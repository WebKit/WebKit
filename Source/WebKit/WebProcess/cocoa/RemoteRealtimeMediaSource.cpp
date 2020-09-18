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
#include "RemoteRealtimeMediaSource.h"

#if PLATFORM(COCOA) && ENABLE(MEDIA_STREAM)

#include "GPUProcessConnection.h"
#include "SharedRingBufferStorage.h"
#include "UserMediaCaptureManager.h"
#include "UserMediaCaptureManagerMessages.h"
#include "UserMediaCaptureManagerProxyMessages.h"
#include "WebCoreArgumentCoders.h"
#include "WebProcess.h"
#include <WebCore/ImageTransferSessionVT.h>
#include <WebCore/MediaConstraints.h>
#include <WebCore/RealtimeMediaSource.h>
#include <WebCore/RealtimeMediaSourceCenter.h>
#include <WebCore/RemoteVideoSample.h>
#include <WebCore/WebAudioBufferList.h>

namespace WebKit {
using namespace PAL;
using namespace WebCore;

Ref<RealtimeMediaSource> RemoteRealtimeMediaSource::create(const WebCore::CaptureDevice& device, const WebCore::MediaConstraints& constraints, String&& name, String&& hashSalt, UserMediaCaptureManager& manager, bool shouldCaptureInGPUProcess)
{
    auto source = adoptRef(*new RemoteRealtimeMediaSource(RealtimeMediaSourceIdentifier::generate(), device.type(), WTFMove(name), WTFMove(hashSalt), manager, shouldCaptureInGPUProcess));
    manager.addSource(source.copyRef());
    source->connection()->sendWithAsyncReply(Messages::UserMediaCaptureManagerProxy::CreateMediaSourceForCaptureDeviceWithConstraints(source->identifier(), device, source->deviceIDHashSalt(), constraints), [source = source.copyRef()](bool succeeded, auto&& errorMessage, auto&& settings, auto&& capabilities) {
        if (!succeeded) {
            source->didFail(WTFMove(errorMessage));
            return;
        }
        source->setName(String { settings.label().string() });
        source->setSettings(WTFMove(settings));
        source->setCapabilities(WTFMove(capabilities));
        source->setAsReady();
    });
    return source;
}

static inline RealtimeMediaSource::Type sourceTypeFromDeviceType(CaptureDevice::DeviceType deviceType)
{
    switch (deviceType) {
    case CaptureDevice::DeviceType::Microphone:
        return RealtimeMediaSource::Type::Audio;
    case CaptureDevice::DeviceType::Camera:
    case CaptureDevice::DeviceType::Screen:
    case CaptureDevice::DeviceType::Window:
        return RealtimeMediaSource::Type::Video;
    case CaptureDevice::DeviceType::Speaker:
    case CaptureDevice::DeviceType::Unknown:
        ASSERT_NOT_REACHED();
    }
    return RealtimeMediaSource::Type::None;
}

RemoteRealtimeMediaSource::RemoteRealtimeMediaSource(RealtimeMediaSourceIdentifier identifier, CaptureDevice::DeviceType deviceType, String&& name, String&& hashSalt, UserMediaCaptureManager& manager, bool shouldCaptureInGPUProcess)
    : RealtimeMediaSource(sourceTypeFromDeviceType(deviceType), WTFMove(name), String::number(identifier.toUInt64()), WTFMove(hashSalt))
    , m_identifier(identifier)
    , m_manager(manager)
    , m_deviceType(deviceType)
    , m_shouldCaptureInGPUProcess(shouldCaptureInGPUProcess)
{
    switch (m_deviceType) {
    case CaptureDevice::DeviceType::Microphone:
#if PLATFORM(IOS_FAMILY)
        RealtimeMediaSourceCenter::singleton().audioCaptureFactory().setActiveSource(*this);
#endif
        break;
    case CaptureDevice::DeviceType::Camera:
#if PLATFORM(IOS_FAMILY)
        RealtimeMediaSourceCenter::singleton().videoCaptureFactory().setActiveSource(*this);
#endif
        break;
    case CaptureDevice::DeviceType::Screen:
    case CaptureDevice::DeviceType::Window:
        break;
    case CaptureDevice::DeviceType::Speaker:
    case CaptureDevice::DeviceType::Unknown:
        ASSERT_NOT_REACHED();
    }
}

RemoteRealtimeMediaSource::~RemoteRealtimeMediaSource()
{
    switch (m_deviceType) {
    case CaptureDevice::DeviceType::Microphone:
#if PLATFORM(IOS_FAMILY)
        RealtimeMediaSourceCenter::singleton().audioCaptureFactory().unsetActiveSource(*this);
#endif
        break;
    case CaptureDevice::DeviceType::Camera:
#if PLATFORM(IOS_FAMILY)
        RealtimeMediaSourceCenter::singleton().videoCaptureFactory().unsetActiveSource(*this);
#endif
        break;
    case CaptureDevice::DeviceType::Screen:
    case CaptureDevice::DeviceType::Window:
        break;
    case CaptureDevice::DeviceType::Speaker:
    case CaptureDevice::DeviceType::Unknown:
        ASSERT_NOT_REACHED();
    }
}

void RemoteRealtimeMediaSource::whenReady(CompletionHandler<void(String)>&& callback)
{
    if (m_isReady)
        return callback(WTFMove(m_errorMessage));
    m_callback = WTFMove(callback);
}

void RemoteRealtimeMediaSource::didFail(String&& errorMessage)
{
    m_isReady = true;
    m_errorMessage = WTFMove(errorMessage);
    if (m_callback)
        m_callback(m_errorMessage);
}

void RemoteRealtimeMediaSource::setAsReady()
{
    m_isReady = true;
    if (m_callback)
        m_callback({ });
}

Ref<RealtimeMediaSource> RemoteRealtimeMediaSource::clone()
{
    switch (type()) {
    case RealtimeMediaSource::Type::Video:
        return cloneVideoSource();
    case RealtimeMediaSource::Type::Audio:
        break;
    case RealtimeMediaSource::Type::None:
        ASSERT_NOT_REACHED();
    }
    return *this;
}

Ref<RealtimeMediaSource> RemoteRealtimeMediaSource::cloneVideoSource()
{
    auto identifier = RealtimeMediaSourceIdentifier::generate();
    if (!connection()->send(Messages::UserMediaCaptureManagerProxy::Clone { m_identifier, identifier }, 0))
        return *this;

    auto cloneSource = adoptRef(*new RemoteRealtimeMediaSource(identifier, deviceType(), String { m_settings.label().string() }, deviceIDHashSalt(), m_manager, m_shouldCaptureInGPUProcess));
    cloneSource->setSettings(RealtimeMediaSourceSettings { m_settings });
    m_manager.addSource(cloneSource.copyRef());
    return cloneSource;
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

void RemoteRealtimeMediaSource::remoteAudioSamplesAvailable(const WTF::MediaTime& time, const PlatformAudioData& data, const AudioStreamDescription& description, size_t size)
{
    ASSERT(!isMainThread());
    audioSamplesAvailable(time, data, description, size);
}

void RemoteRealtimeMediaSource::remoteVideoSampleAvailable(RemoteVideoSample&& remoteSample)
{
    ASSERT(type() == Type::Video);

    setIntrinsicSize(remoteSample.size());

    if (!m_imageTransferSession || m_imageTransferSession->pixelFormat() != remoteSample.videoFormat())
        m_imageTransferSession = ImageTransferSessionVT::create(remoteSample.videoFormat());

    if (!m_imageTransferSession) {
        ASSERT_NOT_REACHED();
        return;
    }

    auto sampleRef = m_imageTransferSession->createMediaSample(remoteSample);
    if (!sampleRef) {
        ASSERT_NOT_REACHED();
        return;
    }

    RealtimeMediaSource::videoSampleAvailable(*sampleRef);
}

IPC::Connection* RemoteRealtimeMediaSource::connection()
{
    ASSERT(isMainThread());
#if ENABLE(GPU_PROCESS)
    if (m_shouldCaptureInGPUProcess)
        return &WebProcess::singleton().ensureGPUProcessConnection().connection();
#endif
    return WebProcess::singleton().parentProcessConnection();
}

void RemoteRealtimeMediaSource::startProducingData()
{
    connection()->send(Messages::UserMediaCaptureManagerProxy::StartProducingData { m_identifier }, 0);
}

void RemoteRealtimeMediaSource::stopProducingData()
{
    connection()->send(Messages::UserMediaCaptureManagerProxy::StopProducingData { m_identifier }, 0);
}

bool RemoteRealtimeMediaSource::setShouldApplyRotation(bool shouldApplyRotation)
{
    connection()->send(Messages::UserMediaCaptureManagerProxy::SetShouldApplyRotation { m_identifier, shouldApplyRotation }, 0);
    return true;
}

const WebCore::RealtimeMediaSourceCapabilities& RemoteRealtimeMediaSource::capabilities()
{
    return m_capabilities;
}

void RemoteRealtimeMediaSource::applyConstraints(const WebCore::MediaConstraints& constraints, ApplyConstraintsHandler&& completionHandler)
{
    m_pendingApplyConstraintsCallbacks.append(WTFMove(completionHandler));
    // FIXME: Use sendAsyncWithReply.
    connection()->send(Messages::UserMediaCaptureManagerProxy::ApplyConstraints { m_identifier, constraints }, 0);
}

void RemoteRealtimeMediaSource::applyConstraintsSucceeded(const WebCore::RealtimeMediaSourceSettings& settings)
{
    setSettings(WebCore::RealtimeMediaSourceSettings(settings));

    auto callback = m_pendingApplyConstraintsCallbacks.takeFirst();
    callback({ });
}

void RemoteRealtimeMediaSource::applyConstraintsFailed(String&& failedConstraint, String&& errorMessage)
{
    auto callback = m_pendingApplyConstraintsCallbacks.takeFirst();
    callback(ApplyConstraintsError { WTFMove(failedConstraint), WTFMove(errorMessage) });
}

void RemoteRealtimeMediaSource::hasEnded()
{
    connection()->send(Messages::UserMediaCaptureManagerProxy::End { m_identifier }, 0);
    m_manager.removeSource(m_identifier);
}

void RemoteRealtimeMediaSource::captureStopped()
{
    stop();
    hasEnded();
}

void RemoteRealtimeMediaSource::captureFailed()
{
    RealtimeMediaSource::captureFailed();
    hasEnded();
}

void RemoteRealtimeMediaSource::stopBeingObserved()
{
    connection()->send(Messages::UserMediaCaptureManagerProxy::RequestToEnd { m_identifier }, 0);
}

void RemoteRealtimeMediaSource::requestToEnd(Observer& observer)
{
    switch (type()) {
    case Type::Audio:
        RealtimeMediaSource::requestToEnd(observer);
        break;
    case Type::Video:
        stopBeingObserved();
        break;
    case Type::None:
        ASSERT_NOT_REACHED();
    }
}

}

#endif
