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
#include "UUID.h"
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

void MockRealtimeMediaSourceCenter::validateRequestConstraints(ValidConstraintsHandler validHandler, InvalidConstraintsHandler invalidHandler, const MediaConstraints& audioConstraints, const MediaConstraints& videoConstraints)
{
    Vector<String> audioSourceIds;
    Vector<String> videoSourceIds;
    String invalidConstraint;

    if (audioConstraints.isValid()) {
        auto audioSource = MockRealtimeAudioSource::create(MockRealtimeMediaSource::mockAudioSourceName(), nullptr);
        if (!audioSource->supportsConstraints(audioConstraints, invalidConstraint)) {
            if (invalidHandler)
                invalidHandler(invalidConstraint);
            return;
        }

        audioSourceIds.append(MockRealtimeMediaSource::mockAudioSourcePersistentID());
    }

    if (videoConstraints.isValid()) {
        auto videoSource = MockRealtimeVideoSource::create(MockRealtimeMediaSource::mockVideoSourceName(), nullptr);
        if (!videoSource->supportsConstraints(videoConstraints, invalidConstraint)) {
            if (invalidHandler)
                invalidHandler(invalidConstraint);
            return;
        }


        videoSourceIds.append(MockRealtimeMediaSource::mockVideoSourcePersistentID());
    }

    validHandler(WTFMove(audioSourceIds), WTFMove(videoSourceIds));
}

void MockRealtimeMediaSourceCenter::createMediaStream(NewMediaStreamHandler completionHandler, const String& audioDeviceID, const String& videoDeviceID, const MediaConstraints* audioConstraints, const MediaConstraints* videoConstraints)
{
    Vector<Ref<RealtimeMediaSource>> audioSources;
    Vector<Ref<RealtimeMediaSource>> videoSources;

    if (audioDeviceID == MockRealtimeMediaSource::mockAudioSourcePersistentID()) {
        auto source = MockRealtimeAudioSource::create(MockRealtimeMediaSource::mockAudioSourceName(), audioConstraints);
        if (source)
            audioSources.append(source.releaseNonNull());
    }

    if (videoDeviceID == MockRealtimeMediaSource::mockVideoSourcePersistentID()) {
        auto source = MockRealtimeVideoSource::create(MockRealtimeMediaSource::mockVideoSourceName(), videoConstraints);
        if (source)
            videoSources.append(source.releaseNonNull());
    }

    if (videoSources.isEmpty() && audioSources.isEmpty())
        completionHandler(nullptr);
    else
        completionHandler(MediaStreamPrivate::create(audioSources, videoSources));
}

Vector<CaptureDevice> MockRealtimeMediaSourceCenter::getMediaStreamDevices()
{
    Vector<CaptureDevice> sources;

    sources.append(MockRealtimeMediaSource::audioDeviceInfo());
    sources.append(MockRealtimeMediaSource::videoDeviceInfo());

    return sources;
}

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM)
