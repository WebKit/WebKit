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

    m_audioTrackMap.clear();
    m_videoTrackMap.clear();

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

    m_ended = false;
    m_paused = false;

    GST_DEBUG("Connecting to live stream, descriptor: %p", m_streamPrivate.get());

    if (m_videoTrack)
        maybeHandleChangeMutedState(*m_videoTrack.get());

    if (m_audioTrack)
        maybeHandleChangeMutedState(*m_audioTrack.get());
}

void MediaPlayerPrivateGStreamerOwr::pause()
{
    GST_DEBUG("Pause");
    m_paused = true;
    disableMediaTracks();
}

bool MediaPlayerPrivateGStreamerOwr::hasVideo() const
{
    return m_videoTrack;
}

bool MediaPlayerPrivateGStreamerOwr::hasAudio() const
{
    return m_audioTrack;
}

void MediaPlayerPrivateGStreamerOwr::setVolume(float volume)
{
    if (!m_audioTrack)
        return;

    auto& realTimeMediaSource = static_cast<RealtimeMediaSourceOwr&>(m_audioTrack->source());
    auto mediaSource = OWR_MEDIA_SOURCE(realTimeMediaSource.mediaSource());

    GST_DEBUG("Setting volume: %f", volume);
    g_object_set(mediaSource, "volume", static_cast<gdouble>(volume), nullptr);
}

void MediaPlayerPrivateGStreamerOwr::setMuted(bool muted)
{
    if (!m_audioTrack)
        return;

    auto& realTimeMediaSource = static_cast<RealtimeMediaSourceOwr&>(m_audioTrack->source());
    auto mediaSource = OWR_MEDIA_SOURCE(realTimeMediaSource.mediaSource());
    if (!mediaSource)
        return;

    GST_DEBUG("Setting mute: %s", muted ? "on":"off");
    g_object_set(mediaSource, "mute", muted, nullptr);
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

    GST_LOG("Position %" GST_TIME_FORMAT, GST_TIME_ARGS(position));
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

    m_streamPrivate = &streamPrivate;
    if (!m_streamPrivate->active()) {
        loadingFailed(MediaPlayer::NetworkError);
        return;
    }

    if (streamPrivate.hasVideo() && !m_videoSink)
        createVideoSink();

    if (streamPrivate.hasAudio() && !m_audioSink)
        createGSTAudioSinkBin();

    GST_DEBUG("Loading MediaStreamPrivate %p video: %s, audio: %s", &streamPrivate, streamPrivate.hasVideo() ? "yes":"no", streamPrivate.hasAudio() ? "yes":"no");

    m_readyState = MediaPlayer::HaveNothing;
    m_networkState = MediaPlayer::Loading;
    m_player->networkStateChanged();
    m_player->readyStateChanged();

    for (auto track : m_streamPrivate->tracks()) {
        if (!track->enabled()) {
            GST_DEBUG("Track %s disabled", track->label().ascii().data());
            continue;
        }

        GST_DEBUG("Processing track %s", track->label().ascii().data());

        bool observeTrack = false;

        // TODO: Support for multiple tracks of the same type.

        switch (track->type()) {
        case RealtimeMediaSource::Type::Audio:
            if (!m_audioTrack) {
                String preSelectedDevice = getenv("WEBKIT_AUDIO_DEVICE");
                if (!preSelectedDevice || (preSelectedDevice == track->label())) {
                    m_audioTrack = track;
                    auto audioTrack = AudioTrackPrivateMediaStream::create(*m_audioTrack.get());
                    m_player->addAudioTrack(*audioTrack);
                    m_audioTrackMap.add(track->id(), audioTrack);
                    observeTrack = true;
                }
            }
            break;
        case RealtimeMediaSource::Type::Video:
            if (!m_videoTrack) {
                String preSelectedDevice = getenv("WEBKIT_VIDEO_DEVICE");
                if (!preSelectedDevice || (preSelectedDevice == track->label())) {
                    m_videoTrack = track;
                    auto videoTrack = VideoTrackPrivateMediaStream::create(*m_videoTrack.get());
                    m_player->addVideoTrack(*videoTrack);
                    videoTrack->setSelected(true);
                    m_videoTrackMap.add(track->id(), videoTrack);
                    observeTrack = true;
                }
            }
            break;
        case RealtimeMediaSource::Type::None:
            GST_WARNING("Loading a track with None type");
        }

        if (observeTrack)
            track->addObserver(*this);
    }

    m_readyState = MediaPlayer::HaveEnoughData;
    m_player->readyStateChanged();
}

