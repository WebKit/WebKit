/*
 * Copyright (C) 2007, 2009 Apple Inc.  All rights reserved.
 * Copyright (C) 2007 Collabora Ltd.  All rights reserved.
 * Copyright (C) 2007 Alp Toker <alp@atoker.com>
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

#include "config.h"

#if ENABLE(VIDEO)

#include "MediaPlayerPrivateGStreamer.h"
#include "VideoSinkGStreamer.h"

#include "CString.h"
#include "GraphicsContext.h"
#include "IntRect.h"
#include "KURL.h"
#include "MIMETypeRegistry.h"
#include "MediaPlayer.h"
#include "NotImplemented.h"
#include "ScrollView.h"
#include "Widget.h"
#include <wtf/GOwnPtr.h>

#include <gdk/gdkx.h>
#include <gst/base/gstbasesrc.h>
#include <gst/gst.h>
#include <gst/interfaces/mixer.h>
#include <gst/interfaces/xoverlay.h>
#include <gst/video/video.h>
#include <limits>
#include <math.h>

using namespace std;

namespace WebCore {

gboolean mediaPlayerPrivateErrorCallback(GstBus* bus, GstMessage* message, gpointer data)
{
    if (GST_MESSAGE_TYPE(message) == GST_MESSAGE_ERROR)
    {
        GOwnPtr<GError> err;
        GOwnPtr<gchar> debug;

        gst_message_parse_error(message, &err.outPtr(), &debug.outPtr());
        if (err->code == 3) {
            LOG_VERBOSE(Media, "File not found");
            MediaPlayerPrivate* mp = reinterpret_cast<MediaPlayerPrivate*>(data);
            if (mp)
                mp->loadingFailed();
        } else
            LOG_VERBOSE(Media, "Error: %d, %s", err->code,  err->message);
    }
    return true;
}

gboolean mediaPlayerPrivateEOSCallback(GstBus* bus, GstMessage* message, gpointer data)
{
    if (GST_MESSAGE_TYPE(message) == GST_MESSAGE_EOS)
    {
        LOG_VERBOSE(Media, "End of Stream");
        MediaPlayerPrivate* mp = reinterpret_cast<MediaPlayerPrivate*>(data);
        mp->didEnd();
    }
    return true;
}

gboolean mediaPlayerPrivateStateCallback(GstBus* bus, GstMessage* message, gpointer data)
{
    if (GST_MESSAGE_TYPE(message) == GST_MESSAGE_STATE_CHANGED)
    {
        MediaPlayerPrivate* mp = reinterpret_cast<MediaPlayerPrivate*>(data);
        mp->updateStates();
    }
    return true;
}

gboolean mediaPlayerPrivateBufferingCallback(GstBus* bus, GstMessage* message, gpointer data)
{
    if (GST_MESSAGE_TYPE(message) == GST_MESSAGE_BUFFERING)
    {
        gint percent = 0;
        gst_message_parse_buffering(message, &percent);
        LOG_VERBOSE(Media, "Buffering %d", percent);
    }
    return true;
}

static void mediaPlayerPrivateRepaintCallback(WebKitVideoSink*, MediaPlayerPrivate* playerPrivate)
{
    playerPrivate->repaint();
}

MediaPlayerPrivateInterface* MediaPlayerPrivate::create(MediaPlayer* player) 
{ 
    return new MediaPlayerPrivate(player);
}

void MediaPlayerPrivate::registerMediaEngine(MediaEngineRegistrar registrar)
{
    if (isAvailable())
        registrar(create, getSupportedTypes, supportsType);
}

MediaPlayerPrivate::MediaPlayerPrivate(MediaPlayer* player)
    : m_player(player)
    , m_playBin(0)
    , m_videoSink(0)
    , m_source(0)
    , m_rate(1.0f)
    , m_endTime(numeric_limits<float>::infinity())
    , m_isEndReached(false)
    , m_volume(0.5f)
    , m_networkState(MediaPlayer::Empty)
    , m_readyState(MediaPlayer::HaveNothing)
    , m_startedPlaying(false)
    , m_isStreaming(false)
    , m_size(IntSize())
    , m_visible(true)
{

    static bool gstInitialized = false;
    // FIXME: We should pass the arguments from the command line
    if (!gstInitialized) {
        gst_init(0, NULL);
        gstInitialized = true;
    }

    // FIXME: The size shouldn't be fixed here, this is just a quick hack.
    m_surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 640, 480);
}

MediaPlayerPrivate::~MediaPlayerPrivate()
{
    if (m_surface)
        cairo_surface_destroy(m_surface);

    if (m_playBin) {
        gst_element_set_state(m_playBin, GST_STATE_NULL);
        gst_object_unref(GST_OBJECT(m_playBin));
    }
}

void MediaPlayerPrivate::load(const String& url)
{
    LOG_VERBOSE(Media, "Load %s", url.utf8().data());
    if (m_networkState != MediaPlayer::Loading) {
        m_networkState = MediaPlayer::Loading;
        m_player->networkStateChanged();
    }
    if (m_readyState != MediaPlayer::HaveNothing) {
        m_readyState = MediaPlayer::HaveNothing;
        m_player->readyStateChanged();
    }

    createGSTPlayBin(url);
    pause();
}

void MediaPlayerPrivate::play()
{
    LOG_VERBOSE(Media, "Play");
    // When end reached, rewind for Test video-seek-past-end-playing
    if (m_isEndReached)
        seek(0);
    m_isEndReached = false;

    gst_element_set_state(m_playBin, GST_STATE_PLAYING);
    m_startedPlaying = true;
}

void MediaPlayerPrivate::pause()
{
    LOG_VERBOSE(Media, "Pause");
    gst_element_set_state(m_playBin, GST_STATE_PAUSED);
    m_startedPlaying = false;
}

float MediaPlayerPrivate::duration() const
{
    if (!m_playBin)
        return 0.0;

    GstFormat timeFormat = GST_FORMAT_TIME;
    gint64 timeLength = 0;

    // FIXME: We try to get the duration, but we do not trust the
    // return value of the query function only; the problem we are
    // trying to work-around here is that pipelines in stream mode may
    // not be able to figure out the duration, but still return true!
    // See https://bugs.webkit.org/show_bug.cgi?id=24639.
    if (!gst_element_query_duration(m_playBin, &timeFormat, &timeLength) || timeLength <= 0) {
        LOG_VERBOSE(Media, "Time duration query failed.");
        m_isStreaming = true;
        return numeric_limits<float>::infinity();
    }

    LOG_VERBOSE(Media, "Duration: %" GST_TIME_FORMAT, GST_TIME_ARGS(timeLength));

    return (float) (timeLength / 1000000000.0);
    // FIXME: handle 3.14.9.5 properly
}

float MediaPlayerPrivate::currentTime() const
{
    if (!m_playBin)
        return 0;
    // Necessary as sometimes, gstreamer return 0:00 at the EOS
    if (m_isEndReached)
        return m_endTime;

    float ret;

    GstQuery* query = gst_query_new_position(GST_FORMAT_TIME);
    if (gst_element_query(m_playBin, query)) {
        gint64 position;
        gst_query_parse_position(query, NULL, &position);
        ret = (float) (position / 1000000000.0);
        LOG_VERBOSE(Media, "Position %" GST_TIME_FORMAT, GST_TIME_ARGS(position));
    } else {
        LOG_VERBOSE(Media, "Position query failed...");
        ret = 0.0;
    }
    gst_query_unref(query);

    return ret;
}

void MediaPlayerPrivate::seek(float time)
{
    GstClockTime sec = (GstClockTime)(time * GST_SECOND);

    if (!m_playBin)
        return;

    if (m_isStreaming)
        return;

    LOG_VERBOSE(Media, "Seek: %" GST_TIME_FORMAT, GST_TIME_ARGS(sec));
    // FIXME: What happens when the seeked position is not available?
    if (!gst_element_seek( m_playBin, m_rate,
            GST_FORMAT_TIME,
            (GstSeekFlags)(GST_SEEK_FLAG_FLUSH),
            GST_SEEK_TYPE_SET, sec,
            GST_SEEK_TYPE_NONE, GST_CLOCK_TIME_NONE))
        LOG_VERBOSE(Media, "Seek to %f failed", time);
}

void MediaPlayerPrivate::setEndTime(float time)
{
    if (!m_playBin)
        return;
    if (m_isStreaming)
        return;
    if (m_endTime != time) {
        m_endTime = time;
        GstClockTime start = (GstClockTime)(currentTime() * GST_SECOND);
        GstClockTime end   = (GstClockTime)(time * GST_SECOND);
        LOG_VERBOSE(Media, "setEndTime: %" GST_TIME_FORMAT, GST_TIME_ARGS(end));
        // FIXME: What happens when the seeked position is not available?
        if (!gst_element_seek(m_playBin, m_rate,
                GST_FORMAT_TIME,
                (GstSeekFlags)(GST_SEEK_FLAG_FLUSH | GST_SEEK_FLAG_ACCURATE),
                GST_SEEK_TYPE_SET, start,
                GST_SEEK_TYPE_SET, end ))
            LOG_VERBOSE(Media, "Seek to %f failed", time);
    }
}

void MediaPlayerPrivate::startEndPointTimerIfNeeded()
{
    notImplemented();
}

void MediaPlayerPrivate::cancelSeek()
{
    notImplemented();
}

void MediaPlayerPrivate::endPointTimerFired(Timer<MediaPlayerPrivate>*)
{
    notImplemented();
}

bool MediaPlayerPrivate::paused() const
{
    return !m_startedPlaying;
}

bool MediaPlayerPrivate::seeking() const
{
    return false;
}

// Returns the size of the video
IntSize MediaPlayerPrivate::naturalSize() const
{
    if (!hasVideo())
        return IntSize();

    int x = 0, y = 0;
    if (GstPad* pad = gst_element_get_static_pad(m_videoSink, "sink")) {
        gst_video_get_size(GST_PAD(pad), &x, &y);
        gst_object_unref(GST_OBJECT(pad));
    }

    return IntSize(x, y);
}

bool MediaPlayerPrivate::hasVideo() const
{
    gint currentVideo = -1;
    if (m_playBin)
        g_object_get(G_OBJECT(m_playBin), "current-video", &currentVideo, NULL);
    return currentVideo > -1;
}

void MediaPlayerPrivate::setVolume(float volume)
{
    m_volume = volume;
    LOG_VERBOSE(Media, "Volume to %f", volume);
    setMuted(false);
}

void MediaPlayerPrivate::setMuted(bool b)
{
    if (!m_playBin)
        return;

    if (b) {
        g_object_get(G_OBJECT(m_playBin), "volume", &m_volume, NULL);
        g_object_set(G_OBJECT(m_playBin), "volume", (double)0.0, NULL);
    } else {
        g_object_set(G_OBJECT(m_playBin), "volume", m_volume, NULL);
    }
}

void MediaPlayerPrivate::setRate(float rate)
{
    if (rate == 0.0) {
        gst_element_set_state(m_playBin, GST_STATE_PAUSED);
        return;
    }
    if (m_isStreaming)
        return;

    m_rate = rate;
    LOG_VERBOSE(Media, "Set Rate to %f", rate);
    if (!gst_element_seek(m_playBin, rate,
            GST_FORMAT_TIME,
            (GstSeekFlags)(GST_SEEK_FLAG_FLUSH | GST_SEEK_FLAG_ACCURATE),
            GST_SEEK_TYPE_SET, (GstClockTime) (currentTime() * GST_SECOND),
            GST_SEEK_TYPE_SET, (GstClockTime) (m_endTime * GST_SECOND)))
        LOG_VERBOSE(Media, "Set Rate to %f failed", rate);
}

int MediaPlayerPrivate::dataRate() const
{
    notImplemented();
    return 1;
}

MediaPlayer::NetworkState MediaPlayerPrivate::networkState() const
{
    return m_networkState;
}

MediaPlayer::ReadyState MediaPlayerPrivate::readyState() const
{
    return m_readyState;
}

float MediaPlayerPrivate::maxTimeBuffered() const
{
    notImplemented();
    LOG_VERBOSE(Media, "maxTimeBuffered");
    // rtsp streams are not buffered
    return m_isStreaming ? 0 : maxTimeLoaded();
}

float MediaPlayerPrivate::maxTimeSeekable() const
{
    // TODO
    LOG_VERBOSE(Media, "maxTimeSeekable");
    if (m_isStreaming)
        return numeric_limits<float>::infinity();
    // infinite duration means live stream
    return maxTimeLoaded();
}

float MediaPlayerPrivate::maxTimeLoaded() const
{
    // TODO
    LOG_VERBOSE(Media, "maxTimeLoaded");
    notImplemented();
    return duration();
}

unsigned MediaPlayerPrivate::bytesLoaded() const
{
    notImplemented();
    LOG_VERBOSE(Media, "bytesLoaded");
    /*if (!m_playBin)
        return 0;
    float dur = duration();
    float maxTime = maxTimeLoaded();
    if (!dur)
        return 0;*/
    return 1;//totalBytes() * maxTime / dur;
}

