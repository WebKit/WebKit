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

#include "MediaStreamCenterMac.h"

#include "AVCaptureDeviceManager.h"
#include "MediaStreamCreationClient.h"
#include "MediaStreamPrivate.h"
#include "MediaStreamTrackSourcesRequestClient.h"
#include <wtf/MainThread.h>

namespace WebCore {

MediaStreamCenter& MediaStreamCenter::platformCenter()
{
    ASSERT(isMainThread());
    DEPRECATED_DEFINE_STATIC_LOCAL(MediaStreamCenterMac, center, ());
    return center;
}

MediaStreamCenterMac::MediaStreamCenterMac()
{
}

MediaStreamCenterMac::~MediaStreamCenterMac()
{
}

void MediaStreamCenterMac::validateRequestConstraints(PassRefPtr<MediaStreamCreationClient> prpQueryClient, PassRefPtr<MediaConstraints> audioConstraints, PassRefPtr<MediaConstraints> videoConstraints)
{
    RefPtr<MediaStreamCreationClient> client = prpQueryClient;
    
    ASSERT(client);

    if (audioConstraints) {
        String invalidConstraint;
        AVCaptureDeviceManager::shared().verifyConstraintsForMediaType(MediaStreamSource::Audio, audioConstraints.get(), invalidConstraint);
        if (!invalidConstraint.isEmpty()) {
            client->constraintsInvalid(invalidConstraint);
            return;
        }
    }

    if (videoConstraints) {
        String invalidConstraint;
        AVCaptureDeviceManager::shared().verifyConstraintsForMediaType(MediaStreamSource::Video, videoConstraints.get(), invalidConstraint);
        if (!invalidConstraint.isEmpty()) {
            client->constraintsInvalid(invalidConstraint);
            return;
        }
    }

    client->constraintsValidated();
}
    
void MediaStreamCenterMac::createMediaStream(PassRefPtr<MediaStreamCreationClient> prpQueryClient, PassRefPtr<MediaConstraints> audioConstraints, PassRefPtr<MediaConstraints> videoConstraints)
{
    RefPtr<MediaStreamCreationClient> client = prpQueryClient;
    
    ASSERT(client);
    
    Vector<RefPtr<MediaStreamSource>> audioSources;
    Vector<RefPtr<MediaStreamSource>> videoSources;
    
    if (audioConstraints) {
        String invalidConstraint;
        AVCaptureDeviceManager::shared().verifyConstraintsForMediaType(MediaStreamSource::Audio, audioConstraints.get(), invalidConstraint);
        if (!invalidConstraint.isEmpty()) {
            client->failedToCreateStreamWithConstraintsError(invalidConstraint);
            return;
        }
        
        RefPtr<MediaStreamSource> audioSource = AVCaptureDeviceManager::shared().bestSourceForTypeAndConstraints(MediaStreamSource::Audio, audioConstraints.get());
        ASSERT(audioSource);
        
        audioSources.append(audioSource.release());
    }
    
    if (videoConstraints) {
        String invalidConstraint;
        AVCaptureDeviceManager::shared().verifyConstraintsForMediaType(MediaStreamSource::Video, videoConstraints.get(), invalidConstraint);
        if (!invalidConstraint.isEmpty()) {
            client->failedToCreateStreamWithConstraintsError(invalidConstraint);
            return;
        }
        
        RefPtr<MediaStreamSource> videoSource = AVCaptureDeviceManager::shared().bestSourceForTypeAndConstraints(MediaStreamSource::Video, videoConstraints.get());
        ASSERT(videoSource);
        
        videoSources.append(videoSource.release());
    }
    
    client->didCreateStream(MediaStreamPrivate::create(audioSources, videoSources));
}

bool MediaStreamCenterMac::getMediaStreamTrackSources(PassRefPtr<MediaStreamTrackSourcesRequestClient> prpClient)
{
    RefPtr<MediaStreamTrackSourcesRequestClient> requestClient = prpClient;

    Vector<RefPtr<TrackSourceInfo>> sources = AVCaptureDeviceManager::shared().getSourcesInfo(requestClient->requestOrigin());

    requestClient->didCompleteRequest(sources);
    return true;
}

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM)
