/*
 * Copyright (C) 2007 Apple Inc.  All rights reserved.
 * Copyright (C) 2007 Collabora Ltd.  All rights reserved.
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

#include "MoviePrivateGStreamer.h"

#include "CString.h"
#include "CString.h"
#include "GraphicsContext.h"
#include "IntRect.h"
#include "KURL.h"
#include "MIMETypeRegistry.h"
#include "Movie.h"
#include "NotImplemented.h"
#include "ScrollView.h"
#include "Widget.h"

#include <gdk/gdkx.h>
#include <gst/base/gstbasesrc.h>
#include <gst/gst.h>
#include <gst/interfaces/mixer.h>
#include <gst/interfaces/xoverlay.h>
#include <gst/video/video.h>
#include <libgnomevfs/gnome-vfs.h>
#include <limits>
#include <math.h>

using namespace std;

namespace WebCore {

gboolean moviePrivateErrorCallback(GstBus* bus, GstMessage* message, gpointer data)
{
    if (GST_MESSAGE_TYPE(message) == GST_MESSAGE_ERROR)
    {
        GError* err;
        gchar* debug;

        gst_message_parse_error(message, &err, &debug);
        if (err->code == 3) {
            LOG_VERBOSE(Media, "File not found");
            MoviePrivate* mp = reinterpret_cast<MoviePrivate*>(data);
            if (mp)
                mp->loadingFailed();
        } else {
            LOG_VERBOSE(Media, "Error: %d, %s", err->code,  err->message);
            g_error_free(err);
            g_free(debug);
        }
    }
    return true;
}

gboolean moviePrivateEOSCallback(GstBus* bus, GstMessage* message, gpointer data)
{
    if (GST_MESSAGE_TYPE(message) == GST_MESSAGE_EOS)
    {
        LOG_VERBOSE(Media, "End of Stream");
        MoviePrivate* mp = reinterpret_cast<MoviePrivate*>(data);
        mp->didEnd();
    }
    return true;
}

gboolean moviePrivateStateCallback(GstBus* bus, GstMessage* message, gpointer data)
{
    if (GST_MESSAGE_TYPE(message) == GST_MESSAGE_STATE_CHANGED)
    {
        MoviePrivate* mp = reinterpret_cast<MoviePrivate*>(data);
        mp->updateStates();
    }
    return true;
}

gboolean moviePrivateBufferingCallback(GstBus* bus, GstMessage* message, gpointer data)
{
    if (GST_MESSAGE_TYPE(message) == GST_MESSAGE_BUFFERING)
    {
        gint percent = 0;
        gst_message_parse_buffering(message, &percent);
        LOG_VERBOSE(Media, "Buffering %d", percent);
    }
    return true;
}

MoviePrivate::MoviePrivate(Movie* movie)
    : m_movie(movie)
    , m_playBin(0)
    , m_videoSink(0)
    , m_source(0)
    , m_rate(1.0f)
    , m_endTime(numeric_limits<float>::infinity())
    , m_isEndReached(false)
    , m_volume(0.5f)
    , m_previousTimeCueTimerFired(0)
    , m_networkState(Movie::Empty)
    , m_readyState(Movie::DataUnavailable)
    , m_startedPlaying(false)
    , m_isStreaming(false)
{
    // FIXME: We should pass the arguments from the command line
    gst_init(0, NULL);
}

MoviePrivate::~MoviePrivate()
{
    gst_element_set_state(m_playBin, GST_STATE_NULL);
    gst_object_unref(GST_OBJECT(m_playBin));
}

void MoviePrivate::load(String url)
{
    LOG_VERBOSE(Media, "Load %s", url.utf8().data());
    if (m_networkState != Movie::Loading) {
        m_networkState = Movie::Loading;
        m_movie->networkStateChanged();
    }
    if (m_readyState != Movie::DataUnavailable) {
        m_readyState = Movie::DataUnavailable;
        m_movie->readyStateChanged();
    }

    createGSTPlayBin(url);
    pause();
}

void MoviePrivate::play()
{
    LOG_VERBOSE(Media, "Play");
    // When end reached, rewind for Test video-seek-past-end-playing
    if (m_isEndReached)
        seek(0);
    m_isEndReached = false;

    gst_element_set_state(m_playBin, GST_STATE_PLAYING);
    m_startedPlaying = true;
}

void MoviePrivate::pause()
{
    LOG_VERBOSE(Media, "Pause");
    gst_element_set_state(m_playBin, GST_STATE_PAUSED);
    m_startedPlaying = false;
}

float MoviePrivate::duration()
{
    if (!m_playBin)
        return 0.0;

    GstFormat fmt = GST_FORMAT_TIME;
    gint64 len = 0;

    if (gst_element_query_duration(m_playBin, &fmt, &len))
        LOG_VERBOSE(Media, "Duration: %" GST_TIME_FORMAT, GST_TIME_ARGS(len));
    else
        LOG_VERBOSE(Media, "Duration query failed ");

    if ((GstClockTime)len == GST_CLOCK_TIME_NONE) {
        m_isStreaming = true;
        return numeric_limits<float>::infinity();
    }
    return (float) (len / 1000000000.0);
    // FIXME: handle 3.14.9.5 properly
}

float MoviePrivate::currentTime() const
{
    if (!m_playBin)
        return 0;
    // Necessary as sometimes, gstreamer return 0:00 at the EOS
    if (m_isEndReached)
        return m_endTime;

    float ret;
    GstQuery* query;
    gboolean res;

    query = gst_query_new_position(GST_FORMAT_TIME);
    res = gst_element_query(m_playBin, query);
    if (res) {
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

void MoviePrivate::seek(float time)
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

void MoviePrivate::setEndTime(float time)
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
        if (!gst_element_seek( m_playBin, m_rate,
                GST_FORMAT_TIME,
                (GstSeekFlags)(GST_SEEK_FLAG_FLUSH | GST_SEEK_FLAG_ACCURATE),
                GST_SEEK_TYPE_SET, start,
                GST_SEEK_TYPE_SET, end ))
            LOG_VERBOSE(Media, "Seek to %f failed", time);
    }
}

void MoviePrivate::addCuePoint(float time)
{
    notImplemented();
}

void MoviePrivate::removeCuePoint(float time)
{
    notImplemented();
}

void MoviePrivate::clearCuePoints()
{
    notImplemented();
}

void MoviePrivate::startCuePointTimerIfNeeded()
{
    notImplemented();
}

void MoviePrivate::cancelSeek()
{
    notImplemented();
}

void MoviePrivate::cuePointTimerFired(Timer<MoviePrivate>*)
{
    notImplemented();
}

bool MoviePrivate::paused() const
{
    return !m_startedPlaying;
}

bool MoviePrivate::seeking() const
{
    return false;;
}

// Returns the size of the video
IntSize MoviePrivate::naturalSize()
{
    int x = 0, y = 0;
    if (hasVideo()) {
        GstPad* pad = NULL;
        pad = gst_element_get_pad(m_videoSink, "sink");
        if (pad)
            gst_video_get_size(GST_PAD(pad), &x, &y);
    }
    return IntSize(x, y);
}

bool MoviePrivate::hasVideo()
{
    gint currentVideo = -1;
    if (m_playBin)
        g_object_get(G_OBJECT(m_playBin), "current-video", &currentVideo, NULL);
    return currentVideo > -1;
}

void MoviePrivate::setVolume(float volume)
{
    m_volume = volume;
    LOG_VERBOSE(Media, "Volume to %f", volume);
    setMuted(false);
}

void MoviePrivate::setMuted(bool b)
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

void MoviePrivate::setRate(float rate)
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

int MoviePrivate::dataRate() const
{
    notImplemented();
    return 1;
}

Movie::NetworkState MoviePrivate::networkState()
{
    return m_networkState;
}

Movie::ReadyState MoviePrivate::readyState()
{
    return m_readyState;
}

float MoviePrivate::maxTimeBuffered()
{
    notImplemented();
    LOG_VERBOSE(Media, "maxTimeBuffered");
    // rtsp streams are not buffered
    return m_isStreaming ? 0 : maxTimeLoaded();
}

float MoviePrivate::maxTimeSeekable()
{
    // TODO
    LOG_VERBOSE(Media, "maxTimeSeekable");
    if (m_isStreaming)
        return numeric_limits<float>::infinity();
    // infinite duration means live stream
    return maxTimeLoaded();
}

float MoviePrivate::maxTimeLoaded()
{
    // TODO
    LOG_VERBOSE(Media, "maxTimeLoaded");
    notImplemented();
    return duration();
}

unsigned MoviePrivate::bytesLoaded()
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

bool MoviePrivate::totalBytesKnown()
{
    notImplemented();
    LOG_VERBOSE(Media, "totalBytesKnown");
    return totalBytes() > 0;
}

unsigned MoviePrivate::totalBytes()
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

void MoviePrivate::cancelLoad()
{
    notImplemented();
}

void MoviePrivate::updateStates()
{
    // There is no (known) way to get such level of information about
    // the state of GStreamer, therefore, when in PAUSED state,
    // we are sure we can display the first frame and go to play

    Movie::NetworkState oldNetworkState = m_networkState;
    Movie::ReadyState oldReadyState = m_readyState;
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
            m_readyState = Movie::CanPlayThrough;
        } else if (state == GST_STATE_PAUSED) {
            m_readyState = Movie::CanPlayThrough;
        }
        if (m_networkState < Movie::Loaded)
            m_networkState = Movie::Loaded;

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
            m_readyState = Movie::CanPlay;
        } else if (state == GST_STATE_PAUSED) {
            m_readyState = Movie::CanPlay;
        }
        if (m_networkState < Movie::LoadedMetaData)
            m_networkState = Movie::LoadedMetaData;
        break;
    default:
        LOG_VERBOSE(Media, "Else : %d", ret);
        break;
    }

    if (seeking())
        m_readyState = Movie::DataUnavailable;

    if (m_networkState != oldNetworkState) {
        LOG_VERBOSE(Media, "Network State Changed from %u to %u",
            oldNetworkState, m_networkState);
        m_movie->networkStateChanged();
    }
    if (m_readyState != oldReadyState) {
        LOG_VERBOSE(Media, "Ready State Changed from %u to %u",
            oldReadyState, m_readyState);
        m_movie->readyStateChanged();
    }
}

void MoviePrivate::loadStateChanged()
{
    updateStates();
}

void MoviePrivate::rateChanged()
{
    updateStates();
}

void MoviePrivate::sizeChanged()
{
    notImplemented();
}

void MoviePrivate::timeChanged()
{
    updateStates();
    m_movie->timeChanged();
}

void MoviePrivate::volumeChanged()
{
    m_movie->volumeChanged();
}

void MoviePrivate::didEnd()
{
    m_isEndReached = true;
    pause();
    timeChanged();
}

void MoviePrivate::loadingFailed()
{
    if (m_networkState != Movie::LoadFailed) {
        m_networkState = Movie::LoadFailed;
        m_movie->networkStateChanged();
    }
    if (m_readyState != Movie::DataUnavailable) {
        m_readyState = Movie::DataUnavailable;
        m_movie->readyStateChanged();
    }
}

void MoviePrivate::setRect(const IntRect& r)
{
    notImplemented();
}

void MoviePrivate::setVisible(bool b)
{
    notImplemented();
}

void MoviePrivate::paint(GraphicsContext* p, const IntRect& r)
{
    // FIXME: do the real thing
    if (p->paintingDisabled())
        return;
    // For now draw a placeholder rectangle
    p->drawRect(r);
}

void MoviePrivate::getSupportedTypes(HashSet<String>& types)
{
    // FIXME: do the real thing
    notImplemented();
    types.add(String("video/x-theora+ogg"));
}

void MoviePrivate::createGSTPlayBin(String url)
{
    GstElement* audioSink;
    GstBus* bus;

    m_playBin = gst_element_factory_make("playbin", "play");

    bus = gst_pipeline_get_bus(GST_PIPELINE(m_playBin));

    gst_bus_add_signal_watch(bus);

    g_signal_connect(bus, "message::error", G_CALLBACK(moviePrivateErrorCallback), this);
    g_signal_connect(bus, "message::eos", G_CALLBACK(moviePrivateEOSCallback), this);
    g_signal_connect(bus, "message::state-changed", G_CALLBACK(moviePrivateStateCallback), this);
    g_signal_connect(bus, "message::buffering", G_CALLBACK(moviePrivateBufferingCallback), this);

    gst_object_unref(bus);

    g_object_set(G_OBJECT(m_playBin), "uri", url.utf8().data(), NULL);
    audioSink = gst_element_factory_make("gconfaudiosink", NULL);
    m_videoSink = gst_element_factory_make("gconfvideosink", NULL);

    g_object_set(m_playBin, "audio-sink", audioSink, NULL);
    g_object_set(m_playBin, "video-sink", m_videoSink, NULL);

    setVolume(m_volume);
}

}

#endif