bool MediaPlayerPrivate::totalBytesKnown() const
{
    notImplemented();
    LOG_VERBOSE(Media, "totalBytesKnown");
    return totalBytes() > 0;
}

unsigned MediaPlayerPrivate::totalBytes() const
{
    notImplemented();
    LOG_VERBOSE(Media, "totalBytes");
    if (!m_playBin)
        return 0;

    if (!m_source)
        return 0;

    // Do something with m_source to get the total bytes of the media

    return 100;
}

void MediaPlayerPrivate::cancelLoad()
{
    notImplemented();
}

void MediaPlayerPrivate::updateStates()
{
    // There is no (known) way to get such level of information about
    // the state of GStreamer, therefore, when in PAUSED state,
    // we are sure we can display the first frame and go to play

    MediaPlayer::NetworkState oldNetworkState = m_networkState;
    MediaPlayer::ReadyState oldReadyState = m_readyState;
    GstState state;
    GstState pending;

    if (!m_playBin)
        return;

    GstStateChangeReturn ret = gst_element_get_state (m_playBin,
        &state, &pending, 250 * GST_NSECOND);

    switch(ret) {
    case GST_STATE_CHANGE_SUCCESS:
        LOG_VERBOSE(Media, "State: %s, pending: %s",
            gst_element_state_get_name(state),
            gst_element_state_get_name(pending));

        if (state == GST_STATE_READY) {
            m_readyState = MediaPlayer::HaveEnoughData;
        } else if (state == GST_STATE_PAUSED) {
            m_readyState = MediaPlayer::HaveEnoughData;
        }
        m_networkState = MediaPlayer::Loaded;

        g_object_get(m_playBin, "source", &m_source, NULL);
        if (!m_source)
            LOG_VERBOSE(Media, "m_source is NULL");
        break;
    case GST_STATE_CHANGE_ASYNC:
        LOG_VERBOSE(Media, "Async: State: %s, pending: %s",
            gst_element_state_get_name(state),
            gst_element_state_get_name(pending));
        // Change in progress
        return;
        break;
    case GST_STATE_CHANGE_NO_PREROLL:
        LOG_VERBOSE(Media, "No preroll: State: %s, pending: %s",
            gst_element_state_get_name(state),
            gst_element_state_get_name(pending));
        if (state == GST_STATE_READY) {
            m_readyState = MediaPlayer::HaveFutureData;
        } else if (state == GST_STATE_PAUSED) {
            m_readyState = MediaPlayer::HaveCurrentData;
        }
        m_networkState = MediaPlayer::Loading;
        break;
    default:
        LOG_VERBOSE(Media, "Else : %d", ret);
        break;
    }

    if (seeking())
        m_readyState = MediaPlayer::HaveNothing;

    if (m_networkState != oldNetworkState) {
        LOG_VERBOSE(Media, "Network State Changed from %u to %u",
            oldNetworkState, m_networkState);
        m_player->networkStateChanged();
    }
    if (m_readyState != oldReadyState) {
        LOG_VERBOSE(Media, "Ready State Changed from %u to %u",
            oldReadyState, m_readyState);
        m_player->readyStateChanged();
    }
}

