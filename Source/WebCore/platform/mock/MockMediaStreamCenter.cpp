/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 * Copyright (C) 2013 Apple Computer, Inc.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "MockMediaStreamCenter.h"

#if ENABLE(MEDIA_STREAM)

#include "MediaStream.h"
#include "MediaStreamCreationClient.h"
#include "MediaStreamDescriptor.h"
#include "MediaStreamTrack.h"
#include "MediaStreamTrackSourcesRequestClient.h"
#include "NotImplemented.h"

namespace WebCore {

static bool isSupportedMockConstraint(const String& constraintName)
{
    return notFound != constraintName.find("_and_supported_");
}

static bool isValidMockConstraint(const String& constraintName)
{
    if (isSupportedMockConstraint(constraintName))
        return true;
    return notFound != constraintName.find("_but_unsupported_");
}

static String verifyConstraints(MediaConstraints* constraints)
{
    Vector<MediaConstraint> mandatoryConstraints;
    constraints->getMandatoryConstraints(mandatoryConstraints);
    if (mandatoryConstraints.size()) {
        for (size_t i = 0; i < mandatoryConstraints.size(); ++i) {
            const MediaConstraint& constraint = mandatoryConstraints[i];
            if (!isSupportedMockConstraint(constraint.m_name) || constraint.m_value != "1")
                return constraint.m_name;
        }
    }
    
    Vector<MediaConstraint> optionalConstraints;
    constraints->getOptionalConstraints(optionalConstraints);
    if (optionalConstraints.size()) {
        for (size_t i = 0; i < optionalConstraints.size(); ++i) {
            const MediaConstraint& constraint = optionalConstraints[i];
            if (!isValidMockConstraint(constraint.m_name) || constraint.m_value != "0")
                return constraint.m_name;
        }
    }
    
    return emptyString();
}

void MockMediaStreamCenter::registerMockMediaStreamCenter()
{
    DEFINE_STATIC_LOCAL(MockMediaStreamCenter, center, ());
    static bool registered = false;
    if (!registered) {
        registered = true;
        MediaStreamCenter::setSharedStreamCenter(&center);
    }
}

void MockMediaStreamCenter::validateRequestConstraints(PassRefPtr<MediaStreamCreationClient> prpQueryClient, PassRefPtr<MediaConstraints> audioConstraints, PassRefPtr<MediaConstraints> videoConstraints)
{
    RefPtr<MediaStreamCreationClient> client = prpQueryClient;
    
    ASSERT(client);
    
    if (audioConstraints) {
        String invalidQuery = verifyConstraints(audioConstraints.get());
        if (!invalidQuery.isEmpty()) {
            client->constraintsInvalid(invalidQuery);
            return;
        }
    }
    
    if (videoConstraints) {
        String invalidQuery = verifyConstraints(videoConstraints.get());
        if (!invalidQuery.isEmpty()) {
            client->constraintsInvalid(invalidQuery);
            return;
        }
    }

    client->constraintsValidated();
}

void MockMediaStreamCenter::createMediaStream(PassRefPtr<MediaStreamCreationClient> prpQueryClient, PassRefPtr<MediaConstraints> audioConstraints, PassRefPtr<MediaConstraints> videoConstraints)
{
    RefPtr<MediaStreamCreationClient> client = prpQueryClient;

    ASSERT(client);
    
    Vector<RefPtr<MediaStreamSource>> audioSources;
    Vector<RefPtr<MediaStreamSource>> videoSources;

    if (audioConstraints) {
        String invalidQuery = verifyConstraints(audioConstraints.get());
        if (!invalidQuery.isEmpty()) {
            client->failedToCreateStreamWithConstraintsError(invalidQuery);
            return;
        }

        if (audioConstraints) {
            RefPtr<MediaStreamSource> audioSource = MediaStreamSource::create(emptyString(), MediaStreamSource::Audio, "Mock audio device");
            audioSource->setReadyState(MediaStreamSource::Live);
            audioSources.append(audioSource.release());
        }
    }

    if (videoConstraints) {
        String invalidQuery = verifyConstraints(videoConstraints.get());
        if (!invalidQuery.isEmpty()) {
            client->failedToCreateStreamWithConstraintsError(invalidQuery);
            return;
        }

        if (videoConstraints) {
            RefPtr<MediaStreamSource> videoSource = MediaStreamSource::create(emptyString(), MediaStreamSource::Video, "Mock video device");
            videoSource->setReadyState(MediaStreamSource::Live);
            videoSources.append(videoSource.release());
        }
    }
    
    client->didCreateStream(MediaStreamDescriptor::create(audioSources, videoSources));
}

bool MockMediaStreamCenter::getMediaStreamTrackSources(PassRefPtr<MediaStreamTrackSourcesRequestClient> prpClient)
{
    RefPtr<MediaStreamTrackSourcesRequestClient> requestClient = prpClient;
    Vector<RefPtr<TrackSourceInfo>> sources(2);

    sources[0] = TrackSourceInfo::create("Mock_audio_device_ID", TrackSourceInfo::Audio, "Mock audio device");
    sources[1] = TrackSourceInfo::create("Mock_video_device_ID", TrackSourceInfo::Video, "Mock video device");

    requestClient->didCompleteRequest(sources);
    return true;
}

void MockMediaStreamCenter::didSetMediaStreamTrackEnabled(MediaStreamSource* source)
{
    source->setReadyState(MediaStreamSource::Live);
}

bool MockMediaStreamCenter::didAddMediaStreamTrack(MediaStreamSource*)
{
    return true;
}

bool MockMediaStreamCenter::didRemoveMediaStreamTrack(MediaStreamSource*)
{
    return true;
}

void MockMediaStreamCenter::didStopLocalMediaStream(MediaStreamDescriptor* stream)
{
    endLocalMediaStream(stream);
}

void MockMediaStreamCenter::didCreateMediaStream(MediaStreamDescriptor*)
{
}

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM)
