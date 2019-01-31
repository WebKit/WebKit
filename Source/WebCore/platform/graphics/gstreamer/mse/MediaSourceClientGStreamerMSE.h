/*
 * Copyright (C) 2016 Metrological Group B.V.
 * Copyright (C) 2016 Igalia S.L
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * aint with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#pragma once

#if ENABLE(VIDEO) && USE(GSTREAMER) && ENABLE(MEDIA_SOURCE)

#include "GStreamerCommon.h"
#include "MediaSourcePrivate.h"
#include "MediaSourcePrivateClient.h"
#include "WebKitMediaSourceGStreamer.h"
#include <wtf/MediaTime.h>

namespace WebCore {

class ContentType;
class MediaPlayerPrivateGStreamerMSE;
class MediaSample;
class SourceBufferPrivateGStreamer;

class MediaSourceClientGStreamerMSE : public RefCounted<MediaSourceClientGStreamerMSE> {
public:
    static Ref<MediaSourceClientGStreamerMSE> create(MediaPlayerPrivateGStreamerMSE&);
    virtual ~MediaSourceClientGStreamerMSE();

    // From MediaSourceGStreamer.
    MediaSourcePrivate::AddStatus addSourceBuffer(RefPtr<SourceBufferPrivateGStreamer>, const ContentType&);
    void durationChanged(const MediaTime&);
    void markEndOfStream(MediaSourcePrivate::EndOfStreamStatus);

    // From SourceBufferPrivateGStreamer.
    void abort(RefPtr<SourceBufferPrivateGStreamer>);
    void resetParserState(RefPtr<SourceBufferPrivateGStreamer>);
    void append(RefPtr<SourceBufferPrivateGStreamer>, Vector<unsigned char>&&);
    void removedFromMediaSource(RefPtr<SourceBufferPrivateGStreamer>);
    void flush(AtomicString);
    void enqueueSample(Ref<MediaSample>&&);
    void allSamplesInTrackEnqueued(const AtomicString&);

    const MediaTime& duration();
    GRefPtr<WebKitMediaSrc> webKitMediaSrc();

private:
    MediaSourceClientGStreamerMSE(MediaPlayerPrivateGStreamerMSE&);

    MediaPlayerPrivateGStreamerMSE& m_playerPrivate;
    MediaTime m_duration;
};

} // namespace WebCore.

#endif // USE(GSTREAMER)
