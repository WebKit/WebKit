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

namespace WebCore {

// Local var, for strange reasons, a member var will make the app crash
GdkWindow* m_window;

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
        LOG_VERBOSE(Media, "END OF STREAM");
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

// Setup the overlay when the message is received
GstBusSyncReply moviePrivateWindowIDCallback(GstBus* bus, GstMessage* message, gpointer data)
{
    if (GST_MESSAGE_TYPE(message) != GST_MESSAGE_ELEMENT)
        return GST_BUS_PASS;

    if (!gst_structure_has_name(message->structure, "prepare-xwindow-id"))
        return GST_BUS_PASS;

    // Disabled for now as it is annoying: the overlay if done for the whole web page
    // not only the specified Rect...

    //gst_x_overlay_set_xwindow_id(
    //    GST_X_OVERLAY(GST_MESSAGE_SRC(message)),
    //    GDK_WINDOW_XID(m_window));
    return GST_BUS_DROP;
}

MoviePrivate::MoviePrivate(Movie* movie)
    : m_movie(movie)
    , m_playBin(0)
    , m_video_sink(0)
    , m_source(0)
    , m_seekTo(-1)
    , m_rate(1.0f)
    , m_endTime(std::numeric_limits<float>::infinity())
    , m_endReached(false)
    , m_oldVolume(0.5f)
    , m_previousTimeCueTimerFired(0)
    , m_networkState(Movie::Empty)
    , m_readyState(Movie::DataUnavailable)
    , m_startedPlaying(false)
    , m_isStreaming(false)
{
    //FIXME We should pass the arguments from the command line
    gst_init(0, NULL);
}

MoviePrivate::~MoviePrivate()
{
    gst_element_set_state(m_playBin, GST_STATE_NULL);
    gst_object_unref(GST_OBJECT(m_playBin));
}

void MoviePrivate::load(String url)
{
    LOG_VERBOSE(Media, "LOAD %s", url.utf8().data());
    if (m_networkState != Movie::Loading) {
        m_networkState = Movie::Loading;
        m_movie->networkStateChanged();
    }
    if (m_readyState != Movie::DataUnavailable) {
        m_readyState = Movie::DataUnavailable;
        m_movie->readyStateChanged();
    }

    cancelSeek();
    createGSTPlayBin(url);
    pause();
}

void MoviePrivate::play()
{
    cancelSeek();
    LOG_VERBOSE(Media, "PLAY");
    // When end reached, rewind for Test video-seek-past-end-playing
    if (m_endReached)
        seek(0);
    m_endReached = false;

    gst_element_set_state(m_playBin, GST_STATE_PLAYING);
    m_startedPlaying = true;
}

void MoviePrivate::pause()
{
    cancelSeek();
    LOG_VERBOSE(Media, "PAUSE");
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
        LOG_VERBOSE(Media, "duration: %" GST_TIME_FORMAT, GST_TIME_ARGS(len));
    else
        LOG_VERBOSE(Media, "duration query failed ");

    if ((GstClockTime)len == GST_CLOCK_TIME_NONE) {
        m_isStreaming = true;
        return std::numeric_limits<float>::infinity();
    }
    return (float) (len / 1000000000.0);
    //FIXME: handle 3.14.9.5 properly
}

float MoviePrivate::currentTime() const
{
    if (!m_playBin)
        return 0;
    if (seeking())
        return m_seekTo;
    // Necessary as sometimes, gstreamer return 0:00 at the EOS
    if (m_endReached)
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
        LOG_VERBOSE(Media, "currentTime %" GST_TIME_FORMAT, GST_TIME_ARGS(position));
    } else {
        LOG_VERBOSE(Media, "position query failed...");
        ret = 0.0;
    }
    gst_query_unref(query);
    return ret;
}

void MoviePrivate::seek(float time)
{
    cancelSeek();
    GstClockTime sec = (GstClockTime)(time * GST_SECOND);

    if (!m_playBin)
        return;

    if (m_isStreaming)
        return;

    LOG_VERBOSE(Media, "seek: %" GST_TIME_FORMAT, GST_TIME_ARGS(sec));
    // FIXME: What happens when the seeked position is not available?
    if (!gst_element_seek( m_playBin, m_rate,
            GST_FORMAT_TIME,
            (GstSeekFlags)(GST_SEEK_FLAG_FLUSH),
            GST_SEEK_TYPE_SET, sec,
            GST_SEEK_TYPE_NONE, GST_CLOCK_TIME_NONE))
        LOG_VERBOSE(Media, "seek to %f failed", time);
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
            LOG_VERBOSE(Media, "seek to %f failed", time);
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
    if (m_seekTo > -1)
        m_seekTo = -1;
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
     if (!m_playBin)
        return false;
    return m_seekTo >= 0;
}

