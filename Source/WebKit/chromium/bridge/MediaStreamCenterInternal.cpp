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

#include "MediaStreamCenterInternal.h"

#include "MediaStreamCenter.h"
#include "MediaStreamComponent.h"
#include "MediaStreamSource.h"
#include "WebKit.h"
#include "platform/WebKitPlatformSupport.h"
#include "platform/WebMediaStreamCenter.h"
#include "platform/WebMediaStreamComponent.h"
#include "platform/WebMediaStreamDescriptor.h"
#include "platform/WebMediaStreamSourcesRequest.h"
#include <wtf/PassOwnPtr.h>

namespace WebCore {

MediaStreamCenterInternal::MediaStreamCenterInternal(MediaStreamCenter* owner)
    : m_private(adoptPtr(WebKit::webKitPlatformSupport()->createMediaStreamCenter(this)))
    , m_owner(owner)
{
}

MediaStreamCenterInternal::~MediaStreamCenterInternal()
{
}

void MediaStreamCenterInternal::queryMediaStreamSources(PassRefPtr<MediaStreamSourcesQueryClient> client)
{
    // FIXME: Remove this "short-circuit" and forward to m_private when Chrome has implemented WebMediaStreamCenter.
    MediaStreamSourceVector audioSources, videoSources;
    client->didCompleteQuery(audioSources, videoSources);
}

void MediaStreamCenterInternal::didSetMediaStreamTrackEnabled(MediaStreamDescriptor* stream, MediaStreamComponent* component)
{
    if (m_private) {
        if (component->enabled())
            m_private->didEnableMediaStreamTrack(stream, component);
        else
            m_private->didDisableMediaStreamTrack(stream, component);
    }
}

void MediaStreamCenterInternal::didStopLocalMediaStream(MediaStreamDescriptor* stream)
{
    if (m_private)
        m_private->didStopLocalMediaStream(stream);
}

void MediaStreamCenterInternal::didConstructMediaStream(MediaStreamDescriptor* stream)
{
    if (m_private)
        m_private->didConstructMediaStream(stream);
}

void MediaStreamCenterInternal::stopLocalMediaStream(const WebKit::WebMediaStreamDescriptor& stream)
{
    m_owner->endLocalMediaStream(stream);
}

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM)
