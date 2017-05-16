/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 * Copyright (C) 2013 Apple Inc.  All rights reserved.
 * Copyright (C) 2013 Nokia Corporation and/or its subsidiary(-ies).
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "MockRealtimeMediaSourceCenter.h"

#if ENABLE(MEDIA_STREAM)

#include "CaptureDevice.h"
#include "Logging.h"
#include "MediaStream.h"
#include "MediaStreamPrivate.h"
#include "MediaStreamTrack.h"
#include "MockRealtimeAudioSource.h"
#include "MockRealtimeMediaSource.h"
#include "MockRealtimeVideoSource.h"
#include "RealtimeMediaSource.h"
#include "RealtimeMediaSourceCapabilities.h"
#include <wtf/NeverDestroyed.h>

namespace WebCore {

void MockRealtimeMediaSourceCenter::setMockRealtimeMediaSourceCenterEnabled(bool enabled)
{
    static NeverDestroyed<MockRealtimeMediaSourceCenter> center;
    static bool active = false;
    if (active != enabled) {
        active = enabled;
        RealtimeMediaSourceCenter::setSharedStreamCenterOverride(enabled ? &center.get() : nullptr);
    }
}

MockRealtimeMediaSourceCenter::MockRealtimeMediaSourceCenter()
{
    m_supportedConstraints.setSupportsWidth(true);
    m_supportedConstraints.setSupportsHeight(true);
    m_supportedConstraints.setSupportsAspectRatio(true);
    m_supportedConstraints.setSupportsFrameRate(true);
    m_supportedConstraints.setSupportsFacingMode(true);
    m_supportedConstraints.setSupportsVolume(true);
    m_supportedConstraints.setSupportsDeviceId(true);
}

void MockRealtimeMediaSourceCenter::createMediaStream(NewMediaStreamHandler&& completionHandler, const String& audioDeviceID, const String& videoDeviceID, const MediaConstraints* audioConstraints, const MediaConstraints* videoConstraints)
{
    Vector<Ref<RealtimeMediaSource>> audioSources;
    Vector<Ref<RealtimeMediaSource>> videoSources;

    if (!audioDeviceID.isEmpty()) {
        for (auto& captureDevice : MockRealtimeMediaSource::audioDevices()) {
            if (!captureDevice.enabled())
                continue;

            if (audioDeviceID == captureDevice.persistentId()) {
                auto sourceOrError = MockRealtimeAudioSource::create(captureDevice.label(), audioConstraints);
                if (sourceOrError)
                    audioSources.append(sourceOrError.source());
            }
        }
    }

    if (!videoDeviceID.isEmpty()) {
        for (auto& captureDevice : MockRealtimeMediaSource::videoDevices()) {
            if (!captureDevice.enabled())
                continue;

            if (videoDeviceID == captureDevice.persistentId()) {
                auto sourceOrError = MockRealtimeVideoSource::create(captureDevice.label(), videoConstraints);
                if (sourceOrError)
                    videoSources.append(sourceOrError.source());
            }
        }
    }

    if (videoSources.isEmpty() && audioSources.isEmpty())
        completionHandler(nullptr);
    else
        completionHandler(MediaStreamPrivate::create(audioSources, videoSources));
}

Vector<CaptureDevice> MockRealtimeMediaSourceCenter::getMediaStreamDevices()
{
    Vector<CaptureDevice> sources;

    for (auto& captureDevice : MockRealtimeMediaSource::audioDevices()) {
        if (!captureDevice.enabled())
            continue;

        sources.append(captureDevice);
    }

    for (auto& captureDevice : MockRealtimeMediaSource::videoDevices()) {
        if (!captureDevice.enabled())
            continue;

        sources.append(captureDevice);
    }

    return sources;
}

RealtimeMediaSource::AudioCaptureFactory& MockRealtimeMediaSourceCenter::defaultAudioFactory()
{
    return MockRealtimeAudioSource::factory();
}

RealtimeMediaSource::VideoCaptureFactory& MockRealtimeMediaSourceCenter::defaultVideoFactory()
{
    return MockRealtimeVideoSource::factory();
}

CaptureDeviceManager& MockRealtimeMediaSourceCenter::defaultAudioCaptureDeviceManager()
{
    return m_defaultAudioCaptureDeviceManager;
}

CaptureDeviceManager& MockRealtimeMediaSourceCenter::defaultVideoCaptureDeviceManager()
{
    return m_defaultVideoCaptureDeviceManager;
}

ExceptionOr<void> MockRealtimeMediaSourceCenter::setDeviceEnabled(const String& id, bool enabled)
{
    for (auto& captureDevice : getMediaStreamDevices()) {
        if (id == captureDevice.persistentId()) {
            if (enabled != captureDevice.enabled()) {
                captureDevice.setEnabled(enabled);
                captureDevicesChanged();
            }

            return { };
        }
    }

    return Exception { NOT_FOUND_ERR };
}

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM)
