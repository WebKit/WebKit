/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 * Copyright (C) 2013 Orange
 * Copyright (C) 2014 Sebastian Dröge <sebastian@centricular.com>
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

#include "NotImplemented.h"
#include "SourceBufferPrivateGStreamer.h"
#include "WebKitMediaSourceGStreamer.h"

#include <wtf/glib/GRefPtr.h>

namespace WebCore {

void MediaSourceGStreamer::open(MediaSourcePrivateClient* mediaSource, WebKitMediaSrc* src)
{
    ASSERT(mediaSource);
    mediaSource->setPrivateAndOpen(adoptRef(*new MediaSourceGStreamer(mediaSource, src)));
}

MediaSourceGStreamer::MediaSourceGStreamer(MediaSourcePrivateClient* mediaSource, WebKitMediaSrc* src)
    : m_client(adoptRef(new MediaSourceClientGStreamer(src)))
    , m_mediaSource(mediaSource)
    , m_readyState(MediaPlayer::HaveNothing)
{
    ASSERT(m_client);
}

MediaSourceGStreamer::~MediaSourceGStreamer()
{
}

MediaSourceGStreamer::AddStatus MediaSourceGStreamer::addSourceBuffer(const ContentType& contentType, RefPtr<SourceBufferPrivate>& sourceBufferPrivate)
{
    RefPtr<SourceBufferPrivateGStreamer> sourceBufferPrivateGStreamer = SourceBufferPrivateGStreamer::create(*m_client, contentType);
    sourceBufferPrivate = sourceBufferPrivateGStreamer;
    return m_client->addSourceBuffer(sourceBufferPrivateGStreamer, contentType);
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
    return m_readyState;
}

void MediaSourceGStreamer::setReadyState(MediaPlayer::ReadyState state)
{
    m_readyState = state;
}

void MediaSourceGStreamer::waitForSeekCompleted()
{
    notImplemented();
}

void MediaSourceGStreamer::seekCompleted()
{
    notImplemented();
}

}
#endif