void MediaPlayerPrivate::loadStateChanged()
{
    updateStates();
}

void MediaPlayerPrivate::rateChanged()
{
    updateStates();
}

void MediaPlayerPrivate::sizeChanged()
{
    notImplemented();
}

void MediaPlayerPrivate::timeChanged()
{
    updateStates();
    m_player->timeChanged();
}

void MediaPlayerPrivate::volumeChanged()
{
    m_player->volumeChanged();
}

void MediaPlayerPrivate::didEnd()
{
    m_isEndReached = true;
    pause();
    timeChanged();
}

void MediaPlayerPrivate::loadingFailed()
{
    if (m_networkState != MediaPlayer::NetworkError) {
        m_networkState = MediaPlayer::NetworkError;
        m_player->networkStateChanged();
    }
    if (m_readyState != MediaPlayer::HaveNothing) {
        m_readyState = MediaPlayer::HaveNothing;
        m_player->readyStateChanged();
    }
}

void MediaPlayerPrivate::setSize(const IntSize& size)
{
    m_size = size;
}

void MediaPlayerPrivate::setVisible(bool visible)
{
    m_visible = visible;
}

void MediaPlayerPrivate::repaint()
{
    m_player->repaint();
}

void MediaPlayerPrivate::paint(GraphicsContext* context, const IntRect& rect)
{
    if (context->paintingDisabled())
        return;

    if (!m_visible)
        return;

    //TODO: m_size vs rect?
    cairo_t* cr = context->platformContext();

    cairo_save(cr);
    cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
    cairo_translate(cr, rect.x(), rect.y());
    cairo_rectangle(cr, 0, 0, rect.width(), rect.height());
    cairo_set_source_surface(cr, m_surface, 0, 0);
    cairo_fill(cr);
    cairo_restore(cr);
}

