/*
 *  Copyright (C) 2012 Collabora Ltd. All rights reserved.
 *  Copyright (C) 2014, 2015 Igalia S.L. All rights reserved.
 *  Copyright (C) 2015 Metrological All rights reserved.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "config.h"

#include "MediaPlayerPrivateGStreamerOwr.h"

#if ENABLE(VIDEO) && ENABLE(MEDIA_STREAM) && USE(GSTREAMER) && USE(OPENWEBRTC)

#include "GStreamerUtilities.h"
#include "MediaPlayer.h"
#include "MediaStreamPrivate.h"
#include "NotImplemented.h"
#include "RealtimeMediaSourceOwr.h"
#include "URL.h"
#include <gst/audio/streamvolume.h>
#include <owr/owr.h>
#include <owr/owr_gst_audio_renderer.h>
#include <owr/owr_gst_video_renderer.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/text/CString.h>

GST_DEBUG_CATEGORY(webkit_openwebrtc_debug);
#define GST_CAT_DEFAULT webkit_openwebrtc_debug

namespace WebCore {

MediaPlayerPrivateGStreamerOwr::MediaPlayerPrivateGStreamerOwr(MediaPlayer* player)
    : MediaPlayerPrivateGStreamerBase(player)
{
    initializeGStreamerAndGStreamerDebugging();
}

MediaPlayerPrivateGStreamerOwr::~MediaPlayerPrivateGStreamerOwr()
{
    GST_TRACE("Destroying");

    if (hasAudio())
        m_audioTrack->removeObserver(*this);
    if (hasVideo())
        m_videoTrack->removeObserver(*this);

    stop();
}

void MediaPlayerPrivateGStreamerOwr::play()
{
    GST_DEBUG("Play");

    if (!m_streamPrivate || !m_streamPrivate->active()) {
        m_readyState = MediaPlayer::HaveNothing;
        loadingFailed(MediaPlayer::Empty);
        return;
    }

    m_paused = false;

    GST_DEBUG("Connecting to live stream, descriptor: %p", m_streamPrivate.get());

    for (auto track : m_streamPrivate->tracks()) {
        if (!track->enabled()) {
            GST_DEBUG("Track %s disabled", track->label().ascii().data());
            continue;
        }

        maybeHandleChangeMutedState(*track);
    }
}

void MediaPlayerPrivateGStreamerOwr::pause()
{
    GST_DEBUG("Pause");
    m_paused = true;
    stop();
}

bool MediaPlayerPrivateGStreamerOwr::hasVideo() const
{
    return m_videoTrack;
}

bool MediaPlayerPrivateGStreamerOwr::hasAudio() const
{
    return m_audioTrack;
}

float MediaPlayerPrivateGStreamerOwr::currentTime() const
{
    gint64 position = GST_CLOCK_TIME_NONE;
    GstQuery* query = gst_query_new_position(GST_FORMAT_TIME);

    if (m_videoTrack && gst_element_query(m_videoSink.get(), query))
        gst_query_parse_position(query, 0, &position);
    else if (m_audioTrack && gst_element_query(m_audioSink.get(), query))
        gst_query_parse_position(query, 0, &position);

    float result = 0;
    if (static_cast<GstClockTime>(position) != GST_CLOCK_TIME_NONE)
        result = static_cast<double>(position) / GST_SECOND;

    GST_DEBUG("Position %" GST_TIME_FORMAT, GST_TIME_ARGS(position));
    gst_query_unref(query);

    return result;
}

void MediaPlayerPrivateGStreamerOwr::load(const String &)
{
    // Properly fail so the global MediaPlayer tries to fallback to the next MediaPlayerPrivate.
    m_networkState = MediaPlayer::FormatError;
    m_player->networkStateChanged();
}

#if ENABLE(MEDIA_SOURCE)
void MediaPlayerPrivateGStreamerOwr::load(const String&, MediaSourcePrivateClient*)
{
    // Properly fail so the global MediaPlayer tries to fallback to the next MediaPlayerPrivate.
    m_networkState = MediaPlayer::FormatError;
    m_player->networkStateChanged();
}
#endif

void MediaPlayerPrivateGStreamerOwr::load(MediaStreamPrivate& streamPrivate)
{
    if (!initializeGStreamer())
        return;

    if (streamPrivate.hasVideo() && !m_videoSink)
        createVideoSink();

    if (streamPrivate.hasAudio() && !m_audioSink)
        createGSTAudioSinkBin();

    GST_DEBUG("Loading MediaStreamPrivate %p", &streamPrivate);

    m_streamPrivate = &streamPrivate;
    if (!m_streamPrivate->active()) {
        loadingFailed(MediaPlayer::NetworkError);
        return;
    }

    m_readyState = MediaPlayer::HaveNothing;
    m_networkState = MediaPlayer::Loading;
    m_player->networkStateChanged();
    m_player->readyStateChanged();

    for (auto track : m_streamPrivate->tracks()) {
        if (!track->enabled()) {
            GST_DEBUG("Track %s disabled", track->label().ascii().data());
            continue;
        }

        track->addObserver(*this);

        switch (track->type()) {
        case RealtimeMediaSource::Audio:
            m_audioTrack = track;
            break;
        case RealtimeMediaSource::Video:
            m_videoTrack = track;
            break;
        case RealtimeMediaSource::None:
            GST_WARNING("Loading a track with None type");
        }
    }

    m_readyState = MediaPlayer::HaveEnoughData;
    m_player->readyStateChanged();
}

void MediaPlayerPrivateGStreamerOwr::loadingFailed(MediaPlayer::NetworkState error)
{
    if (m_networkState != error) {
        m_networkState = error;
        m_player->networkStateChanged();
    }
    if (m_readyState != MediaPlayer::HaveNothing) {
        m_readyState = MediaPlayer::HaveNothing;
        m_player->readyStateChanged();
    }
}

bool MediaPlayerPrivateGStreamerOwr::didLoadingProgress() const
{
    // FIXME: Implement loading progress support.
    return true;
}

void MediaPlayerPrivateGStreamerOwr::stop()
{
    if (m_audioTrack) {
        GST_DEBUG("Stop: disconnecting audio");
        g_object_set(m_audioRenderer.get(), "disabled", TRUE, nullptr);
        owr_media_renderer_set_source(OWR_MEDIA_RENDERER(m_audioRenderer.get()), nullptr);
    }
    if (m_videoTrack) {
        GST_DEBUG("Stop: disconnecting video");
        g_object_set(m_videoRenderer.get(), "disabled", TRUE, nullptr);
        owr_media_renderer_set_source(OWR_MEDIA_RENDERER(m_videoRenderer.get()), nullptr);
    }
}

void MediaPlayerPrivateGStreamerOwr::registerMediaEngine(MediaEngineRegistrar registrar)
{
    if (initializeGStreamerAndGStreamerDebugging()) {
        registrar([](MediaPlayer* player) {
            return std::make_unique<MediaPlayerPrivateGStreamerOwr>(player);
        }, getSupportedTypes, supportsType, 0, 0, 0, 0);
    }
}

void MediaPlayerPrivateGStreamerOwr::getSupportedTypes(HashSet<String, ASCIICaseInsensitiveHash>& types)
{
    // Not supported in this media player.
    static NeverDestroyed<HashSet<String, ASCIICaseInsensitiveHash>> cache;
    types = cache;
}

MediaPlayer::SupportsType MediaPlayerPrivateGStreamerOwr::supportsType(const MediaEngineSupportParameters& parameters)
{
    if (parameters.isMediaStream)
        return MediaPlayer::IsSupported;
    return MediaPlayer::IsNotSupported;
}

bool MediaPlayerPrivateGStreamerOwr::initializeGStreamerAndGStreamerDebugging()
{
    if (!initializeGStreamer())
        return false;

    static std::once_flag debugRegisteredFlag;
    std::call_once(debugRegisteredFlag, [] {
        GST_DEBUG_CATEGORY_INIT(webkit_openwebrtc_debug, "webkitowrplayer", 0, "WebKit OpenWebRTC player");
    });

    return true;
}

void MediaPlayerPrivateGStreamerOwr::createGSTAudioSinkBin()
{
    ASSERT(!m_audioSink);
    GST_DEBUG("Creating audio sink");
    // FIXME: volume/mute support: https://webkit.org/b/153828.

    GRefPtr<GstElement> sink = gst_element_factory_make("autoaudiosink", 0);
    GstChildProxy* childProxy = GST_CHILD_PROXY(sink.get());
    m_audioSink = adoptGRef(GST_ELEMENT(gst_child_proxy_get_child_by_index(childProxy, 0)));
    gst_element_set_state(sink.get(), GST_STATE_NULL);

    m_audioRenderer = adoptGRef(owr_gst_audio_renderer_new(m_audioSink.get()));
}

void MediaPlayerPrivateGStreamerOwr::trackEnded(MediaStreamTrackPrivate& track)
{
    GST_DEBUG("Track ended");

    if (!m_streamPrivate || !m_streamPrivate->active()) {
        stop();
        return;
    }

    if (&track == m_audioTrack)
        g_object_set(m_audioRenderer.get(), "disabled", TRUE, nullptr);
    else if (&track == m_videoTrack)
        g_object_set(m_videoRenderer.get(), "disabled", TRUE, nullptr);
}

void MediaPlayerPrivateGStreamerOwr::trackMutedChanged(MediaStreamTrackPrivate& track)
{
    GST_DEBUG("Track muted state changed");

    maybeHandleChangeMutedState(track);
}

void MediaPlayerPrivateGStreamerOwr::maybeHandleChangeMutedState(MediaStreamTrackPrivate& track)
{
    auto realTimeMediaSource = reinterpret_cast<RealtimeMediaSourceOwr*>(&track.source());
    auto mediaSource = OWR_MEDIA_SOURCE(realTimeMediaSource->mediaSource());

    switch (track.type()) {
    case RealtimeMediaSource::Audio:
        if (!realTimeMediaSource->muted()) {
            g_object_set(m_audioRenderer.get(), "disabled", false, nullptr);
            owr_media_renderer_set_source(OWR_MEDIA_RENDERER(m_audioRenderer.get()), mediaSource);
        } else {
            g_object_set(m_audioRenderer.get(), "disabled", true, nullptr);
            owr_media_renderer_set_source(OWR_MEDIA_RENDERER(m_audioRenderer.get()), nullptr);
        }
        break;
    case RealtimeMediaSource::Video:
        if (!realTimeMediaSource->muted()) {
            g_object_set(m_videoRenderer.get(), "disabled", false, nullptr);
            owr_media_renderer_set_source(OWR_MEDIA_RENDERER(m_videoRenderer.get()), mediaSource);
        } else {
            g_object_set(m_videoRenderer.get(), "disabled", true, nullptr);
            owr_media_renderer_set_source(OWR_MEDIA_RENDERER(m_videoRenderer.get()), nullptr);
        }
        break;
    case RealtimeMediaSource::None:
        GST_WARNING("Trying to change mute state of a track with None type");
    }
}

void MediaPlayerPrivateGStreamerOwr::trackSettingsChanged(MediaStreamTrackPrivate&)
{
    GST_DEBUG("Track settings changed");
}

void MediaPlayerPrivateGStreamerOwr::trackEnabledChanged(MediaStreamTrackPrivate&)
{
    GST_DEBUG("Track enabled changed");
}

GstElement* MediaPlayerPrivateGStreamerOwr::createVideoSink()
{
#if USE(GSTREAMER_GL)
    // No need to create glupload and glcolorconvert here because they are
    // already created by the video renderer.
    GstElement* sink = MediaPlayerPrivateGStreamerBase::createGLAppSink();
    m_videoSink = sink;
#else
    GstElement* sink = gst_bin_new(nullptr);
    GstElement* gldownload = gst_element_factory_make("gldownload", nullptr);
    GstElement* videoconvert = gst_element_factory_make("videoconvert", nullptr);
    GstElement* webkitSink = MediaPlayerPrivateGStreamerBase::createVideoSink();
    gst_bin_add_many(GST_BIN(sink), gldownload, videoconvert, webkitSink, nullptr);
    gst_element_link_many(gldownload, videoconvert, webkitSink, nullptr);
    GRefPtr<GstPad> pad = gst_element_get_static_pad(gldownload, "sink");
    gst_element_add_pad(sink, gst_ghost_pad_new("sink", pad.get()));
#endif
    m_videoRenderer = adoptGRef(owr_gst_video_renderer_new(sink));

    return sink;
}

void MediaPlayerPrivateGStreamerOwr::setSize(const IntSize& size)
{
    if (size == m_size)
        return;

    MediaPlayerPrivateGStreamerBase::setSize(size);
    g_object_set(m_videoRenderer.get(), "width", size.width(), "height", size.height(), nullptr);
}

} // namespace WebCore

#endif // ENABLE(VIDEO) && ENABLE(MEDIA_STREAM) && USE(GSTREAMER) && USE(OPENWEBRTC)
