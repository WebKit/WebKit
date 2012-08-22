/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
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

#include "MockWebMediaStreamCenter.h"

#include <public/WebICECandidateDescriptor.h>
#include <public/WebMediaStreamCenterClient.h>
#include <public/WebMediaStreamComponent.h>
#include <public/WebMediaStreamDescriptor.h>
#include <public/WebMediaStreamSource.h>
#include <public/WebMediaStreamSourcesRequest.h>
#include <public/WebSessionDescriptionDescriptor.h>
#include <public/WebVector.h>

using namespace WebKit;

MockWebMediaStreamCenter::MockWebMediaStreamCenter(WebMediaStreamCenterClient* client)
{
}

void MockWebMediaStreamCenter::queryMediaStreamSources(const WebMediaStreamSourcesRequest& request)
{
    WebVector<WebMediaStreamSource> audioSources, videoSources;
    request.didCompleteQuery(audioSources, videoSources);
}

void MockWebMediaStreamCenter::didEnableMediaStreamTrack(const WebMediaStreamDescriptor&, const WebMediaStreamComponent& component)
{
    component.source().setReadyState(WebMediaStreamSource::ReadyStateLive);
}

void MockWebMediaStreamCenter::didDisableMediaStreamTrack(const WebMediaStreamDescriptor&, const WebMediaStreamComponent& component)
{
    component.source().setReadyState(WebMediaStreamSource::ReadyStateMuted);
}

void MockWebMediaStreamCenter::didStopLocalMediaStream(const WebMediaStreamDescriptor& stream)
{
    WebVector<WebMediaStreamComponent> audioComponents;
    stream.audioSources(audioComponents);
    for (size_t i = 0; i < audioComponents.size(); ++i)
        audioComponents[i].source().setReadyState(WebMediaStreamSource::ReadyStateEnded);

    WebVector<WebMediaStreamComponent> videoComponents;
    stream.videoSources(videoComponents);
    for (size_t i = 0; i < videoComponents.size(); ++i)
        videoComponents[i].source().setReadyState(WebMediaStreamSource::ReadyStateEnded);
}

void MockWebMediaStreamCenter::didCreateMediaStream(WebMediaStreamDescriptor&)
{
}

WebString MockWebMediaStreamCenter::constructSDP(const WebICECandidateDescriptor& iceCandidate)
{
    string16 result = iceCandidate.label();
    result += WebString(":");
    result += iceCandidate.candidateLine();
    result += WebString(";");
    return result;
}

WebString MockWebMediaStreamCenter::constructSDP(const WebSessionDescriptionDescriptor& sessionDescription)
{
    string16 result = sessionDescription.initialSDP();
    result += WebString(";");
    for (size_t i = 0; i < sessionDescription.numberOfAddedCandidates(); ++i) {
        result += constructSDP(sessionDescription.candidate(i));
        result += WebString(";");
    }
    return result;
}

#endif // ENABLE(MEDIA_STREAM)
