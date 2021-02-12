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
#include "RemoteRealtimeVideoSource.h"

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

Ref<RealtimeVideoCaptureSource> RemoteRealtimeVideoSource::create(const CaptureDevice& device, const MediaConstraints* constraints, String&& name, String&& hashSalt, UserMediaCaptureManager& manager, bool shouldCaptureInGPUProcess)
{
    auto source = adoptRef(*new RemoteRealtimeVideoSource(RealtimeMediaSourceIdentifier::generate(), device, constraints, WTFMove(name), WTFMove(hashSalt), manager, shouldCaptureInGPUProcess));
    manager.addVideoSource(source.copyRef());
    source->createRemoteMediaSource();
    return source;
}

RemoteRealtimeVideoSource::RemoteRealtimeVideoSource(RealtimeMediaSourceIdentifier identifier, const CaptureDevice& device, const MediaConstraints* constraints, String&& name, String&& hashSalt, UserMediaCaptureManager& manager, bool shouldCaptureInGPUProcess)
    : RealtimeVideoCaptureSource(WTFMove(name), String::number(identifier.toUInt64()), WTFMove(hashSalt))
    , m_identifier(identifier)
    , m_manager(manager)
    , m_device(device)
    , m_shouldCaptureInGPUProcess(shouldCaptureInGPUProcess)
{
    if (constraints)
        m_constraints = *constraints;
#if PLATFORM(IOS_FAMILY)
    if (m_device.type() == CaptureDevice::DeviceType::Camera)
        RealtimeMediaSourceCenter::singleton().videoCaptureFactory().setActiveSource(*this);
#endif
}

void RemoteRealtimeVideoSource::createRemoteMediaSource()
{
    connection()->sendWithAsyncReply(Messages::UserMediaCaptureManagerProxy::CreateMediaSourceForCaptureDeviceWithConstraints(identifier(), m_device, deviceIDHashSalt(), m_constraints), [this, protectedThis = makeRef(*this)](bool succeeded, auto&& errorMessage, auto&& settings, auto&& capabilities, auto&& presets, auto size, auto frameRate) mutable {
        if (!succeeded) {
            didFail(WTFMove(errorMessage));
            return;
        }

        if (m_shouldCaptureInGPUProcess)
            WebProcess::singleton().ensureGPUProcessConnection().addClient(*this);

        setSize(size);
        setFrameRate(frameRate);

        setSettings(WTFMove(settings));
        setCapabilities(WTFMove(capabilities));
        setSupportedPresets(WTFMove(presets));

        setAsReady();
    });
}

RemoteRealtimeVideoSource::~RemoteRealtimeVideoSource()
{
    if (m_shouldCaptureInGPUProcess)
        WebProcess::singleton().ensureGPUProcessConnection().removeClient(*this);

#if PLATFORM(IOS_FAMILY)
    if (m_device.type() == CaptureDevice::DeviceType::Camera)
        RealtimeMediaSourceCenter::singleton().videoCaptureFactory().unsetActiveSource(*this);
#endif
}

void RemoteRealtimeVideoSource::whenReady(CompletionHandler<void(String)>&& callback)
{
    if (m_isReady)
        return callback(WTFMove(m_errorMessage));
    m_callback = WTFMove(callback);
}

void RemoteRealtimeVideoSource::didFail(String&& errorMessage)
{
    m_isReady = true;
    m_errorMessage = WTFMove(errorMessage);
    if (m_callback)
        m_callback(m_errorMessage);
}

