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
    m_useAVFoundationAudioCapture = enabled;
}

RealtimeMediaSourceCenterMac& RealtimeMediaSourceCenterMac::singleton()
{
    ASSERT(isMainThread());
    static NeverDestroyed<RealtimeMediaSourceCenterMac> center;
    return center;
}

RealtimeMediaSourceCenter& RealtimeMediaSourceCenter::platformCenter()
{
    return RealtimeMediaSourceCenterMac::singleton();
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
}

RealtimeMediaSourceCenterMac::~RealtimeMediaSourceCenterMac()
{
}

void RealtimeMediaSourceCenterMac::createMediaStream(NewMediaStreamHandler&& completionHandler, const String& audioDeviceID, const String& videoDeviceID, const MediaConstraints* audioConstraints, const MediaConstraints* videoConstraints)
{
    Vector<Ref<RealtimeMediaSource>> audioSources;
    Vector<Ref<RealtimeMediaSource>> videoSources;
    String invalidConstraint;

    if (!audioDeviceID.isEmpty()) {
        auto audioSource = audioFactory().createAudioCaptureSource(audioDeviceID, audioConstraints);
        if (audioSource)
            audioSources.append(audioSource.source());
        else {
#if !LOG_DISABLED
            if (!audioSource.errorMessage.isEmpty())
                LOG(Media, "RealtimeMediaSourceCenterMac::createMediaStream(%p), audio constraints failed to apply: %s", this, audioSource.errorMessage.utf8().data());
#endif
            completionHandler(nullptr);
            return;
        }
    }
    if (!videoDeviceID.isEmpty()) {
        auto videoSource = videoFactory().createVideoCaptureSource(videoDeviceID, videoConstraints);
        if (videoSource)
            videoSources.append(videoSource.source());
        else {
#if !LOG_DISABLED
            if (!videoSource.errorMessage.isEmpty())
                LOG(Media, "RealtimeMediaSourceCenterMac::createMediaStream(%p), video constraints failed to apply: %s", this, videoSource.errorMessage.utf8().data());
#endif
            completionHandler(nullptr);
            return;
        }
    }

    completionHandler(MediaStreamPrivate::create(audioSources, videoSources));
}

Vector<CaptureDevice> RealtimeMediaSourceCenterMac::getMediaStreamDevices()
{
    Vector<CaptureDevice> result;

    result.appendVector(audioCaptureDeviceManager().getAudioSourcesInfo());
    result.appendVector(videoCaptureDeviceManager().getVideoSourcesInfo());

    return result;
}

RealtimeMediaSource::AudioCaptureFactory& RealtimeMediaSourceCenterMac::defaultAudioFactory()
{
    return m_useAVFoundationAudioCapture ? AVAudioCaptureSource::factory() : CoreAudioCaptureSource::factory();
}

RealtimeMediaSource::VideoCaptureFactory& RealtimeMediaSourceCenterMac::defaultVideoFactory()
{
    return AVVideoCaptureSource::factory();
}

CaptureDeviceManager& RealtimeMediaSourceCenterMac::defaultAudioCaptureDeviceManager()
{
    if (m_useAVFoundationAudioCapture)
        return AVCaptureDeviceManager::singleton();
#if PLATFORM(MAC)
    return CoreAudioCaptureDeviceManager::singleton();
#else
    return AVAudioSessionCaptureDeviceManager::singleton();
#endif
}

CaptureDeviceManager& RealtimeMediaSourceCenterMac::defaultVideoCaptureDeviceManager()
{
    return AVCaptureDeviceManager::singleton();
}

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM)
