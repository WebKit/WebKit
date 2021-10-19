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
    manager.addSource(source.copyRef());
    manager.remoteCaptureSampleManager().addSource(source.copyRef());
    source->createRemoteMediaSource();
    return source;
}

RemoteRealtimeVideoSource::RemoteRealtimeVideoSource(RealtimeMediaSourceIdentifier identifier, const CaptureDevice& device, const MediaConstraints* constraints, String&& name, String&& hashSalt, UserMediaCaptureManager& manager, bool shouldCaptureInGPUProcess)
    : RealtimeVideoCaptureSource(WTFMove(name), String { device.persistentId() }, WTFMove(hashSalt))
    , m_proxy(identifier, device, shouldCaptureInGPUProcess, constraints)
    , m_manager(manager)
{
#if PLATFORM(IOS_FAMILY)
    if (deviceType() == CaptureDevice::DeviceType::Camera)
        RealtimeMediaSourceCenter::singleton().videoCaptureFactory().setActiveSource(*this);
#endif
}

void RemoteRealtimeVideoSource::createRemoteMediaSource()
{
    m_proxy.createRemoteMediaSource(deviceIDHashSalt(), [this, protectedThis = Ref { *this }](bool succeeded, auto&& errorMessage, auto&& settings, auto&& capabilities, auto&& presets, auto size, auto frameRate) mutable {
        if (!succeeded) {
            m_proxy.didFail(WTFMove(errorMessage));
            return;
        }

        setSize(size);
        setFrameRate(frameRate);

        setSettings(WTFMove(settings));
        setCapabilities(WTFMove(capabilities));
        setSupportedPresets(WTFMove(presets));
        setName(String { m_settings.label().string() });

        m_proxy.setAsReady();
        if (m_proxy.shouldCaptureInGPUProcess())
            WebProcess::singleton().ensureGPUProcessConnection().addClient(*this);
    });
}

RemoteRealtimeVideoSource::~RemoteRealtimeVideoSource()
{
    if (m_proxy.shouldCaptureInGPUProcess()) {
        if (auto* connection = WebProcess::singleton().existingGPUProcessConnection())
            connection->removeClient(*this);
    }

#if PLATFORM(IOS_FAMILY)
    if (deviceType() == CaptureDevice::DeviceType::Camera)
        RealtimeMediaSourceCenter::singleton().videoCaptureFactory().unsetActiveSource(*this);
#endif
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

void RemoteRealtimeVideoSource::videoSampleAvailable(MediaSample& sample, IntSize sampleSize)
{
    ASSERT(type() == Type::Video);

    setIntrinsicSize(sampleSize);

    if (m_sampleRotation != sample.videoRotation()) {
        m_sampleRotation = sample.videoRotation();

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
    dispatchMediaSampleToObservers(sample);
}

bool RemoteRealtimeVideoSource::setShouldApplyRotation(bool shouldApplyRotation)
{
    connection()->send(Messages::UserMediaCaptureManagerProxy::SetShouldApplyRotation { identifier(), shouldApplyRotation }, 0);
    return true;
}

const RealtimeMediaSourceCapabilities& RemoteRealtimeVideoSource::capabilities()
{
    return m_capabilities;
}

void RemoteRealtimeVideoSource::hasEnded()
{
    m_proxy.hasEnded();
    m_manager.removeSource(identifier());
    m_manager.remoteCaptureSampleManager().removeSource(identifier());
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
    ASSERT(m_proxy.isReady());
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

    m_sizeConstraints = constraints;
    m_proxy.applyConstraints(constraints, [](auto) { });
}

bool RemoteRealtimeVideoSource::prefersPreset(VideoPreset&)
{
    return true;
}

#if ENABLE(GPU_PROCESS)
void RemoteRealtimeVideoSource::gpuProcessConnectionDidClose(GPUProcessConnection&)
{
    ASSERT(m_proxy.shouldCaptureInGPUProcess());
    if (isEnded())
        return;

#if PLATFORM(IOS_FAMILY)
    if (deviceType() == CaptureDevice::DeviceType::Camera && this != RealtimeMediaSourceCenter::singleton().videoCaptureFactory().activeSource()) {
        // Track is muted and has no chance of being unmuted, let's end it.
        captureFailed();
        return;
    }
#endif

    m_manager.remoteCaptureSampleManager().didUpdateSourceConnection(connection());
    m_proxy.resetReady();
    createRemoteMediaSource();

    m_proxy.failApplyConstraintCallbacks("GPU Process terminated"_s);
    if (m_sizeConstraints)
        m_proxy.applyConstraints(*m_sizeConstraints, [](auto) { });

    if (isProducingData())
        startProducingData();
}
#endif

}

#endif
