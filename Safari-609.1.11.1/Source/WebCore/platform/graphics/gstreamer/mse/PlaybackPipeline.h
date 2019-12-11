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

// PlaybackPipeline is (sort of) a friend class of WebKitMediaSourceGStreamer.

#include "WebKitMediaSourceGStreamer.h"
#include "WebKitMediaSourceGStreamerPrivate.h"

#include <gst/gst.h>
#include <wtf/Condition.h>
#include <wtf/glib/GRefPtr.h>

namespace WTF {
template<> GRefPtr<WebKitMediaSrc> adoptGRef(WebKitMediaSrc*);
template<> WebKitMediaSrc* refGPtr<WebKitMediaSrc>(WebKitMediaSrc*);
template<> void derefGPtr<WebKitMediaSrc>(WebKitMediaSrc*);
};

namespace WebCore {

class ContentType;
class SourceBufferPrivateGStreamer;
class MediaSourceGStreamer;

class PlaybackPipeline: public RefCounted<PlaybackPipeline> {
public:
    static Ref<PlaybackPipeline> create()
    {
        return adoptRef(*new PlaybackPipeline());
    }

    virtual ~PlaybackPipeline() = default;

    void setWebKitMediaSrc(WebKitMediaSrc*);
    WebKitMediaSrc* webKitMediaSrc();

    MediaSourcePrivate::AddStatus addSourceBuffer(RefPtr<SourceBufferPrivateGStreamer>);
    void removeSourceBuffer(RefPtr<SourceBufferPrivateGStreamer>);
    void attachTrack(RefPtr<SourceBufferPrivateGStreamer>, RefPtr<TrackPrivateBase>, GstCaps*);
    void reattachTrack(RefPtr<SourceBufferPrivateGStreamer>, RefPtr<TrackPrivateBase>, GstCaps*);
    void notifyDurationChanged();

    // From MediaSourceGStreamer.
    void markEndOfStream(MediaSourcePrivate::EndOfStreamStatus);

    // From SourceBufferPrivateGStreamer.
    void flush(AtomString);
    void enqueueSample(Ref<MediaSample>&&);
    void allSamplesInTrackEnqueued(const AtomString&);

    GstElement* pipeline();
private:
    PlaybackPipeline() = default;
    GRefPtr<WebKitMediaSrc> m_webKitMediaSrc;
};

} // namespace WebCore.

#endif // USE(GSTREAMER)
