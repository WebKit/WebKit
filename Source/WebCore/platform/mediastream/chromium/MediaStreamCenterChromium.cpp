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

#include "MediaStreamCenterChromium.h"

#include "IceCandidateDescriptor.h"
#include "MediaStreamDescriptor.h"
#include "MediaStreamSourcesQueryClient.h"
#include "SessionDescriptionDescriptor.h"
#include <public/Platform.h>
#include <public/WebICECandidateDescriptor.h>
#include <public/WebMediaStreamCenter.h>
#include <public/WebMediaStreamComponent.h>
#include <public/WebMediaStreamDescriptor.h>
#include <public/WebMediaStreamSourcesRequest.h>
#include <public/WebSessionDescriptionDescriptor.h>
#include <wtf/MainThread.h>
#include <wtf/PassOwnPtr.h>

namespace WebCore {

MediaStreamCenter& MediaStreamCenter::instance()
{
    ASSERT(isMainThread());
    DEFINE_STATIC_LOCAL(MediaStreamCenterChromium, center, ());
    return center;
}

MediaStreamCenterChromium::MediaStreamCenterChromium()
    : m_private(adoptPtr(WebKit::Platform::current()->createMediaStreamCenter(this)))
{
}

MediaStreamCenterChromium::~MediaStreamCenterChromium()
{
}

void MediaStreamCenterChromium::queryMediaStreamSources(PassRefPtr<MediaStreamSourcesQueryClient> client)
{
    // FIXME: Remove this "short-circuit" and forward to m_private when Chrome and DumpRenderTree has implemented WebMediaStreamCenter.
    MediaStreamSourceVector audioSources, videoSources;
    client->didCompleteQuery(audioSources, videoSources);
}

void MediaStreamCenterChromium::didSetMediaStreamTrackEnabled(MediaStreamDescriptor* stream,  MediaStreamComponent* component)
{
    if (m_private) {
        if (component->enabled())
            m_private->didEnableMediaStreamTrack(stream, component);
        else
            m_private->didDisableMediaStreamTrack(stream, component);
    }
}

void MediaStreamCenterChromium::didStopLocalMediaStream(MediaStreamDescriptor* stream)
{
    if (m_private)
        m_private->didStopLocalMediaStream(stream);
}

void MediaStreamCenterChromium::didCreateMediaStream(MediaStreamDescriptor* stream)
{
    if (m_private) {
        // FIXME: Remove when Chromium have switched to the new API.
        m_private->didConstructMediaStream(stream);

        WebKit::WebMediaStreamDescriptor webStream(stream);
        m_private->didCreateMediaStream(webStream);
    }
}

String MediaStreamCenterChromium::constructSDP(IceCandidateDescriptor* iceCandidate)
{
    return m_private ? m_private->constructSDP(WebKit::WebICECandidateDescriptor(iceCandidate)) : "";
}

String MediaStreamCenterChromium::constructSDP(SessionDescriptionDescriptor* sessionDescription)
{
    return m_private ? m_private->constructSDP(WebKit::WebSessionDescriptionDescriptor(sessionDescription)) : "";
}

void MediaStreamCenterChromium::stopLocalMediaStream(const WebKit::WebMediaStreamDescriptor& stream)
{
    endLocalMediaStream(stream);
}

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM)
