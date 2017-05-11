/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 * Copyright (C) 2013 Orange
 * Copyright (C) 2014 Sebastian Dröge <sebastian@centricular.com>
 * Copyright (C) 2015, 2016 Metrological Group B.V.
 * Copyright (C) 2015, 2016 Igalia, S.L
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
#include "MediaSourceGStreamer.h"

#if ENABLE(MEDIA_SOURCE) && USE(GSTREAMER)

#include "ContentType.h"
#include "MediaPlayerPrivateGStreamer.h"
#include "MediaPlayerPrivateGStreamerMSE.h"
#include "MediaSourceClientGStreamerMSE.h"
#include "NotImplemented.h"
#include "SourceBufferPrivateGStreamer.h"
#include "TimeRanges.h"
#include "WebKitMediaSourceGStreamer.h"
#include <wtf/RefPtr.h>
#include <wtf/glib/GRefPtr.h>

namespace WebCore {

void MediaSourceGStreamer::open(MediaSourcePrivateClient& mediaSource, MediaPlayerPrivateGStreamerMSE& playerPrivate)
{
    mediaSource.setPrivateAndOpen(adoptRef(*new MediaSourceGStreamer(mediaSource, playerPrivate)));
}

MediaSourceGStreamer::MediaSourceGStreamer(MediaSourcePrivateClient& mediaSource, MediaPlayerPrivateGStreamerMSE& playerPrivate)
    : MediaSourcePrivate()
    , m_client(MediaSourceClientGStreamerMSE::create(playerPrivate))
    , m_mediaSource(mediaSource)
    , m_playerPrivate(playerPrivate)
{
}

MediaSourceGStreamer::~MediaSourceGStreamer()
{
    for (auto& sourceBufferPrivate : m_sourceBuffers)
        sourceBufferPrivate->clearMediaSource();
}

MediaSourceGStreamer::AddStatus MediaSourceGStreamer::addSourceBuffer(const ContentType& contentType, RefPtr<SourceBufferPrivate>& sourceBufferPrivate)
{
    sourceBufferPrivate = SourceBufferPrivateGStreamer::create(this, m_client.get(), contentType);
    RefPtr<SourceBufferPrivateGStreamer> sourceBufferPrivateGStreamer = static_cast<SourceBufferPrivateGStreamer*>(sourceBufferPrivate.get());
    m_sourceBuffers.add(sourceBufferPrivateGStreamer);
    return m_client->addSourceBuffer(sourceBufferPrivateGStreamer, contentType);
}

void MediaSourceGStreamer::removeSourceBuffer(SourceBufferPrivate* sourceBufferPrivate)
{
    RefPtr<SourceBufferPrivateGStreamer> sourceBufferPrivateGStreamer = static_cast<SourceBufferPrivateGStreamer*>(sourceBufferPrivate);
    ASSERT(m_sourceBuffers.contains(sourceBufferPrivateGStreamer));

    sourceBufferPrivateGStreamer->clearMediaSource();
    m_sourceBuffers.remove(sourceBufferPrivateGStreamer);
    m_activeSourceBuffers.remove(sourceBufferPrivateGStreamer.get());
}

void MediaSourceGStreamer::durationChanged()
{
    m_client->durationChanged(m_mediaSource->duration());
}

void MediaSourceGStreamer::markEndOfStream(EndOfStreamStatus status)
{
    m_client->markEndOfStream(status);
}

void MediaSourceGStreamer::unmarkEndOfStream()
{
    notImplemented();
}

MediaPlayer::ReadyState MediaSourceGStreamer::readyState() const
{
    return m_playerPrivate.readyState();
}

void MediaSourceGStreamer::setReadyState(MediaPlayer::ReadyState state)
{
    m_playerPrivate.setReadyState(state);
}

void MediaSourceGStreamer::waitForSeekCompleted()
{
    m_playerPrivate.waitForSeekCompleted();
}

void MediaSourceGStreamer::seekCompleted()
{
    m_playerPrivate.seekCompleted();
}

void MediaSourceGStreamer::sourceBufferPrivateDidChangeActiveState(SourceBufferPrivateGStreamer* sourceBufferPrivate, bool isActive)
{
    if (!isActive)
        m_activeSourceBuffers.remove(sourceBufferPrivate);
    else if (!m_activeSourceBuffers.contains(sourceBufferPrivate))
        m_activeSourceBuffers.add(sourceBufferPrivate);
}

std::unique_ptr<PlatformTimeRanges> MediaSourceGStreamer::buffered()
{
    return m_mediaSource->buffered();
}

}
#endif
