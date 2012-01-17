/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 * Copyright (C) 2011 Ericsson AB. All rights reserved.
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
#include "MediaStreamTrack.h"

#if ENABLE(MEDIA_STREAM)

#include "MediaStreamCenter.h"
#include "MediaStreamComponent.h"

namespace WebCore {

PassRefPtr<MediaStreamTrack> MediaStreamTrack::create(PassRefPtr<MediaStreamDescriptor> streamDescriptor, size_t trackIndex)
{
    return adoptRef(new MediaStreamTrack(streamDescriptor, trackIndex));
}

MediaStreamTrack::MediaStreamTrack(PassRefPtr<MediaStreamDescriptor> streamDescriptor, size_t trackIndex)
    : m_streamDescriptor(streamDescriptor)
    , m_trackIndex(trackIndex)
{
}

MediaStreamTrack::~MediaStreamTrack()
{
}

String MediaStreamTrack::kind() const
{
    DEFINE_STATIC_LOCAL(String, audioKind, ("audio"));
    DEFINE_STATIC_LOCAL(String, videoKind, ("video"));

    switch (m_streamDescriptor->component(m_trackIndex)->source()->type()) {
    case MediaStreamSource::TypeAudio:
        return audioKind;
    case MediaStreamSource::TypeVideo:
        return videoKind;
    }

    ASSERT_NOT_REACHED();
    return audioKind;
}

String MediaStreamTrack::label() const
{
    return m_streamDescriptor->component(m_trackIndex)->source()->name();
}

bool MediaStreamTrack::enabled() const
{
    return m_streamDescriptor->component(m_trackIndex)->enabled();
}

void MediaStreamTrack::setEnabled(bool enabled)
{
    if (enabled == m_streamDescriptor->component(m_trackIndex)->enabled())
        return;

    m_streamDescriptor->component(m_trackIndex)->setEnabled(enabled);

    MediaStreamCenter::instance().didSetMediaStreamTrackEnabled(m_streamDescriptor.get(), m_trackIndex);
}

MediaStreamComponent* MediaStreamTrack::component()
{
    return m_streamDescriptor->component(m_trackIndex);
}

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM)
