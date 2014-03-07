/*
 * Copyright (C) 2007, 2009 Apple Inc.  All rights reserved.
 * Copyright (C) 2007 Collabora Ltd.  All rights reserved.
 * Copyright (C) 2007 Alp Toker <alp@atoker.com>
 * Copyright (C) 2009 Gustavo Noronha Silva <gns@gnome.org>
 * Copyright (C) 2009, 2010, 2011, 2012, 2013 Igalia S.L
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
#include "MediaPlayerPrivateGStreamer.h"

#if ENABLE(VIDEO) && USE(GSTREAMER)

#include "GStreamerUtilities.h"
#include "URL.h"
#include "MIMETypeRegistry.h"
#include "MediaPlayer.h"
#include "NotImplemented.h"
#include "SecurityOrigin.h"
#include "TimeRanges.h"
#include "WebKitWebSourceGStreamer.h"
#include <gst/gst.h>
#include <gst/pbutils/missing-plugins.h>
#include <limits>
#include <wtf/gobject/GUniquePtr.h>
#include <wtf/text/CString.h>

#if ENABLE(VIDEO_TRACK)
#include "AudioTrackPrivateGStreamer.h"
#include "InbandMetadataTextTrackPrivateGStreamer.h"
#include "InbandTextTrackPrivateGStreamer.h"
#include "TextCombinerGStreamer.h"
#include "TextSinkGStreamer.h"
#include "VideoTrackPrivateGStreamer.h"
#endif

#include <gst/audio/streamvolume.h>

#if ENABLE(MEDIA_SOURCE)
#include "MediaSource.h"
#include "WebKitMediaSourceGStreamer.h"
#endif

// Max interval in seconds to stay in the READY state on manual
// state change requests.
static const guint gReadyStateTimerInterval = 60;

GST_DEBUG_CATEGORY_EXTERN(webkit_media_player_debug);
#define GST_CAT_DEFAULT webkit_media_player_debug

using namespace std;

namespace WebCore {

static gboolean mediaPlayerPrivateMessageCallback(GstBus*, GstMessage* message, MediaPlayerPrivateGStreamer* player)
{
    return player->handleMessage(message);
}

static void mediaPlayerPrivateSourceChangedCallback(GObject*, GParamSpec*, MediaPlayerPrivateGStreamer* player)
{
    player->sourceChanged();
}

static void mediaPlayerPrivateVideoSinkCapsChangedCallback(GObject*, GParamSpec*, MediaPlayerPrivateGStreamer* player)
{
    player->videoCapsChanged();
}

static void mediaPlayerPrivateVideoChangedCallback(GObject*, MediaPlayerPrivateGStreamer* player)
{
    player->videoChanged();
}

static void mediaPlayerPrivateAudioChangedCallback(GObject*, MediaPlayerPrivateGStreamer* player)
{
    player->audioChanged();
}

static gboolean mediaPlayerPrivateAudioChangeTimeoutCallback(MediaPlayerPrivateGStreamer* player)
{
    // This is the callback of the timeout source created in ::audioChanged.
    player->notifyPlayerOfAudio();
    return FALSE;
}

static void setAudioStreamPropertiesCallback(GstChildProxy*, GObject* object, gchar*,
    MediaPlayerPrivateGStreamer* player)
{
    player->setAudioStreamProperties(object);
}

static gboolean mediaPlayerPrivateVideoChangeTimeoutCallback(MediaPlayerPrivateGStreamer* player)
{
    // This is the callback of the timeout source created in ::videoChanged.
    player->notifyPlayerOfVideo();
    return FALSE;
}

static gboolean mediaPlayerPrivateVideoCapsChangeTimeoutCallback(MediaPlayerPrivateGStreamer* player)
{
    // This is the callback of the timeout source created in ::videoCapsChanged.
    player->notifyPlayerOfVideoCaps();
    return FALSE;
}

#if ENABLE(VIDEO_TRACK)
static void mediaPlayerPrivateTextChangedCallback(GObject*, MediaPlayerPrivateGStreamer* player)
{
    player->textChanged();
}

static gboolean mediaPlayerPrivateTextChangeTimeoutCallback(MediaPlayerPrivateGStreamer* player)
{
    // This is the callback of the timeout source created in ::textChanged.
    player->notifyPlayerOfText();
    return FALSE;
}

static GstFlowReturn mediaPlayerPrivateNewTextSampleCallback(GObject*, MediaPlayerPrivateGStreamer* player)
{
    player->newTextSample();
    return GST_FLOW_OK;
}
#endif

static gboolean mediaPlayerPrivateReadyStateTimeoutCallback(MediaPlayerPrivateGStreamer* player)
{
    // This is the callback of the timeout source created in ::changePipelineState.
    // Reset pipeline if we are sitting on READY state when timeout is reached
    player->changePipelineState(GST_STATE_NULL);
    return FALSE;
}

static void mediaPlayerPrivatePluginInstallerResultFunction(GstInstallPluginsReturn result, gpointer userData)
{
    MediaPlayerPrivateGStreamer* player = reinterpret_cast<MediaPlayerPrivateGStreamer*>(userData);
    player->handlePluginInstallerResult(result);
}

static GstClockTime toGstClockTime(float time)
{
    // Extract the integer part of the time (seconds) and the fractional part (microseconds). Attempt to
    // round the microseconds so no floating point precision is lost and we can perform an accurate seek.
    float seconds;
    float microSeconds = modf(time, &seconds) * 1000000;
    GTimeVal timeValue;
    timeValue.tv_sec = static_cast<glong>(seconds);
    timeValue.tv_usec = static_cast<glong>(roundf(microSeconds / 10000) * 10000);
    return GST_TIMEVAL_TO_TIME(timeValue);
}

void MediaPlayerPrivateGStreamer::setAudioStreamProperties(GObject* object)
{
    if (g_strcmp0(G_OBJECT_TYPE_NAME(object), "GstPulseSink"))
        return;

    const char* role = m_player->mediaPlayerClient() && m_player->mediaPlayerClient()->mediaPlayerIsVideo()
        ? "video" : "music";
    GstStructure* structure = gst_structure_new("stream-properties", "media.role", G_TYPE_STRING, role, NULL);
    g_object_set(object, "stream-properties", structure, NULL);
    gst_structure_free(structure);
    GUniquePtr<gchar> elementName(gst_element_get_name(GST_ELEMENT(object)));
    LOG_MEDIA_MESSAGE("Set media.role as %s at %s", role, elementName.get());
}

PassOwnPtr<MediaPlayerPrivateInterface> MediaPlayerPrivateGStreamer::create(MediaPlayer* player)
{
    return adoptPtr(new MediaPlayerPrivateGStreamer(player));
}

void MediaPlayerPrivateGStreamer::registerMediaEngine(MediaEngineRegistrar registrar)
{
    if (isAvailable())
        registrar(create, getSupportedTypes, supportsType, 0, 0, 0, 0);
}

bool initializeGStreamerAndRegisterWebKitElements()
{
    if (!initializeGStreamer())
        return false;

    GRefPtr<GstElementFactory> srcFactory = gst_element_factory_find("webkitwebsrc");
    if (!srcFactory) {
        GST_DEBUG_CATEGORY_INIT(webkit_media_player_debug, "webkitmediaplayer", 0, "WebKit media player");
        gst_element_register(0, "webkitwebsrc", GST_RANK_PRIMARY + 100, WEBKIT_TYPE_WEB_SRC);
    }

#if ENABLE(MEDIA_SOURCE)
    GRefPtr<GstElementFactory> WebKitMediaSrcFactory = gst_element_factory_find("webkitmediasrc");
    if (!WebKitMediaSrcFactory)
        gst_element_register(0, "webkitmediasrc", GST_RANK_PRIMARY + 100, WEBKIT_TYPE_MEDIA_SRC);
#endif
    return true;
}

bool MediaPlayerPrivateGStreamer::isAvailable()
{
    if (!initializeGStreamerAndRegisterWebKitElements())
        return false;

    GRefPtr<GstElementFactory> factory = gst_element_factory_find("playbin");
    return factory;
}

MediaPlayerPrivateGStreamer::MediaPlayerPrivateGStreamer(MediaPlayer* player)
    : MediaPlayerPrivateGStreamerBase(player)
    , m_source(0)
    , m_seekTime(0)
    , m_changingRate(false)
    , m_endTime(numeric_limits<float>::infinity())
    , m_isEndReached(false)
    , m_isStreaming(false)
    , m_mediaLocations(0)
    , m_mediaLocationCurrentIndex(0)
    , m_resetPipeline(false)
    , m_paused(true)
    , m_seeking(false)
    , m_seekIsPending(false)
    , m_timeOfOverlappingSeek(-1)
    , m_buffering(false)
    , m_playbackRate(1)
    , m_lastPlaybackRate(1)
    , m_errorOccured(false)
    , m_mediaDuration(0)
    , m_downloadFinished(false)
    , m_fillTimer(this, &MediaPlayerPrivateGStreamer::fillTimerFired)
    , m_maxTimeLoaded(0)
    , m_bufferingPercentage(0)
    , m_preload(player->preload())
    , m_delayingLoad(false)
    , m_mediaDurationKnown(true)
    , m_maxTimeLoadedAtLastDidLoadingProgress(0)
    , m_volumeAndMuteInitialized(false)
    , m_hasVideo(false)
    , m_hasAudio(false)
    , m_audioTimerHandler(0)
    , m_textTimerHandler(0)
    , m_videoTimerHandler(0)
    , m_videoCapsTimerHandler(0)
    , m_readyTimerHandler(0)
    , m_totalBytes(-1)
    , m_preservesPitch(false)
    , m_requestedState(GST_STATE_VOID_PENDING)
    , m_missingPlugins(false)
{
}

MediaPlayerPrivateGStreamer::~MediaPlayerPrivateGStreamer()
{
#if ENABLE(VIDEO_TRACK)
    for (size_t i = 0; i < m_audioTracks.size(); ++i)
        m_audioTracks[i]->disconnect();

    for (size_t i = 0; i < m_textTracks.size(); ++i)
        m_textTracks[i]->disconnect();

    for (size_t i = 0; i < m_videoTracks.size(); ++i)
        m_videoTracks[i]->disconnect();
#endif
    if (m_fillTimer.isActive())
        m_fillTimer.stop();

    if (m_mediaLocations) {
        gst_structure_free(m_mediaLocations);
        m_mediaLocations = 0;
    }

    if (m_autoAudioSink)
        g_signal_handlers_disconnect_by_func(G_OBJECT(m_autoAudioSink.get()),
            reinterpret_cast<gpointer>(setAudioStreamPropertiesCallback), this);

    if (m_readyTimerHandler)
        g_source_remove(m_readyTimerHandler);

    if (m_playBin) {
        GRefPtr<GstBus> bus = adoptGRef(gst_pipeline_get_bus(GST_PIPELINE(m_playBin.get())));
        ASSERT(bus);
        g_signal_handlers_disconnect_by_func(bus.get(), reinterpret_cast<gpointer>(mediaPlayerPrivateMessageCallback), this);
        gst_bus_remove_signal_watch(bus.get());

        g_signal_handlers_disconnect_by_func(m_playBin.get(), reinterpret_cast<gpointer>(mediaPlayerPrivateSourceChangedCallback), this);
        g_signal_handlers_disconnect_by_func(m_playBin.get(), reinterpret_cast<gpointer>(mediaPlayerPrivateVideoChangedCallback), this);
        g_signal_handlers_disconnect_by_func(m_playBin.get(), reinterpret_cast<gpointer>(mediaPlayerPrivateAudioChangedCallback), this);
#if ENABLE(VIDEO_TRACK)
        g_signal_handlers_disconnect_by_func(m_playBin.get(), reinterpret_cast<gpointer>(mediaPlayerPrivateNewTextSampleCallback), this);
        g_signal_handlers_disconnect_by_func(m_playBin.get(), reinterpret_cast<gpointer>(mediaPlayerPrivateTextChangedCallback), this);
#endif

        gst_element_set_state(m_playBin.get(), GST_STATE_NULL);
        m_playBin.clear();
    }

    GRefPtr<GstPad> videoSinkPad = adoptGRef(gst_element_get_static_pad(m_webkitVideoSink.get(), "sink"));
    g_signal_handlers_disconnect_by_func(videoSinkPad.get(), reinterpret_cast<gpointer>(mediaPlayerPrivateVideoSinkCapsChangedCallback), this);

    if (m_videoTimerHandler)
        g_source_remove(m_videoTimerHandler);

    if (m_audioTimerHandler)
        g_source_remove(m_audioTimerHandler);

    if (m_textTimerHandler)
        g_source_remove(m_textTimerHandler);

    if (m_videoCapsTimerHandler)
        g_source_remove(m_videoCapsTimerHandler);
}

void MediaPlayerPrivateGStreamer::load(const String& url)
{
    if (!initializeGStreamerAndRegisterWebKitElements())
        return;

    URL kurl(URL(), url);
    String cleanUrl(url);

    // Clean out everything after file:// url path.
    if (kurl.isLocalFile())
        cleanUrl = cleanUrl.substring(0, kurl.pathEnd());

    if (!m_playBin)
        createGSTPlayBin();

    ASSERT(m_playBin);

    m_url = URL(URL(), cleanUrl);
    g_object_set(m_playBin.get(), "uri", cleanUrl.utf8().data(), NULL);

    INFO_MEDIA_MESSAGE("Load %s", cleanUrl.utf8().data());

    if (m_preload == MediaPlayer::None) {
        LOG_MEDIA_MESSAGE("Delaying load.");
        m_delayingLoad = true;
    }

    // Reset network and ready states. Those will be set properly once
    // the pipeline pre-rolled.
    m_networkState = MediaPlayer::Loading;
    m_player->networkStateChanged();
    m_readyState = MediaPlayer::HaveNothing;
    m_player->readyStateChanged();
    m_volumeAndMuteInitialized = false;

    if (!m_delayingLoad)
        commitLoad();
}

#if ENABLE(MEDIA_SOURCE)
void MediaPlayerPrivateGStreamer::load(const String& url, MediaSourcePrivateClient* mediaSource)
{
    String mediasourceUri = String::format("mediasource%s", url.utf8().data());
    m_mediaSource = mediaSource;
    load(mediasourceUri);
}
#endif

void MediaPlayerPrivateGStreamer::commitLoad()
{
    ASSERT(!m_delayingLoad);
    LOG_MEDIA_MESSAGE("Committing load.");

    // GStreamer needs to have the pipeline set to a paused state to
    // start providing anything useful.
    changePipelineState(GST_STATE_PAUSED);

    setDownloadBuffering();
    updateStates();
}

float MediaPlayerPrivateGStreamer::playbackPosition() const
{
    if (m_isEndReached) {
        // Position queries on a null pipeline return 0. If we're at
        // the end of the stream the pipeline is null but we want to
        // report either the seek time or the duration because this is
        // what the Media element spec expects us to do.
        if (m_seeking)
            return m_seekTime;
        if (m_mediaDuration)
            return m_mediaDuration;
        return 0;
    }

    // Position is only available if no async state change is going on and the state is either paused or playing.
    gint64 position = GST_CLOCK_TIME_NONE;
    GstQuery* query= gst_query_new_position(GST_FORMAT_TIME);
    if (gst_element_query(m_playBin.get(), query))
        gst_query_parse_position(query, 0, &position);

    float result = 0.0f;
    if (static_cast<GstClockTime>(position) != GST_CLOCK_TIME_NONE)
        result = static_cast<double>(position) / GST_SECOND;
    else if (m_canFallBackToLastFinishedSeekPositon)
        result = m_seekTime;

    LOG_MEDIA_MESSAGE("Position %" GST_TIME_FORMAT, GST_TIME_ARGS(position));

    gst_query_unref(query);

    return result;
}

bool MediaPlayerPrivateGStreamer::changePipelineState(GstState newState)
{
    ASSERT(m_playBin);

    GstState currentState;
    GstState pending;

    gst_element_get_state(m_playBin.get(), &currentState, &pending, 0);
    if (currentState == newState || pending == newState) {
        LOG_MEDIA_MESSAGE("Rejected state change to %s from %s with %s pending", gst_element_state_get_name(newState),
            gst_element_state_get_name(currentState), gst_element_state_get_name(pending));
        return true;
    }

    LOG_MEDIA_MESSAGE("Changing state change to %s from %s with %s pending", gst_element_state_get_name(newState),
        gst_element_state_get_name(currentState), gst_element_state_get_name(pending));

    GstStateChangeReturn setStateResult = gst_element_set_state(m_playBin.get(), newState);
    GstState pausedOrPlaying = newState == GST_STATE_PLAYING ? GST_STATE_PAUSED : GST_STATE_PLAYING;
    if (currentState != pausedOrPlaying && setStateResult == GST_STATE_CHANGE_FAILURE) {
        return false;
    }

    // Create a timer when entering the READY state so that we can free resources
    // if we stay for too long on READY.
    // Also lets remove the timer if we request a state change for any state other than READY.
    // See also https://bugs.webkit.org/show_bug.cgi?id=117354
    if (newState == GST_STATE_READY && !m_readyTimerHandler) {
        m_readyTimerHandler = g_timeout_add_seconds(gReadyStateTimerInterval, reinterpret_cast<GSourceFunc>(mediaPlayerPrivateReadyStateTimeoutCallback), this);
        g_source_set_name_by_id(m_readyTimerHandler, "[WebKit] mediaPlayerPrivateReadyStateTimeoutCallback");
    } else if (newState != GST_STATE_READY && m_readyTimerHandler) {
        g_source_remove(m_readyTimerHandler);
        m_readyTimerHandler = 0;
    }

    return true;
}

void MediaPlayerPrivateGStreamer::prepareToPlay()
{
    m_preload = MediaPlayer::Auto;
    if (m_delayingLoad) {
        m_delayingLoad = false;
        commitLoad();
    }
}

void MediaPlayerPrivateGStreamer::play()
{
    if (changePipelineState(GST_STATE_PLAYING)) {
        m_isEndReached = false;
        m_delayingLoad = false;
        m_preload = MediaPlayer::Auto;
        setDownloadBuffering();
        LOG_MEDIA_MESSAGE("Play");
    } else {
        loadingFailed(MediaPlayer::Empty);
    }
}

void MediaPlayerPrivateGStreamer::pause()
{
    GstState currentState, pendingState;
    gst_element_get_state(m_playBin.get(), &currentState, &pendingState, 0);
    if (currentState < GST_STATE_PAUSED && pendingState <= GST_STATE_PAUSED)
        return;

    if (changePipelineState(GST_STATE_PAUSED))
        INFO_MEDIA_MESSAGE("Pause");
    else
        loadingFailed(MediaPlayer::Empty);
}

float MediaPlayerPrivateGStreamer::duration() const
{
    if (!m_playBin)
        return 0.0f;

    if (m_errorOccured)
        return 0.0f;

    // Media duration query failed already, don't attempt new useless queries.
    if (!m_mediaDurationKnown)
        return numeric_limits<float>::infinity();

    if (m_mediaDuration)
        return m_mediaDuration;

    GstFormat timeFormat = GST_FORMAT_TIME;
    gint64 timeLength = 0;

    bool failure = !gst_element_query_duration(m_playBin.get(), timeFormat, &timeLength) || static_cast<guint64>(timeLength) == GST_CLOCK_TIME_NONE;
    if (failure) {
        LOG_MEDIA_MESSAGE("Time duration query failed for %s", m_url.string().utf8().data());
        return numeric_limits<float>::infinity();
    }

    LOG_MEDIA_MESSAGE("Duration: %" GST_TIME_FORMAT, GST_TIME_ARGS(timeLength));

    m_mediaDuration = static_cast<double>(timeLength) / GST_SECOND;
    return m_mediaDuration;
    // FIXME: handle 3.14.9.5 properly
}

float MediaPlayerPrivateGStreamer::currentTime() const
{
    if (!m_playBin)
        return 0.0f;

    if (m_errorOccured)
        return 0.0f;

    if (m_seeking)
        return m_seekTime;

    // Workaround for
    // https://bugzilla.gnome.org/show_bug.cgi?id=639941 In GStreamer
    // 0.10.35 basesink reports wrong duration in case of EOS and
    // negative playback rate. There's no upstream accepted patch for
    // this bug yet, hence this temporary workaround.
    if (m_isEndReached && m_playbackRate < 0)
        return 0.0f;

    return playbackPosition();
}

void MediaPlayerPrivateGStreamer::seek(float time)
{
    if (!m_playBin)
        return;

    if (m_errorOccured)
        return;

    INFO_MEDIA_MESSAGE("[Seek] seek attempt to %f secs", time);

    // Avoid useless seeking.
    if (time == currentTime())
        return;

    if (isLiveStream())
        return;

    GstClockTime clockTime = toGstClockTime(time);
    INFO_MEDIA_MESSAGE("[Seek] seeking to %" GST_TIME_FORMAT " (%f)", GST_TIME_ARGS(clockTime), time);

    if (m_seeking) {
        m_timeOfOverlappingSeek = time;
        if (m_seekIsPending) {
            m_seekTime = time;
            return;
        }
    }

    GstState state;
    GstStateChangeReturn getStateResult = gst_element_get_state(m_playBin.get(), &state, 0, 0);
    if (getStateResult == GST_STATE_CHANGE_FAILURE || getStateResult == GST_STATE_CHANGE_NO_PREROLL) {
        LOG_MEDIA_MESSAGE("[Seek] cannot seek, current state change is %s", gst_element_state_change_return_get_name(getStateResult));
        return;
    }
    if (getStateResult == GST_STATE_CHANGE_ASYNC || state < GST_STATE_PAUSED || m_isEndReached) {
        m_seekIsPending = true;
        if (m_isEndReached) {
            LOG_MEDIA_MESSAGE("[Seek] reset pipeline");
            m_resetPipeline = true;
            if (!changePipelineState(GST_STATE_PAUSED))
                loadingFailed(MediaPlayer::Empty);
        }
    } else {
        // We can seek now.
        if (!doSeek(clockTime, m_player->rate(), static_cast<GstSeekFlags>(GST_SEEK_FLAG_FLUSH | GST_SEEK_FLAG_ACCURATE))) {
            LOG_MEDIA_MESSAGE("[Seek] seeking to %f failed", time);
            return;
        }
    }

    m_seeking = true;
    m_seekTime = time;
    m_isEndReached = false;
}

bool MediaPlayerPrivateGStreamer::doSeek(gint64 position, float rate, GstSeekFlags seekType)
{
    gint64 startTime, endTime;

    if (rate > 0) {
        startTime = position;
        endTime = GST_CLOCK_TIME_NONE;
    } else {
        startTime = 0;
        // If we are at beginning of media, start from the end to
        // avoid immediate EOS.
        if (position < 0)
            endTime = static_cast<gint64>(duration() * GST_SECOND);
        else
            endTime = position;
    }

    return gst_element_seek(m_playBin.get(), rate, GST_FORMAT_TIME, seekType,
        GST_SEEK_TYPE_SET, startTime, GST_SEEK_TYPE_SET, endTime);
}

void MediaPlayerPrivateGStreamer::updatePlaybackRate()
{
    if (!m_changingRate)
        return;

    float currentPosition = static_cast<float>(playbackPosition() * GST_SECOND);
    bool mute = false;

    INFO_MEDIA_MESSAGE("Set Rate to %f", m_playbackRate);

    if (m_playbackRate > 0) {
        // Mute the sound if the playback rate is too extreme and
        // audio pitch is not adjusted.
        mute = (!m_preservesPitch && (m_playbackRate < 0.8 || m_playbackRate > 2));
    } else {
        if (currentPosition == 0.0f)
            currentPosition = -1.0f;
        mute = true;
    }

    INFO_MEDIA_MESSAGE("Need to mute audio?: %d", (int) mute);
    if (doSeek(currentPosition, m_playbackRate, static_cast<GstSeekFlags>(GST_SEEK_FLAG_FLUSH))) {
        g_object_set(m_playBin.get(), "mute", mute, NULL);
        m_lastPlaybackRate = m_playbackRate;
    } else {
        m_playbackRate = m_lastPlaybackRate;
        ERROR_MEDIA_MESSAGE("Set rate to %f failed", m_playbackRate);
    }
    m_changingRate = false;
    m_player->rateChanged();
}

bool MediaPlayerPrivateGStreamer::paused() const
{
    if (m_isEndReached) {
        LOG_MEDIA_MESSAGE("Ignoring pause at EOS");
        return true;
    }

    GstState state;
    gst_element_get_state(m_playBin.get(), &state, 0, 0);
    return state == GST_STATE_PAUSED;
}

bool MediaPlayerPrivateGStreamer::seeking() const
{
    return m_seeking;
}

void MediaPlayerPrivateGStreamer::videoChanged()
{
    if (m_videoTimerHandler)
        g_source_remove(m_videoTimerHandler);
    m_videoTimerHandler = g_idle_add_full(G_PRIORITY_DEFAULT, reinterpret_cast<GSourceFunc>(mediaPlayerPrivateVideoChangeTimeoutCallback), this, 0);
    g_source_set_name_by_id(m_videoTimerHandler, "[WebKit] mediaPlayerPrivateVideoChangeTimeoutCallback");
}

void MediaPlayerPrivateGStreamer::videoCapsChanged()
{
    if (m_videoCapsTimerHandler)
        g_source_remove(m_videoCapsTimerHandler);
    m_videoCapsTimerHandler = g_timeout_add(0, reinterpret_cast<GSourceFunc>(mediaPlayerPrivateVideoCapsChangeTimeoutCallback), this);
    g_source_set_name_by_id(m_videoCapsTimerHandler, "[WebKit] mediaPlayerPrivateVideoCapsChangeTimeoutCallback");
}

void MediaPlayerPrivateGStreamer::notifyPlayerOfVideo()
{
    m_videoTimerHandler = 0;

    gint numTracks = 0;
    if (m_playBin)
        g_object_get(m_playBin.get(), "n-video", &numTracks, NULL);

    m_hasVideo = numTracks > 0;

#if ENABLE(VIDEO_TRACK)
    for (gint i = 0; i < numTracks; ++i) {
        GRefPtr<GstPad> pad;
        g_signal_emit_by_name(m_playBin.get(), "get-video-pad", i, &pad.outPtr(), NULL);
        ASSERT(pad);

        if (i < static_cast<gint>(m_videoTracks.size())) {
            RefPtr<VideoTrackPrivateGStreamer> existingTrack = m_videoTracks[i];
            existingTrack->setIndex(i);
            if (existingTrack->pad() == pad)
                continue;
        }

        RefPtr<VideoTrackPrivateGStreamer> track = VideoTrackPrivateGStreamer::create(m_playBin, i, pad);
        m_videoTracks.append(track);
        m_player->addVideoTrack(track.release());
    }

    while (static_cast<gint>(m_videoTracks.size()) > numTracks) {
        RefPtr<VideoTrackPrivateGStreamer> track = m_videoTracks.last();
        track->disconnect();
        m_videoTracks.removeLast();
        m_player->removeVideoTrack(track.release());
    }
#endif

    m_player->mediaPlayerClient()->mediaPlayerEngineUpdated(m_player);
}

void MediaPlayerPrivateGStreamer::notifyPlayerOfVideoCaps()
{
    m_videoCapsTimerHandler = 0;
    m_videoSize = IntSize();
    m_player->mediaPlayerClient()->mediaPlayerEngineUpdated(m_player);
}

void MediaPlayerPrivateGStreamer::audioChanged()
{
    if (m_audioTimerHandler)
        g_source_remove(m_audioTimerHandler);
    m_audioTimerHandler = g_idle_add_full(G_PRIORITY_DEFAULT, reinterpret_cast<GSourceFunc>(mediaPlayerPrivateAudioChangeTimeoutCallback), this, 0);
    g_source_set_name_by_id(m_audioTimerHandler, "[WebKit] mediaPlayerPrivateAudioChangeTimeoutCallback");
}

void MediaPlayerPrivateGStreamer::notifyPlayerOfAudio()
{
    m_audioTimerHandler = 0;

    gint numTracks = 0;
    if (m_playBin)
        g_object_get(m_playBin.get(), "n-audio", &numTracks, NULL);

    m_hasAudio = numTracks > 0;

#if ENABLE(VIDEO_TRACK)
    for (gint i = 0; i < numTracks; ++i) {
        GRefPtr<GstPad> pad;
        g_signal_emit_by_name(m_playBin.get(), "get-audio-pad", i, &pad.outPtr(), NULL);
        ASSERT(pad);

        if (i < static_cast<gint>(m_audioTracks.size())) {
            RefPtr<AudioTrackPrivateGStreamer> existingTrack = m_audioTracks[i];
            existingTrack->setIndex(i);
            if (existingTrack->pad() == pad)
                continue;
        }

        RefPtr<AudioTrackPrivateGStreamer> track = AudioTrackPrivateGStreamer::create(m_playBin, i, pad);
        m_audioTracks.insert(i, track);
        m_player->addAudioTrack(track.release());
    }

    while (static_cast<gint>(m_audioTracks.size()) > numTracks) {
        RefPtr<AudioTrackPrivateGStreamer> track = m_audioTracks.last();
        track->disconnect();
        m_audioTracks.removeLast();
        m_player->removeAudioTrack(track.release());
    }
#endif

    m_player->mediaPlayerClient()->mediaPlayerEngineUpdated(m_player);
}

#if ENABLE(VIDEO_TRACK)
void MediaPlayerPrivateGStreamer::textChanged()
{
    if (m_textTimerHandler)
        g_source_remove(m_textTimerHandler);
    m_textTimerHandler = g_timeout_add(0, reinterpret_cast<GSourceFunc>(mediaPlayerPrivateTextChangeTimeoutCallback), this);
    g_source_set_name_by_id(m_textTimerHandler, "[WebKit] mediaPlayerPrivateTextChangeTimeoutCallback");
}

void MediaPlayerPrivateGStreamer::notifyPlayerOfText()
{
    m_textTimerHandler = 0;

    gint numTracks = 0;
    if (m_playBin)
        g_object_get(m_playBin.get(), "n-text", &numTracks, NULL);

    for (gint i = 0; i < numTracks; ++i) {
        GRefPtr<GstPad> pad;
        g_signal_emit_by_name(m_playBin.get(), "get-text-pad", i, &pad.outPtr(), NULL);
        ASSERT(pad);

        if (i < static_cast<gint>(m_textTracks.size())) {
            RefPtr<InbandTextTrackPrivateGStreamer> existingTrack = m_textTracks[i];
            existingTrack->setIndex(i);
            if (existingTrack->pad() == pad)
                continue;
        }

        RefPtr<InbandTextTrackPrivateGStreamer> track = InbandTextTrackPrivateGStreamer::create(i, pad);
        m_textTracks.insert(i, track);
        m_player->addTextTrack(track.release());
    }

    while (static_cast<gint>(m_textTracks.size()) > numTracks) {
        RefPtr<InbandTextTrackPrivateGStreamer> track = m_textTracks.last();
        track->disconnect();
        m_textTracks.removeLast();
        m_player->removeTextTrack(track.release());
    }
}

void MediaPlayerPrivateGStreamer::newTextSample()
{
    if (!m_textAppSink)
        return;

    GRefPtr<GstEvent> streamStartEvent = adoptGRef(
        gst_pad_get_sticky_event(m_textAppSinkPad.get(), GST_EVENT_STREAM_START, 0));

    GRefPtr<GstSample> sample;
    g_signal_emit_by_name(m_textAppSink.get(), "pull-sample", &sample.outPtr(), NULL);
    ASSERT(sample);

    if (streamStartEvent) {
        bool found = FALSE;
        const gchar* id;
        gst_event_parse_stream_start(streamStartEvent.get(), &id);
        for (size_t i = 0; i < m_textTracks.size(); ++i) {
            RefPtr<InbandTextTrackPrivateGStreamer> track = m_textTracks[i];
            if (track->streamId() == id) {
                track->handleSample(sample);
                found = true;
                break;
            }
        }
        if (!found)
            WARN_MEDIA_MESSAGE("Got sample with unknown stream ID.");
    } else
        WARN_MEDIA_MESSAGE("Unable to handle sample with no stream start event.");
}
#endif

void MediaPlayerPrivateGStreamer::setRate(float rate)
{
    // Higher rate causes crash.
    rate = clampTo(rate, -20, 20);

    // Avoid useless playback rate update.
    if (m_playbackRate == rate) {
        // and make sure that upper layers were notified if rate was set

        if (!m_changingRate && m_player->rate() != m_playbackRate)
            m_player->rateChanged();
        return;
    }

    if (isLiveStream()) {
        // notify upper layers that we cannot handle passed rate.
        m_changingRate = false;
        m_player->rateChanged();
        return;
    }

    GstState state;
    GstState pending;

    m_playbackRate = rate;
    m_changingRate = true;

    gst_element_get_state(m_playBin.get(), &state, &pending, 0);
    if ((state != GST_STATE_PLAYING && state != GST_STATE_PAUSED)
        || (pending == GST_STATE_PAUSED))
        return;

    if (!rate) {
        changePipelineState(GST_STATE_PAUSED);
        return;
    }

    updatePlaybackRate();
}

void MediaPlayerPrivateGStreamer::setPreservesPitch(bool preservesPitch)
{
    m_preservesPitch = preservesPitch;
}

std::unique_ptr<PlatformTimeRanges> MediaPlayerPrivateGStreamer::buffered() const
{
    auto timeRanges = PlatformTimeRanges::create();
    if (m_errorOccured || isLiveStream())
        return timeRanges;

#if GST_CHECK_VERSION(0, 10, 31)
    float mediaDuration(duration());
    if (!mediaDuration || std::isinf(mediaDuration))
        return timeRanges;

    GstQuery* query = gst_query_new_buffering(GST_FORMAT_PERCENT);

    if (!gst_element_query(m_playBin.get(), query)) {
        gst_query_unref(query);
        return timeRanges;
    }

    for (guint index = 0; index < gst_query_get_n_buffering_ranges(query); index++) {
        gint64 rangeStart = 0, rangeStop = 0;
        if (gst_query_parse_nth_buffering_range(query, index, &rangeStart, &rangeStop))
            timeRanges->add(static_cast<float>((rangeStart * mediaDuration) / GST_FORMAT_PERCENT_MAX),
                static_cast<float>((rangeStop * mediaDuration) / GST_FORMAT_PERCENT_MAX));
    }

    // Fallback to the more general maxTimeLoaded() if no range has
    // been found.
    if (!timeRanges->length())
        if (float loaded = maxTimeLoaded())
            timeRanges->add(0, loaded);

    gst_query_unref(query);
#else
    float loaded = maxTimeLoaded();
    if (!m_errorOccured && !isLiveStream() && loaded > 0)
        timeRanges->add(0, loaded);
#endif
    return timeRanges;
}

gboolean MediaPlayerPrivateGStreamer::handleMessage(GstMessage* message)
{
    GUniqueOutPtr<GError> err;
    GUniqueOutPtr<gchar> debug;
    MediaPlayer::NetworkState error;
    bool issueError = true;
    bool attemptNextLocation = false;
    const GstStructure* structure = gst_message_get_structure(message);
    GstState requestedState, currentState;

    m_canFallBackToLastFinishedSeekPositon = false;

    if (structure) {
        const gchar* messageTypeName = gst_structure_get_name(structure);

        // Redirect messages are sent from elements, like qtdemux, to
        // notify of the new location(s) of the media.
        if (!g_strcmp0(messageTypeName, "redirect")) {
            mediaLocationChanged(message);
            return TRUE;
        }
    }

    // We ignore state changes from internal elements. They are forwarded to playbin2 anyway.
    bool messageSourceIsPlaybin = GST_MESSAGE_SRC(message) == reinterpret_cast<GstObject*>(m_playBin.get());

    LOG_MEDIA_MESSAGE("Message %s received from element %s", GST_MESSAGE_TYPE_NAME(message), GST_MESSAGE_SRC_NAME(message));
    switch (GST_MESSAGE_TYPE(message)) {
    case GST_MESSAGE_ERROR:
        if (m_resetPipeline)
            break;
        if (m_missingPlugins)
            break;
        gst_message_parse_error(message, &err.outPtr(), &debug.outPtr());
        ERROR_MEDIA_MESSAGE("Error %d: %s (url=%s)", err->code, err->message, m_url.string().utf8().data());

        GST_DEBUG_BIN_TO_DOT_FILE_WITH_TS(GST_BIN(m_playBin.get()), GST_DEBUG_GRAPH_SHOW_ALL, "webkit-video.error");

        error = MediaPlayer::Empty;
        if (err->code == GST_STREAM_ERROR_CODEC_NOT_FOUND
            || err->code == GST_STREAM_ERROR_WRONG_TYPE
            || err->code == GST_STREAM_ERROR_FAILED
            || err->code == GST_CORE_ERROR_MISSING_PLUGIN
            || err->code == GST_RESOURCE_ERROR_NOT_FOUND)
            error = MediaPlayer::FormatError;
        else if (err->domain == GST_STREAM_ERROR) {
            // Let the mediaPlayerClient handle the stream error, in
            // this case the HTMLMediaElement will emit a stalled
            // event.
            if (err->code == GST_STREAM_ERROR_TYPE_NOT_FOUND) {
                ERROR_MEDIA_MESSAGE("Decode error, let the Media element emit a stalled event.");
                break;
            }
            error = MediaPlayer::DecodeError;
            attemptNextLocation = true;
        } else if (err->domain == GST_RESOURCE_ERROR)
            error = MediaPlayer::NetworkError;

        if (attemptNextLocation)
            issueError = !loadNextLocation();
        if (issueError)
            loadingFailed(error);
        break;
    case GST_MESSAGE_EOS:
        didEnd();
        break;
    case GST_MESSAGE_ASYNC_DONE:
        if (!messageSourceIsPlaybin || m_delayingLoad)
            break;
        asyncStateChangeDone();
        break;
    case GST_MESSAGE_STATE_CHANGED: {
        if (!messageSourceIsPlaybin || m_delayingLoad)
            break;
        updateStates();

        // Construct a filename for the graphviz dot file output.
        GstState newState;
        gst_message_parse_state_changed(message, &currentState, &newState, 0);
        CString dotFileName = String::format("webkit-video.%s_%s", gst_element_state_get_name(currentState), gst_element_state_get_name(newState)).utf8();
        GST_DEBUG_BIN_TO_DOT_FILE_WITH_TS(GST_BIN(m_playBin.get()), GST_DEBUG_GRAPH_SHOW_ALL, dotFileName.data());

        break;
    }
    case GST_MESSAGE_BUFFERING:
        processBufferingStats(message);
        break;
    case GST_MESSAGE_DURATION_CHANGED:
        if (messageSourceIsPlaybin)
            durationChanged();
        break;
    case GST_MESSAGE_REQUEST_STATE:
        gst_message_parse_request_state(message, &requestedState);
        gst_element_get_state(m_playBin.get(), &currentState, NULL, 250);
        if (requestedState < currentState) {
            GUniquePtr<gchar> elementName(gst_element_get_name(GST_ELEMENT(message)));
            INFO_MEDIA_MESSAGE("Element %s requested state change to %s", elementName.get(),
                gst_element_state_get_name(requestedState));
            m_requestedState = requestedState;
            if (!changePipelineState(requestedState))
                loadingFailed(MediaPlayer::Empty);
        }
        break;
    case GST_MESSAGE_ELEMENT:
        if (gst_is_missing_plugin_message(message)) {
            gchar* detail = gst_missing_plugin_message_get_installer_detail(message);
            gchar* detailArray[2] = {detail, 0};
            GstInstallPluginsReturn result = gst_install_plugins_async(detailArray, 0, mediaPlayerPrivatePluginInstallerResultFunction, this);
            m_missingPlugins = result == GST_INSTALL_PLUGINS_STARTED_OK;
            g_free(detail);
        }
        break;
#if ENABLE(VIDEO_TRACK)
    case GST_MESSAGE_TOC:
        processTableOfContents(message);
        break;
#endif
    default:
        LOG_MEDIA_MESSAGE("Unhandled GStreamer message type: %s",
                    GST_MESSAGE_TYPE_NAME(message));
        break;
    }
    return TRUE;
}

void MediaPlayerPrivateGStreamer::handlePluginInstallerResult(GstInstallPluginsReturn result)
{
    m_missingPlugins = false;
    if (result == GST_INSTALL_PLUGINS_SUCCESS) {
        changePipelineState(GST_STATE_READY);
        changePipelineState(GST_STATE_PAUSED);
    }
}

void MediaPlayerPrivateGStreamer::processBufferingStats(GstMessage* message)
{
    m_buffering = true;
    const GstStructure *structure = gst_message_get_structure(message);
    gst_structure_get_int(structure, "buffer-percent", &m_bufferingPercentage);

    LOG_MEDIA_MESSAGE("[Buffering] Buffering: %d%%.", m_bufferingPercentage);

    updateStates();
}

#if ENABLE(VIDEO_TRACK)
void MediaPlayerPrivateGStreamer::processTableOfContents(GstMessage* message)
{
    if (m_chaptersTrack)
        m_player->removeTextTrack(m_chaptersTrack);

    m_chaptersTrack = InbandMetadataTextTrackPrivateGStreamer::create(InbandTextTrackPrivate::Chapters);
    m_player->addTextTrack(m_chaptersTrack);

    GRefPtr<GstToc> toc;
    gboolean updated;
    gst_message_parse_toc(message, &toc.outPtr(), &updated);
    ASSERT(toc);

    for (GList* i = gst_toc_get_entries(toc.get()); i; i = i->next)
        processTableOfContentsEntry(static_cast<GstTocEntry*>(i->data), 0);
}

void MediaPlayerPrivateGStreamer::processTableOfContentsEntry(GstTocEntry* entry, GstTocEntry* parent)
{
    UNUSED_PARAM(parent);
    ASSERT(entry);

    RefPtr<GenericCueData> cue = GenericCueData::create();

    gint64 start = -1, stop = -1;
    gst_toc_entry_get_start_stop_times(entry, &start, &stop);
    if (start != -1)
        cue->setStartTime(static_cast<double>(start) / GST_SECOND);
    if (stop != -1)
        cue->setEndTime(static_cast<double>(stop) / GST_SECOND);

    GstTagList* tags = gst_toc_entry_get_tags(entry);
    if (tags) {
        gchar* title =  0;
        gst_tag_list_get_string(tags, GST_TAG_TITLE, &title);
        if (title) {
            cue->setContent(title);
            g_free(title);
        }
    }

    m_chaptersTrack->client()->addGenericCue(m_chaptersTrack.get(), cue.release());

    for (GList* i = gst_toc_entry_get_sub_entries(entry); i; i = i->next)
        processTableOfContentsEntry(static_cast<GstTocEntry*>(i->data), entry);
}
#endif

void MediaPlayerPrivateGStreamer::fillTimerFired(Timer<MediaPlayerPrivateGStreamer>*)
{
    GstQuery* query = gst_query_new_buffering(GST_FORMAT_PERCENT);

    if (!gst_element_query(m_playBin.get(), query)) {
        gst_query_unref(query);
        return;
    }

    gint64 start, stop;
    gdouble fillStatus = 100.0;

    gst_query_parse_buffering_range(query, 0, &start, &stop, 0);
    gst_query_unref(query);

    if (stop != -1)
        fillStatus = 100.0 * stop / GST_FORMAT_PERCENT_MAX;

    LOG_MEDIA_MESSAGE("[Buffering] Download buffer filled up to %f%%", fillStatus);

    if (!m_mediaDuration)
        durationChanged();

    // Update maxTimeLoaded only if the media duration is
    // available. Otherwise we can't compute it.
    if (m_mediaDuration) {
        if (fillStatus == 100.0)
            m_maxTimeLoaded = m_mediaDuration;
        else
            m_maxTimeLoaded = static_cast<float>((fillStatus * m_mediaDuration) / 100.0);
        LOG_MEDIA_MESSAGE("[Buffering] Updated maxTimeLoaded: %f", m_maxTimeLoaded);
    }

    m_downloadFinished = fillStatus == 100.0;
    if (!m_downloadFinished) {
        updateStates();
        return;
    }

    // Media is now fully loaded. It will play even if network
    // connection is cut. Buffering is done, remove the fill source
    // from the main loop.
    m_fillTimer.stop();
    updateStates();
}

float MediaPlayerPrivateGStreamer::maxTimeSeekable() const
{
    if (m_errorOccured)
        return 0.0f;

    LOG_MEDIA_MESSAGE("maxTimeSeekable");
    // infinite duration means live stream
    if (std::isinf(duration()))
        return 0.0f;

    return duration();
}

float MediaPlayerPrivateGStreamer::maxTimeLoaded() const
{
    if (m_errorOccured)
        return 0.0f;

    float loaded = m_maxTimeLoaded;
    if (m_isEndReached && m_mediaDuration)
        loaded = m_mediaDuration;
    LOG_MEDIA_MESSAGE("maxTimeLoaded: %f", loaded);
    return loaded;
}

bool MediaPlayerPrivateGStreamer::didLoadingProgress() const
{
    if (!m_playBin || !m_mediaDuration || !totalBytes())
        return false;
    float currentMaxTimeLoaded = maxTimeLoaded();
    bool didLoadingProgress = currentMaxTimeLoaded != m_maxTimeLoadedAtLastDidLoadingProgress;
    m_maxTimeLoadedAtLastDidLoadingProgress = currentMaxTimeLoaded;
    LOG_MEDIA_MESSAGE("didLoadingProgress: %d", didLoadingProgress);
    return didLoadingProgress;
}

unsigned MediaPlayerPrivateGStreamer::totalBytes() const
{
    if (m_errorOccured)
        return 0;

    if (m_totalBytes != -1)
        return m_totalBytes;

    if (!m_source)
        return 0;

    GstFormat fmt = GST_FORMAT_BYTES;
    gint64 length = 0;
    if (gst_element_query_duration(m_source.get(), fmt, &length)) {
        INFO_MEDIA_MESSAGE("totalBytes %" G_GINT64_FORMAT, length);
        m_totalBytes = static_cast<unsigned>(length);
        m_isStreaming = !length;
        return m_totalBytes;
    }

    // Fall back to querying the source pads manually.
    // See also https://bugzilla.gnome.org/show_bug.cgi?id=638749
    GstIterator* iter = gst_element_iterate_src_pads(m_source.get());
    bool done = false;
    while (!done) {
        GValue item = G_VALUE_INIT;
        switch (gst_iterator_next(iter, &item)) {
        case GST_ITERATOR_OK: {
            GstPad* pad = static_cast<GstPad*>(g_value_get_object(&item));
            gint64 padLength = 0;
            if (gst_pad_query_duration(pad, fmt, &padLength) && padLength > length)
                length = padLength;
            break;
        }
        case GST_ITERATOR_RESYNC:
            gst_iterator_resync(iter);
            break;
        case GST_ITERATOR_ERROR:
            FALLTHROUGH;
        case GST_ITERATOR_DONE:
            done = true;
            break;
        }

        g_value_unset(&item);
    }

    gst_iterator_free(iter);

    INFO_MEDIA_MESSAGE("totalBytes %" G_GINT64_FORMAT, length);
    m_totalBytes = static_cast<unsigned>(length);
    m_isStreaming = !length;
    return m_totalBytes;
}

void MediaPlayerPrivateGStreamer::sourceChanged()
{
    m_source.clear();
    g_object_get(m_playBin.get(), "source", &m_source.outPtr(), NULL);

    if (WEBKIT_IS_WEB_SRC(m_source.get()))
        webKitWebSrcSetMediaPlayer(WEBKIT_WEB_SRC(m_source.get()), m_player);
#if ENABLE(MEDIA_SOURCE)
    if (m_mediaSource && WEBKIT_IS_MEDIA_SRC(m_source.get())) {
        MediaSourceGStreamer::open(m_mediaSource.get(), WEBKIT_MEDIA_SRC(m_source.get()));
        webKitMediaSrcSetPlayBin(WEBKIT_MEDIA_SRC(m_source.get()), m_playBin.get());
    }
#endif
}

void MediaPlayerPrivateGStreamer::cancelLoad()
{
    if (m_networkState < MediaPlayer::Loading || m_networkState == MediaPlayer::Loaded)
        return;

    if (m_playBin)
        changePipelineState(GST_STATE_READY);
}

void MediaPlayerPrivateGStreamer::asyncStateChangeDone()
{
    if (!m_playBin || m_errorOccured)
        return;

    if (m_seeking) {
        if (m_seekIsPending)
            updateStates();
        else {
            LOG_MEDIA_MESSAGE("[Seek] seeked to %f", m_seekTime);
            m_seeking = false;
            if (m_timeOfOverlappingSeek != m_seekTime && m_timeOfOverlappingSeek != -1) {
                seek(m_timeOfOverlappingSeek);
                m_timeOfOverlappingSeek = -1;
                return;
            }
            m_timeOfOverlappingSeek = -1;

            // The pipeline can still have a pending state. In this case a position query will fail.
            // Right now we can use m_seekTime as a fallback.
            m_canFallBackToLastFinishedSeekPositon = true;
            timeChanged();
        }
    } else
        updateStates();
}

void MediaPlayerPrivateGStreamer::updateStates()
{
    if (!m_playBin)
        return;

    if (m_errorOccured)
        return;

    MediaPlayer::NetworkState oldNetworkState = m_networkState;
    MediaPlayer::ReadyState oldReadyState = m_readyState;
    GstState state;
    GstState pending;

    GstStateChangeReturn getStateResult = gst_element_get_state(m_playBin.get(), &state, &pending, 250 * GST_NSECOND);

    bool shouldUpdatePlaybackState = false;
    switch (getStateResult) {
    case GST_STATE_CHANGE_SUCCESS: {
        LOG_MEDIA_MESSAGE("State: %s, pending: %s", gst_element_state_get_name(state), gst_element_state_get_name(pending));

        // Do nothing if on EOS and state changed to READY to avoid recreating the player
        // on HTMLMediaElement and properly generate the video 'ended' event.
        if (m_isEndReached && state == GST_STATE_READY)
            break;

        if (state <= GST_STATE_READY) {
            m_resetPipeline = true;
            m_mediaDuration = 0;
        } else {
            m_resetPipeline = false;
            cacheDuration();
        }

        bool didBuffering = m_buffering;

        // Update ready and network states.
        switch (state) {
        case GST_STATE_NULL:
            m_readyState = MediaPlayer::HaveNothing;
            m_networkState = MediaPlayer::Empty;
            break;
        case GST_STATE_READY:
            m_readyState = MediaPlayer::HaveMetadata;
            m_networkState = MediaPlayer::Empty;
            break;
        case GST_STATE_PAUSED:
        case GST_STATE_PLAYING:
            if (m_buffering) {
                if (m_bufferingPercentage == 100) {
                    LOG_MEDIA_MESSAGE("[Buffering] Complete.");
                    m_buffering = false;
                    m_readyState = MediaPlayer::HaveEnoughData;
                    m_networkState = m_downloadFinished ? MediaPlayer::Idle : MediaPlayer::Loading;
                } else {
                    m_readyState = MediaPlayer::HaveCurrentData;
                    m_networkState = MediaPlayer::Loading;
                }
            } else if (m_downloadFinished) {
                m_readyState = MediaPlayer::HaveEnoughData;
                m_networkState = MediaPlayer::Loaded;
            } else {
                m_readyState = MediaPlayer::HaveFutureData;
                m_networkState = MediaPlayer::Loading;
            }

            break;
        default:
            ASSERT_NOT_REACHED();
            break;
        }

        // Sync states where needed.
        if (state == GST_STATE_PAUSED) {
            if (!m_volumeAndMuteInitialized) {
                notifyPlayerOfVolumeChange();
                notifyPlayerOfMute();
                m_volumeAndMuteInitialized = true;
            }

            if (didBuffering && !m_buffering && !m_paused) {
                LOG_MEDIA_MESSAGE("[Buffering] Restarting playback.");
                changePipelineState(GST_STATE_PLAYING);
            }
        } else if (state == GST_STATE_PLAYING) {
            m_paused = false;

            if (m_buffering && !isLiveStream()) {
                LOG_MEDIA_MESSAGE("[Buffering] Pausing stream for buffering.");
                changePipelineState(GST_STATE_PAUSED);
            }
        } else
            m_paused = true;

        if (m_requestedState == GST_STATE_PAUSED && state == GST_STATE_PAUSED) {
            shouldUpdatePlaybackState = true;
            LOG_MEDIA_MESSAGE("Requested state change to %s was completed", gst_element_state_get_name(state));
        }

        break;
    }
    case GST_STATE_CHANGE_ASYNC:
        LOG_MEDIA_MESSAGE("Async: State: %s, pending: %s", gst_element_state_get_name(state), gst_element_state_get_name(pending));
        // Change in progress.
        break;
    case GST_STATE_CHANGE_FAILURE:
        LOG_MEDIA_MESSAGE("Failure: State: %s, pending: %s", gst_element_state_get_name(state), gst_element_state_get_name(pending));
        // Change failed
        return;
    case GST_STATE_CHANGE_NO_PREROLL:
        LOG_MEDIA_MESSAGE("No preroll: State: %s, pending: %s", gst_element_state_get_name(state), gst_element_state_get_name(pending));

        // Live pipelines go in PAUSED without prerolling.
        m_isStreaming = true;
        setDownloadBuffering();

        if (state == GST_STATE_READY)
            m_readyState = MediaPlayer::HaveNothing;
        else if (state == GST_STATE_PAUSED) {
            m_readyState = MediaPlayer::HaveEnoughData;
            m_paused = true;
        } else if (state == GST_STATE_PLAYING)
            m_paused = false;

        if (!m_paused)
            changePipelineState(GST_STATE_PLAYING);

        m_networkState = MediaPlayer::Loading;
        break;
    default:
        LOG_MEDIA_MESSAGE("Else : %d", getStateResult);
        break;
    }

    m_requestedState = GST_STATE_VOID_PENDING;

    if (shouldUpdatePlaybackState)
        m_player->playbackStateChanged();

    if (m_networkState != oldNetworkState) {
        LOG_MEDIA_MESSAGE("Network State Changed from %u to %u", oldNetworkState, m_networkState);
        m_player->networkStateChanged();
    }
    if (m_readyState != oldReadyState) {
        LOG_MEDIA_MESSAGE("Ready State Changed from %u to %u", oldReadyState, m_readyState);
        m_player->readyStateChanged();
    }

    if (getStateResult == GST_STATE_CHANGE_SUCCESS && state >= GST_STATE_PAUSED) {
        updatePlaybackRate();
        if (m_seekIsPending) {
            LOG_MEDIA_MESSAGE("[Seek] committing pending seek to %f", m_seekTime);
            m_seekIsPending = false;
            m_seeking = doSeek(toGstClockTime(m_seekTime), m_player->rate(), static_cast<GstSeekFlags>(GST_SEEK_FLAG_FLUSH | GST_SEEK_FLAG_ACCURATE));
            if (!m_seeking)
                LOG_MEDIA_MESSAGE("[Seek] seeking to %f failed", m_seekTime);
        }
    }
}

void MediaPlayerPrivateGStreamer::mediaLocationChanged(GstMessage* message)
{
    if (m_mediaLocations)
        gst_structure_free(m_mediaLocations);

    const GstStructure* structure = gst_message_get_structure(message);
    if (structure) {
        // This structure can contain:
        // - both a new-location string and embedded locations structure
        // - or only a new-location string.
        m_mediaLocations = gst_structure_copy(structure);
        const GValue* locations = gst_structure_get_value(m_mediaLocations, "locations");

        if (locations)
            m_mediaLocationCurrentIndex = static_cast<int>(gst_value_list_get_size(locations)) -1;

        loadNextLocation();
    }
}

bool MediaPlayerPrivateGStreamer::loadNextLocation()
{
    if (!m_mediaLocations)
        return false;

    const GValue* locations = gst_structure_get_value(m_mediaLocations, "locations");
    const gchar* newLocation = 0;

    if (!locations) {
        // Fallback on new-location string.
        newLocation = gst_structure_get_string(m_mediaLocations, "new-location");
        if (!newLocation)
            return false;
    }

    if (!newLocation) {
        if (m_mediaLocationCurrentIndex < 0) {
            m_mediaLocations = 0;
            return false;
        }

        const GValue* location = gst_value_list_get_value(locations,
                                                          m_mediaLocationCurrentIndex);
        const GstStructure* structure = gst_value_get_structure(location);

        if (!structure) {
            m_mediaLocationCurrentIndex--;
            return false;
        }

        newLocation = gst_structure_get_string(structure, "new-location");
    }

    if (newLocation) {
        // Found a candidate. new-location is not always an absolute url
        // though. We need to take the base of the current url and
        // append the value of new-location to it.
        URL baseUrl = gst_uri_is_valid(newLocation) ? URL() : m_url;
        URL newUrl = URL(baseUrl, newLocation);

        RefPtr<SecurityOrigin> securityOrigin = SecurityOrigin::create(m_url);
        if (securityOrigin->canRequest(newUrl)) {
            INFO_MEDIA_MESSAGE("New media url: %s", newUrl.string().utf8().data());

            // Reset player states.
            m_networkState = MediaPlayer::Loading;
            m_player->networkStateChanged();
            m_readyState = MediaPlayer::HaveNothing;
            m_player->readyStateChanged();

            // Reset pipeline state.
            m_resetPipeline = true;
            changePipelineState(GST_STATE_READY);

            GstState state;
            gst_element_get_state(m_playBin.get(), &state, 0, 0);
            if (state <= GST_STATE_READY) {
                // Set the new uri and start playing.
                g_object_set(m_playBin.get(), "uri", newUrl.string().utf8().data(), NULL);
                m_url = newUrl;
                changePipelineState(GST_STATE_PLAYING);
                return true;
            }
        } else
            INFO_MEDIA_MESSAGE("Not allowed to load new media location: %s", newUrl.string().utf8().data());
    }
    m_mediaLocationCurrentIndex--;
    return false;
}

void MediaPlayerPrivateGStreamer::loadStateChanged()
{
    updateStates();
}

void MediaPlayerPrivateGStreamer::timeChanged()
{
    updateStates();
    m_player->timeChanged();
}

void MediaPlayerPrivateGStreamer::didEnd()
{
    // Synchronize position and duration values to not confuse the
    // HTMLMediaElement. In some cases like reverse playback the
    // position is not always reported as 0 for instance.
    float now = currentTime();
    if (now > 0 && now <= duration() && m_mediaDuration != now) {
        m_mediaDurationKnown = true;
        m_mediaDuration = now;
        m_player->durationChanged();
    }

    m_isEndReached = true;
    timeChanged();

    if (!m_player->mediaPlayerClient()->mediaPlayerIsLooping()) {
        m_paused = true;
        changePipelineState(GST_STATE_READY);
        m_downloadFinished = false;
    }
}

void MediaPlayerPrivateGStreamer::cacheDuration()
{
    if (m_mediaDuration || !m_mediaDurationKnown)
        return;

    float newDuration = duration();
    if (std::isinf(newDuration)) {
        // Only pretend that duration is not available if the the query failed in a stable pipeline state.
        GstState state;
        if (gst_element_get_state(m_playBin.get(), &state, 0, 0) == GST_STATE_CHANGE_SUCCESS && state > GST_STATE_READY)
            m_mediaDurationKnown = false;
        return;
    }

    m_mediaDuration = newDuration;
}

void MediaPlayerPrivateGStreamer::durationChanged()
{
    float previousDuration = m_mediaDuration;

    cacheDuration();
    // Avoid emiting durationchanged in the case where the previous
    // duration was 0 because that case is already handled by the
    // HTMLMediaElement.
    if (previousDuration && m_mediaDuration != previousDuration)
        m_player->durationChanged();
}

void MediaPlayerPrivateGStreamer::loadingFailed(MediaPlayer::NetworkState error)
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

    // Loading failed, remove ready timer.
    if (m_readyTimerHandler) {
        g_source_remove(m_readyTimerHandler);
        m_readyTimerHandler = 0;
    }
}

static HashSet<String> mimeTypeCache()
{
    initializeGStreamerAndRegisterWebKitElements();

    DEFINE_STATIC_LOCAL(HashSet<String>, cache, ());
    static bool typeListInitialized = false;

    if (typeListInitialized)
        return cache;

    const char* mimeTypes[] = {
        "application/ogg",
        "application/vnd.apple.mpegurl",
        "application/vnd.rn-realmedia",
        "application/x-3gp",
        "application/x-pn-realaudio",
        "audio/3gpp",
        "audio/aac",
        "audio/flac",
        "audio/iLBC-sh",
        "audio/midi",
        "audio/mobile-xmf",
        "audio/mp1",
        "audio/mp2",
        "audio/mp3",
        "audio/mp4",
        "audio/mpeg",
        "audio/ogg",
        "audio/opus",
        "audio/qcelp",
        "audio/riff-midi",
        "audio/speex",
        "audio/wav",
        "audio/webm",
        "audio/x-ac3",
        "audio/x-aiff",
        "audio/x-amr-nb-sh",
        "audio/x-amr-wb-sh",
        "audio/x-au",
        "audio/x-ay",
        "audio/x-celt",
        "audio/x-dts",
        "audio/x-flac",
        "audio/x-gbs",
        "audio/x-gsm",
        "audio/x-gym",
        "audio/x-imelody",
        "audio/x-ircam",
        "audio/x-kss",
        "audio/x-m4a",
        "audio/x-mod",
        "audio/x-mp3",
        "audio/x-mpeg",
        "audio/x-musepack",
        "audio/x-nist",
        "audio/x-nsf",
        "audio/x-paris",
        "audio/x-sap",
        "audio/x-sbc",
        "audio/x-sds",
        "audio/x-shorten",
        "audio/x-sid",
        "audio/x-spc",
        "audio/x-speex",
        "audio/x-svx",
        "audio/x-ttafile",
        "audio/x-vgm",
        "audio/x-voc",
        "audio/x-vorbis+ogg",
        "audio/x-w64",
        "audio/x-wav",
        "audio/x-wavpack",
        "audio/x-wavpack-correction",
        "video/3gpp",
        "video/mj2",
        "video/mp4",
        "video/mpeg",
        "video/mpegts",
        "video/ogg",
        "video/quicktime",
        "video/vivo",
        "video/webm",
        "video/x-cdxa",
        "video/x-dirac",
        "video/x-dv",
        "video/x-fli",
        "video/x-flv",
        "video/x-h263",
        "video/x-ivf",
        "video/x-m4v",
        "video/x-matroska",
        "video/x-mng",
        "video/x-ms-asf",
        "video/x-msvideo",
        "video/x-mve",
        "video/x-nuv",
        "video/x-vcd"
    };

    for (unsigned i = 0; i < (sizeof(mimeTypes) / sizeof(*mimeTypes)); ++i)
        cache.add(String(mimeTypes[i]));

    typeListInitialized = true;
    return cache;
}

void MediaPlayerPrivateGStreamer::getSupportedTypes(HashSet<String>& types)
{
    types = mimeTypeCache();
}

MediaPlayer::SupportsType MediaPlayerPrivateGStreamer::supportsType(const MediaEngineSupportParameters& parameters)
{
    if (parameters.type.isNull() || parameters.type.isEmpty())
        return MediaPlayer::IsNotSupported;

    // spec says we should not return "probably" if the codecs string is empty
    if (mimeTypeCache().contains(parameters.type))
        return parameters.codecs.isEmpty() ? MediaPlayer::MayBeSupported : MediaPlayer::IsSupported;
    return MediaPlayer::IsNotSupported;
}

void MediaPlayerPrivateGStreamer::setDownloadBuffering()
{
    if (!m_playBin)
        return;

    unsigned flags;
    g_object_get(m_playBin.get(), "flags", &flags, NULL);

    unsigned flagDownload = getGstPlaysFlag("download");

    // We don't want to stop downloading if we already started it.
    if (flags & flagDownload && m_readyState > MediaPlayer::HaveNothing && !m_resetPipeline)
        return;

    bool shouldDownload = !isLiveStream() && m_preload == MediaPlayer::Auto;
    if (shouldDownload) {
        LOG_MEDIA_MESSAGE("Enabling on-disk buffering");
        g_object_set(m_playBin.get(), "flags", flags | flagDownload, NULL);
        m_fillTimer.startRepeating(0.2);
    } else {
        LOG_MEDIA_MESSAGE("Disabling on-disk buffering");
        g_object_set(m_playBin.get(), "flags", flags & ~flagDownload, NULL);
        m_fillTimer.stop();
    }
}

void MediaPlayerPrivateGStreamer::setPreload(MediaPlayer::Preload preload)
{
    if (preload == MediaPlayer::Auto && isLiveStream())
        return;

    m_preload = preload;
    setDownloadBuffering();

    if (m_delayingLoad && m_preload != MediaPlayer::None) {
        m_delayingLoad = false;
        commitLoad();
    }
}

GstElement* MediaPlayerPrivateGStreamer::createAudioSink()
{
    m_autoAudioSink = gst_element_factory_make("autoaudiosink", 0);
    g_signal_connect(m_autoAudioSink.get(), "child-added", G_CALLBACK(setAudioStreamPropertiesCallback), this);

    // Construct audio sink only if pitch preserving is enabled.
    if (!m_preservesPitch)
        return m_autoAudioSink.get();

    GstElement* scale = gst_element_factory_make("scaletempo", 0);
    if (!scale) {
        GST_WARNING("Failed to create scaletempo");
        return m_autoAudioSink.get();
    }

    GstElement* audioSinkBin = gst_bin_new("audio-sink");
    GstElement* convert = gst_element_factory_make("audioconvert", 0);
    GstElement* resample = gst_element_factory_make("audioresample", 0);

    gst_bin_add_many(GST_BIN(audioSinkBin), scale, convert, resample, m_autoAudioSink.get(), NULL);

    if (!gst_element_link_many(scale, convert, resample, m_autoAudioSink.get(), NULL)) {
        GST_WARNING("Failed to link audio sink elements");
        gst_object_unref(audioSinkBin);
        return m_autoAudioSink.get();
    }

    GRefPtr<GstPad> pad = adoptGRef(gst_element_get_static_pad(scale, "sink"));
    gst_element_add_pad(audioSinkBin, gst_ghost_pad_new("sink", pad.get()));
    return audioSinkBin;
}

GstElement* MediaPlayerPrivateGStreamer::audioSink() const
{
    GstElement* sink;
    g_object_get(m_playBin.get(), "audio-sink", &sink, nullptr);
    return sink;
}

void MediaPlayerPrivateGStreamer::createGSTPlayBin()
{
    ASSERT(!m_playBin);

    // gst_element_factory_make() returns a floating reference so
    // we should not adopt.
    m_playBin = gst_element_factory_make("playbin", "play");
    setStreamVolumeElement(GST_STREAM_VOLUME(m_playBin.get()));

    GRefPtr<GstBus> bus = adoptGRef(gst_pipeline_get_bus(GST_PIPELINE(m_playBin.get())));
    gst_bus_add_signal_watch(bus.get());
    g_signal_connect(bus.get(), "message", G_CALLBACK(mediaPlayerPrivateMessageCallback), this);

    g_object_set(m_playBin.get(), "mute", m_player->muted(), NULL);

    g_signal_connect(m_playBin.get(), "notify::source", G_CALLBACK(mediaPlayerPrivateSourceChangedCallback), this);
    g_signal_connect(m_playBin.get(), "video-changed", G_CALLBACK(mediaPlayerPrivateVideoChangedCallback), this);
    g_signal_connect(m_playBin.get(), "audio-changed", G_CALLBACK(mediaPlayerPrivateAudioChangedCallback), this);
#if ENABLE(VIDEO_TRACK)
    if (webkitGstCheckVersion(1, 1, 2)) {
        g_signal_connect(m_playBin.get(), "text-changed", G_CALLBACK(mediaPlayerPrivateTextChangedCallback), this);

        GstElement* textCombiner = webkitTextCombinerNew();
        ASSERT(textCombiner);
        g_object_set(m_playBin.get(), "text-stream-combiner", textCombiner, NULL);

        m_textAppSink = webkitTextSinkNew();
        ASSERT(m_textAppSink);

        m_textAppSinkPad = adoptGRef(gst_element_get_static_pad(m_textAppSink.get(), "sink"));
        ASSERT(m_textAppSinkPad);

        g_object_set(m_textAppSink.get(), "emit-signals", true, "enable-last-sample", false, "caps", gst_caps_new_empty_simple("text/vtt"), NULL);
        g_signal_connect(m_textAppSink.get(), "new-sample", G_CALLBACK(mediaPlayerPrivateNewTextSampleCallback), this);

        g_object_set(m_playBin.get(), "text-sink", m_textAppSink.get(), NULL);
    }
#endif

    g_object_set(m_playBin.get(), "video-sink", createVideoSink(), "audio-sink", createAudioSink(), nullptr);

    GRefPtr<GstPad> videoSinkPad = adoptGRef(gst_element_get_static_pad(m_webkitVideoSink.get(), "sink"));
    if (videoSinkPad)
        g_signal_connect(videoSinkPad.get(), "notify::caps", G_CALLBACK(mediaPlayerPrivateVideoSinkCapsChangedCallback), this);
}

void MediaPlayerPrivateGStreamer::simulateAudioInterruption()
{
    GstMessage* message = gst_message_new_request_state(GST_OBJECT(m_playBin.get()), GST_STATE_PAUSED);
    gst_element_post_message(m_playBin.get(), message);
}

}

#endif // USE(GSTREAMER)
