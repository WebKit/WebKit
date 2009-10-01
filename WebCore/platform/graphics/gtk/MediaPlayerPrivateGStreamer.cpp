/*
 * Copyright (C) 2007, 2009 Apple Inc.  All rights reserved.
 * Copyright (C) 2007 Collabora Ltd.  All rights reserved.
 * Copyright (C) 2007 Alp Toker <alp@atoker.com>
 * Copyright (C) 2009 Gustavo Noronha Silva <gns@gnome.org>
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
#include "DataSourceGStreamer.h"

#include "CString.h"
#include "GraphicsContext.h"
#include "IntRect.h"
#include "KURL.h"
#include "MIMETypeRegistry.h"
#include "MediaPlayer.h"
#include "NotImplemented.h"
#include "ScrollView.h"
#include "VideoSinkGStreamer.h"
#include "Widget.h"
#include "TimeRanges.h"

#include <gst/base/gstbasesrc.h>
#include <gst/gst.h>
#include <gst/interfaces/mixer.h>
#include <gst/interfaces/xoverlay.h>
#include <gst/video/video.h>
#include <limits>
#include <math.h>
#include <wtf/GOwnPtr.h>

using namespace std;

namespace WebCore {

gboolean mediaPlayerPrivateErrorCallback(GstBus* bus, GstMessage* message, gpointer data)
{
    if (GST_MESSAGE_TYPE(message) == GST_MESSAGE_ERROR) {
        GOwnPtr<GError> err;
        GOwnPtr<gchar> debug;

        gst_message_parse_error(message, &err.outPtr(), &debug.outPtr());
        LOG_VERBOSE(Media, "Error: %d, %s", err->code,  err->message);

        MediaPlayer::NetworkState error = MediaPlayer::Empty;
        if (err->domain == GST_CORE_ERROR || err->domain == GST_LIBRARY_ERROR)
            error = MediaPlayer::DecodeError;
        else if (err->domain == GST_RESOURCE_ERROR)
            error = MediaPlayer::FormatError;
        else if (err->domain == GST_STREAM_ERROR)
            error = MediaPlayer::NetworkError;

        MediaPlayerPrivate* mp = reinterpret_cast<MediaPlayerPrivate*>(data);
        if (mp)
            mp->loadingFailed(error);
    }
    return true;
}

gboolean mediaPlayerPrivateEOSCallback(GstBus* bus, GstMessage* message, gpointer data)
{
    if (GST_MESSAGE_TYPE(message) == GST_MESSAGE_EOS) {
        LOG_VERBOSE(Media, "End of Stream");
        MediaPlayerPrivate* mp = reinterpret_cast<MediaPlayerPrivate*>(data);
        mp->didEnd();
    }
    return true;
}

gboolean mediaPlayerPrivateStateCallback(GstBus* bus, GstMessage* message, gpointer data)
{
    if (GST_MESSAGE_TYPE(message) == GST_MESSAGE_STATE_CHANGED) {
        MediaPlayerPrivate* mp = reinterpret_cast<MediaPlayerPrivate*>(data);
        mp->updateStates();
    }
    return true;
}

gboolean mediaPlayerPrivateBufferingCallback(GstBus* bus, GstMessage* message, gpointer data)
{
    if (GST_MESSAGE_TYPE(message) == GST_MESSAGE_BUFFERING) {
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

static bool gstInitialized = false;

static void do_gst_init() {
    // FIXME: We should pass the arguments from the command line
    if (!gstInitialized) {
        gst_init(0, 0);
        gstInitialized = true;
        gst_element_register(0, "webkitmediasrc", GST_RANK_PRIMARY,
                             WEBKIT_TYPE_DATA_SRC);

    }
}

MediaPlayerPrivate::MediaPlayerPrivate(MediaPlayer* player)
    : m_player(player)
    , m_playBin(0)
    , m_videoSink(0)
    , m_source(0)
    , m_rate(1.0f)
    , m_endTime(numeric_limits<float>::infinity())
    , m_volume(0.5f)
    , m_networkState(MediaPlayer::Empty)
    , m_readyState(MediaPlayer::HaveNothing)
    , m_startedPlaying(false)
    , m_isStreaming(false)
    , m_size(IntSize())
    , m_visible(true)
    , m_paused(true)
    , m_seeking(false)
    , m_errorOccured(false)
{
    do_gst_init();

    // FIXME: The size shouldn't be fixed here, this is just a quick hack.
    m_surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 640, 480);
}

static gboolean idleUnref(gpointer data)
{
    g_object_unref(reinterpret_cast<GObject*>(data));
    return FALSE;
}

MediaPlayerPrivate::~MediaPlayerPrivate()
{
    if (m_surface)
        cairo_surface_destroy(m_surface);

    if (m_playBin) {
        gst_element_set_state(m_playBin, GST_STATE_NULL);
        gst_object_unref(GST_OBJECT(m_playBin));
    }

    // FIXME: We should find a better way to handle the lifetime of this object; this is
    // needed because the object is sometimes being destroyed inbetween a call to
    // webkit_video_sink_render, and the idle it schedules. Adding a ref in
    // webkit_video_sink_render that would be balanced by the idle is not an option,
    // because in some cases the destruction of the sink may happen in time for the idle
    // to be removed from the queue, so it may not run. It would also cause lots of ref
    // counting churn (render/idle are called many times). This is an ugly race.
    if (m_videoSink) {
        g_idle_add(idleUnref, m_videoSink);
        m_videoSink = 0;
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
    gst_element_set_state(m_playBin, GST_STATE_PLAYING);
}

void MediaPlayerPrivate::pause()
{
    LOG_VERBOSE(Media, "Pause");
    gst_element_set_state(m_playBin, GST_STATE_PAUSED);
}

float MediaPlayerPrivate::duration() const
{
    if (!m_playBin)
        return 0.0;

    if (m_errorOccured)
        return 0.0;

    GstFormat timeFormat = GST_FORMAT_TIME;
    gint64 timeLength = 0;

#if !GST_CHECK_VERSION(0, 10, 23)
    // We try to get the duration, but we do not trust the
    // return value of the query function only; the problem we are
    // trying to work-around here is that pipelines in stream mode may
    // not be able to figure out the duration, but still return true!
    // See https://bugs.webkit.org/show_bug.cgi?id=24639 which has been
    // fixed in gst-plugins-base 0.10.23
    if (!gst_element_query_duration(m_playBin, &timeFormat, &timeLength) || timeLength <= 0) {
#else
    if (!gst_element_query_duration(m_playBin, &timeFormat, &timeLength)) {
#endif
        LOG_VERBOSE(Media, "Time duration query failed.");
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

    if (m_errorOccured)
        return 0;

    float ret = 0.0;

    GstQuery* query = gst_query_new_position(GST_FORMAT_TIME);
    if (!gst_element_query(m_playBin, query)) {
        LOG_VERBOSE(Media, "Position query failed...");
        gst_query_unref(query);
        return ret;
    }

    gint64 position;
    gst_query_parse_position(query, 0, &position);
    ret = (float) (position / 1000000000.0);
    LOG_VERBOSE(Media, "Position %" GST_TIME_FORMAT, GST_TIME_ARGS(position));

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

    if (m_errorOccured)
        return;

    LOG_VERBOSE(Media, "Seek: %" GST_TIME_FORMAT, GST_TIME_ARGS(sec));
    if (!gst_element_seek( m_playBin, m_rate,
            GST_FORMAT_TIME,
            (GstSeekFlags)(GST_SEEK_FLAG_FLUSH),
            GST_SEEK_TYPE_SET, sec,
            GST_SEEK_TYPE_NONE, GST_CLOCK_TIME_NONE))
        LOG_VERBOSE(Media, "Seek to %f failed", time);
    else
        m_seeking = true;
}

void MediaPlayerPrivate::setEndTime(float time)
{
    notImplemented();
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
    return m_paused;
}

bool MediaPlayerPrivate::seeking() const
{
    return m_seeking;
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

bool MediaPlayerPrivate::hasAudio() const
{
    gint currentAudio = -1;
    if (m_playBin)
        g_object_get(G_OBJECT(m_playBin), "current-audio", &currentAudio, NULL);
    return currentAudio > -1;
}

void MediaPlayerPrivate::setVolume(float volume)
{
    m_volume = volume;
    LOG_VERBOSE(Media, "Volume to %f", volume);

    if (!m_playBin)
        return;

    g_object_set(G_OBJECT(m_playBin), "volume", m_volume, NULL);
}

void MediaPlayerPrivate::setMuted(bool mute)
{
    if (!m_playBin)
        return;

    g_object_set(G_OBJECT(m_playBin), "mute", mute, NULL);
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
    seek(currentTime());
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

PassRefPtr<TimeRanges> MediaPlayerPrivate::buffered() const
{
    RefPtr<TimeRanges> timeRanges = TimeRanges::create();
    float loaded = maxTimeLoaded();
    if (!m_errorOccured && !m_isStreaming && loaded > 0)
        timeRanges->add(0, loaded);
    return timeRanges.release();
}

float MediaPlayerPrivate::maxTimeSeekable() const
{
    if (m_errorOccured)
        return 0.0;

    // TODO
    LOG_VERBOSE(Media, "maxTimeSeekable");
    if (m_isStreaming)
        return numeric_limits<float>::infinity();
    // infinite duration means live stream
    return maxTimeLoaded();
}

float MediaPlayerPrivate::maxTimeLoaded() const
{
    if (m_errorOccured)
        return 0.0;

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
    LOG_VERBOSE(Media, "totalBytesKnown");
    return totalBytes() > 0;
}

unsigned MediaPlayerPrivate::totalBytes() const
{
    LOG_VERBOSE(Media, "totalBytes");
    if (!m_source)
        return 0;

    if (m_errorOccured)
        return 0;

    GstFormat fmt = GST_FORMAT_BYTES;
    gint64 length = 0;
    gst_element_query_duration(m_source, &fmt, &length);

    return length;
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

    if (!m_playBin)
        return;

    if (m_errorOccured)
        return;

    MediaPlayer::NetworkState oldNetworkState = m_networkState;
    MediaPlayer::ReadyState oldReadyState = m_readyState;
    GstState state;
    GstState pending;

    GstStateChangeReturn ret = gst_element_get_state(m_playBin,
        &state, &pending, 250 * GST_NSECOND);

    bool shouldUpdateAfterSeek = false;
    switch (ret) {
    case GST_STATE_CHANGE_SUCCESS:
        LOG_VERBOSE(Media, "State: %s, pending: %s",
            gst_element_state_get_name(state),
            gst_element_state_get_name(pending));

        if (state == GST_STATE_READY) {
            m_readyState = MediaPlayer::HaveEnoughData;
        } else if (state == GST_STATE_PAUSED)
            m_readyState = MediaPlayer::HaveEnoughData;

        if (state == GST_STATE_PLAYING)
            m_paused = false;
        else
            m_paused = true;

        if (m_seeking) {
            shouldUpdateAfterSeek = true;
            m_seeking = false;
        }

        m_networkState = MediaPlayer::Loaded;

        g_object_get(m_playBin, "source", &m_source, NULL);
        if (!m_source)
            LOG_VERBOSE(Media, "m_source is 0");
        break;
    case GST_STATE_CHANGE_ASYNC:
        LOG_VERBOSE(Media, "Async: State: %s, pending: %s",
            gst_element_state_get_name(state),
            gst_element_state_get_name(pending));
        // Change in progress
        return;
    case GST_STATE_CHANGE_FAILURE:
        LOG_VERBOSE(Media, "Failure: State: %s, pending: %s",
            gst_element_state_get_name(state),
            gst_element_state_get_name(pending));
        // Change failed
        return;
    case GST_STATE_CHANGE_NO_PREROLL:
        LOG_VERBOSE(Media, "No preroll: State: %s, pending: %s",
            gst_element_state_get_name(state),
            gst_element_state_get_name(pending));

        if (state == GST_STATE_READY) {
            m_readyState = MediaPlayer::HaveFutureData;
        } else if (state == GST_STATE_PAUSED)
            m_readyState = MediaPlayer::HaveCurrentData;

        m_networkState = MediaPlayer::Loading;
        break;
    default:
        LOG_VERBOSE(Media, "Else : %d", ret);
        break;
    }

    if (seeking())
        m_readyState = MediaPlayer::HaveNothing;

    if (shouldUpdateAfterSeek)
        timeChanged();

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
    timeChanged();
}

void MediaPlayerPrivate::loadingFailed(MediaPlayer::NetworkState error)
{
    m_errorOccured = true;
    if (m_networkState != error) {
        m_networkState = error;
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

static HashSet<String> mimeTypeCache()
{

    do_gst_init();

    static HashSet<String> cache;
    static bool typeListInitialized = false;

    if (!typeListInitialized) {
        // These subtypes are already beeing supported by WebKit itself
        HashSet<String> ignoredApplicationSubtypes;
        ignoredApplicationSubtypes.add(String("javascript"));
        ignoredApplicationSubtypes.add(String("ecmascript"));
        ignoredApplicationSubtypes.add(String("x-javascript"));
        ignoredApplicationSubtypes.add(String("xml"));
        ignoredApplicationSubtypes.add(String("xhtml+xml"));
        ignoredApplicationSubtypes.add(String("rss+xml"));
        ignoredApplicationSubtypes.add(String("atom+xml"));
        ignoredApplicationSubtypes.add(String("x-ftp-directory"));
        ignoredApplicationSubtypes.add(String("x-java-applet"));
        ignoredApplicationSubtypes.add(String("x-java-bean"));
        ignoredApplicationSubtypes.add(String("x-java-vm"));
        ignoredApplicationSubtypes.add(String("x-shockwave-flash"));

        GList* factories = gst_type_find_factory_get_list();
        for (GList* iterator = factories; iterator; iterator = iterator->next) {
            GstTypeFindFactory* factory = GST_TYPE_FIND_FACTORY(iterator->data);
            GstCaps* caps = gst_type_find_factory_get_caps(factory);

            // Splitting the capability by comma and taking the first part
            // as capability can be something like "audio/x-wavpack, framed=(boolean)false"
            GOwnPtr<gchar> capabilityString(gst_caps_to_string(caps));
            gchar** capability = g_strsplit(capabilityString.get(), ",", 2);
            gchar** mimetype = g_strsplit(capability[0], "/", 2);

            // GStreamer plugins can be capable of supporting types which WebKit supports
            // by default. In that case, we should not consider these types supportable by GStreamer.
            // Examples of what GStreamer can support but should not be added:
            // text/plain, text/html, image/jpeg, application/xml
            if (g_str_equal(mimetype[0], "audio") ||
                    g_str_equal(mimetype[0], "video") ||
                    (g_str_equal(mimetype[0], "application") &&
                        !ignoredApplicationSubtypes.contains(String(mimetype[1])))) {
                cache.add(String(capability[0]));

                // These formats are supported by GStreamer, but not correctly advertised
                if (g_str_equal(capability[0], "video/x-h264") ||
                    g_str_equal(capability[0], "audio/x-m4a")) {
                    cache.add(String("video/mp4"));
                    cache.add(String("audio/aac"));
                }

                if (g_str_equal(capability[0], "video/x-theora"))
                    cache.add(String("video/ogg"));

                if (g_str_equal(capability[0], "audio/x-wav"))
                    cache.add(String("audio/wav"));

                if (g_str_equal(capability[0], "audio/mpeg")) {
                    // This is what we are handling: mpegversion=(int)1, layer=(int)[ 1, 3 ]
                    gchar** versionAndLayer = g_strsplit(capability[1], ",", 2);

                    if (g_str_has_suffix (versionAndLayer[0], "(int)1")) {
                        for (int i = 0; versionAndLayer[1][i] != '\0'; i++) {
                            if (versionAndLayer[1][i] == '1')
                                cache.add(String("audio/mp1"));
                            else if (versionAndLayer[1][i] == '2')
                                cache.add(String("audio/mp2"));
                            else if (versionAndLayer[1][i] == '3')
                                cache.add(String("audio/mp3"));
                        }
                    }

                    g_strfreev(versionAndLayer);
                }
            }

            g_strfreev(capability);
            g_strfreev(mimetype);
        }

        gst_plugin_feature_list_free(factories);
        typeListInitialized = true;
    }

    return cache;
}

void MediaPlayerPrivate::getSupportedTypes(HashSet<String>& types)
{
    types = mimeTypeCache();
}

MediaPlayer::SupportsType MediaPlayerPrivate::supportsType(const String& type, const String& codecs)
{
    if (type.isNull() || type.isEmpty())
        return MediaPlayer::IsNotSupported;

    // spec says we should not return "probably" if the codecs string is empty
    if (mimeTypeCache().contains(type))
        return codecs.isEmpty() ? MediaPlayer::MayBeSupported : MediaPlayer::IsSupported;
    return MediaPlayer::IsNotSupported;
}

bool MediaPlayerPrivate::hasSingleSecurityOrigin() const
{
    return true;
}

bool MediaPlayerPrivate::supportsFullscreen() const
{
    return true;
}

void MediaPlayerPrivate::createGSTPlayBin(String url)
{
    ASSERT(!m_playBin);
    m_playBin = gst_element_factory_make("playbin2", "play");

    GstBus* bus = gst_pipeline_get_bus(GST_PIPELINE(m_playBin));
    gst_bus_add_signal_watch(bus);
    g_signal_connect(bus, "message::error", G_CALLBACK(mediaPlayerPrivateErrorCallback), this);
    g_signal_connect(bus, "message::eos", G_CALLBACK(mediaPlayerPrivateEOSCallback), this);
    g_signal_connect(bus, "message::state-changed", G_CALLBACK(mediaPlayerPrivateStateCallback), this);
    g_signal_connect(bus, "message::buffering", G_CALLBACK(mediaPlayerPrivateBufferingCallback), this);
    gst_object_unref(bus);

    g_object_set(G_OBJECT(m_playBin), "uri", url.utf8().data(), NULL);

    m_videoSink = webkit_video_sink_new(m_surface);

    // This ref is to protect the sink from being destroyed before we stop the idle it
    // creates internally. See the comment in ~MediaPlayerPrivate.
    g_object_ref(m_videoSink);

    g_object_set(m_playBin, "video-sink", m_videoSink, NULL);

    g_signal_connect(m_videoSink, "repaint-requested", G_CALLBACK(mediaPlayerPrivateRepaintCallback), this);

    setVolume(m_volume);
}

}

#endif
