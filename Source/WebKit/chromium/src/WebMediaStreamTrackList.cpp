/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "WebMediaStreamTrackList.h"

#if ENABLE(MEDIA_STREAM)

#include "MediaStreamTrack.h"
#include "MediaStreamTrackList.h"
#include "WebMediaStreamTrack.h"
#include "WebVector.h"

using namespace WebCore;

namespace WebKit {

void WebMediaStreamTrackList::initialize(const WebVector<WebMediaStreamTrack>& webTracks)
{
    TrackVector tracks(webTracks.size());
    for (size_t i = 0; i < webTracks.size(); ++i)
        tracks[i] = webTracks[i];

    m_private = MediaStreamTrackList::create(tracks);
}

void WebMediaStreamTrackList::reset()
{
    m_private.reset();
}

WebMediaStreamTrackList::WebMediaStreamTrackList(const WTF::PassRefPtr<MediaStreamTrackList>& trackList)
    : m_private(trackList)
{
}

WebMediaStreamTrackList::operator WTF::PassRefPtr<MediaStreamTrackList>() const
{
    return m_private.get();
}

} // namespace WebKit

#endif // ENABLE(MEDIA_STREAM)
