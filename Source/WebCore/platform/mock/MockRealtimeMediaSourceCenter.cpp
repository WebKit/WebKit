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

#include "MediaConstraintsMock.h"
#include "MediaStream.h"
#include "MediaStreamCreationClient.h"
#include "MediaStreamPrivate.h"
#include "MediaStreamTrack.h"
#include "MediaStreamTrackSourcesRequestClient.h"
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

void MockRealtimeMediaSourceCenter::validateRequestConstraints(MediaStreamCreationClient* client, MediaConstraints& audioConstraints, MediaConstraints& videoConstraints)
{
    ASSERT(client);

    Vector<RefPtr<RealtimeMediaSource>> audioSources;
    Vector<RefPtr<RealtimeMediaSource>> videoSources;

    if (audioConstraints.isValid()) {
        String invalidQuery = MediaConstraintsMock::verifyConstraints(RealtimeMediaSource::Audio, audioConstraints);
        if (!invalidQuery.isEmpty()) {
            client->constraintsInvalid(invalidQuery);
            return;
        }

        auto audioSource = MockRealtimeAudioSource::create();
        audioSources.append(WTFMove(audioSource));
    }

    if (videoConstraints.isValid()) {
        String invalidQuery = MediaConstraintsMock::verifyConstraints(RealtimeMediaSource::Video, videoConstraints);
        if (!invalidQuery.isEmpty()) {
            client->constraintsInvalid(invalidQuery);
            return;
        }

        auto videoSource = MockRealtimeVideoSource::create();
        videoSources.append(WTFMove(videoSource));
    }

    client->constraintsValidated(audioSources, videoSources);
}

void MockRealtimeMediaSourceCenter::createMediaStream(PassRefPtr<MediaStreamCreationClient> prpQueryClient, MediaConstraints& audioConstraints, MediaConstraints& videoConstraints)
{
    RefPtr<MediaStreamCreationClient> client = prpQueryClient;

    ASSERT(client);
    
    Vector<RefPtr<RealtimeMediaSource>> audioSources;
    Vector<RefPtr<RealtimeMediaSource>> videoSources;

    if (audioConstraints.isValid()) {
        const String& invalidQuery = MediaConstraintsMock::verifyConstraints(RealtimeMediaSource::Audio, audioConstraints);
        if (!invalidQuery.isEmpty()) {
            client->failedToCreateStreamWithConstraintsError(invalidQuery);
            return;
        }

        auto audioSource = MockRealtimeAudioSource::create();
        audioSources.append(WTFMove(audioSource));
    }

    if (videoConstraints.isValid()) {
        const String& invalidQuery = MediaConstraintsMock::verifyConstraints(RealtimeMediaSource::Video, videoConstraints);
        if (!invalidQuery.isEmpty()) {
            client->failedToCreateStreamWithConstraintsError(invalidQuery);
            return;
        }

        auto videoSource = MockRealtimeVideoSource::create();
        videoSources.append(WTFMove(videoSource));
    }
    
    client->didCreateStream(MediaStreamPrivate::create(audioSources, videoSources));
}

void MockRealtimeMediaSourceCenter::createMediaStream(MediaStreamCreationClient* client, const String& audioDeviceID, const String& videoDeviceID)
{
    ASSERT(client);
    Vector<RefPtr<RealtimeMediaSource>> audioSources;
    Vector<RefPtr<RealtimeMediaSource>> videoSources;

    if (!audioDeviceID.isEmpty() && audioDeviceID == MockRealtimeMediaSource::mockAudioSourcePersistentID()) {
        auto audioSource = MockRealtimeAudioSource::create();
        audioSources.append(WTFMove(audioSource));
    }
    if (!videoDeviceID.isEmpty() && videoDeviceID == MockRealtimeMediaSource::mockVideoSourcePersistentID()) {
        auto videoSource = MockRealtimeVideoSource::create();
        videoSources.append(WTFMove(videoSource));
    }

    client->didCreateStream(MediaStreamPrivate::create(audioSources, videoSources));
}

bool MockRealtimeMediaSourceCenter::getMediaStreamTrackSources(PassRefPtr<MediaStreamTrackSourcesRequestClient> prpClient)
{
    RefPtr<MediaStreamTrackSourcesRequestClient> requestClient = prpClient;
    TrackSourceInfoVector sources;

    sources.append(MockRealtimeMediaSource::trackSourceWithUID(MockRealtimeMediaSource::mockAudioSourcePersistentID(), nullptr));
    sources.append(MockRealtimeMediaSource::trackSourceWithUID(MockRealtimeMediaSource::mockVideoSourcePersistentID(), nullptr));

    callOnMainThread([this, requestClient = WTFMove(requestClient), sources = WTFMove(sources)] {
        requestClient->didCompleteTrackSourceInfoRequest(sources);
    });

    return true;
}

RefPtr<TrackSourceInfo> MockRealtimeMediaSourceCenter::sourceWithUID(const String& UID, RealtimeMediaSource::Type, MediaConstraints* constraints)
{
    return MockRealtimeMediaSource::trackSourceWithUID(UID, constraints);
}

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM)