// Returns the size of the video
IntSize MoviePrivate::naturalSize()
{
    int x = 0, y = 0;
    if (hasVideo()) {
        GstPad* pad = NULL;
        pad = gst_element_get_pad(m_video_sink, "sink");
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
    m_oldVolume = volume;
    LOG_VERBOSE(Media, "Volume to %f", volume);
    setMuted(false);
}

void MoviePrivate::setMuted(bool b)
{
    if (!m_playBin) 
        return;

    if (b) {
        g_object_get(G_OBJECT(m_playBin), "volume", &m_oldVolume, NULL);
        g_object_set(G_OBJECT(m_playBin), "volume", (double)0.0, NULL);
    } else {
        g_object_set(G_OBJECT(m_playBin), "volume", m_oldVolume, NULL);
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
    //TODO
    LOG_VERBOSE(Media, "maxTimeBuffered");
    // rtsp streams are not buffered
    return m_isStreaming ? 0 : maxTimeLoaded();
}

float MoviePrivate::maxTimeSeekable()
{
    //TODO
    LOG_VERBOSE(Media, "maxTimeSeekable");
    if (m_isStreaming)
        return std::numeric_limits<float>::infinity();
    // infinite duration means live stream
    return maxTimeLoaded();
}

float MoviePrivate::maxTimeLoaded()
{
    //TODO
    LOG_VERBOSE(Media, "maxTimeLoaded");
    notImplemented();
    return duration();
}

unsigned MoviePrivate::bytesLoaded()
{
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
    LOG_VERBOSE(Media, "totalBytesKnown");
    return totalBytes() > 0;
}

unsigned MoviePrivate::totalBytes()
{

    LOG_VERBOSE(Media, "totalBytes");
    if (!m_playBin)
        return 0;

    if (!m_source)
        return 0;
    /*GstFormat fmt = GST_FORMAT_BYTES;


    if (gst_element_query_duration(m_playBin, &fmt, &len)) 
        LOG_VERBOSE(Media, "totalBytes: %d", (len));
    else {
        LOG_VERBOSE(Media, "totalBytes failed ");
        return 0;
    }*/
    guint64 len;
    if (G_OBJECT_TYPE_NAME(m_source) == "GstFileSrc") {
        // FIXME: Get file size some other way, maybe with the fd property
        len = 0;
    } else if (G_OBJECT_TYPE_NAME(m_source) == "GstGnomeVFSSrc") {
        /*
        GnomeVFSHandle* handle;
        g_object_get(m_source, "handle", handle, NULL);
        GnomeVFSFileInfo info;
        gnome_vfs_get_file_info_from_handle(handle, &info,
            GNOME_VFS_FILE_INFO_DEFAULT);
        len = info.size;
        */
    }
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

    if (m_seekTo >= currentTime())
        m_seekTo = -1;

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
    m_endReached = true;
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
    // FIXME do the real thing
    if (p->paintingDisabled())
        return;
    // For now draw a placeholder rectangle
    p->drawRect(r);
    // This will be used to draw with XOverlay
    m_window = p->gdkDrawable();
}

void MoviePrivate::getSupportedTypes(HashSet<String>& types)
{
    // FIXME do the real thing
    notImplemented();
    types.add(String("video/x-theora+ogg"));
}

void MoviePrivate::createGSTPlayBin(String url)
{
    GstElement* audio_sink;
    GstBus* bus;

    m_playBin = gst_element_factory_make("playbin", "play");

    bus = gst_pipeline_get_bus(GST_PIPELINE(m_playBin));

    gst_bus_set_sync_handler(bus, (GstBusSyncHandler) moviePrivateWindowIDCallback, m_playBin);
    gst_bus_add_signal_watch(bus);

    g_signal_connect(bus, "message::error", G_CALLBACK(moviePrivateErrorCallback), this);
    g_signal_connect(bus, "message::eos", G_CALLBACK(moviePrivateEOSCallback), this);
    g_signal_connect(bus, "message::prepare-xwindow-id", G_CALLBACK(moviePrivateWindowIDCallback), NULL);
    g_signal_connect(bus, "message::state-changed", G_CALLBACK(moviePrivateStateCallback), this);
    g_signal_connect(bus, "message::buffering", G_CALLBACK(moviePrivateBufferingCallback), this);

    gst_object_unref(bus);

    g_object_set(G_OBJECT(m_playBin), "uri", url.utf8().data(), NULL);
    audio_sink = gst_element_factory_make("gconfaudiosink", NULL);
    m_video_sink = gst_element_factory_make("gconfvideosink", NULL);

    g_object_set(m_playBin, "audio-sink", audio_sink, NULL);
    g_object_set(m_playBin, "video-sink", m_video_sink, NULL);

    setVolume(m_oldVolume);
}

}

#endif

