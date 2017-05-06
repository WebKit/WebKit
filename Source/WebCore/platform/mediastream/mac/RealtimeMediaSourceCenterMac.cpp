/*
 * Copyright (C) 2013-2016 Apple, Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer
 *    in the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name of Ericsson nor the names of its contributors
 *    may be used to endorse or promote products derived from this
 *    software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#if ENABLE(MEDIA_STREAM)
#include "RealtimeMediaSourceCenterMac.h"

#include "AVAudioCaptureSource.h"
#include "AVAudioSessionCaptureDeviceManager.h"
#include "AVCaptureDeviceManager.h"
#include "AVVideoCaptureSource.h"
#include "CoreAudioCaptureDeviceManager.h"
#include "CoreAudioCaptureSource.h"
#include "Logging.h"
#include "MediaStreamPrivate.h"
#include <wtf/MainThread.h>

namespace WebCore {

void RealtimeMediaSourceCenterMac::setUseAVFoundationAudioCapture(bool enabled)
{
    static std::optional<bool> active = std::nullopt;
    if (active && active.value() == enabled)
        return;

    active = enabled;
    if (active.value()) {
        RealtimeMediaSourceCenter::singleton().setAudioFactory(AVAudioCaptureSource::factory());
        RealtimeMediaSourceCenter::singleton().setAudioCaptureDeviceManager(AVCaptureDeviceManager::singleton());
    } else {
        RealtimeMediaSourceCenter::singleton().setAudioFactory(CoreAudioCaptureSource::factory());
#if PLATFORM(MAC)
        RealtimeMediaSourceCenter::singleton().setAudioCaptureDeviceManager(CoreAudioCaptureDeviceManager::singleton());
#else
        RealtimeMediaSourceCenter::singleton().setAudioCaptureDeviceManager(AVAudioSessionCaptureDeviceManager::singleton());
#endif
    }
}



RealtimeMediaSourceCenter& RealtimeMediaSourceCenter::platformCenter()
{
    ASSERT(isMainThread());
    static NeverDestroyed<RealtimeMediaSourceCenterMac> center;
    return center;
}

RealtimeMediaSourceCenterMac::RealtimeMediaSourceCenterMac()
{
    m_supportedConstraints.setSupportsWidth(true);
    m_supportedConstraints.setSupportsHeight(true);
    m_supportedConstraints.setSupportsAspectRatio(true);
    m_supportedConstraints.setSupportsFrameRate(true);
    m_supportedConstraints.setSupportsFacingMode(true);
    m_supportedConstraints.setSupportsVolume(true);
    m_supportedConstraints.setSupportsSampleRate(false);
    m_supportedConstraints.setSupportsSampleSize(false);
    m_supportedConstraints.setSupportsEchoCancellation(false);
    m_supportedConstraints.setSupportsDeviceId(true);
    m_supportedConstraints.setSupportsGroupId(true);

    m_audioFactory = &CoreAudioCaptureSource::factory();
    m_videoFactory = &AVVideoCaptureSource::factory();

#if PLATFORM(MAC)
    m_audioCaptureDeviceManager = &CoreAudioCaptureDeviceManager::singleton();
#else
    // FIXME 170861: Use AVAudioSession to enumerate audio capture devices on iOS
    m_audioCaptureDeviceManager = &AVCaptureDeviceManager::singleton();
#endif
    m_videoCaptureDeviceManager = &AVCaptureDeviceManager::singleton();
}

RealtimeMediaSourceCenterMac::~RealtimeMediaSourceCenterMac()
{
}

void RealtimeMediaSourceCenterMac::validateRequestConstraints(ValidConstraintsHandler&& validHandler, InvalidConstraintsHandler&& invalidHandler, const MediaConstraints& audioConstraints, const MediaConstraints& videoConstraints)
{
    Vector<String> audioSourceUIDs;
    Vector<String> videoSourceUIDs;
    String invalidConstraint;

    if (audioConstraints.isValid()) {
        audioSourceUIDs = bestSourcesForTypeAndConstraints(RealtimeMediaSource::Type::Audio, audioConstraints, invalidConstraint);
        if (!invalidConstraint.isEmpty()) {
            invalidHandler(invalidConstraint);
            return;
        }
    }

    if (videoConstraints.isValid()) {
        videoSourceUIDs = bestSourcesForTypeAndConstraints(RealtimeMediaSource::Type::Video, videoConstraints, invalidConstraint);
        if (!invalidConstraint.isEmpty()) {
            invalidHandler(invalidConstraint);
            return;
        }
    }

    validHandler(WTFMove(audioSourceUIDs), WTFMove(videoSourceUIDs));
}

void RealtimeMediaSourceCenterMac::createMediaStream(NewMediaStreamHandler&& completionHandler, const String& audioDeviceID, const String& videoDeviceID, const MediaConstraints* audioConstraints, const MediaConstraints* videoConstraints)
{
    Vector<Ref<RealtimeMediaSource>> audioSources;
    Vector<Ref<RealtimeMediaSource>> videoSources;
    String invalidConstraint;

    if (!audioDeviceID.isEmpty() && m_audioFactory) {
        auto audioSource = m_audioFactory->createAudioCaptureSource(audioDeviceID, audioConstraints);
        if (audioSource)
            audioSources.append(audioSource.source());
#if !LOG_DISABLED
        if (!audioSource.errorMessage.isEmpty())
            LOG(Media, "RealtimeMediaSourceCenterMac::createMediaStream(%p), audio constraints failed to apply: %s", this, audioSource.errorMessage.utf8().data());
#endif
    }
    if (!videoDeviceID.isEmpty() && m_videoFactory) {
        auto videoSource = m_videoFactory->createVideoCaptureSource(videoDeviceID, videoConstraints);
        if (videoSource)
            videoSources.append(videoSource.source());
#if !LOG_DISABLED
        if (!videoSource.errorMessage.isEmpty())
            LOG(Media, "RealtimeMediaSourceCenterMac::createMediaStream(%p), video constraints failed to apply: %s", this, videoSource.errorMessage.utf8().data());
#endif
    }

    if (videoSources.isEmpty() && audioSources.isEmpty())
        completionHandler(nullptr);
    else
        completionHandler(MediaStreamPrivate::create(audioSources, videoSources));
}

Vector<CaptureDevice> RealtimeMediaSourceCenterMac::getMediaStreamDevices()
{
    Vector<CaptureDevice> result;

    if (m_audioCaptureDeviceManager)
        result.appendVector(m_audioCaptureDeviceManager->getAudioSourcesInfo());

    if (m_videoCaptureDeviceManager)
        result.appendVector(m_videoCaptureDeviceManager->getVideoSourcesInfo());

    return result;
}

Vector<String> RealtimeMediaSourceCenterMac::bestSourcesForTypeAndConstraints(RealtimeMediaSource::Type type, const MediaConstraints& constraints, String& invalidConstraint)
{
    Vector<RefPtr<RealtimeMediaSource>> bestSources;

    struct {
        bool operator()(RefPtr<RealtimeMediaSource> a, RefPtr<RealtimeMediaSource> b)
        {
            return a->fitnessScore() < b->fitnessScore();
        }
    } sortBasedOnFitnessScore;

    CaptureDevice::DeviceType deviceType = type == RealtimeMediaSource::Type::Video ? CaptureDevice::DeviceType::Video : CaptureDevice::DeviceType::Audio;
    for (auto& captureDevice : getMediaStreamDevices()) {
        if (!captureDevice.enabled() || captureDevice.type() != deviceType)
            continue;

        CaptureSourceOrError sourceOrError;
        if (type == RealtimeMediaSource::Type::Video && m_videoFactory)
            sourceOrError = m_videoFactory->createVideoCaptureSource(captureDevice.persistentId(), &constraints);
        else if (type == RealtimeMediaSource::Type::Audio && m_audioFactory)
            sourceOrError = m_audioFactory->createAudioCaptureSource(captureDevice.persistentId(), &constraints);

        if (!sourceOrError) {
            // FIXME: Handle the case of invalid constraints on more than one device.
            invalidConstraint = WTFMove(sourceOrError.errorMessage);
            continue;
        }
        bestSources.append(sourceOrError.source());
    }

    Vector<String> sourceUIDs;
    if (bestSources.isEmpty())
        return sourceUIDs;

    sourceUIDs.reserveInitialCapacity(bestSources.size());
    std::sort(bestSources.begin(), bestSources.end(), sortBasedOnFitnessScore);
    for (auto& device : bestSources)
        sourceUIDs.uncheckedAppend(device->persistentID());

    return sourceUIDs;
}

RealtimeMediaSource::AudioCaptureFactory* RealtimeMediaSourceCenterMac::defaultAudioFactory()
{
    return &CoreAudioCaptureSource::factory();
}

RealtimeMediaSource::VideoCaptureFactory* RealtimeMediaSourceCenterMac::defaultVideoFactory()
{
    return &AVVideoCaptureSource::factory();
}

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM)