void RemoteRealtimeVideoSource::setAsReady()
{
    ASSERT(!m_isReady);
    m_isReady = true;

    setName(String { m_settings.label().string() });

    if (m_callback)
        m_callback({ });
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

void RemoteRealtimeVideoSource::remoteVideoSampleAvailable(RemoteVideoSample&& remoteSample)
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

    if (m_sampleRotation != sampleRef->videoRotation()) {
        m_sampleRotation = sampleRef->videoRotation();

        auto size = this->size();
        if (m_sampleRotation == MediaSample::VideoRotation::Left || m_sampleRotation == MediaSample::VideoRotation::Right) {
            size = size.transposedSize();
            m_settings.setWidth(size.width());
            m_settings.setHeight(size.height());
        }
        scheduleDeferredTask([this] {
            notifySettingsDidChangeObservers({ RealtimeMediaSourceSettings::Flag::Width, RealtimeMediaSourceSettings::Flag::Height });
        });
    }
    dispatchMediaSampleToObservers(*sampleRef);
}

IPC::Connection* RemoteRealtimeVideoSource::connection()
{
    ASSERT(isMainThread());
#if ENABLE(GPU_PROCESS)
    if (m_shouldCaptureInGPUProcess)
        return &WebProcess::singleton().ensureGPUProcessConnection().connection();
#endif
    return WebProcess::singleton().parentProcessConnection();
}

void RemoteRealtimeVideoSource::startProducingData()
{
    connection()->send(Messages::UserMediaCaptureManagerProxy::StartProducingData { m_identifier }, 0);
}

void RemoteRealtimeVideoSource::stopProducingData()
{
    connection()->send(Messages::UserMediaCaptureManagerProxy::StopProducingData { m_identifier }, 0);
}

bool RemoteRealtimeVideoSource::setShouldApplyRotation(bool shouldApplyRotation)
{
    connection()->send(Messages::UserMediaCaptureManagerProxy::SetShouldApplyRotation { m_identifier, shouldApplyRotation }, 0);
    return true;
}

const RealtimeMediaSourceCapabilities& RemoteRealtimeVideoSource::capabilities()
{
    return m_capabilities;
}

void RemoteRealtimeVideoSource::hasEnded()
{
    connection()->send(Messages::UserMediaCaptureManagerProxy::End { m_identifier }, 0);
    m_manager.removeAudioSource(m_identifier);
}

void RemoteRealtimeVideoSource::captureStopped()
{
    stop();
    hasEnded();
}

void RemoteRealtimeVideoSource::captureFailed()
{
    RealtimeMediaSource::captureFailed();
    hasEnded();
}

void RemoteRealtimeVideoSource::generatePresets()
{
    ASSERT(m_isReady);
}

void RemoteRealtimeVideoSource::setFrameRateWithPreset(double frameRate, RefPtr<VideoPreset> preset)
{
    MediaConstraints constraints;

    constraints.isValid = true;
    DoubleConstraint frameRateConstraint("frameRate"_s, MediaConstraintType::FrameRate);
    frameRateConstraint.setIdeal(frameRate);
    constraints.mandatoryConstraints.set(MediaConstraintType::FrameRate, frameRateConstraint);

    if (preset) {
        IntConstraint widthConstraint("width"_s, MediaConstraintType::Width);
        widthConstraint.setIdeal(preset->size.width());
        constraints.mandatoryConstraints.set(MediaConstraintType::Width, widthConstraint);

        IntConstraint heightConstraint("height"_s, MediaConstraintType::Height);
        heightConstraint.setIdeal(preset->size.height());
        constraints.mandatoryConstraints.set(MediaConstraintType::Height, heightConstraint);
    }

    connection()->send(Messages::UserMediaCaptureManagerProxy::ApplyConstraints { m_identifier, constraints }, 0);
}

bool RemoteRealtimeVideoSource::prefersPreset(VideoPreset&)
{
    return true;
}

#if ENABLE(GPU_PROCESS)
void RemoteRealtimeVideoSource::gpuProcessConnectionDidClose(GPUProcessConnection&)
{
    ASSERT(m_shouldCaptureInGPUProcess);
    if (isEnded())
        return;

    m_isReady = false;
    createRemoteMediaSource();
    // FIXME: We should update the track according current settings.
    if (isProducingData())
        startProducingData();
}
#endif

}

#endif