void MediaPlayerPrivateGStreamerOwr::loadingFailed(MediaPlayer::NetworkState error)
{
    if (m_networkState != error) {
        GST_WARNING("Loading failed, error: %d", error);
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

void MediaPlayerPrivateGStreamerOwr::disableMediaTracks()
{
    if (m_audioTrack) {
        GST_DEBUG("Stop: disconnecting audio");
        g_object_set(m_audioRenderer.get(), "disabled", true, nullptr);
        owr_media_renderer_set_source(OWR_MEDIA_RENDERER(m_audioRenderer.get()), nullptr);
    }

    if (m_videoTrack) {
        GST_DEBUG("Stop: disconnecting video");
        g_object_set(m_videoRenderer.get(), "disabled", true, nullptr);
        owr_media_renderer_set_source(OWR_MEDIA_RENDERER(m_videoRenderer.get()), nullptr);
    }
}

void MediaPlayerPrivateGStreamerOwr::stop()
{
    disableMediaTracks();
    if (m_videoTrack) {
        auto videoTrack = m_videoTrackMap.get(m_videoTrack->id());
        if (videoTrack)
            videoTrack->setSelected(false);
    }
}

void MediaPlayerPrivateGStreamerOwr::registerMediaEngine(MediaEngineRegistrar registrar)
{
    if (initializeGStreamerAndGStreamerDebugging()) {
        registrar([](MediaPlayer* player) {
            return std::make_unique<MediaPlayerPrivateGStreamerOwr>(player);
        }, getSupportedTypes, supportsType, nullptr, nullptr, nullptr, nullptr);
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

    // Pre-roll an autoaudiosink so that the platform audio sink is created and
    // can be retrieved from the autoaudiosink bin.
    GRefPtr<GstElement> sink = gst_element_factory_make("autoaudiosink", nullptr);
    GstChildProxy* childProxy = GST_CHILD_PROXY(sink.get());
    gst_element_set_state(sink.get(), GST_STATE_READY);
    GRefPtr<GstElement> platformSink = adoptGRef(GST_ELEMENT(gst_child_proxy_get_child_by_index(childProxy, 0)));
    GstElementFactory* factory = gst_element_get_factory(platformSink.get());

    // Dispose now un-needed autoaudiosink.
    gst_element_set_state(sink.get(), GST_STATE_NULL);

    // Create a fresh new audio sink compatible with the platform.
    m_audioSink = gst_element_factory_create(factory, nullptr);
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
        g_object_set(m_audioRenderer.get(), "disabled", true, nullptr);
    else if (&track == m_videoTrack) {
        g_object_set(m_videoRenderer.get(), "disabled", true, nullptr);
        auto& realTimeMediaSource = static_cast<RealtimeMediaSourceOwr&>(m_videoTrack->source());
        realTimeMediaSource.setWidth(0);
        realTimeMediaSource.setHeight(0);
        auto videoTrack = m_videoTrackMap.get(m_videoTrack->id());
        if (videoTrack)
            videoTrack->setSelected(false);
    }

    gboolean audioDisabled;
    gboolean videoDisabled;
    g_object_get(m_audioRenderer.get(), "disabled", &audioDisabled, nullptr);
    g_object_get(m_videoRenderer.get(), "disabled", &videoDisabled, nullptr);
    if (audioDisabled && videoDisabled) {
        m_ended = true;
        m_player->timeChanged();
    }
}

void MediaPlayerPrivateGStreamerOwr::trackMutedChanged(MediaStreamTrackPrivate& track)
{
    GST_DEBUG("Track muted state changed");

    maybeHandleChangeMutedState(track);
}

void MediaPlayerPrivateGStreamerOwr::maybeHandleChangeMutedState(MediaStreamTrackPrivate& track)
{
    auto& realTimeMediaSource = static_cast<RealtimeMediaSourceOwr&>(track.source());
    auto mediaSource = OWR_MEDIA_SOURCE(realTimeMediaSource.mediaSource());

    GST_DEBUG("%s track now %s", track.type() == RealtimeMediaSource::Type::Audio ? "audio":"video", realTimeMediaSource.muted() ? "muted":"un-muted");
    switch (track.type()) {
    case RealtimeMediaSource::Type::Audio:
        if (!realTimeMediaSource.muted()) {
            g_object_set(m_audioRenderer.get(), "disabled", false, nullptr);
            owr_media_renderer_set_source(OWR_MEDIA_RENDERER(m_audioRenderer.get()), mediaSource);
        } else {
            g_object_set(m_audioRenderer.get(), "disabled", true, nullptr);
            owr_media_renderer_set_source(OWR_MEDIA_RENDERER(m_audioRenderer.get()), nullptr);
        }
        if (mediaSource)
            g_object_set(mediaSource, "mute", !track.enabled(), nullptr);
        break;
    case RealtimeMediaSource::Type::Video:
        if (!realTimeMediaSource.muted()) {
            g_object_set(m_videoRenderer.get(), "disabled", false, nullptr);
            owr_media_renderer_set_source(OWR_MEDIA_RENDERER(m_videoRenderer.get()), mediaSource);
        } else {
            g_object_set(m_videoRenderer.get(), "disabled", true, nullptr);
            owr_media_renderer_set_source(OWR_MEDIA_RENDERER(m_videoRenderer.get()), nullptr);
        }
        break;
    case RealtimeMediaSource::Type::None:
        GST_WARNING("Trying to change mute state of a track with None type");
    }
}

void MediaPlayerPrivateGStreamerOwr::trackSettingsChanged(MediaStreamTrackPrivate&)
{
    GST_DEBUG("Track settings changed");
}

void MediaPlayerPrivateGStreamerOwr::trackEnabledChanged(MediaStreamTrackPrivate& track)
{
    GST_DEBUG("%s track now %s", track.type() == RealtimeMediaSource::Type::Audio ? "audio":"video", track.enabled() ? "enabled":"disabled");

    switch (track.type()) {
    case RealtimeMediaSource::Type::Audio:
        g_object_set(m_audioRenderer.get(), "disabled", !track.enabled(), nullptr);
        break;
    case RealtimeMediaSource::Type::Video:
        g_object_set(m_videoRenderer.get(), "disabled", !track.enabled(), nullptr);
        break;
    case RealtimeMediaSource::Type::None:
        GST_WARNING("Trying to change enabled state of a track with None type");
    }
}

GstElement* MediaPlayerPrivateGStreamerOwr::createVideoSink()
{
    GstElement* sink;
#if USE(GSTREAMER_GL)
    // No need to create glupload and glcolorconvert here because they are
    // already created by the video renderer.
    // FIXME: This should probably return a RefPtr. See https://bugs.webkit.org/show_bug.cgi?id=164709.
    sink = MediaPlayerPrivateGStreamerBase::createGLAppSink();
    m_videoSink = sink;
#else
    if (m_streamPrivate->getVideoRenderer()) {
        m_videoRenderer = m_streamPrivate->getVideoRenderer();
        m_videoSink = m_streamPrivate->getVideoSinkElement();
        g_signal_connect_swapped(m_videoSink.get(), "repaint-requested", G_CALLBACK(MediaPlayerPrivateGStreamerBase::repaintCallback), this);
        g_object_get(m_videoRenderer.get(), "sink", &sink, nullptr);
    } else {
        GstElement* gldownload = gst_element_factory_make("gldownload", nullptr);
        GstElement* videoconvert = gst_element_factory_make("videoconvert", nullptr);
        GstElement* webkitSink = MediaPlayerPrivateGStreamerBase::createVideoSink();
        sink = gst_bin_new(nullptr);
        gst_bin_add_many(GST_BIN(sink), gldownload, videoconvert, webkitSink, nullptr);
        gst_element_link_many(gldownload, videoconvert, webkitSink, nullptr);
        GRefPtr<GstPad> pad = adoptGRef(gst_element_get_static_pad(gldownload, "sink"));
        gst_element_add_pad(sink, gst_ghost_pad_new("sink", pad.get()));
    }
#endif
    if (!m_videoRenderer) {
        m_videoRenderer = adoptGRef(owr_gst_video_renderer_new(sink));
#if USE(GSTREAMER_GL)
        owr_video_renderer_set_request_context_callback(OWR_VIDEO_RENDERER(m_videoRenderer.get()), (OwrVideoRendererRequestContextCallback) MediaPlayerPrivateGStreamerBase::requestGLContext, this, nullptr);
#endif
        m_streamPrivate->setVideoRenderer(m_videoRenderer.get(), videoSink());
    }
    return sink;
}

void MediaPlayerPrivateGStreamerOwr::setSize(const IntSize& size)
{
    if (size == m_size)
        return;

    MediaPlayerPrivateGStreamerBase::setSize(size);
    if (m_videoRenderer)
        g_object_set(m_videoRenderer.get(), "width", size.width(), "height", size.height(), nullptr);

    if (!m_videoTrack)
        return;

    auto& realTimeMediaSource = static_cast<RealtimeMediaSourceOwr&>(m_videoTrack->source());
    realTimeMediaSource.setWidth(size.width());
    realTimeMediaSource.setHeight(size.height());
}

FloatSize MediaPlayerPrivateGStreamerOwr::naturalSize() const
{
    auto size = MediaPlayerPrivateGStreamerBase::naturalSize();

    // In case we are not playing the video we return the size we set to the media source.
    if (m_videoTrack && size.isZero()) {
        auto& realTimeMediaSource = static_cast<RealtimeMediaSourceOwr&>(m_videoTrack->source());
        return realTimeMediaSource.size();
    }

    return size;
}

} // namespace WebCore

#endif // ENABLE(VIDEO) && ENABLE(MEDIA_STREAM) && USE(GSTREAMER) && USE(OPENWEBRTC)
