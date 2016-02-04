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
#include <wtf/text/CString.h>

GST_DEBUG_CATEGORY(webkit_openwebrtc_debug);
#define GST_CAT_DEFAULT webkit_openwebrtc_debug

namespace WebCore {

MediaPlayerPrivateGStreamerOwr::MediaPlayerPrivateGStreamerOwr(MediaPlayer* player)
    : MediaPlayerPrivateGStreamerBase(player)
{
    if (initializeGStreamerAndGStreamerDebugging()) {
        LOG_MEDIA_MESSAGE("Creating MediaPlayerPrivateGStreamerOwr");

        createVideoSink();
        createGSTAudioSinkBin();
    }
}

MediaPlayerPrivateGStreamerOwr::~MediaPlayerPrivateGStreamerOwr()
{
    LOG_MEDIA_MESSAGE("Destroying");

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
    return m_videoSource;
}

bool MediaPlayerPrivateGStreamerOwr::hasAudio() const
{
    return m_audioSource;
}

float MediaPlayerPrivateGStreamerOwr::currentTime() const
{
    gint64 position = GST_CLOCK_TIME_NONE;
    GstQuery* query = gst_query_new_position(GST_FORMAT_TIME);

    if (m_videoSource && gst_element_query(m_videoSink.get(), query))
        gst_query_parse_position(query, 0, &position);
    else if (m_audioSource && gst_element_query(m_audioSink.get(), query))
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
    notImplemented();
}

void MediaPlayerPrivateGStreamerOwr::load(MediaStreamPrivate& streamPrivate)
{
    if (!initializeGStreamer())
        return;

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

        RealtimeMediaSourceOwr* source = reinterpret_cast<RealtimeMediaSourceOwr*>(&track->source());
        OwrMediaSource* mediaSource = OWR_MEDIA_SOURCE(source->mediaSource());

        switch (track->type()) {
        case RealtimeMediaSource::Audio:
            if (m_audioSource && (m_audioSource.get() == source))
                g_object_set(m_audioRenderer.get(), "disabled", FALSE, nullptr);

            owr_media_renderer_set_source(OWR_MEDIA_RENDERER(m_audioRenderer.get()), mediaSource);
            m_audioSource = source;
            source->addObserver(this);
            break;
        case RealtimeMediaSource::Video:
            if (m_videoSource && (m_videoSource.get() == source))
                g_object_set(m_videoRenderer.get(), "disabled", FALSE, nullptr);

            // FIXME: Remove hardcoded video dimensions when the rendering performance:
            // https://webkit.org/b/153826.
            g_object_set(m_videoRenderer.get(), "width", 640, "height", 480, nullptr);
            owr_media_renderer_set_source(OWR_MEDIA_RENDERER(m_videoRenderer.get()), mediaSource);
            m_videoSource = source;
            source->addObserver(this);
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
    if (m_audioSource) {
        LOG_MEDIA_MESSAGE("Stop: disconnecting audio");
        g_object_set(m_audioRenderer.get(), "disabled", TRUE, nullptr);
    }
    if (m_videoSource) {
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

void MediaPlayerPrivateGStreamerOwr::getSupportedTypes(HashSet<String, ASCIICaseInsensitiveHash>&)
{
    // Not supported in this media player.
}

MediaPlayer::SupportsType MediaPlayerPrivateGStreamerOwr::supportsType(const MediaEngineSupportParameters&)
{
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

void MediaPlayerPrivateGStreamerOwr::sourceStopped()
{
    LOG_MEDIA_MESSAGE("Source stopped");

    if (!m_streamPrivate || !m_streamPrivate->active()) {
        stop();
        return;
    }

    for (auto& track : m_streamPrivate->tracks()) {
        auto* source = reinterpret_cast<RealtimeMediaSourceOwr*>(&track->source());
        if (track->enabled())
            continue;
        if (source == m_audioSource)
            g_object_set(m_audioRenderer.get(), "disabled", TRUE, nullptr);
        else if (source == m_videoSource)
            g_object_set(m_videoRenderer.get(), "disabled", TRUE, nullptr);
    }
}

void MediaPlayerPrivateGStreamerOwr::sourceMutedChanged()
{
    LOG_MEDIA_MESSAGE("Source muted state changed");
}

void MediaPlayerPrivateGStreamerOwr::sourceSettingsChanged()
{
    LOG_MEDIA_MESSAGE("Source settings changed");
}

bool MediaPlayerPrivateGStreamerOwr::preventSourceFromStopping()
{
    LOG_MEDIA_MESSAGE("Prevent source from stopping");
    return false;
}

GstElement* MediaPlayerPrivateGStreamerOwr::createVideoSink()
{
    GstElement* sink = MediaPlayerPrivateGStreamerBase::createVideoSink();
    m_videoRenderer = adoptGRef(owr_gst_video_renderer_new(sink));
    return sink;
}

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM) && USE(GSTREAMER) && USE(OPENWEBRTC)
