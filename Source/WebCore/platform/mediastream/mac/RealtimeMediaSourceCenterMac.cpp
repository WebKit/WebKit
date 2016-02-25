/*
 * Copyright (C) 2013 Apple, Inc. All rights reserved.
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

#include "AVCaptureDeviceManager.h"
#include "MediaStreamCreationClient.h"
#include "MediaStreamPrivate.h"
#include "MediaStreamTrackSourcesRequestClient.h"
#include <wtf/MainThread.h>

namespace WebCore {

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
}

RealtimeMediaSourceCenterMac::~RealtimeMediaSourceCenterMac()
{
}

void RealtimeMediaSourceCenterMac::validateRequestConstraints(MediaStreamCreationClient* client, RefPtr<MediaConstraints>& audioConstraints, RefPtr<MediaConstraints>& videoConstraints)
{
    ASSERT(client);

    Vector<RefPtr<RealtimeMediaSource>> audioSources;
    Vector<RefPtr<RealtimeMediaSource>> videoSources;

    if (audioConstraints) {
        String invalidConstraint;
        AVCaptureDeviceManager::singleton().verifyConstraintsForMediaType(RealtimeMediaSource::Audio, audioConstraints.get(), nullptr, invalidConstraint);
        if (!invalidConstraint.isEmpty()) {
            client->constraintsInvalid(invalidConstraint);
            return;
        }

        audioSources = AVCaptureDeviceManager::singleton().bestSourcesForTypeAndConstraints(RealtimeMediaSource::Type::Audio, audioConstraints);
    }

    if (videoConstraints) {
        String invalidConstraint;
        AVCaptureDeviceManager::singleton().verifyConstraintsForMediaType(RealtimeMediaSource::Video, videoConstraints.get(), nullptr, invalidConstraint);
        if (!invalidConstraint.isEmpty()) {
            client->constraintsInvalid(invalidConstraint);
            return;
        }

        videoSources = AVCaptureDeviceManager::singleton().bestSourcesForTypeAndConstraints(RealtimeMediaSource::Type::Video, videoConstraints);
    }
    client->constraintsValidated(audioSources, videoSources);
}

void RealtimeMediaSourceCenterMac::createMediaStream(PassRefPtr<MediaStreamCreationClient> prpQueryClient, PassRefPtr<MediaConstraints> audioConstraints, PassRefPtr<MediaConstraints> videoConstraints)
{
    RefPtr<MediaStreamCreationClient> client = prpQueryClient;
    
    ASSERT(client);
    
    Vector<RefPtr<RealtimeMediaSource>> audioSources;
    Vector<RefPtr<RealtimeMediaSource>> videoSources;
    
    if (audioConstraints) {
        String invalidConstraint;
        AVCaptureDeviceManager::singleton().verifyConstraintsForMediaType(RealtimeMediaSource::Audio, audioConstraints.get(), nullptr, invalidConstraint);
        if (!invalidConstraint.isEmpty()) {
            client->failedToCreateStreamWithConstraintsError(invalidConstraint);
            return;
        }
        // FIXME: Consider the constraints when choosing among multiple devices. For now just select the first available
        // device of the appropriate type.
        RefPtr<RealtimeMediaSource> audioSource = AVCaptureDeviceManager::singleton().bestSourcesForTypeAndConstraints(RealtimeMediaSource::Audio, audioConstraints.get()).at(0);
        ASSERT(audioSource);
        
        audioSources.append(audioSource.release());
    }
    
    if (videoConstraints) {
        String invalidConstraint;
        AVCaptureDeviceManager::singleton().verifyConstraintsForMediaType(RealtimeMediaSource::Video, videoConstraints.get(), nullptr, invalidConstraint);
        if (!invalidConstraint.isEmpty()) {
            client->failedToCreateStreamWithConstraintsError(invalidConstraint);
            return;
        }
        // FIXME: Consider the constraints when choosing among multiple devices. For now just select the first available
        // device of the appropriate type.
        RefPtr<RealtimeMediaSource> videoSource = AVCaptureDeviceManager::singleton().bestSourcesForTypeAndConstraints(RealtimeMediaSource::Video, videoConstraints.get()).at(0);
        ASSERT(videoSource);
        
        videoSources.append(videoSource.release());
    }
    
    client->didCreateStream(MediaStreamPrivate::create(audioSources, videoSources));
}

void RealtimeMediaSourceCenterMac::createMediaStream(MediaStreamCreationClient* client, const String& audioDeviceID, const String& videoDeviceID)
{
    ASSERT(client);
    Vector<RefPtr<RealtimeMediaSource>> audioSources;
    Vector<RefPtr<RealtimeMediaSource>> videoSources;

    if (!audioDeviceID.isEmpty()) {
        RefPtr<RealtimeMediaSource> audioSource = AVCaptureDeviceManager::singleton().sourceWithUID(audioDeviceID, RealtimeMediaSource::Audio, nullptr);
        if (audioSource)
            audioSources.append(audioSource.release());
    }
    if (!videoDeviceID.isEmpty()) {
        RefPtr<RealtimeMediaSource> videoSource = AVCaptureDeviceManager::singleton().sourceWithUID(videoDeviceID, RealtimeMediaSource::Video, nullptr);
        if (videoSource)
            videoSources.append(videoSource.release());
    }

    client->didCreateStream(MediaStreamPrivate::create(audioSources, videoSources));
}

bool RealtimeMediaSourceCenterMac::getMediaStreamTrackSources(PassRefPtr<MediaStreamTrackSourcesRequestClient> prpClient)
{
    RefPtr<MediaStreamTrackSourcesRequestClient> requestClient = prpClient;

    TrackSourceInfoVector sources = AVCaptureDeviceManager::singleton().getSourcesInfo(requestClient->requestOrigin());

    callOnMainThread([this, requestClient, sources] {
        requestClient->didCompleteTrackSourceInfoRequest(sources);
    });

    return true;
}

RefPtr<TrackSourceInfo> RealtimeMediaSourceCenterMac::sourceWithUID(const String& UID, RealtimeMediaSource::Type type, MediaConstraints* constraints)
{
    RefPtr<RealtimeMediaSource> mediaSource = AVCaptureDeviceManager::singleton().sourceWithUID(UID, type, constraints);
    if (!mediaSource)
        return nullptr;
    return TrackSourceInfo::create(mediaSource->persistentID(), mediaSource->id(), mediaSource->type() == RealtimeMediaSource::Type::Video ? TrackSourceInfo::SourceKind::Video : TrackSourceInfo::SourceKind::Audio, mediaSource->name());
}

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM)