void MediaPlayerPrivate::getSupportedTypes(HashSet<String>& types)
{
    // FIXME: query the engine to see what types are supported
    notImplemented();
    types.add(String("video/x-theora+ogg"));
}

MediaPlayer::SupportsType MediaPlayerPrivate::supportsType(const String& type, const String& codecs)
{
    // FIXME: query the engine to see what types are supported
    notImplemented();
    return type == "video/x-theora+ogg" ? (codecs.isEmpty() ? MediaPlayer::MayBeSupported : MediaPlayer::IsSupported) : MediaPlayer::IsNotSupported;
}

void MediaPlayerPrivate::createGSTPlayBin(String url)
{
    ASSERT(!m_playBin);
    m_playBin = gst_element_factory_make("playbin", "play");

    GstBus* bus = gst_pipeline_get_bus(GST_PIPELINE(m_playBin));
    gst_bus_add_signal_watch(bus);
    g_signal_connect(bus, "message::error", G_CALLBACK(mediaPlayerPrivateErrorCallback), this);
    g_signal_connect(bus, "message::eos", G_CALLBACK(mediaPlayerPrivateEOSCallback), this);
    g_signal_connect(bus, "message::state-changed", G_CALLBACK(mediaPlayerPrivateStateCallback), this);
    g_signal_connect(bus, "message::buffering", G_CALLBACK(mediaPlayerPrivateBufferingCallback), this);
    gst_object_unref(bus);

    g_object_set(G_OBJECT(m_playBin), "uri", url.utf8().data(), NULL);

    GstElement* audioSink = gst_element_factory_make("gconfaudiosink", NULL);
    m_videoSink = webkit_video_sink_new(m_surface);

    g_object_set(m_playBin, "audio-sink", audioSink, NULL);
    g_object_set(m_playBin, "video-sink", m_videoSink, NULL);

    g_signal_connect(m_videoSink, "repaint-requested", G_CALLBACK(mediaPlayerPrivateRepaintCallback), this);

    setVolume(m_volume);
}

}

#endif

