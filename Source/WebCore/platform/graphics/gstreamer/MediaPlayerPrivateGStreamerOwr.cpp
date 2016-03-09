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

#if ENABLE(MEDIA_STREAM) && USE(GSTREAMER) && USE(OPENWEBRTC)

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
    LOG_MEDIA_MESSAGE("Destroying");

    if (hasAudio())
        m_audioTrack->removeObserver(*this);
    if (hasVideo())
        m_videoTrack->removeObserver(*this);

    stop();
}

void MediaPlayerPrivateGStreamerOwr::play()
{
    LOG_MEDIA_MESSAGE("Play");

    if (!m_streamPrivate || !m_streamPrivate->active()) {
        m_readyState = MediaPlayer::HaveNothing;
        loadingFailed(MediaPlayer::Empty);
        return;
    }

    m_paused = false;
    internalLoad();
}

void MediaPlayerPrivateGStreamerOwr::pause()
{
    LOG_MEDIA_MESSAGE("Pause");
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

    LOG_MEDIA_MESSAGE("Position %" GST_TIME_FORMAT, GST_TIME_ARGS(position));
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

    if (!m_videoSink)
        createVideoSink();

    if (!m_audioSink)
        createGSTAudioSinkBin();

    LOG_MEDIA_MESSAGE("Loading MediaStreamPrivate %p", &streamPrivate);

    m_streamPrivate = &streamPrivate;
    if (!m_streamPrivate->active()) {
        loadingFailed(MediaPlayer::NetworkError);
        return;
    }

    m_readyState = MediaPlayer::HaveNothing;
    m_networkState = MediaPlayer::Loading;
    m_player->networkStateChanged();
    m_player->readyStateChanged();

    if (!internalLoad())
        return;

    // If the stream contains video, wait for first video frame before setting
    // HaveEnoughData.
    if (!hasVideo())
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

bool MediaPlayerPrivateGStreamerOwr::internalLoad()
{
    if (!m_stopped)
        return false;

    m_stopped = false;
    if (!m_streamPrivate || !m_streamPrivate->active()) {
        loadingFailed(MediaPlayer::NetworkError);
        return false;
    }

    LOG_MEDIA_MESSAGE("Connecting to live stream, descriptor: %p", m_streamPrivate.get());

    for (auto track : m_streamPrivate->tracks()) {
        if (!track->enabled()) {
            LOG_MEDIA_MESSAGE("Track %s disabled", track->label().ascii().data());
            continue;
        }

        OwrMediaSource* mediaSource = OWR_MEDIA_SOURCE(reinterpret_cast<RealtimeMediaSourceOwr*>(&track->source())->mediaSource());

        switch (track->type()) {
        case RealtimeMediaSource::Audio:
            if (m_audioTrack && (m_audioTrack.get() == track))
                g_object_set(m_audioRenderer.get(), "disabled", FALSE, nullptr);

            owr_media_renderer_set_source(OWR_MEDIA_RENDERER(m_audioRenderer.get()), mediaSource);
            m_audioTrack = track;
            track->addObserver(*this);
            break;
        case RealtimeMediaSource::Video:
            if (m_videoTrack && (m_videoTrack.get() == track))
                g_object_set(m_videoRenderer.get(), "disabled", FALSE, nullptr);

            // FIXME: Remove hardcoded video dimensions when the rendering performance:
            // https://webkit.org/b/153826.
            g_object_set(m_videoRenderer.get(), "width", 640, "height", 480, nullptr);
            owr_media_renderer_set_source(OWR_MEDIA_RENDERER(m_videoRenderer.get()), mediaSource);
            m_videoTrack = track;
            track->addObserver(*this);
            break;
        case RealtimeMediaSource::None:
            WARN_MEDIA_MESSAGE("Loading a track with None type");
        }
    }

    m_readyState = MediaPlayer::HaveEnoughData;
    m_player->readyStateChanged();
    return true;
}

void MediaPlayerPrivateGStreamerOwr::stop()
{
    if (m_stopped)
        return;

    m_stopped = true;
    if (m_audioTrack) {
        LOG_MEDIA_MESSAGE("Stop: disconnecting audio");
        g_object_set(m_audioRenderer.get(), "disabled", TRUE, nullptr);
    }
    if (m_videoTrack) {
        LOG_MEDIA_MESSAGE("Stop: disconnecting video");
        g_object_set(m_videoRenderer.get(), "disabled", TRUE, nullptr);
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
    LOG_MEDIA_MESSAGE("Creating audio sink");
    // FIXME: volume/mute support: https://webkit.org/b/153828.

    GRefPtr<GstElement> sink = gst_element_factory_make("autoaudiosink", 0);
    GstChildProxy* childProxy = GST_CHILD_PROXY(sink.get());
    m_audioSink = adoptGRef(GST_ELEMENT(gst_child_proxy_get_child_by_index(childProxy, 0)));
    gst_element_set_state(sink.get(), GST_STATE_NULL);

    m_audioRenderer = adoptGRef(owr_gst_audio_renderer_new(m_audioSink.get()));
}

void MediaPlayerPrivateGStreamerOwr::trackEnded(MediaStreamTrackPrivate& track)
{
    LOG_MEDIA_MESSAGE("Track ended");

    if (!m_streamPrivate || !m_streamPrivate->active()) {
        stop();
        return;
    }

    if (&track == m_audioTrack)
        g_object_set(m_audioRenderer.get(), "disabled", TRUE, nullptr);
    else if (&track == m_videoTrack)
        g_object_set(m_videoRenderer.get(), "disabled", TRUE, nullptr);
}

void MediaPlayerPrivateGStreamerOwr::trackMutedChanged(MediaStreamTrackPrivate&)
{
    LOG_MEDIA_MESSAGE("Track muted state changed");
}

void MediaPlayerPrivateGStreamerOwr::trackSettingsChanged(MediaStreamTrackPrivate&)
{
    LOG_MEDIA_MESSAGE("Track settings changed");
}

void MediaPlayerPrivateGStreamerOwr::trackEnabledChanged(MediaStreamTrackPrivate&)
{
    LOG_MEDIA_MESSAGE("Track enabled changed");
}

GstElement* MediaPlayerPrivateGStreamerOwr::createVideoSink()
{
    GstElement* sink = MediaPlayerPrivateGStreamerBase::createVideoSink();
    m_videoRenderer = adoptGRef(owr_gst_video_renderer_new(sink));
    return sink;
}

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM) && USE(GSTREAMER) && USE(OPENWEBRTC)
