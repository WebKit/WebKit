/*
 * Copyright (C) 2007, 2009 Apple Inc.  All rights reserved.
 * Copyright (C) 2007 Collabora Ltd.  All rights reserved.
 * Copyright (C) 2007 Alp Toker <alp@atoker.com>
 * Copyright (C) 2009 Gustavo Noronha Silva <gns@gnome.org>
 * Copyright (C) 2009, 2010, 2011, 2012, 2013 Igalia S.L
 * Copyright (C) 2014 Cable Television Laboratories, Inc.
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
#include "MediaPlayerRequestInstallMissingPluginsCallback.h"
#include "NotImplemented.h"
#include "SecurityOrigin.h"
#include "TimeRanges.h"
#include "WebKitWebSourceGStreamer.h"
#include <gst/gst.h>
#include <gst/pbutils/missing-plugins.h>
#include <limits>
#include <wtf/HexNumber.h>
#include <wtf/MediaTime.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/glib/GUniquePtr.h>
#include <wtf/text/CString.h>

#if ENABLE(VIDEO_TRACK)
#include "AudioTrackPrivateGStreamer.h"
#include "InbandMetadataTextTrackPrivateGStreamer.h"
#include "InbandTextTrackPrivateGStreamer.h"
#include "TextCombinerGStreamer.h"
#include "TextSinkGStreamer.h"
#include "VideoTrackPrivateGStreamer.h"
#endif

#if ENABLE(VIDEO_TRACK) && USE(GSTREAMER_MPEGTS)
#define GST_USE_UNSTABLE_API
#include <gst/mpegts/mpegts.h>
#undef GST_USE_UNSTABLE_API
#endif
#include <gst/audio/streamvolume.h>

#if ENABLE(MEDIA_SOURCE)
#include "MediaSource.h"
#include "WebKitMediaSourceGStreamer.h"
#endif

#if ENABLE(WEB_AUDIO)
#include "AudioSourceProviderGStreamer.h"
#endif

GST_DEBUG_CATEGORY_EXTERN(webkit_media_player_debug);
#define GST_CAT_DEFAULT webkit_media_player_debug

using namespace std;

namespace WebCore {

void MediaPlayerPrivateGStreamer::setAudioStreamPropertiesCallback(MediaPlayerPrivateGStreamer* player, GObject* object)
{
    player->setAudioStreamProperties(object);
}

void MediaPlayerPrivateGStreamer::setAudioStreamProperties(GObject* object)
{
    if (g_strcmp0(G_OBJECT_TYPE_NAME(object), "GstPulseSink"))
        return;

    const char* role = m_player->client().mediaPlayerIsVideo() ? "video" : "music";
    GstStructure* structure = gst_structure_new("stream-properties", "media.role", G_TYPE_STRING, role, NULL);
    g_object_set(object, "stream-properties", structure, NULL);
    gst_structure_free(structure);
    GUniquePtr<gchar> elementName(gst_element_get_name(GST_ELEMENT(object)));
    GST_DEBUG("Set media.role as %s at %s", role, elementName.get());
}

void MediaPlayerPrivateGStreamer::registerMediaEngine(MediaEngineRegistrar registrar)
{
    if (isAvailable())
        registrar([](MediaPlayer* player) { return std::make_unique<MediaPlayerPrivateGStreamer>(player); },
            getSupportedTypes, supportsType, 0, 0, 0, 0);
}

bool initializeGStreamerAndRegisterWebKitElements()
{
    if (!initializeGStreamer())
        return false;

    GRefPtr<GstElementFactory> srcFactory = adoptGRef(gst_element_factory_find("webkitwebsrc"));
    if (!srcFactory) {
        GST_DEBUG_CATEGORY_INIT(webkit_media_player_debug, "webkitmediaplayer", 0, "WebKit media player");
        gst_element_register(0, "webkitwebsrc", GST_RANK_PRIMARY + 100, WEBKIT_TYPE_WEB_SRC);
    }

#if ENABLE(MEDIA_SOURCE)
    GRefPtr<GstElementFactory> WebKitMediaSrcFactory = adoptGRef(gst_element_factory_find("webkitmediasrc"));
    if (!WebKitMediaSrcFactory)
        gst_element_register(0, "webkitmediasrc", GST_RANK_PRIMARY + 100, WEBKIT_TYPE_MEDIA_SRC);
#endif
    return true;
}

bool MediaPlayerPrivateGStreamer::isAvailable()
{
    if (!initializeGStreamerAndRegisterWebKitElements())
        return false;

    GRefPtr<GstElementFactory> factory = adoptGRef(gst_element_factory_find("playbin"));
    return factory;
}

MediaPlayerPrivateGStreamer::MediaPlayerPrivateGStreamer(MediaPlayer* player)
    : MediaPlayerPrivateGStreamerBase(player)
    , m_weakPtrFactory(this)
    , m_source(0)
    , m_seekTime(0)
    , m_changingRate(false)
    , m_isEndReached(false)
    , m_isStreaming(false)
    , m_mediaLocations(0)
    , m_mediaLocationCurrentIndex(0)
    , m_resetPipeline(false)
    , m_paused(true)
    , m_playbackRatePause(false)
    , m_seeking(false)
    , m_seekIsPending(false)
    , m_timeOfOverlappingSeek(-1)
    , m_canFallBackToLastFinishedSeekPosition(false)
    , m_buffering(false)
    , m_playbackRate(1)
    , m_lastPlaybackRate(1)
    , m_errorOccured(false)
    , m_downloadFinished(false)
    , m_durationAtEOS(0)
    , m_fillTimer(*this, &MediaPlayerPrivateGStreamer::fillTimerFired)
    , m_maxTimeLoaded(0)
    , m_bufferingPercentage(0)
    , m_preload(player->preload())
    , m_delayingLoad(false)
    , m_maxTimeLoadedAtLastDidLoadingProgress(0)
    , m_volumeAndMuteInitialized(false)
    , m_hasVideo(false)
    , m_hasAudio(false)
    , m_readyTimerHandler(RunLoop::main(), this, &MediaPlayerPrivateGStreamer::readyTimerFired)
    , m_totalBytes(0)
    , m_preservesPitch(false)
#if ENABLE(WEB_AUDIO)
    , m_audioSourceProvider(std::make_unique<AudioSourceProviderGStreamer>())
#endif
    , m_requestedState(GST_STATE_VOID_PENDING)
{
#if USE(GLIB) && !PLATFORM(EFL)
    m_readyTimerHandler.setPriority(G_PRIORITY_DEFAULT_IDLE);
#endif
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

    m_readyTimerHandler.stop();
    if (m_missingPluginsCallback) {
        m_missingPluginsCallback->invalidate();
        m_missingPluginsCallback = nullptr;
    }

    if (m_pipeline) {
        GRefPtr<GstBus> bus = adoptGRef(gst_pipeline_get_bus(GST_PIPELINE(m_pipeline.get())));
        ASSERT(bus);
        gst_bus_set_sync_handler(bus.get(), nullptr, nullptr, nullptr);
        g_signal_handlers_disconnect_matched(m_pipeline.get(), G_SIGNAL_MATCH_DATA, 0, 0, nullptr, nullptr, this);
        gst_element_set_state(m_pipeline.get(), GST_STATE_NULL);
    }

    if (m_videoSink) {
        GRefPtr<GstPad> videoSinkPad = adoptGRef(gst_element_get_static_pad(m_videoSink.get(), "sink"));
        g_signal_handlers_disconnect_by_func(videoSinkPad.get(), reinterpret_cast<gpointer>(videoSinkCapsChangedCallback), this);
    }
}

void MediaPlayerPrivateGStreamer::load(const String& urlString)
{
    if (!initializeGStreamerAndRegisterWebKitElements())
        return;

    URL url(URL(), urlString);
    if (url.isBlankURL())
        return;

    // Clean out everything after file:// url path.
    String cleanURL(urlString);
    if (url.isLocalFile())
        cleanURL = cleanURL.substring(0, url.pathEnd());

    if (!m_pipeline)
        createGSTPlayBin();

    if (m_fillTimer.isActive())
        m_fillTimer.stop();

    ASSERT(m_pipeline);

    m_url = URL(URL(), cleanURL);
    g_object_set(m_pipeline.get(), "uri", cleanURL.utf8().data(), nullptr);

    GST_INFO("Load %s", cleanURL.utf8().data());

    if (m_preload == MediaPlayer::None) {
        GST_DEBUG("Delaying load.");
        m_delayingLoad = true;
    }

    // Reset network and ready states. Those will be set properly once
    // the pipeline pre-rolled.
    m_networkState = MediaPlayer::Loading;
    m_player->networkStateChanged();
    m_readyState = MediaPlayer::HaveNothing;
    m_player->readyStateChanged();
    m_volumeAndMuteInitialized = false;
    m_durationAtEOS = 0;

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

#if ENABLE(MEDIA_STREAM)
void MediaPlayerPrivateGStreamer::load(MediaStreamPrivate&)
{
    notImplemented();
}
#endif

void MediaPlayerPrivateGStreamer::commitLoad()
{
    ASSERT(!m_delayingLoad);
    GST_DEBUG("Committing load.");

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

        float mediaDuration = duration();
        if (mediaDuration)
            return mediaDuration;
        return 0;
    }

    // Position is only available if no async state change is going on and the state is either paused or playing.
    gint64 position = GST_CLOCK_TIME_NONE;
    GstQuery* query= gst_query_new_position(GST_FORMAT_TIME);
    if (gst_element_query(m_pipeline.get(), query))
        gst_query_parse_position(query, 0, &position);
    gst_query_unref(query);

    GST_DEBUG("Position %" GST_TIME_FORMAT, GST_TIME_ARGS(position));

    float result = 0.0f;
    if (static_cast<GstClockTime>(position) != GST_CLOCK_TIME_NONE) {
        GTimeVal timeValue;
        GST_TIME_TO_TIMEVAL(position, timeValue);
        result = static_cast<float>(timeValue.tv_sec + (timeValue.tv_usec / 1000000.0));
    } else if (m_canFallBackToLastFinishedSeekPosition)
        result = m_seekTime;

    return result;
}

void MediaPlayerPrivateGStreamer::readyTimerFired()
{
    changePipelineState(GST_STATE_NULL);
}

bool MediaPlayerPrivateGStreamer::changePipelineState(GstState newState)
{
    ASSERT(m_pipeline);

    GstState currentState;
    GstState pending;

    gst_element_get_state(m_pipeline.get(), &currentState, &pending, 0);
    if (currentState == newState || pending == newState) {
        GST_DEBUG("Rejected state change to %s from %s with %s pending", gst_element_state_get_name(newState),
            gst_element_state_get_name(currentState), gst_element_state_get_name(pending));
        return true;
    }

    GST_DEBUG("Changing state change to %s from %s with %s pending", gst_element_state_get_name(newState),
        gst_element_state_get_name(currentState), gst_element_state_get_name(pending));

    GstStateChangeReturn setStateResult = gst_element_set_state(m_pipeline.get(), newState);
    GstState pausedOrPlaying = newState == GST_STATE_PLAYING ? GST_STATE_PAUSED : GST_STATE_PLAYING;
    if (currentState != pausedOrPlaying && setStateResult == GST_STATE_CHANGE_FAILURE) {
        return false;
    }

    // Create a timer when entering the READY state so that we can free resources
    // if we stay for too long on READY.
    // Also lets remove the timer if we request a state change for any state other than READY.
    // See also https://bugs.webkit.org/show_bug.cgi?id=117354
    if (newState == GST_STATE_READY && !m_readyTimerHandler.isActive()) {
        // Max interval in seconds to stay in the READY state on manual
        // state change requests.
        static const double readyStateTimerDelay = 60;
        m_readyTimerHandler.startOneShot(readyStateTimerDelay);
    } else if (newState != GST_STATE_READY)
        m_readyTimerHandler.stop();

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
    if (!m_playbackRate) {
        m_playbackRatePause = true;
        return;
    }

    if (changePipelineState(GST_STATE_PLAYING)) {
        m_isEndReached = false;
        m_delayingLoad = false;
        m_preload = MediaPlayer::Auto;
        setDownloadBuffering();
        GST_DEBUG("Play");
    } else {
        loadingFailed(MediaPlayer::Empty);
    }
}

void MediaPlayerPrivateGStreamer::pause()
{
    m_playbackRatePause = false;
    GstState currentState, pendingState;
    gst_element_get_state(m_pipeline.get(), &currentState, &pendingState, 0);
    if (currentState < GST_STATE_PAUSED && pendingState <= GST_STATE_PAUSED)
        return;

    if (changePipelineState(GST_STATE_PAUSED))
        GST_INFO("Pause");
    else
        loadingFailed(MediaPlayer::Empty);
}

float MediaPlayerPrivateGStreamer::duration() const
{
    if (!m_pipeline)
        return 0.0f;

    if (m_errorOccured)
        return 0.0f;

    if (m_durationAtEOS)
        return m_durationAtEOS;

    // The duration query would fail on a not-prerolled pipeline.
    if (GST_STATE(m_pipeline.get()) < GST_STATE_PAUSED)
        return 0.0f;

    GstFormat timeFormat = GST_FORMAT_TIME;
    gint64 timeLength = 0;

    bool failure = !gst_element_query_duration(m_pipeline.get(), timeFormat, &timeLength) || static_cast<guint64>(timeLength) == GST_CLOCK_TIME_NONE;
    if (failure) {
        GST_DEBUG("Time duration query failed for %s", m_url.string().utf8().data());
        return numeric_limits<float>::infinity();
    }

    GST_DEBUG("Duration: %" GST_TIME_FORMAT, GST_TIME_ARGS(timeLength));

    return static_cast<double>(timeLength) / GST_SECOND;
    // FIXME: handle 3.14.9.5 properly
}

float MediaPlayerPrivateGStreamer::currentTime() const
{
    if (!m_pipeline)
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
    if (!m_pipeline)
        return;

    if (m_errorOccured)
        return;

    GST_INFO("[Seek] seek attempt to %f secs", time);

    // Avoid useless seeking.
    if (time == currentTime())
        return;

    if (isLiveStream())
        return;

    GstClockTime clockTime = toGstClockTime(time);
    GST_INFO("[Seek] seeking to %" GST_TIME_FORMAT " (%f)", GST_TIME_ARGS(clockTime), time);

    if (m_seeking) {
        m_timeOfOverlappingSeek = time;
        if (m_seekIsPending) {
            m_seekTime = time;
            return;
        }
    }

    GstState state;
    GstStateChangeReturn getStateResult = gst_element_get_state(m_pipeline.get(), &state, nullptr, 0);
    if (getStateResult == GST_STATE_CHANGE_FAILURE || getStateResult == GST_STATE_CHANGE_NO_PREROLL) {
        GST_DEBUG("[Seek] cannot seek, current state change is %s", gst_element_state_change_return_get_name(getStateResult));
        return;
    }
    if (getStateResult == GST_STATE_CHANGE_ASYNC || state < GST_STATE_PAUSED || m_isEndReached) {
        m_seekIsPending = true;
        if (m_isEndReached) {
            GST_DEBUG("[Seek] reset pipeline");
            m_resetPipeline = true;
            if (!changePipelineState(GST_STATE_PAUSED))
                loadingFailed(MediaPlayer::Empty);
        }
    } else {
        // We can seek now.
        if (!doSeek(clockTime, m_player->rate(), static_cast<GstSeekFlags>(GST_SEEK_FLAG_FLUSH | GST_SEEK_FLAG_ACCURATE))) {
            GST_DEBUG("[Seek] seeking to %f failed", time);
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

    // TODO: Should do more than that, need to notify the media source
    // and probably flush the pipeline at least.
    if (isMediaSource())
        return true;

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

    if (!rate)
        rate = 1.0;

    return gst_element_seek(m_pipeline.get(), rate, GST_FORMAT_TIME, seekType,
        GST_SEEK_TYPE_SET, startTime, GST_SEEK_TYPE_SET, endTime);
}

void MediaPlayerPrivateGStreamer::updatePlaybackRate()
{
    if (!m_changingRate)
        return;

    float currentPosition = static_cast<float>(playbackPosition() * GST_SECOND);
    bool mute = false;

    GST_INFO("Set Rate to %f", m_playbackRate);

    if (m_playbackRate > 0) {
        // Mute the sound if the playback rate is too extreme and
        // audio pitch is not adjusted.
        mute = (!m_preservesPitch && (m_playbackRate < 0.8 || m_playbackRate > 2));
    } else {
        if (currentPosition == 0.0f)
            currentPosition = -1.0f;
        mute = true;
    }

    GST_INFO("Need to mute audio?: %d", (int) mute);
    if (doSeek(currentPosition, m_playbackRate, static_cast<GstSeekFlags>(GST_SEEK_FLAG_FLUSH))) {
        g_object_set(m_pipeline.get(), "mute", mute, nullptr);
        m_lastPlaybackRate = m_playbackRate;
    } else {
        m_playbackRate = m_lastPlaybackRate;
        GST_ERROR("Set rate to %f failed", m_playbackRate);
    }

    if (m_playbackRatePause) {
        GstState state;
        GstState pending;

        gst_element_get_state(m_pipeline.get(), &state, &pending, 0);
        if (state != GST_STATE_PLAYING && pending != GST_STATE_PLAYING)
            changePipelineState(GST_STATE_PLAYING);
        m_playbackRatePause = false;
    }

    m_changingRate = false;
    m_player->rateChanged();
}

bool MediaPlayerPrivateGStreamer::paused() const
{
    if (m_isEndReached) {
        GST_DEBUG("Ignoring pause at EOS");
        return true;
    }

    if (m_playbackRatePause)
        return false;

    GstState state;
    gst_element_get_state(m_pipeline.get(), &state, nullptr, 0);
    return state == GST_STATE_PAUSED;
}

bool MediaPlayerPrivateGStreamer::seeking() const
{
    return m_seeking;
}

void MediaPlayerPrivateGStreamer::videoChangedCallback(MediaPlayerPrivateGStreamer* player)
{
    player->m_notifier.notify(MainThreadNotification::VideoChanged, [player] { player->notifyPlayerOfVideo(); });
}

void MediaPlayerPrivateGStreamer::notifyPlayerOfVideo()
{
    gint numTracks = 0;
    if (m_pipeline)
        g_object_get(m_pipeline.get(), "n-video", &numTracks, nullptr);

    m_hasVideo = numTracks > 0;

#if ENABLE(VIDEO_TRACK)
    for (gint i = 0; i < numTracks; ++i) {
        GRefPtr<GstPad> pad;
        g_signal_emit_by_name(m_pipeline.get(), "get-video-pad", i, &pad.outPtr(), nullptr);
        ASSERT(pad);

        if (i < static_cast<gint>(m_videoTracks.size())) {
            RefPtr<VideoTrackPrivateGStreamer> existingTrack = m_videoTracks[i];
            existingTrack->setIndex(i);
            if (existingTrack->pad() == pad)
                continue;
        }

        RefPtr<VideoTrackPrivateGStreamer> track = VideoTrackPrivateGStreamer::create(m_pipeline, i, pad);
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

    m_player->client().mediaPlayerEngineUpdated(m_player);
}

void MediaPlayerPrivateGStreamer::videoSinkCapsChangedCallback(MediaPlayerPrivateGStreamer* player)
{
    player->m_notifier.notify(MainThreadNotification::VideoCapsChanged, [player] { player->notifyPlayerOfVideoCaps(); });
}

void MediaPlayerPrivateGStreamer::notifyPlayerOfVideoCaps()
{
    m_videoSize = IntSize();
    m_player->client().mediaPlayerEngineUpdated(m_player);
}

void MediaPlayerPrivateGStreamer::audioChangedCallback(MediaPlayerPrivateGStreamer* player)
{
    player->m_notifier.notify(MainThreadNotification::AudioChanged, [player] { player->notifyPlayerOfAudio(); });
}

void MediaPlayerPrivateGStreamer::notifyPlayerOfAudio()
{
    gint numTracks = 0;
    if (m_pipeline)
        g_object_get(m_pipeline.get(), "n-audio", &numTracks, nullptr);

    m_hasAudio = numTracks > 0;

#if ENABLE(VIDEO_TRACK)
    for (gint i = 0; i < numTracks; ++i) {
        GRefPtr<GstPad> pad;
        g_signal_emit_by_name(m_pipeline.get(), "get-audio-pad", i, &pad.outPtr(), nullptr);
        ASSERT(pad);

        if (i < static_cast<gint>(m_audioTracks.size())) {
            RefPtr<AudioTrackPrivateGStreamer> existingTrack = m_audioTracks[i];
            existingTrack->setIndex(i);
            if (existingTrack->pad() == pad)
                continue;
        }

        RefPtr<AudioTrackPrivateGStreamer> track = AudioTrackPrivateGStreamer::create(m_pipeline, i, pad);
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

    m_player->client().mediaPlayerEngineUpdated(m_player);
}

#if ENABLE(VIDEO_TRACK)
void MediaPlayerPrivateGStreamer::textChangedCallback(MediaPlayerPrivateGStreamer* player)
{
    player->m_notifier.notify(MainThreadNotification::TextChanged, [player] { player->notifyPlayerOfText(); });
}

void MediaPlayerPrivateGStreamer::notifyPlayerOfText()
{
    gint numTracks = 0;
    if (m_pipeline)
        g_object_get(m_pipeline.get(), "n-text", &numTracks, nullptr);

    for (gint i = 0; i < numTracks; ++i) {
        GRefPtr<GstPad> pad;
        g_signal_emit_by_name(m_pipeline.get(), "get-text-pad", i, &pad.outPtr(), nullptr);
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

GstFlowReturn MediaPlayerPrivateGStreamer::newTextSampleCallback(MediaPlayerPrivateGStreamer* player)
{
    player->newTextSample();
    return GST_FLOW_OK;
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
            GST_WARNING("Got sample with unknown stream ID.");
    } else
        GST_WARNING("Unable to handle sample with no stream start event.");
}
#endif

void MediaPlayerPrivateGStreamer::setRate(float rate)
{
    // Higher rate causes crash.
    rate = clampTo(rate, -20.0, 20.0);

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

    gst_element_get_state(m_pipeline.get(), &state, &pending, 0);

    if (!rate) {
        m_changingRate = false;
        m_playbackRatePause = true;
        if (state != GST_STATE_PAUSED && pending != GST_STATE_PAUSED)
            changePipelineState(GST_STATE_PAUSED);
        return;
    }

    if ((state != GST_STATE_PLAYING && state != GST_STATE_PAUSED)
        || (pending == GST_STATE_PAUSED))
        return;

    updatePlaybackRate();
}

double MediaPlayerPrivateGStreamer::rate() const
{
    return m_playbackRate;
}

void MediaPlayerPrivateGStreamer::setPreservesPitch(bool preservesPitch)
{
    m_preservesPitch = preservesPitch;
}

std::unique_ptr<PlatformTimeRanges> MediaPlayerPrivateGStreamer::buffered() const
{
    auto timeRanges = std::make_unique<PlatformTimeRanges>();
    if (m_errorOccured || isLiveStream())
        return timeRanges;

    float mediaDuration(duration());
    if (!mediaDuration || std::isinf(mediaDuration))
        return timeRanges;

    GstQuery* query = gst_query_new_buffering(GST_FORMAT_PERCENT);

    if (!gst_element_query(m_pipeline.get(), query)) {
        gst_query_unref(query);
        return timeRanges;
    }

    guint numBufferingRanges = gst_query_get_n_buffering_ranges(query);
    for (guint index = 0; index < numBufferingRanges; index++) {
        gint64 rangeStart = 0, rangeStop = 0;
        if (gst_query_parse_nth_buffering_range(query, index, &rangeStart, &rangeStop))
            timeRanges->add(MediaTime::createWithDouble((rangeStart * mediaDuration) / GST_FORMAT_PERCENT_MAX),
                MediaTime::createWithDouble((rangeStop * mediaDuration) / GST_FORMAT_PERCENT_MAX));
    }

    // Fallback to the more general maxTimeLoaded() if no range has
    // been found.
    if (!timeRanges->length())
        if (float loaded = maxTimeLoaded())
            timeRanges->add(MediaTime::zeroTime(), MediaTime::createWithDouble(loaded));

    gst_query_unref(query);

    return timeRanges;
}

void MediaPlayerPrivateGStreamer::handleMessage(GstMessage* message)
{
    GUniqueOutPtr<GError> err;
    GUniqueOutPtr<gchar> debug;
    MediaPlayer::NetworkState error;
    bool issueError = true;
    bool attemptNextLocation = false;
    const GstStructure* structure = gst_message_get_structure(message);
    GstState requestedState, currentState;

    m_canFallBackToLastFinishedSeekPosition = false;

    if (structure) {
        const gchar* messageTypeName = gst_structure_get_name(structure);

        // Redirect messages are sent from elements, like qtdemux, to
        // notify of the new location(s) of the media.
        if (!g_strcmp0(messageTypeName, "redirect")) {
            mediaLocationChanged(message);
            return;
        }
    }

    // We ignore state changes from internal elements. They are forwarded to playbin2 anyway.
    bool messageSourceIsPlaybin = GST_MESSAGE_SRC(message) == reinterpret_cast<GstObject*>(m_pipeline.get());

    GST_DEBUG("Message %s received from element %s", GST_MESSAGE_TYPE_NAME(message), GST_MESSAGE_SRC_NAME(message));
    switch (GST_MESSAGE_TYPE(message)) {
    case GST_MESSAGE_ERROR:
        if (m_resetPipeline)
            break;
        if (m_missingPluginsCallback)
            break;
        gst_message_parse_error(message, &err.outPtr(), &debug.outPtr());
        GST_ERROR("Error %d: %s (url=%s)", err->code, err->message, m_url.string().utf8().data());

        GST_DEBUG_BIN_TO_DOT_FILE_WITH_TS(GST_BIN(m_pipeline.get()), GST_DEBUG_GRAPH_SHOW_ALL, "webkit-video.error");

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
                GST_ERROR("Decode error, let the Media element emit a stalled event.");
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
        GST_DEBUG_BIN_TO_DOT_FILE_WITH_TS(GST_BIN(m_pipeline.get()), GST_DEBUG_GRAPH_SHOW_ALL, dotFileName.data());

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
        gst_element_get_state(m_pipeline.get(), &currentState, nullptr, 250 * GST_NSECOND);
        if (requestedState < currentState) {
            GUniquePtr<gchar> elementName(gst_element_get_name(GST_ELEMENT(message)));
            GST_INFO("Element %s requested state change to %s", elementName.get(),
                gst_element_state_get_name(requestedState));
            m_requestedState = requestedState;
            if (!changePipelineState(requestedState))
                loadingFailed(MediaPlayer::Empty);
        }
        break;
    case GST_MESSAGE_CLOCK_LOST:
        // This can only happen in PLAYING state and we should just
        // get a new clock by moving back to PAUSED and then to
        // PLAYING again.
        // This can happen if the stream that ends in a sink that
        // provides the current clock disappears, for example if
        // the audio sink provides the clock and the audio stream
        // is disabled. It also happens relatively often with
        // HTTP adaptive streams when switching between different
        // variants of a stream.
        gst_element_set_state(m_pipeline.get(), GST_STATE_PAUSED);
        gst_element_set_state(m_pipeline.get(), GST_STATE_PLAYING);
        break;
    case GST_MESSAGE_LATENCY:
        // Recalculate the latency, we don't need any special handling
        // here other than the GStreamer default.
        // This can happen if the latency of live elements changes, or
        // for one reason or another a new live element is added or
        // removed from the pipeline.
        gst_bin_recalculate_latency(GST_BIN(m_pipeline.get()));
        break;
    case GST_MESSAGE_ELEMENT:
        if (gst_is_missing_plugin_message(message)) {
            if (gst_install_plugins_supported()) {
                m_missingPluginsCallback = MediaPlayerRequestInstallMissingPluginsCallback::create([this](uint32_t result) {
                    m_missingPluginsCallback = nullptr;
                    if (result != GST_INSTALL_PLUGINS_SUCCESS)
                        return;

                    changePipelineState(GST_STATE_READY);
                    changePipelineState(GST_STATE_PAUSED);
                });
                GUniquePtr<char> detail(gst_missing_plugin_message_get_installer_detail(message));
                GUniquePtr<char> description(gst_missing_plugin_message_get_description(message));
                m_player->client().requestInstallMissingPlugins(String::fromUTF8(detail.get()), String::fromUTF8(description.get()), *m_missingPluginsCallback);
            }
        }
#if ENABLE(VIDEO_TRACK) && USE(GSTREAMER_MPEGTS)
        else {
            GstMpegtsSection* section = gst_message_parse_mpegts_section(message);
            if (section) {
                processMpegTsSection(section);
                gst_mpegts_section_unref(section);
            }
        }
#endif
        break;
#if ENABLE(VIDEO_TRACK)
    case GST_MESSAGE_TOC:
        processTableOfContents(message);
        break;
#endif
    case GST_MESSAGE_TAG: {
        GstTagList* tags = nullptr;
        GUniqueOutPtr<gchar> tag;
        gst_message_parse_tag(message, &tags);
        if (gst_tag_list_get_string(tags, GST_TAG_IMAGE_ORIENTATION, &tag.outPtr())) {
            if (!g_strcmp0(tag.get(), "rotate-90"))
                setVideoSourceOrientation(ImageOrientation(OriginRightTop));
            else if (!g_strcmp0(tag.get(), "rotate-180"))
                setVideoSourceOrientation(ImageOrientation(OriginBottomRight));
            else if (!g_strcmp0(tag.get(), "rotate-270"))
                setVideoSourceOrientation(ImageOrientation(OriginLeftBottom));
        }
        gst_tag_list_unref(tags);
        break;
    }
    default:
        GST_DEBUG("Unhandled GStreamer message type: %s",
                    GST_MESSAGE_TYPE_NAME(message));
        break;
    }
    return;
}

void MediaPlayerPrivateGStreamer::processBufferingStats(GstMessage* message)
{
    m_buffering = true;
    gst_message_parse_buffering(message, &m_bufferingPercentage);

    GST_DEBUG("[Buffering] Buffering: %d%%.", m_bufferingPercentage);

    updateStates();
}

#if ENABLE(VIDEO_TRACK) && USE(GSTREAMER_MPEGTS)
void MediaPlayerPrivateGStreamer::processMpegTsSection(GstMpegtsSection* section)
{
    ASSERT(section);

    if (section->section_type == GST_MPEGTS_SECTION_PMT) {
        const GstMpegtsPMT* pmt = gst_mpegts_section_get_pmt(section);
        m_metadataTracks.clear();
        for (guint i = 0; i < pmt->streams->len; ++i) {
            const GstMpegtsPMTStream* stream = static_cast<const GstMpegtsPMTStream*>(g_ptr_array_index(pmt->streams, i));
            if (stream->stream_type == 0x05 || stream->stream_type >= 0x80) {
                AtomicString pid = String::number(stream->pid);
                RefPtr<InbandMetadataTextTrackPrivateGStreamer> track = InbandMetadataTextTrackPrivateGStreamer::create(
                    InbandTextTrackPrivate::Metadata, InbandTextTrackPrivate::Data, pid);

                // 4.7.10.12.2 Sourcing in-band text tracks
                // If the new text track's kind is metadata, then set the text track in-band metadata track dispatch
                // type as follows, based on the type of the media resource:
                // Let stream type be the value of the "stream_type" field describing the text track's type in the
                // file's program map section, interpreted as an 8-bit unsigned integer. Let length be the value of
                // the "ES_info_length" field for the track in the same part of the program map section, interpreted
                // as an integer as defined by the MPEG-2 specification. Let descriptor bytes be the length bytes
                // following the "ES_info_length" field. The text track in-band metadata track dispatch type must be
                // set to the concatenation of the stream type byte and the zero or more descriptor bytes bytes,
                // expressed in hexadecimal using uppercase ASCII hex digits.
                String inbandMetadataTrackDispatchType;
                appendUnsignedAsHexFixedSize(stream->stream_type, inbandMetadataTrackDispatchType, 2);
                for (guint j = 0; j < stream->descriptors->len; ++j) {
                    const GstMpegtsDescriptor* descriptor = static_cast<const GstMpegtsDescriptor*>(g_ptr_array_index(stream->descriptors, j));
                    for (guint k = 0; k < descriptor->length; ++k)
                        appendByteAsHex(descriptor->data[k], inbandMetadataTrackDispatchType);
                }
                track->setInBandMetadataTrackDispatchType(inbandMetadataTrackDispatchType);

                m_metadataTracks.add(pid, track);
                m_player->addTextTrack(track);
            }
        }
    } else {
        AtomicString pid = String::number(section->pid);
        RefPtr<InbandMetadataTextTrackPrivateGStreamer> track = m_metadataTracks.get(pid);
        if (!track)
            return;

        GRefPtr<GBytes> data = gst_mpegts_section_get_data(section);
        gsize size;
        const void* bytes = g_bytes_get_data(data.get(), &size);

        track->addDataCue(MediaTime::createWithDouble(currentTimeDouble()), MediaTime::createWithDouble(currentTimeDouble()), bytes, size);
    }
}
#endif

#if ENABLE(VIDEO_TRACK)
void MediaPlayerPrivateGStreamer::processTableOfContents(GstMessage* message)
{
    if (m_chaptersTrack)
        m_player->removeTextTrack(m_chaptersTrack);

    m_chaptersTrack = InbandMetadataTextTrackPrivateGStreamer::create(InbandTextTrackPrivate::Chapters, InbandTextTrackPrivate::Generic);
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
        cue->setStartTime(MediaTime(start, GST_SECOND));
    if (stop != -1)
        cue->setEndTime(MediaTime(stop, GST_SECOND));

    GstTagList* tags = gst_toc_entry_get_tags(entry);
    if (tags) {
        gchar* title =  0;
        gst_tag_list_get_string(tags, GST_TAG_TITLE, &title);
        if (title) {
            cue->setContent(title);
            g_free(title);
        }
    }

    m_chaptersTrack->addGenericCue(cue.release());

    for (GList* i = gst_toc_entry_get_sub_entries(entry); i; i = i->next)
        processTableOfContentsEntry(static_cast<GstTocEntry*>(i->data), entry);
}
#endif

void MediaPlayerPrivateGStreamer::fillTimerFired()
{
    GstQuery* query = gst_query_new_buffering(GST_FORMAT_PERCENT);

    if (!gst_element_query(m_pipeline.get(), query)) {
        gst_query_unref(query);
        return;
    }

    gint64 start, stop;
    gdouble fillStatus = 100.0;

    gst_query_parse_buffering_range(query, 0, &start, &stop, 0);
    gst_query_unref(query);

    if (stop != -1)
        fillStatus = 100.0 * stop / GST_FORMAT_PERCENT_MAX;

    GST_DEBUG("[Buffering] Download buffer filled up to %f%%", fillStatus);

    float mediaDuration = duration();

    // Update maxTimeLoaded only if the media duration is
    // available. Otherwise we can't compute it.
    if (mediaDuration) {
        if (fillStatus == 100.0)
            m_maxTimeLoaded = mediaDuration;
        else
            m_maxTimeLoaded = static_cast<float>((fillStatus * mediaDuration) / 100.0);
        GST_DEBUG("[Buffering] Updated maxTimeLoaded: %f", m_maxTimeLoaded);
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

    float mediaDuration = duration();
    GST_DEBUG("maxTimeSeekable, duration: %f", mediaDuration);
    // infinite duration means live stream
    if (std::isinf(mediaDuration))
        return 0.0f;

    return mediaDuration;
}

float MediaPlayerPrivateGStreamer::maxTimeLoaded() const
{
    if (m_errorOccured)
        return 0.0f;

    float loaded = m_maxTimeLoaded;
    if (m_isEndReached)
        loaded = duration();
    GST_DEBUG("maxTimeLoaded: %f", loaded);
    return loaded;
}

bool MediaPlayerPrivateGStreamer::didLoadingProgress() const
{
    if (!m_pipeline || !duration() || (!isMediaSource() && !totalBytes()))
        return false;
    float currentMaxTimeLoaded = maxTimeLoaded();
    bool didLoadingProgress = currentMaxTimeLoaded != m_maxTimeLoadedAtLastDidLoadingProgress;
    m_maxTimeLoadedAtLastDidLoadingProgress = currentMaxTimeLoaded;
    GST_DEBUG("didLoadingProgress: %d", didLoadingProgress);
    return didLoadingProgress;
}

unsigned long long MediaPlayerPrivateGStreamer::totalBytes() const
{
    if (m_errorOccured)
        return 0;

    if (m_totalBytes)
        return m_totalBytes;

    if (!m_source)
        return 0;

    GstFormat fmt = GST_FORMAT_BYTES;
    gint64 length = 0;
    if (gst_element_query_duration(m_source.get(), fmt, &length)) {
        GST_INFO("totalBytes %" G_GINT64_FORMAT, length);
        m_totalBytes = static_cast<unsigned long long>(length);
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

    GST_INFO("totalBytes %" G_GINT64_FORMAT, length);
    m_totalBytes = static_cast<unsigned long long>(length);
    m_isStreaming = !length;
    return m_totalBytes;
}

void MediaPlayerPrivateGStreamer::sourceChangedCallback(MediaPlayerPrivateGStreamer* player)
{
    player->sourceChanged();
}

void MediaPlayerPrivateGStreamer::sourceChanged()
{
    m_source.clear();
    g_object_get(m_pipeline.get(), "source", &m_source.outPtr(), nullptr);

    if (WEBKIT_IS_WEB_SRC(m_source.get()))
        webKitWebSrcSetMediaPlayer(WEBKIT_WEB_SRC(m_source.get()), m_player);
#if ENABLE(MEDIA_SOURCE)
    if (m_mediaSource && WEBKIT_IS_MEDIA_SRC(m_source.get())) {
        MediaSourceGStreamer::open(m_mediaSource.get(), WEBKIT_MEDIA_SRC(m_source.get()));
    }
#endif
}

void MediaPlayerPrivateGStreamer::cancelLoad()
{
    if (m_networkState < MediaPlayer::Loading || m_networkState == MediaPlayer::Loaded)
        return;

    if (m_pipeline)
        changePipelineState(GST_STATE_READY);
}

void MediaPlayerPrivateGStreamer::asyncStateChangeDone()
{
    if (!m_pipeline || m_errorOccured)
        return;

    if (m_seeking) {
        if (m_seekIsPending)
            updateStates();
        else {
            GST_DEBUG("[Seek] seeked to %f", m_seekTime);
            m_seeking = false;
            if (m_timeOfOverlappingSeek != m_seekTime && m_timeOfOverlappingSeek != -1) {
                seek(m_timeOfOverlappingSeek);
                m_timeOfOverlappingSeek = -1;
                return;
            }
            m_timeOfOverlappingSeek = -1;

            // The pipeline can still have a pending state. In this case a position query will fail.
            // Right now we can use m_seekTime as a fallback.
            m_canFallBackToLastFinishedSeekPosition = true;
            timeChanged();
        }
    } else
        updateStates();
}

void MediaPlayerPrivateGStreamer::updateStates()
{
    if (!m_pipeline)
        return;

    if (m_errorOccured)
        return;

    MediaPlayer::NetworkState oldNetworkState = m_networkState;
    MediaPlayer::ReadyState oldReadyState = m_readyState;
    GstState state;
    GstState pending;

    GstStateChangeReturn getStateResult = gst_element_get_state(m_pipeline.get(), &state, &pending, 250 * GST_NSECOND);

    bool shouldUpdatePlaybackState = false;
    switch (getStateResult) {
    case GST_STATE_CHANGE_SUCCESS: {
        GST_DEBUG("State: %s, pending: %s", gst_element_state_get_name(state), gst_element_state_get_name(pending));

        // Do nothing if on EOS and state changed to READY to avoid recreating the player
        // on HTMLMediaElement and properly generate the video 'ended' event.
        if (m_isEndReached && state == GST_STATE_READY)
            break;

        m_resetPipeline = state <= GST_STATE_READY;

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
                    GST_DEBUG("[Buffering] Complete.");
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

            if (didBuffering && !m_buffering && !m_paused && m_playbackRate) {
                GST_DEBUG("[Buffering] Restarting playback.");
                changePipelineState(GST_STATE_PLAYING);
            }
        } else if (state == GST_STATE_PLAYING) {
            m_paused = false;

            if ((m_buffering && !isLiveStream()) || !m_playbackRate) {
                GST_DEBUG("[Buffering] Pausing stream for buffering.");
                changePipelineState(GST_STATE_PAUSED);
            }
        } else
            m_paused = true;

        if (m_requestedState == GST_STATE_PAUSED && state == GST_STATE_PAUSED) {
            shouldUpdatePlaybackState = true;
            GST_DEBUG("Requested state change to %s was completed", gst_element_state_get_name(state));
        }

        break;
    }
    case GST_STATE_CHANGE_ASYNC:
        GST_DEBUG("Async: State: %s, pending: %s", gst_element_state_get_name(state), gst_element_state_get_name(pending));
        // Change in progress.
        break;
    case GST_STATE_CHANGE_FAILURE:
        GST_DEBUG("Failure: State: %s, pending: %s", gst_element_state_get_name(state), gst_element_state_get_name(pending));
        // Change failed
        return;
    case GST_STATE_CHANGE_NO_PREROLL:
        GST_DEBUG("No preroll: State: %s, pending: %s", gst_element_state_get_name(state), gst_element_state_get_name(pending));

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

        if (!m_paused && m_playbackRate)
            changePipelineState(GST_STATE_PLAYING);

        m_networkState = MediaPlayer::Loading;
        break;
    default:
        GST_DEBUG("Else : %d", getStateResult);
        break;
    }

    m_requestedState = GST_STATE_VOID_PENDING;

    if (shouldUpdatePlaybackState)
        m_player->playbackStateChanged();

    if (m_networkState != oldNetworkState) {
        GST_DEBUG("Network State Changed from %u to %u", oldNetworkState, m_networkState);
        m_player->networkStateChanged();
    }
    if (m_readyState != oldReadyState) {
        GST_DEBUG("Ready State Changed from %u to %u", oldReadyState, m_readyState);
        m_player->readyStateChanged();
    }

    if (getStateResult == GST_STATE_CHANGE_SUCCESS && state >= GST_STATE_PAUSED) {
        updatePlaybackRate();
        if (m_seekIsPending) {
            GST_DEBUG("[Seek] committing pending seek to %f", m_seekTime);
            m_seekIsPending = false;
            m_seeking = doSeek(toGstClockTime(m_seekTime), m_player->rate(), static_cast<GstSeekFlags>(GST_SEEK_FLAG_FLUSH | GST_SEEK_FLAG_ACCURATE));
            if (!m_seeking)
                GST_DEBUG("[Seek] seeking to %f failed", m_seekTime);
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
            GST_INFO("New media url: %s", newUrl.string().utf8().data());

            // Reset player states.
            m_networkState = MediaPlayer::Loading;
            m_player->networkStateChanged();
            m_readyState = MediaPlayer::HaveNothing;
            m_player->readyStateChanged();

            // Reset pipeline state.
            m_resetPipeline = true;
            changePipelineState(GST_STATE_READY);

            GstState state;
            gst_element_get_state(m_pipeline.get(), &state, nullptr, 0);
            if (state <= GST_STATE_READY) {
                // Set the new uri and start playing.
                g_object_set(m_pipeline.get(), "uri", newUrl.string().utf8().data(), nullptr);
                m_url = newUrl;
                changePipelineState(GST_STATE_PLAYING);
                return true;
            }
        } else
            GST_INFO("Not allowed to load new media location: %s", newUrl.string().utf8().data());
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
    if (now > 0 && now <= duration())
        m_player->durationChanged();

    m_isEndReached = true;
    timeChanged();

    if (!m_player->client().mediaPlayerIsLooping()) {
        m_paused = true;
        m_durationAtEOS = duration();
        changePipelineState(GST_STATE_READY);
        m_downloadFinished = false;
    }
}

void MediaPlayerPrivateGStreamer::durationChanged()
{
    float previousDuration = duration();

    // Avoid emiting durationchanged in the case where the previous
    // duration was 0 because that case is already handled by the
    // HTMLMediaElement.
    if (previousDuration && duration() != previousDuration)
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
    m_readyTimerHandler.stop();
}

static HashSet<String, ASCIICaseInsensitiveHash>& mimeTypeSet()
{
    static NeverDestroyed<HashSet<String, ASCIICaseInsensitiveHash>> mimeTypes = []()
    {
        initializeGStreamerAndRegisterWebKitElements();
        HashSet<String, ASCIICaseInsensitiveHash> set;

        GList* audioDecoderFactories = gst_element_factory_list_get_elements(GST_ELEMENT_FACTORY_TYPE_DECODER | GST_ELEMENT_FACTORY_TYPE_MEDIA_AUDIO, GST_RANK_MARGINAL);
        GList* videoDecoderFactories = gst_element_factory_list_get_elements(GST_ELEMENT_FACTORY_TYPE_DECODER | GST_ELEMENT_FACTORY_TYPE_MEDIA_VIDEO, GST_RANK_MARGINAL);
        GList* demuxerFactories = gst_element_factory_list_get_elements(GST_ELEMENT_FACTORY_TYPE_DEMUXER, GST_RANK_MARGINAL);

        enum ElementType {
            AudioDecoder = 0,
            VideoDecoder,
            Demuxer
        };
        struct GstCapsWebKitMapping {
            ElementType elementType;
            const char* capsString;
            Vector<AtomicString> webkitMimeTypes;
        };

        Vector<GstCapsWebKitMapping> mapping = {
            {AudioDecoder, "audio/midi", {"audio/midi", "audio/riff-midi"}},
            {AudioDecoder, "audio/x-sbc", { }},
            {AudioDecoder, "audio/x-sid", { }},
            {AudioDecoder, "audio/x-flac", {"audio/x-flac", "audio/flac"}},
            {AudioDecoder, "audio/x-wav", {"audio/x-wav", "audio/wav"}},
            {AudioDecoder, "audio/x-wavpack", {"audio/x-wavpack"}},
            {AudioDecoder, "audio/x-speex", {"audio/speex", "audio/x-speex"}},
            {AudioDecoder, "audio/x-ac3", { }},
            {AudioDecoder, "audio/x-eac3", {"audio/x-ac3"}},
            {AudioDecoder, "audio/x-dts", { }},
            {VideoDecoder, "video/x-h264, profile=(string)high", {"video/mp4", "video/x-m4v"}},
            {VideoDecoder, "video/x-msvideocodec", {"video/x-msvideo"}},
            {VideoDecoder, "video/x-h263", { }},
            {VideoDecoder, "video/mpegts", { }},
            {VideoDecoder, "video/mpeg, mpegversion=(int){1,2}, systemstream=(boolean)false", {"video/mpeg"}},
            {VideoDecoder, "video/x-dirac", { }},
            {VideoDecoder, "video/x-flash-video", {"video/flv", "video/x-flv"}},
            {Demuxer, "video/quicktime", { }},
            {Demuxer, "video/quicktime, variant=(string)3gpp", {"video/3gpp"}},
            {Demuxer, "application/x-3gp", { }},
            {Demuxer, "video/x-ms-asf", { }},
            {Demuxer, "audio/x-aiff", { }},
            {Demuxer, "application/x-pn-realaudio", { }},
            {Demuxer, "application/vnd.rn-realmedia", { }},
            {Demuxer, "audio/x-wav", {"audio/x-wav", "audio/wav"}},
            {Demuxer, "application/x-hls", {"application/vnd.apple.mpegurl", "application/x-mpegurl"}}
        };

        for (auto& current : mapping) {
            GList* factories = demuxerFactories;
            if (current.elementType == AudioDecoder)
                factories = audioDecoderFactories;
            else if (current.elementType == VideoDecoder)
                factories = videoDecoderFactories;

            if (gstRegistryHasElementForMediaType(factories, current.capsString)) {
                if (!current.webkitMimeTypes.isEmpty()) {
                    for (const auto& mimeType : current.webkitMimeTypes)
                        set.add(mimeType);
                } else
                    set.add(AtomicString(current.capsString));
            }
        }

        bool opusSupported = false;
        if (gstRegistryHasElementForMediaType(audioDecoderFactories, "audio/x-opus")) {
            opusSupported = true;
            set.add(AtomicString("audio/opus"));
        }

        bool vorbisSupported = false;
        if (gstRegistryHasElementForMediaType(demuxerFactories, "application/ogg")) {
            set.add(AtomicString("application/ogg"));

            vorbisSupported = gstRegistryHasElementForMediaType(audioDecoderFactories, "audio/x-vorbis");
            if (vorbisSupported) {
                set.add(AtomicString("audio/ogg"));
                set.add(AtomicString("audio/x-vorbis+ogg"));
            }

            if (gstRegistryHasElementForMediaType(videoDecoderFactories, "video/x-theora"))
                set.add(AtomicString("video/ogg"));
        }

        bool audioMpegSupported = false;
        if (gstRegistryHasElementForMediaType(audioDecoderFactories, "audio/mpeg, mpegversion=(int)1, layer=(int)[1, 3]")) {
            audioMpegSupported = true;
            set.add(AtomicString("audio/mp1"));
            set.add(AtomicString("audio/mp3"));
            set.add(AtomicString("audio/x-mp3"));
        }

        if (gstRegistryHasElementForMediaType(audioDecoderFactories, "audio/mpeg, mpegversion=(int){2, 4}")) {
            audioMpegSupported = true;
            set.add(AtomicString("audio/aac"));
            set.add(AtomicString("audio/mp2"));
            set.add(AtomicString("audio/mp4"));
            set.add(AtomicString("audio/x-m4a"));
        }

        if (audioMpegSupported) {
            set.add(AtomicString("audio/mpeg"));
            set.add(AtomicString("audio/x-mpeg"));
        }

        if (gstRegistryHasElementForMediaType(demuxerFactories, "video/x-matroska")) {
            set.add(AtomicString("video/x-matroska"));

            if (gstRegistryHasElementForMediaType(videoDecoderFactories, "video/x-vp8")
                || gstRegistryHasElementForMediaType(videoDecoderFactories, "video/x-vp9")
                || gstRegistryHasElementForMediaType(videoDecoderFactories, "video/x-vp10"))
                set.add(AtomicString("video/webm"));

            if (vorbisSupported || opusSupported)
                set.add(AtomicString("audio/webm"));
        }

        gst_plugin_feature_list_free(audioDecoderFactories);
        gst_plugin_feature_list_free(videoDecoderFactories);
        gst_plugin_feature_list_free(demuxerFactories);
        return set;
    }();
    return mimeTypes;
}

void MediaPlayerPrivateGStreamer::getSupportedTypes(HashSet<String, ASCIICaseInsensitiveHash>& types)
{
    types = mimeTypeSet();
}

MediaPlayer::SupportsType MediaPlayerPrivateGStreamer::supportsType(const MediaEngineSupportParameters& parameters)
{
    // MediaStream playback is handled by the OpenWebRTC player.
    if (parameters.isMediaStream)
        return MediaPlayer::IsNotSupported;


    if (parameters.type.isNull() || parameters.type.isEmpty())
        return MediaPlayer::IsNotSupported;

    // spec says we should not return "probably" if the codecs string is empty
    if (mimeTypeSet().contains(parameters.type))
        return parameters.codecs.isEmpty() ? MediaPlayer::MayBeSupported : MediaPlayer::IsSupported;
    return MediaPlayer::IsNotSupported;
}

void MediaPlayerPrivateGStreamer::setDownloadBuffering()
{
    if (!m_pipeline)
        return;

    unsigned flags;
    g_object_get(m_pipeline.get(), "flags", &flags, nullptr);

    unsigned flagDownload = getGstPlayFlag("download");

    // We don't want to stop downloading if we already started it.
    if (flags & flagDownload && m_readyState > MediaPlayer::HaveNothing && !m_resetPipeline)
        return;

    bool shouldDownload = !isLiveStream() && m_preload == MediaPlayer::Auto;
    if (shouldDownload) {
        GST_DEBUG("Enabling on-disk buffering");
        g_object_set(m_pipeline.get(), "flags", flags | flagDownload, nullptr);
        m_fillTimer.startRepeating(0.2);
    } else {
        GST_DEBUG("Disabling on-disk buffering");
        g_object_set(m_pipeline.get(), "flags", flags & ~flagDownload, nullptr);
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
    if (!m_autoAudioSink) {
        GST_WARNING("GStreamer's autoaudiosink not found. Please check your gst-plugins-good installation");
        return nullptr;
    }

    g_signal_connect_swapped(m_autoAudioSink.get(), "child-added", G_CALLBACK(setAudioStreamPropertiesCallback), this);

    GstElement* audioSinkBin;

    if (webkitGstCheckVersion(1, 4, 2)) {
#if ENABLE(WEB_AUDIO)
        audioSinkBin = gst_bin_new("audio-sink");
        m_audioSourceProvider->configureAudioBin(audioSinkBin, nullptr);
        return audioSinkBin;
#else
        return m_autoAudioSink.get();
#endif
    }

    // Construct audio sink only if pitch preserving is enabled.
    // If GStreamer 1.4.2 is used the audio-filter playbin property is used instead.
    if (m_preservesPitch) {
        GstElement* scale = gst_element_factory_make("scaletempo", nullptr);
        if (!scale) {
            GST_WARNING("Failed to create scaletempo");
            return m_autoAudioSink.get();
        }

        audioSinkBin = gst_bin_new("audio-sink");
        gst_bin_add(GST_BIN(audioSinkBin), scale);
        GRefPtr<GstPad> pad = adoptGRef(gst_element_get_static_pad(scale, "sink"));
        gst_element_add_pad(audioSinkBin, gst_ghost_pad_new("sink", pad.get()));

#if ENABLE(WEB_AUDIO)
        m_audioSourceProvider->configureAudioBin(audioSinkBin, scale);
#else
        GstElement* convert = gst_element_factory_make("audioconvert", nullptr);
        GstElement* resample = gst_element_factory_make("audioresample", nullptr);

        gst_bin_add_many(GST_BIN(audioSinkBin), convert, resample, m_autoAudioSink.get(), nullptr);

        if (!gst_element_link_many(scale, convert, resample, m_autoAudioSink.get(), nullptr)) {
            GST_WARNING("Failed to link audio sink elements");
            gst_object_unref(audioSinkBin);
            return m_autoAudioSink.get();
        }
#endif
        return audioSinkBin;
    }

#if ENABLE(WEB_AUDIO)
    audioSinkBin = gst_bin_new("audio-sink");
    m_audioSourceProvider->configureAudioBin(audioSinkBin, nullptr);
    return audioSinkBin;
#endif
    ASSERT_NOT_REACHED();
    return nullptr;
}

GstElement* MediaPlayerPrivateGStreamer::audioSink() const
{
    GstElement* sink;
    g_object_get(m_pipeline.get(), "audio-sink", &sink, nullptr);
    return sink;
}

void MediaPlayerPrivateGStreamer::createGSTPlayBin()
{
    ASSERT(!m_pipeline);

    // gst_element_factory_make() returns a floating reference so
    // we should not adopt.
    setPipeline(gst_element_factory_make("playbin", "play"));
    setStreamVolumeElement(GST_STREAM_VOLUME(m_pipeline.get()));

    GRefPtr<GstBus> bus = adoptGRef(gst_pipeline_get_bus(GST_PIPELINE(m_pipeline.get())));
    gst_bus_set_sync_handler(bus.get(), [](GstBus*, GstMessage* message, gpointer userData) {
        auto& player = *static_cast<MediaPlayerPrivateGStreamer*>(userData);

        if (!player.handleSyncMessage(message)) {
            GRefPtr<GstMessage> protectedMessage(message);
            auto weakThis = player.createWeakPtr();
            RunLoop::main().dispatch([weakThis, protectedMessage] {
                if (weakThis)
                    weakThis->handleMessage(protectedMessage.get());
            });
        }
        gst_message_unref(message);
        return GST_BUS_DROP;
    }, this, nullptr);

    g_object_set(m_pipeline.get(), "mute", m_player->muted(), nullptr);

    g_signal_connect_swapped(m_pipeline.get(), "notify::source", G_CALLBACK(sourceChangedCallback), this);
    g_signal_connect_swapped(m_pipeline.get(), "video-changed", G_CALLBACK(videoChangedCallback), this);
    g_signal_connect_swapped(m_pipeline.get(), "audio-changed", G_CALLBACK(audioChangedCallback), this);
#if ENABLE(VIDEO_TRACK)
    if (webkitGstCheckVersion(1, 1, 2)) {
        g_signal_connect_swapped(m_pipeline.get(), "text-changed", G_CALLBACK(textChangedCallback), this);

        GstElement* textCombiner = webkitTextCombinerNew();
        ASSERT(textCombiner);
        g_object_set(m_pipeline.get(), "text-stream-combiner", textCombiner, nullptr);

        m_textAppSink = webkitTextSinkNew();
        ASSERT(m_textAppSink);

        m_textAppSinkPad = adoptGRef(gst_element_get_static_pad(m_textAppSink.get(), "sink"));
        ASSERT(m_textAppSinkPad);

        g_object_set(m_textAppSink.get(), "emit-signals", true, "enable-last-sample", false, "caps", gst_caps_new_empty_simple("text/vtt"), NULL);
        g_signal_connect_swapped(m_textAppSink.get(), "new-sample", G_CALLBACK(newTextSampleCallback), this);

        g_object_set(m_pipeline.get(), "text-sink", m_textAppSink.get(), NULL);
    }
#endif

    g_object_set(m_pipeline.get(), "video-sink", createVideoSink(), "audio-sink", createAudioSink(), nullptr);

    // On 1.4.2 and newer we use the audio-filter property instead.
    // See https://bugzilla.gnome.org/show_bug.cgi?id=735748 for
    // the reason for using >= 1.4.2 instead of >= 1.4.0.
    if (m_preservesPitch && webkitGstCheckVersion(1, 4, 2)) {
        GstElement* scale = gst_element_factory_make("scaletempo", 0);

        if (!scale)
            GST_WARNING("Failed to create scaletempo");
        else
            g_object_set(m_pipeline.get(), "audio-filter", scale, nullptr);
    }

    if (!m_player->client().mediaPlayerRenderingCanBeAccelerated(m_player)) {
        // If not using accelerated compositing, let GStreamer handle
        // the image-orientation tag.
        GstElement* videoFlip = gst_element_factory_make("videoflip", nullptr);
        g_object_set(videoFlip, "method", 8, nullptr);
        g_object_set(m_pipeline.get(), "video-filter", videoFlip, nullptr);
    }

    GRefPtr<GstPad> videoSinkPad = adoptGRef(gst_element_get_static_pad(m_videoSink.get(), "sink"));
    if (videoSinkPad)
        g_signal_connect_swapped(videoSinkPad.get(), "notify::caps", G_CALLBACK(videoSinkCapsChangedCallback), this);
}

void MediaPlayerPrivateGStreamer::simulateAudioInterruption()
{
    GstMessage* message = gst_message_new_request_state(GST_OBJECT(m_pipeline.get()), GST_STATE_PAUSED);
    gst_element_post_message(m_pipeline.get(), message);
}

bool MediaPlayerPrivateGStreamer::didPassCORSAccessCheck() const
{
    if (WEBKIT_IS_WEB_SRC(m_source.get()))
        return webKitSrcPassedCORSAccessCheck(WEBKIT_WEB_SRC(m_source.get()));
    return false;
}

bool MediaPlayerPrivateGStreamer::canSaveMediaData() const
{
    if (isLiveStream())
        return false;

    if (m_url.isLocalFile())
        return true;

    if (m_url.protocolIsInHTTPFamily())
        return true;

    return false;
}

}

#endif // USE(GSTREAMER)
