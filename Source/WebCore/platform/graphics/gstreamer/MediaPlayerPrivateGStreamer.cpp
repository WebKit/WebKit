/*
 * Copyright (C) 2007, 2009 Apple Inc.  All rights reserved.
 * Copyright (C) 2007 Collabora Ltd.  All rights reserved.
 * Copyright (C) 2007 Alp Toker <alp@atoker.com>
 * Copyright (C) 2009 Gustavo Noronha Silva <gns@gnome.org>
 * Copyright (C) 2009, 2010, 2011, 2012, 2013, 2015, 2016 Igalia S.L
 * Copyright (C) 2014 Cable Television Laboratories, Inc.
 * Copyright (C) 2015, 2016 Metrological Group B.V.
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

#include "GStreamerCommon.h"
#include "GStreamerRegistryScanner.h"
#include "HTTPHeaderNames.h"
#include "MIMETypeRegistry.h"
#include "MediaPlayer.h"
#include "MediaPlayerRequestInstallMissingPluginsCallback.h"
#include "NotImplemented.h"
#include "SecurityOrigin.h"
#include "TimeRanges.h"
#include "WebKitWebSourceGStreamer.h"
#include <glib.h>
#include <gst/gst.h>
#include <gst/pbutils/missing-plugins.h>
#include <limits>
#include <wtf/Environment.h>
#include <wtf/FileSystem.h>
#include <wtf/HexNumber.h>
#include <wtf/MediaTime.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/StringPrintStream.h>
#include <wtf/URL.h>
#include <wtf/WallTime.h>
#include <wtf/glib/GUniquePtr.h>
#include <wtf/glib/RunLoopSourcePriority.h>
#include <wtf/text/CString.h>

#if ENABLE(MEDIA_STREAM) && GST_CHECK_VERSION(1, 10, 0)
#include "GStreamerMediaStreamSource.h"
#endif

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


namespace WebCore {
using namespace std;

static void busMessageCallback(GstBus*, GstMessage* message, MediaPlayerPrivateGStreamer* player)
{
    player->handleMessage(message);
}

void MediaPlayerPrivateGStreamer::setAudioStreamPropertiesCallback(MediaPlayerPrivateGStreamer* player, GObject* object)
{
    player->setAudioStreamProperties(object);
}

void MediaPlayerPrivateGStreamer::setAudioStreamProperties(GObject* object)
{
    if (g_strcmp0(G_OBJECT_TYPE_NAME(object), "GstPulseSink"))
        return;

    const char* role = m_player->client().mediaPlayerIsVideo() ? "video" : "music";
    GstStructure* structure = gst_structure_new("stream-properties", "media.role", G_TYPE_STRING, role, nullptr);
    g_object_set(object, "stream-properties", structure, nullptr);
    gst_structure_free(structure);
    GUniquePtr<gchar> elementName(gst_element_get_name(GST_ELEMENT(object)));
    GST_DEBUG_OBJECT(pipeline(), "Set media.role as %s at %s", role, elementName.get());
}

void MediaPlayerPrivateGStreamer::registerMediaEngine(MediaEngineRegistrar registrar)
{
    MediaPlayerPrivateGStreamerBase::initializeDebugCategory();
    if (isAvailable()) {
        registrar([](MediaPlayer* player) { return std::make_unique<MediaPlayerPrivateGStreamer>(player); },
            getSupportedTypes, supportsType, nullptr, nullptr, nullptr, supportsKeySystem);
    }
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
    , m_buffering(false)
    , m_bufferingPercentage(0)
    , m_cachedPosition(MediaTime::invalidTime())
    , m_canFallBackToLastFinishedSeekPosition(false)
    , m_changingRate(false)
    , m_downloadFinished(false)
    , m_errorOccured(false)
    , m_isEndReached(false)
    , m_isStreaming(false)
    , m_durationAtEOS(MediaTime::invalidTime())
    , m_paused(true)
    , m_playbackRate(1)
    , m_requestedState(GST_STATE_VOID_PENDING)
    , m_resetPipeline(false)
    , m_seeking(false)
    , m_seekIsPending(false)
    , m_seekTime(MediaTime::invalidTime())
    , m_source(nullptr)
    , m_volumeAndMuteInitialized(false)
    , m_mediaLocations(nullptr)
    , m_mediaLocationCurrentIndex(0)
    , m_playbackRatePause(false)
    , m_timeOfOverlappingSeek(MediaTime::invalidTime())
    , m_lastPlaybackRate(1)
    , m_fillTimer(*this, &MediaPlayerPrivateGStreamer::fillTimerFired)
    , m_maxTimeLoaded(MediaTime::zeroTime())
    , m_preload(player->preload())
    , m_delayingLoad(false)
    , m_maxTimeLoadedAtLastDidLoadingProgress(MediaTime::zeroTime())
    , m_hasVideo(false)
    , m_hasAudio(false)
    , m_readyTimerHandler(RunLoop::main(), this, &MediaPlayerPrivateGStreamer::readyTimerFired)
    , m_totalBytes(0)
    , m_preservesPitch(false)
{
#if USE(GLIB)
    m_readyTimerHandler.setPriority(G_PRIORITY_DEFAULT_IDLE);
#endif
}

MediaPlayerPrivateGStreamer::~MediaPlayerPrivateGStreamer()
{
    GST_DEBUG_OBJECT(pipeline(), "Disposing player");

#if ENABLE(VIDEO_TRACK)
    for (auto& track : m_audioTracks.values())
        track->disconnect();

    for (auto& track : m_textTracks.values())
        track->disconnect();

    for (auto& track : m_videoTracks.values())
        track->disconnect();
#endif
    if (m_fillTimer.isActive())
        m_fillTimer.stop();

    if (m_mediaLocations) {
        gst_structure_free(m_mediaLocations);
        m_mediaLocations = nullptr;
    }

    if (WEBKIT_IS_WEB_SRC(m_source.get()) && GST_OBJECT_PARENT(m_source.get()))
        g_signal_handlers_disconnect_by_func(GST_ELEMENT_PARENT(m_source.get()), reinterpret_cast<gpointer>(uriDecodeBinElementAddedCallback), this);

    if (m_autoAudioSink) {
        g_signal_handlers_disconnect_by_func(G_OBJECT(m_autoAudioSink.get()),
            reinterpret_cast<gpointer>(setAudioStreamPropertiesCallback), this);
    }

    m_readyTimerHandler.stop();
    for (auto& missingPluginCallback : m_missingPluginCallbacks) {
        if (missingPluginCallback)
            missingPluginCallback->invalidate();
    }
    m_missingPluginCallbacks.clear();

    if (m_videoSink) {
        GRefPtr<GstPad> videoSinkPad = adoptGRef(gst_element_get_static_pad(m_videoSink.get(), "sink"));
        g_signal_handlers_disconnect_matched(videoSinkPad.get(), G_SIGNAL_MATCH_DATA, 0, 0, nullptr, nullptr, this);
    }

    if (m_pipeline) {
        GRefPtr<GstBus> bus = adoptGRef(gst_pipeline_get_bus(GST_PIPELINE(m_pipeline.get())));
        ASSERT(bus);
        g_signal_handlers_disconnect_by_func(bus.get(), gpointer(busMessageCallback), this);
        gst_bus_remove_signal_watch(bus.get());
        gst_bus_set_sync_handler(bus.get(), nullptr, nullptr, nullptr);
        g_signal_handlers_disconnect_matched(m_pipeline.get(), G_SIGNAL_MATCH_DATA, 0, 0, nullptr, nullptr, this);
    }
}

static void convertToInternalProtocol(URL& url)
{
    if (url.protocolIsInHTTPFamily() || url.protocolIsBlob())
        url.setProtocol("webkit+" + url.protocol());
}

void MediaPlayerPrivateGStreamer::setPlaybinURL(const URL& url)
{
    // Clean out everything after file:// url path.
    String cleanURLString(url.string());
    if (url.isLocalFile())
        cleanURLString = cleanURLString.substring(0, url.pathEnd());

    m_url = URL(URL(), cleanURLString);
    convertToInternalProtocol(m_url);

    GST_INFO_OBJECT(pipeline(), "Load %s", m_url.string().utf8().data());
    g_object_set(m_pipeline.get(), "uri", m_url.string().utf8().data(), nullptr);
}

void MediaPlayerPrivateGStreamer::load(const String& urlString)
{
    loadFull(urlString, nullptr, String());
}

static void setSyncOnClock(GstElement *element, bool sync)
{
    if (!GST_IS_BIN(element)) {
        g_object_set(element, "sync", sync, NULL);
        return;
    }

    GstIterator* it = gst_bin_iterate_sinks(GST_BIN(element));
    while (gst_iterator_foreach(it, (GstIteratorForeachFunction)([](const GValue* item, void* syncPtr) {
        bool* sync = static_cast<bool*>(syncPtr);
        setSyncOnClock(GST_ELEMENT(g_value_get_object(item)), *sync);
    }), &sync) == GST_ITERATOR_RESYNC)
        gst_iterator_resync(it);
    gst_iterator_free(it);
}

void MediaPlayerPrivateGStreamer::syncOnClock(bool sync)
{
    setSyncOnClock(videoSink(), sync);
    setSyncOnClock(audioSink(), sync);
}

void MediaPlayerPrivateGStreamer::loadFull(const String& urlString, const gchar* playbinName,
    const String& pipelineName)
{
    // FIXME: This method is still called even if supportsType() returned
    // IsNotSupported. This would deserve more investigation but meanwhile make
    // sure we don't ever try to play animated gif assets.
    if (m_player->contentMIMEType() == "image/gif") {
        loadingFailed(MediaPlayer::FormatError);
        return;
    }

    URL url(URL(), urlString);
    if (url.protocolIsAbout())
        return;

    if (!m_pipeline)
        createGSTPlayBin(isMediaSource() ? "playbin" : playbinName, pipelineName);
    syncOnClock(true);
    if (m_fillTimer.isActive())
        m_fillTimer.stop();

    ASSERT(m_pipeline);

    setPlaybinURL(url);

    GST_DEBUG_OBJECT(pipeline(), "preload: %s", convertEnumerationToString(m_preload).utf8().data());
    if (m_preload == MediaPlayer::None) {
        GST_INFO_OBJECT(pipeline(), "Delaying load.");
        m_delayingLoad = true;
    }

    // Reset network and ready states. Those will be set properly once
    // the pipeline pre-rolled.
    m_networkState = MediaPlayer::Loading;
    m_player->networkStateChanged();
    m_readyState = MediaPlayer::HaveNothing;
    m_player->readyStateChanged();
    m_volumeAndMuteInitialized = false;
    m_durationAtEOS = MediaTime::invalidTime();
    m_hasTaintedOrigin = WTF::nullopt;

    if (!m_delayingLoad)
        commitLoad();
}

#if ENABLE(MEDIA_SOURCE)
void MediaPlayerPrivateGStreamer::load(const String&, MediaSourcePrivateClient*)
{
    // Properly fail so the global MediaPlayer tries to fallback to the next MediaPlayerPrivate.
    m_networkState = MediaPlayer::FormatError;
    m_player->networkStateChanged();
}
#endif

#if ENABLE(MEDIA_STREAM)
void MediaPlayerPrivateGStreamer::load(MediaStreamPrivate& stream)
{
#if GST_CHECK_VERSION(1, 10, 0)
    m_streamPrivate = &stream;
    auto pipelineName = String::format("mediastream_%s_%p",
        (stream.hasCaptureVideoSource() || stream.hasCaptureAudioSource()) ? "Local" : "Remote", this);

    loadFull(String("mediastream://") + stream.id(), "playbin3", pipelineName);
    syncOnClock(false);

#if USE(GSTREAMER_GL)
    ensureGLVideoSinkContext();
#endif
    m_player->play();
#else
    // Properly fail so the global MediaPlayer tries to fallback to the next MediaPlayerPrivate.
    m_networkState = MediaPlayer::FormatError;
    m_player->networkStateChanged();
    notImplemented();
#endif
}
#endif

void MediaPlayerPrivateGStreamer::commitLoad()
{
    ASSERT(!m_delayingLoad);
    GST_DEBUG_OBJECT(pipeline(), "Committing load.");

    // GStreamer needs to have the pipeline set to a paused state to
    // start providing anything useful.
    changePipelineState(GST_STATE_PAUSED);

    setDownloadBuffering();
    updateStates();
}

MediaTime MediaPlayerPrivateGStreamer::playbackPosition() const
{
    if (m_isEndReached && m_seeking)
        return m_seekTime;

    // This constant should remain lower than HTMLMediaElement's maxTimeupdateEventFrequency.
    static const Seconds positionCacheThreshold = 200_ms;
    Seconds now = WTF::WallTime::now().secondsSinceEpoch();
    if (m_lastQueryTime && (now - m_lastQueryTime.value()) < positionCacheThreshold && m_cachedPosition.isValid())
        return m_cachedPosition;

    m_lastQueryTime = now;

    // Position is only available if no async state change is going on and the state is either paused or playing.
    gint64 position = GST_CLOCK_TIME_NONE;
    GstQuery* query = gst_query_new_position(GST_FORMAT_TIME);
    if (gst_element_query(m_pipeline.get(), query))
        gst_query_parse_position(query, 0, &position);
    gst_query_unref(query);

    GST_TRACE_OBJECT(pipeline(), "Position %" GST_TIME_FORMAT, GST_TIME_ARGS(position));

    MediaTime playbackPosition = MediaTime::zeroTime();
    GstClockTime gstreamerPosition = static_cast<GstClockTime>(position);
    if (GST_CLOCK_TIME_IS_VALID(gstreamerPosition))
        playbackPosition = MediaTime(gstreamerPosition, GST_SECOND);
    else if (m_canFallBackToLastFinishedSeekPosition)
        playbackPosition = m_seekTime;

    m_cachedPosition = playbackPosition;
    return playbackPosition;
}

void MediaPlayerPrivateGStreamer::readyTimerFired()
{
    GST_DEBUG_OBJECT(pipeline(), "In READY for too long. Releasing pipeline resources.");
    changePipelineState(GST_STATE_NULL);
}

bool MediaPlayerPrivateGStreamer::changePipelineState(GstState newState)
{
    ASSERT(m_pipeline);

    GstState currentState;
    GstState pending;

    gst_element_get_state(m_pipeline.get(), &currentState, &pending, 0);
    if (currentState == newState || pending == newState) {
        GST_DEBUG_OBJECT(pipeline(), "Rejected state change to %s from %s with %s pending", gst_element_state_get_name(newState),
            gst_element_state_get_name(currentState), gst_element_state_get_name(pending));
        return true;
    }

    GST_DEBUG_OBJECT(pipeline(), "Changing state change to %s from %s with %s pending", gst_element_state_get_name(newState),
        gst_element_state_get_name(currentState), gst_element_state_get_name(pending));

#if USE(GSTREAMER_GL)
    if (currentState == GST_STATE_READY && newState == GST_STATE_PAUSED)
        ensureGLVideoSinkContext();
#endif

    GstStateChangeReturn setStateResult = gst_element_set_state(m_pipeline.get(), newState);
    GstState pausedOrPlaying = newState == GST_STATE_PLAYING ? GST_STATE_PAUSED : GST_STATE_PLAYING;
    if (currentState != pausedOrPlaying && setStateResult == GST_STATE_CHANGE_FAILURE)
        return false;

    // Create a timer when entering the READY state so that we can free resources
    // if we stay for too long on READY.
    // Also lets remove the timer if we request a state change for any state other than READY.
    // See also https://bugs.webkit.org/show_bug.cgi?id=117354
    if (newState == GST_STATE_READY && !m_readyTimerHandler.isActive()) {
        // Max interval in seconds to stay in the READY state on manual
        // state change requests.
        static const Seconds readyStateTimerDelay { 1_min };
        m_readyTimerHandler.startOneShot(readyStateTimerDelay);
    } else if (newState != GST_STATE_READY)
        m_readyTimerHandler.stop();

    return true;
}

void MediaPlayerPrivateGStreamer::prepareToPlay()
{
    GST_DEBUG_OBJECT(pipeline(), "Prepare to play");
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
        GST_INFO_OBJECT(pipeline(), "Play");
    } else
        loadingFailed(MediaPlayer::Empty);
}

void MediaPlayerPrivateGStreamer::pause()
{
    m_playbackRatePause = false;
    GstState currentState, pendingState;
    gst_element_get_state(m_pipeline.get(), &currentState, &pendingState, 0);
    if (currentState < GST_STATE_PAUSED && pendingState <= GST_STATE_PAUSED)
        return;

    if (changePipelineState(GST_STATE_PAUSED))
        GST_INFO_OBJECT(pipeline(), "Pause");
    else
        loadingFailed(MediaPlayer::Empty);
}

MediaTime MediaPlayerPrivateGStreamer::durationMediaTime() const
{
    if (!m_pipeline || m_errorOccured)
        return MediaTime::invalidTime();

    if (m_durationAtEOS.isValid())
        return m_durationAtEOS;

    // The duration query would fail on a not-prerolled pipeline.
    if (GST_STATE(m_pipeline.get()) < GST_STATE_PAUSED)
        return MediaTime::positiveInfiniteTime();

    gint64 timeLength = 0;

    if (!gst_element_query_duration(m_pipeline.get(), GST_FORMAT_TIME, &timeLength) || !GST_CLOCK_TIME_IS_VALID(timeLength)) {
        GST_DEBUG_OBJECT(pipeline(), "Time duration query failed for %s", m_url.string().utf8().data());
        return MediaTime::positiveInfiniteTime();
    }

    GST_LOG("Duration: %" GST_TIME_FORMAT, GST_TIME_ARGS(timeLength));

    return MediaTime(timeLength, GST_SECOND);
    // FIXME: handle 3.14.9.5 properly
}

MediaTime MediaPlayerPrivateGStreamer::currentMediaTime() const
{
    if (!m_pipeline || m_errorOccured)
        return MediaTime::invalidTime();

    if (m_seeking)
        return m_seekTime;

    return playbackPosition();
}

void MediaPlayerPrivateGStreamer::seek(const MediaTime& mediaTime)
{
    if (!m_pipeline)
        return;

    if (m_errorOccured)
        return;

    GST_INFO_OBJECT(pipeline(), "[Seek] seek attempt to %s", toString(mediaTime).utf8().data());

    // Avoid useless seeking.
    if (mediaTime == currentMediaTime())
        return;

    MediaTime time = std::min(mediaTime, durationMediaTime());

    if (isLiveStream())
        return;

    GST_INFO_OBJECT(pipeline(), "[Seek] seeking to %s", toString(time).utf8().data());

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
        GST_DEBUG_OBJECT(pipeline(), "[Seek] cannot seek, current state change is %s", gst_element_state_change_return_get_name(getStateResult));
        return;
    }
    if (getStateResult == GST_STATE_CHANGE_ASYNC || state < GST_STATE_PAUSED || m_isEndReached) {
        m_seekIsPending = true;
        if (m_isEndReached) {
            GST_DEBUG_OBJECT(pipeline(), "[Seek] reset pipeline");
            m_resetPipeline = true;
            if (!changePipelineState(GST_STATE_PAUSED))
                loadingFailed(MediaPlayer::Empty);
        }
    } else {
        // We can seek now.
        if (!doSeek(time, m_player->rate(), static_cast<GstSeekFlags>(GST_SEEK_FLAG_FLUSH | GST_SEEK_FLAG_ACCURATE))) {
            GST_DEBUG_OBJECT(pipeline(), "[Seek] seeking to %s failed", toString(time).utf8().data());
            return;
        }
    }

    m_seeking = true;
    m_seekTime = time;
    m_isEndReached = false;
}

bool MediaPlayerPrivateGStreamer::doSeek(const MediaTime& position, float rate, GstSeekFlags seekType)
{
    // Default values for rate >= 0.
    MediaTime startTime = position, endTime = MediaTime::invalidTime();

    // TODO: Should do more than that, need to notify the media source
    // and probably flush the pipeline at least.
    if (isMediaSource())
        return true;

    if (rate < 0) {
        startTime = MediaTime::zeroTime();
        // If we are at beginning of media, start from the end to
        // avoid immediate EOS.
        if (position < MediaTime::zeroTime())
            endTime = durationMediaTime();
        else
            endTime = position;
    }

    if (!rate)
        rate = 1.0;

    return gst_element_seek(m_pipeline.get(), rate, GST_FORMAT_TIME, seekType,
        GST_SEEK_TYPE_SET, toGstClockTime(startTime), GST_SEEK_TYPE_SET, toGstClockTime(endTime));
}

void MediaPlayerPrivateGStreamer::updatePlaybackRate()
{
    if (!m_changingRate)
        return;

    GST_INFO_OBJECT(pipeline(), "Set Rate to %f", m_playbackRate);

    // Mute the sound if the playback rate is negative or too extreme and audio pitch is not adjusted.
    bool mute = m_playbackRate <= 0 || (!m_preservesPitch && (m_playbackRate < 0.8 || m_playbackRate > 2));

    GST_INFO_OBJECT(pipeline(), mute ? "Need to mute audio" : "Do not need to mute audio");

    if (doSeek(playbackPosition(), m_playbackRate, static_cast<GstSeekFlags>(GST_SEEK_FLAG_FLUSH))) {
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
        GST_DEBUG_OBJECT(pipeline(), "Ignoring pause at EOS");
        return true;
    }

    if (m_playbackRatePause) {
        GST_DEBUG_OBJECT(pipeline(), "Playback rate is 0, simulating PAUSED state");
        return false;
    }

    GstState state;
    gst_element_get_state(m_pipeline.get(), &state, nullptr, 0);
    bool paused = state <= GST_STATE_PAUSED;
    GST_DEBUG_OBJECT(pipeline(), "Paused: %s", toString(paused).utf8().data());
    return paused;
}

bool MediaPlayerPrivateGStreamer::seeking() const
{
    return m_seeking;
}

#if GST_CHECK_VERSION(1, 10, 0)
#define CLEAR_TRACKS(tracks, method) \
    for (auto& track : tracks.values())\
        method(*track);\
    tracks.clear();

void MediaPlayerPrivateGStreamer::clearTracks()
{
#if ENABLE(VIDEO_TRACK)
    CLEAR_TRACKS(m_audioTracks, m_player->removeAudioTrack);
    CLEAR_TRACKS(m_videoTracks, m_player->removeVideoTrack);
    CLEAR_TRACKS(m_textTracks, m_player->removeTextTrack);
#endif // ENABLE(VIDEO_TRACK)
}
#undef CLEAR_TRACKS

#if ENABLE(VIDEO_TRACK)
#define CREATE_TRACK(type, Type) \
    m_has##Type = true; \
    if (!useMediaSource) {\
        RefPtr<Type##TrackPrivateGStreamer> track = Type##TrackPrivateGStreamer::create(makeWeakPtr(*this), i, stream); \
        m_##type##Tracks.add(track->id(), track); \
        m_player->add##Type##Track(*track);\
        if (gst_stream_get_stream_flags(stream.get()) & GST_STREAM_FLAG_SELECT) {                                    \
            m_current##Type##StreamId = String(gst_stream_get_stream_id(stream.get()));                              \
        }                                                                                                            \
    }

FloatSize MediaPlayerPrivateGStreamer::naturalSize() const
{
#if ENABLE(MEDIA_STREAM)
    if (!m_isLegacyPlaybin && !m_currentVideoStreamId.isEmpty()) {
        RefPtr<VideoTrackPrivateGStreamer> videoTrack = m_videoTracks.get(m_currentVideoStreamId);

        if (videoTrack) {
            auto tags = adoptGRef(gst_stream_get_tags(videoTrack->stream()));
            gint width, height;

            if (tags && gst_tag_list_get_int(tags.get(), WEBKIT_MEDIA_TRACK_TAG_WIDTH, &width) && gst_tag_list_get_int(tags.get(), WEBKIT_MEDIA_TRACK_TAG_HEIGHT, &height))
                return FloatSize(width, height);
        }
    }
#endif // ENABLE(MEDIA_STREAM)

    return MediaPlayerPrivateGStreamerBase::naturalSize();
}
#else
#define CREATE_TRACK(type, _id, tracks, method, stream) m_has##Type## = true;
#endif // ENABLE(VIDEO_TRACK)

void MediaPlayerPrivateGStreamer::updateTracks()
{
    ASSERT(!m_isLegacyPlaybin);

    bool useMediaSource = isMediaSource();
    unsigned length = gst_stream_collection_get_size(m_streamCollection.get());

    bool oldHasAudio = m_hasAudio;
    bool oldHasVideo = m_hasVideo;
    // New stream collections override previous ones.
    clearTracks();
    unsigned textTrackIndex = 0;
    for (unsigned i = 0; i < length; i++) {
        GRefPtr<GstStream> stream = gst_stream_collection_get_stream(m_streamCollection.get(), i);
        String streamId(gst_stream_get_stream_id(stream.get()));
        GstStreamType type = gst_stream_get_stream_type(stream.get());

        GST_DEBUG_OBJECT(pipeline(), "Inspecting %s track with ID %s", gst_stream_type_get_name(type), streamId.utf8().data());
        if (type & GST_STREAM_TYPE_AUDIO) {
            CREATE_TRACK(audio, Audio)
        } else if (type & GST_STREAM_TYPE_VIDEO) {
            CREATE_TRACK(video, Video)
        } else if (type & GST_STREAM_TYPE_TEXT && !useMediaSource) {
#if ENABLE(VIDEO_TRACK)
            auto track = InbandTextTrackPrivateGStreamer::create(textTrackIndex++, stream);
            m_textTracks.add(streamId, track.copyRef());
            m_player->addTextTrack(track.get());
#endif
        } else
            GST_WARNING("Unknown track type found for stream %s", streamId.utf8().data());
    }

    if ((oldHasVideo != m_hasVideo) || (oldHasAudio != m_hasAudio))
        m_player->characteristicChanged();

    if (m_hasVideo)
        m_player->sizeChanged();

    m_player->client().mediaPlayerEngineUpdated(m_player);
}
#endif // GST_CHECK_VERSION(1, 10, 0)

void MediaPlayerPrivateGStreamer::enableTrack(TrackPrivateBaseGStreamer::TrackType trackType, unsigned index)
{
    // FIXME: Remove isMediaSource() test below when fixing https://bugs.webkit.org/show_bug.cgi?id=182531.
    if (isMediaSource()) {
        GST_FIXME_OBJECT(m_pipeline.get(), "Audio/Video/Text track switching is not yet supported by the MSE backend.");
        return;
    }

    const char* propertyName;
    const char* trackTypeAsString;
    Vector<String> selectedStreams;
    String selectedStreamId;

#if GST_CHECK_VERSION(1, 10, 0)
    GstStream* stream = nullptr;

    if (!m_isLegacyPlaybin) {
        stream = gst_stream_collection_get_stream(m_streamCollection.get(), index);
        if (!stream) {
            GST_WARNING_OBJECT(pipeline(), "No stream to select at index %u", index);
            return;
        }
        selectedStreamId = String::fromUTF8(gst_stream_get_stream_id(stream));
        selectedStreams.append(selectedStreamId);
    }
#endif // GST_CHECK_VERSION(1,0,0)

    switch (trackType) {
    case TrackPrivateBaseGStreamer::TrackType::Audio:
        propertyName = "current-audio";
        trackTypeAsString = "audio";
        if (!selectedStreamId.isEmpty() && selectedStreamId == m_currentAudioStreamId) {
            GST_INFO_OBJECT(pipeline(), "%s stream: %s already selected, not doing anything.", trackTypeAsString, selectedStreamId.utf8().data());
            return;
        }

        if (!m_currentTextStreamId.isEmpty())
            selectedStreams.append(m_currentTextStreamId);
        if (!m_currentVideoStreamId.isEmpty())
            selectedStreams.append(m_currentVideoStreamId);
        break;
    case TrackPrivateBaseGStreamer::TrackType::Video:
        propertyName = "current-video";
        trackTypeAsString = "video";
        if (!selectedStreamId.isEmpty() && selectedStreamId == m_currentVideoStreamId) {
            GST_INFO_OBJECT(pipeline(), "%s stream: %s already selected, not doing anything.", trackTypeAsString, selectedStreamId.utf8().data());
            return;
        }

        if (!m_currentAudioStreamId.isEmpty())
            selectedStreams.append(m_currentAudioStreamId);
        if (!m_currentTextStreamId.isEmpty())
            selectedStreams.append(m_currentTextStreamId);
        break;
    case TrackPrivateBaseGStreamer::TrackType::Text:
        if (!selectedStreamId.isEmpty() && selectedStreamId == m_currentTextStreamId) {
            GST_INFO_OBJECT(pipeline(), "%s stream: %s already selected, not doing anything.", trackTypeAsString, selectedStreamId.utf8().data());
            return;
        }

        propertyName = "current-text";
        trackTypeAsString = "text";
        if (!m_currentAudioStreamId.isEmpty())
            selectedStreams.append(m_currentAudioStreamId);
        if (!m_currentVideoStreamId.isEmpty())
            selectedStreams.append(m_currentVideoStreamId);
        break;
    case TrackPrivateBaseGStreamer::TrackType::Unknown:
    default:
        ASSERT_NOT_REACHED();
    }

    GST_INFO_OBJECT(pipeline(), "Enabling %s track with index: %u", trackTypeAsString, index);
    if (m_isLegacyPlaybin)
        g_object_set(m_pipeline.get(), propertyName, index, nullptr);
#if GST_CHECK_VERSION(1, 10, 0)
    else {
        GList* selectedStreamsList = nullptr;

        for (const auto& streamId : selectedStreams)
            selectedStreamsList = g_list_append(selectedStreamsList, g_strdup(streamId.utf8().data()));

        // TODO: MSE GstStream API support: https://bugs.webkit.org/show_bug.cgi?id=182531
        gst_element_send_event(m_pipeline.get(), gst_event_new_select_streams(selectedStreamsList));
        g_list_free_full(selectedStreamsList, reinterpret_cast<GDestroyNotify>(g_free));
    }
#endif
}

void MediaPlayerPrivateGStreamer::videoChangedCallback(MediaPlayerPrivateGStreamer* player)
{
    player->m_notifier->notify(MainThreadNotification::VideoChanged, [player] {
        player->notifyPlayerOfVideo();
    });
}

void MediaPlayerPrivateGStreamer::notifyPlayerOfVideo()
{
    if (UNLIKELY(!m_pipeline || !m_source))
        return;

    ASSERT(m_isLegacyPlaybin || isMediaSource());

    gint numTracks = 0;
    bool useMediaSource = isMediaSource();
    GstElement* element = useMediaSource ? m_source.get() : m_pipeline.get();
    g_object_get(element, "n-video", &numTracks, nullptr);

    GST_INFO_OBJECT(pipeline(), "Media has %d video tracks", numTracks);

    bool oldHasVideo = m_hasVideo;
    m_hasVideo = numTracks > 0;
    if (oldHasVideo != m_hasVideo)
        m_player->characteristicChanged();

    if (m_hasVideo)
        m_player->sizeChanged();

    if (useMediaSource) {
        GST_DEBUG_OBJECT(pipeline(), "Tracks managed by source element. Bailing out now.");
        m_player->client().mediaPlayerEngineUpdated(m_player);
        return;
    }

#if ENABLE(VIDEO_TRACK)
    Vector<String> validVideoStreams;
    for (gint i = 0; i < numTracks; ++i) {
        GRefPtr<GstPad> pad;
        g_signal_emit_by_name(m_pipeline.get(), "get-video-pad", i, &pad.outPtr(), nullptr);
        ASSERT(pad);

        String streamId = "V" + String::number(i);
        validVideoStreams.append(streamId);
        if (i < static_cast<gint>(m_videoTracks.size())) {
            RefPtr<VideoTrackPrivateGStreamer> existingTrack = m_videoTracks.get(streamId);
            if (existingTrack) {
                existingTrack->setIndex(i);
                if (existingTrack->pad() == pad)
                    continue;
            }
        }

        auto track = VideoTrackPrivateGStreamer::create(makeWeakPtr(*this), i, pad);
        ASSERT(streamId == track->id());
        m_videoTracks.add(streamId, track.copyRef());
        m_player->addVideoTrack(track.get());
    }

    purgeInvalidVideoTracks(validVideoStreams);
#endif

    m_player->client().mediaPlayerEngineUpdated(m_player);
}

void MediaPlayerPrivateGStreamer::videoSinkCapsChangedCallback(MediaPlayerPrivateGStreamer* player)
{
    player->m_notifier->notify(MainThreadNotification::VideoCapsChanged, [player] {
        player->notifyPlayerOfVideoCaps();
    });
}

void MediaPlayerPrivateGStreamer::notifyPlayerOfVideoCaps()
{
    m_videoSize = IntSize();
    m_player->client().mediaPlayerEngineUpdated(m_player);
}

void MediaPlayerPrivateGStreamer::audioChangedCallback(MediaPlayerPrivateGStreamer* player)
{
    player->m_notifier->notify(MainThreadNotification::AudioChanged, [player] {
        player->notifyPlayerOfAudio();
    });
}

void MediaPlayerPrivateGStreamer::notifyPlayerOfAudio()
{
    if (UNLIKELY(!m_pipeline || !m_source))
        return;

    ASSERT(m_isLegacyPlaybin || isMediaSource());

    gint numTracks = 0;
    bool useMediaSource = isMediaSource();
    GstElement* element = useMediaSource ? m_source.get() : m_pipeline.get();
    g_object_get(element, "n-audio", &numTracks, nullptr);

    GST_INFO_OBJECT(pipeline(), "Media has %d audio tracks", numTracks);
    bool oldHasAudio = m_hasAudio;
    m_hasAudio = numTracks > 0;
    if (oldHasAudio != m_hasAudio)
        m_player->characteristicChanged();

    if (useMediaSource) {
        GST_DEBUG_OBJECT(pipeline(), "Tracks managed by source element. Bailing out now.");
        m_player->client().mediaPlayerEngineUpdated(m_player);
        return;
    }

#if ENABLE(VIDEO_TRACK)
    Vector<String> validAudioStreams;
    for (gint i = 0; i < numTracks; ++i) {
        GRefPtr<GstPad> pad;
        g_signal_emit_by_name(m_pipeline.get(), "get-audio-pad", i, &pad.outPtr(), nullptr);
        ASSERT(pad);

        String streamId = "A" + String::number(i);
        validAudioStreams.append(streamId);
        if (i < static_cast<gint>(m_audioTracks.size())) {
            RefPtr<AudioTrackPrivateGStreamer> existingTrack = m_audioTracks.get(streamId);
            if (existingTrack) {
                existingTrack->setIndex(i);
                if (existingTrack->pad() == pad)
                    continue;
            }
        }

        auto track = AudioTrackPrivateGStreamer::create(makeWeakPtr(*this), i, pad);
        ASSERT(streamId == track->id());
        m_audioTracks.add(streamId, track);
        m_player->addAudioTrack(*track);
    }

    purgeInvalidAudioTracks(validAudioStreams);
#endif

    m_player->client().mediaPlayerEngineUpdated(m_player);
}

#if ENABLE(VIDEO_TRACK)
void MediaPlayerPrivateGStreamer::textChangedCallback(MediaPlayerPrivateGStreamer* player)
{
    player->m_notifier->notify(MainThreadNotification::TextChanged, [player] {
        player->notifyPlayerOfText();
    });
}

void MediaPlayerPrivateGStreamer::notifyPlayerOfText()
{
    if (UNLIKELY(!m_pipeline || !m_source))
        return;

    ASSERT(m_isLegacyPlaybin || isMediaSource());

    gint numTracks = 0;
    bool useMediaSource = isMediaSource();
    GstElement* element = useMediaSource ? m_source.get() : m_pipeline.get();
    g_object_get(element, "n-text", &numTracks, nullptr);

    GST_INFO_OBJECT(pipeline(), "Media has %d text tracks", numTracks);

    if (useMediaSource) {
        GST_DEBUG_OBJECT(pipeline(), "Tracks managed by source element. Bailing out now.");
        return;
    }

    Vector<String> validTextStreams;
    for (gint i = 0; i < numTracks; ++i) {
        GRefPtr<GstPad> pad;
        g_signal_emit_by_name(m_pipeline.get(), "get-text-pad", i, &pad.outPtr(), nullptr);
        ASSERT(pad);

        // We can't assume the pad has a sticky event here like implemented in
        // InbandTextTrackPrivateGStreamer because it might be emitted after the
        // track was created. So fallback to a dummy stream ID like in the Audio
        // and Video tracks.
        String streamId = "T" + String::number(i);

        validTextStreams.append(streamId);
        if (i < static_cast<gint>(m_textTracks.size())) {
            RefPtr<InbandTextTrackPrivateGStreamer> existingTrack = m_textTracks.get(streamId);
            if (existingTrack) {
                existingTrack->setIndex(i);
                if (existingTrack->pad() == pad)
                    continue;
            }
        }

        auto track = InbandTextTrackPrivateGStreamer::create(i, pad);
        m_textTracks.add(streamId, track.copyRef());
        m_player->addTextTrack(track.get());
    }

    purgeInvalidTextTracks(validTextStreams);
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
    g_signal_emit_by_name(m_textAppSink.get(), "pull-sample", &sample.outPtr(), nullptr);
    ASSERT(sample);

    if (streamStartEvent) {
        bool found = FALSE;
        const gchar* id;
        gst_event_parse_stream_start(streamStartEvent.get(), &id);
        for (auto& track : m_textTracks.values()) {
            if (!strcmp(track->streamId().utf8().data(), id)) {
                track->handleSample(sample);
                found = true;
                break;
            }
        }
        if (!found)
            GST_WARNING("Got sample with unknown stream ID %s.", id);
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

    MediaTime mediaDuration = durationMediaTime();
    if (!mediaDuration || mediaDuration.isPositiveInfinite())
        return timeRanges;

    GstQuery* query = gst_query_new_buffering(GST_FORMAT_PERCENT);

    if (!gst_element_query(m_pipeline.get(), query)) {
        gst_query_unref(query);
        return timeRanges;
    }

    guint numBufferingRanges = gst_query_get_n_buffering_ranges(query);
    for (guint index = 0; index < numBufferingRanges; index++) {
        gint64 rangeStart = 0, rangeStop = 0;
        if (gst_query_parse_nth_buffering_range(query, index, &rangeStart, &rangeStop)) {
            uint64_t startTime = gst_util_uint64_scale_int_round(toGstUnsigned64Time(mediaDuration), rangeStart, GST_FORMAT_PERCENT_MAX);
            uint64_t stopTime = gst_util_uint64_scale_int_round(toGstUnsigned64Time(mediaDuration), rangeStop, GST_FORMAT_PERCENT_MAX);
            timeRanges->add(MediaTime(startTime, GST_SECOND), MediaTime(stopTime, GST_SECOND));
        }
    }

    // Fallback to the more general maxTimeLoaded() if no range has
    // been found.
    if (!timeRanges->length()) {
        MediaTime loaded = maxTimeLoaded();
        if (loaded.isValid() && loaded)
            timeRanges->add(MediaTime::zeroTime(), loaded);
    }

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

    GST_LOG("Message %s received from element %s", GST_MESSAGE_TYPE_NAME(message), GST_MESSAGE_SRC_NAME(message));
    switch (GST_MESSAGE_TYPE(message)) {
    case GST_MESSAGE_ERROR:
        if (m_resetPipeline || !m_missingPluginCallbacks.isEmpty() || m_errorOccured)
            break;
        gst_message_parse_error(message, &err.outPtr(), &debug.outPtr());
        GST_ERROR("Error %d: %s (url=%s)", err->code, err->message, m_url.string().utf8().data());

        GST_DEBUG_BIN_TO_DOT_FILE_WITH_TS(GST_BIN(m_pipeline.get()), GST_DEBUG_GRAPH_SHOW_ALL, "webkit-video.error");

        error = MediaPlayer::Empty;
        if (g_error_matches(err.get(), GST_STREAM_ERROR, GST_STREAM_ERROR_CODEC_NOT_FOUND)
            || g_error_matches(err.get(), GST_STREAM_ERROR, GST_STREAM_ERROR_WRONG_TYPE)
            || g_error_matches(err.get(), GST_STREAM_ERROR, GST_STREAM_ERROR_FAILED)
            || g_error_matches(err.get(), GST_CORE_ERROR, GST_CORE_ERROR_MISSING_PLUGIN)
            || g_error_matches(err.get(), GST_RESOURCE_ERROR, GST_RESOURCE_ERROR_NOT_FOUND))
            error = MediaPlayer::FormatError;
        else if (g_error_matches(err.get(), GST_STREAM_ERROR, GST_STREAM_ERROR_TYPE_NOT_FOUND)) {
            // Let the mediaPlayerClient handle the stream error, in
            // this case the HTMLMediaElement will emit a stalled
            // event.
            GST_ERROR("Decode error, let the Media element emit a stalled event.");
            m_loadingStalled = true;
            break;
        } else if (err->domain == GST_STREAM_ERROR) {
            error = MediaPlayer::DecodeError;
            attemptNextLocation = true;
        } else if (err->domain == GST_RESOURCE_ERROR)
            error = MediaPlayer::NetworkError;

        if (attemptNextLocation)
            issueError = !loadNextLocation();
        if (issueError) {
            m_errorOccured = true;
            if (m_networkState != error) {
                m_networkState = error;
                m_player->networkStateChanged();
            }
        }
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
        gst_message_parse_state_changed(message, &currentState, &newState, nullptr);
        CString dotFileName = makeString(GST_OBJECT_NAME(m_pipeline.get()), '.',
            gst_element_state_get_name(currentState), '_', gst_element_state_get_name(newState)).utf8();
        GST_DEBUG_BIN_TO_DOT_FILE_WITH_TS(GST_BIN(m_pipeline.get()), GST_DEBUG_GRAPH_SHOW_ALL, dotFileName.data());

        break;
    }
    case GST_MESSAGE_BUFFERING:
        processBufferingStats(message);
        break;
    case GST_MESSAGE_DURATION_CHANGED:
        // Duration in MSE is managed by MediaSource, SourceBuffer and AppendPipeline.
        if (messageSourceIsPlaybin && !isMediaSource())
            durationChanged();
        break;
    case GST_MESSAGE_REQUEST_STATE:
        gst_message_parse_request_state(message, &requestedState);
        gst_element_get_state(m_pipeline.get(), &currentState, nullptr, 250 * GST_NSECOND);
        if (requestedState < currentState) {
            GST_INFO_OBJECT(pipeline(), "Element %s requested state change to %s", GST_MESSAGE_SRC_NAME(message),
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
                auto missingPluginCallback = MediaPlayerRequestInstallMissingPluginsCallback::create([weakThis = makeWeakPtr(*this)](uint32_t result, MediaPlayerRequestInstallMissingPluginsCallback& missingPluginCallback) {
                    if (!weakThis) {
                        GST_INFO("got missing pluging installation callback in destroyed player with result %u", result);
                        return;
                    }

                    GST_DEBUG("got missing plugin installation callback with result %u", result);
                    RefPtr<MediaPlayerRequestInstallMissingPluginsCallback> protectedMissingPluginCallback = &missingPluginCallback;
                    weakThis->m_missingPluginCallbacks.removeFirst(protectedMissingPluginCallback);
                    if (result != GST_INSTALL_PLUGINS_SUCCESS)
                        return;

                    weakThis->changePipelineState(GST_STATE_READY);
                    weakThis->changePipelineState(GST_STATE_PAUSED);
                });
                m_missingPluginCallbacks.append(missingPluginCallback.copyRef());
                GUniquePtr<char> detail(gst_missing_plugin_message_get_installer_detail(message));
                GUniquePtr<char> description(gst_missing_plugin_message_get_description(message));
                m_player->client().requestInstallMissingPlugins(String::fromUTF8(detail.get()), String::fromUTF8(description.get()), missingPluginCallback.get());
            }
        }
#if ENABLE(VIDEO_TRACK) && USE(GSTREAMER_MPEGTS)
        else if (GstMpegtsSection* section = gst_message_parse_mpegts_section(message)) {
            processMpegTsSection(section);
            gst_mpegts_section_unref(section);
        }
#endif
#if ENABLE(ENCRYPTED_MEDIA)
        else if (gst_structure_has_name(structure, "drm-waiting-for-key")) {
            GST_DEBUG_OBJECT(pipeline(), "drm-waiting-for-key message from %s", GST_MESSAGE_SRC_NAME(message));
            setWaitingForKey(true);
            // FIXME: The decryptors should be able to attempt to decrypt after being created and linked in a pipeline but currently they are not and current
            // architecture does not make this very easy. Fortunately, the arch will change soon and it does not pay off to fix this now with something that could be
            // more convoluted. In the meantime, force attempt to decrypt when they get blocked.
            attemptToDecryptWithLocalInstance();
        } else if (gst_structure_has_name(structure, "drm-key-received")) {
            GST_DEBUG_OBJECT(pipeline(), "drm-key-received message from %s", GST_MESSAGE_SRC_NAME(message));
            setWaitingForKey(false);
        }
#endif
        else if (gst_structure_has_name(structure, "http-headers")) {
            GstStructure* responseHeaders;
            if (gst_structure_get(structure, "response-headers", GST_TYPE_STRUCTURE, &responseHeaders, nullptr)) {
                if (!gst_structure_has_field(responseHeaders, httpHeaderNameString(HTTPHeaderName::ContentLength).utf8().data())) {
                    GST_INFO_OBJECT(pipeline(), "Live stream detected. Disabling on-disk buffering");
                    m_isStreaming = true;
                    setDownloadBuffering();
                }
                gst_structure_free(responseHeaders);
            }
        } else if (gst_structure_has_name(structure, "adaptive-streaming-statistics")) {
            if (WEBKIT_IS_WEB_SRC(m_source.get()))
                if (const char* uri = gst_structure_get_string(structure, "uri"))
                    m_hasTaintedOrigin = webKitSrcWouldTaintOrigin(WEBKIT_WEB_SRC(m_source.get()), SecurityOrigin::create(URL(URL(), uri)));
        } else
            GST_DEBUG_OBJECT(pipeline(), "Unhandled element message: %" GST_PTR_FORMAT, structure);
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
#if GST_CHECK_VERSION(1, 10, 0)
    case GST_MESSAGE_STREAMS_SELECTED: {
        GRefPtr<GstStreamCollection> collection;
        gst_message_parse_streams_selected(message, &collection.outPtr());

        if (!collection)
            break;

        m_streamCollection.swap(collection);
        m_currentAudioStreamId = "";
        m_currentVideoStreamId = "";
        m_currentTextStreamId = "";

        unsigned length = gst_message_streams_selected_get_size(message);
        for (unsigned i = 0; i < length; i++) {
            GRefPtr<GstStream> stream = gst_message_streams_selected_get_stream(message, i);
            if (!stream)
                continue;

            GstStreamType type = gst_stream_get_stream_type(stream.get());
            String streamId(gst_stream_get_stream_id(stream.get()));

            GST_DEBUG_OBJECT(pipeline(), "Selecting %s track with ID: %s", gst_stream_type_get_name(type), streamId.utf8().data());
            // Playbin3 can send more than one selected stream of the same type
            // but there's no priority or ordering system in place, so we assume
            // the selected stream is the last one as reported by playbin3.
            if (type & GST_STREAM_TYPE_AUDIO) {
                m_currentAudioStreamId = streamId;
                auto track = m_audioTracks.get(m_currentAudioStreamId);
                ASSERT(track);
                track->markAsActive();
            } else if (type & GST_STREAM_TYPE_VIDEO) {
                m_currentVideoStreamId = streamId;
                auto track = m_videoTracks.get(m_currentVideoStreamId);
                ASSERT(track);
                track->markAsActive();
            } else if (type & GST_STREAM_TYPE_TEXT)
                m_currentTextStreamId = streamId;
            else
                GST_WARNING("Unknown stream type with stream-id %s", streamId.utf8().data());
        }
        break;
    }
#endif
    default:
        GST_DEBUG_OBJECT(pipeline(), "Unhandled GStreamer message type: %s", GST_MESSAGE_TYPE_NAME(message));
        break;
    }
}

void MediaPlayerPrivateGStreamer::processBufferingStats(GstMessage* message)
{
    m_buffering = true;
    gst_message_parse_buffering(message, &m_bufferingPercentage);

    GST_DEBUG_OBJECT(pipeline(), "[Buffering] Buffering: %d%%.", m_bufferingPercentage);

    if (m_bufferingPercentage == 100)
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
                auto track = InbandMetadataTextTrackPrivateGStreamer::create(
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
                m_player->addTextTrack(*track);
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

        track->addDataCue(currentMediaTime(), currentMediaTime(), bytes, size);
    }
}
#endif

#if ENABLE(VIDEO_TRACK)
void MediaPlayerPrivateGStreamer::processTableOfContents(GstMessage* message)
{
    if (m_chaptersTrack)
        m_player->removeTextTrack(*m_chaptersTrack);

    m_chaptersTrack = InbandMetadataTextTrackPrivateGStreamer::create(InbandTextTrackPrivate::Chapters, InbandTextTrackPrivate::Generic);
    m_player->addTextTrack(*m_chaptersTrack);

    GRefPtr<GstToc> toc;
    gboolean updated;
    gst_message_parse_toc(message, &toc.outPtr(), &updated);
    ASSERT(toc);

    for (GList* i = gst_toc_get_entries(toc.get()); i; i = i->next)
        processTableOfContentsEntry(static_cast<GstTocEntry*>(i->data));
}

void MediaPlayerPrivateGStreamer::processTableOfContentsEntry(GstTocEntry* entry)
{
    ASSERT(entry);

    auto cue = GenericCueData::create();

    gint64 start = -1, stop = -1;
    gst_toc_entry_get_start_stop_times(entry, &start, &stop);
    if (start != -1)
        cue->setStartTime(MediaTime(start, GST_SECOND));
    if (stop != -1)
        cue->setEndTime(MediaTime(stop, GST_SECOND));

    GstTagList* tags = gst_toc_entry_get_tags(entry);
    if (tags) {
        gchar* title = nullptr;
        gst_tag_list_get_string(tags, GST_TAG_TITLE, &title);
        if (title) {
            cue->setContent(title);
            g_free(title);
        }
    }

    m_chaptersTrack->addGenericCue(cue);

    for (GList* i = gst_toc_entry_get_sub_entries(entry); i; i = i->next)
        processTableOfContentsEntry(static_cast<GstTocEntry*>(i->data));
}

void MediaPlayerPrivateGStreamer::purgeInvalidAudioTracks(Vector<String> validTrackIds)
{
    m_audioTracks.removeIf([validTrackIds](auto& keyAndValue) {
        return !validTrackIds.contains(keyAndValue.key);
    });
}

void MediaPlayerPrivateGStreamer::purgeInvalidVideoTracks(Vector<String> validTrackIds)
{
    m_videoTracks.removeIf([validTrackIds](auto& keyAndValue) {
        return !validTrackIds.contains(keyAndValue.key);
    });
}

void MediaPlayerPrivateGStreamer::purgeInvalidTextTracks(Vector<String> validTrackIds)
{
    m_textTracks.removeIf([validTrackIds](auto& keyAndValue) {
        return !validTrackIds.contains(keyAndValue.key);
    });
}
#endif

static gint findHLSQueue(gconstpointer a, gconstpointer)
{
    GValue* item = static_cast<GValue*>(const_cast<gpointer>(a));
    GstElement* element = GST_ELEMENT(g_value_get_object(item));
    if (g_str_has_prefix(GST_ELEMENT_NAME(element), "queue")) {
        GstElement* parent = GST_ELEMENT(GST_ELEMENT_PARENT(element));
        if (!GST_IS_OBJECT(parent))
            return 1;

        if (g_str_has_prefix(GST_ELEMENT_NAME(GST_ELEMENT_PARENT(parent)), "hlsdemux"))
            return 0;
    }

    return 1;
}

static bool isHLSProgressing(GstElement* playbin, GstQuery* query)
{
    GValue item = { };
    GstIterator* binIterator = gst_bin_iterate_recurse(GST_BIN(playbin));
    bool foundHLSQueue = gst_iterator_find_custom(binIterator, reinterpret_cast<GCompareFunc>(findHLSQueue), &item, nullptr);
    gst_iterator_free(binIterator);

    if (!foundHLSQueue)
        return false;

    GstElement* queueElement = GST_ELEMENT(g_value_get_object(&item));
    bool queryResult = gst_element_query(queueElement, query);
    g_value_unset(&item);

    return queryResult;
}

void MediaPlayerPrivateGStreamer::fillTimerFired()
{
    GstQuery* query = gst_query_new_buffering(GST_FORMAT_PERCENT);

    if (G_UNLIKELY(!gst_element_query(m_pipeline.get(), query))) {
        // This query always fails for live pipelines. In the case of HLS, try and find
        // the queue inside the HLS element to get a proxy measure of progress. Note
        // that the percentage value is rather meaningless as used below.
        // This is a hack, see https://bugs.webkit.org/show_bug.cgi?id=141469.
        if (!isHLSProgressing(m_pipeline.get(), query)) {
            gst_query_unref(query);
            return;
        }
    }

    gint64 start, stop;
    gdouble fillStatus = 100.0;

    gst_query_parse_buffering_range(query, nullptr, &start, &stop, nullptr);
    gst_query_unref(query);

    if (stop != -1)
        fillStatus = 100.0 * stop / GST_FORMAT_PERCENT_MAX;

    GST_DEBUG_OBJECT(pipeline(), "[Buffering] Download buffer filled up to %f%%", fillStatus);

    MediaTime mediaDuration = durationMediaTime();

    // Update maxTimeLoaded only if the media duration is
    // available. Otherwise we can't compute it.
    if (mediaDuration) {
        if (fillStatus == 100.0)
            m_maxTimeLoaded = mediaDuration;
        else
            m_maxTimeLoaded = MediaTime(fillStatus * static_cast<double>(toGstUnsigned64Time(mediaDuration)) / 100, GST_SECOND);
        GST_DEBUG_OBJECT(pipeline(), "[Buffering] Updated maxTimeLoaded: %s", toString(m_maxTimeLoaded).utf8().data());
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

MediaTime MediaPlayerPrivateGStreamer::maxMediaTimeSeekable() const
{
    if (m_errorOccured)
        return MediaTime::zeroTime();

    MediaTime duration = durationMediaTime();
    GST_DEBUG_OBJECT(pipeline(), "maxMediaTimeSeekable, duration: %s", toString(duration).utf8().data());
    // infinite duration means live stream
    if (duration.isPositiveInfinite())
        return MediaTime::zeroTime();

    return duration;
}

MediaTime MediaPlayerPrivateGStreamer::maxTimeLoaded() const
{
    if (m_errorOccured)
        return MediaTime::zeroTime();

    MediaTime loaded = m_maxTimeLoaded;
    if (m_isEndReached)
        loaded = durationMediaTime();
    GST_LOG("maxTimeLoaded: %s", toString(loaded).utf8().data());
    return loaded;
}

bool MediaPlayerPrivateGStreamer::didLoadingProgress() const
{
    if (m_errorOccured || m_loadingStalled)
        return false;

    if (isLiveStream())
        return true;

    if (UNLIKELY(!m_pipeline || !durationMediaTime() || (!isMediaSource() && !totalBytes())))
        return false;

    MediaTime currentMaxTimeLoaded = maxTimeLoaded();
    bool didLoadingProgress = currentMaxTimeLoaded != m_maxTimeLoadedAtLastDidLoadingProgress;
    m_maxTimeLoadedAtLastDidLoadingProgress = currentMaxTimeLoaded;
    GST_LOG("didLoadingProgress: %s", toString(didLoadingProgress).utf8().data());
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

    if (isLiveStream())
        return 0;

    GstFormat fmt = GST_FORMAT_BYTES;
    gint64 length = 0;
    if (gst_element_query_duration(m_source.get(), fmt, &length)) {
        GST_INFO_OBJECT(pipeline(), "totalBytes %" G_GINT64_FORMAT, length);
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

    GST_INFO_OBJECT(pipeline(), "totalBytes %" G_GINT64_FORMAT, length);
    m_totalBytes = static_cast<unsigned long long>(length);
    m_isStreaming = !length;
    return m_totalBytes;
}

void MediaPlayerPrivateGStreamer::sourceSetupCallback(MediaPlayerPrivateGStreamer* player, GstElement* sourceElement)
{
    player->sourceSetup(sourceElement);
}

void MediaPlayerPrivateGStreamer::uriDecodeBinElementAddedCallback(GstBin* bin, GstElement* element, MediaPlayerPrivateGStreamer* player)
{
    if (g_strcmp0(G_OBJECT_TYPE_NAME(element), "GstDownloadBuffer"))
        return;

    player->m_downloadBuffer = element;
    g_signal_handlers_disconnect_by_func(bin, reinterpret_cast<gpointer>(uriDecodeBinElementAddedCallback), player);
    g_signal_connect_swapped(element, "notify::temp-location", G_CALLBACK(downloadBufferFileCreatedCallback), player);

    GUniqueOutPtr<char> oldDownloadTemplate;
    g_object_get(element, "temp-template", &oldDownloadTemplate.outPtr(), nullptr);

    GUniquePtr<char> newDownloadTemplate(g_build_filename(G_DIR_SEPARATOR_S, "var", "tmp", "WebKit-Media-XXXXXX", nullptr));
    g_object_set(element, "temp-template", newDownloadTemplate.get(), nullptr);
    GST_DEBUG_OBJECT(player->pipeline(), "Reconfigured file download template from '%s' to '%s'", oldDownloadTemplate.get(), newDownloadTemplate.get());

    player->purgeOldDownloadFiles(oldDownloadTemplate.get());
}

void MediaPlayerPrivateGStreamer::downloadBufferFileCreatedCallback(MediaPlayerPrivateGStreamer* player)
{
    ASSERT(player->m_downloadBuffer);

    g_signal_handlers_disconnect_by_func(player->m_downloadBuffer.get(), reinterpret_cast<gpointer>(downloadBufferFileCreatedCallback), player);

    GUniqueOutPtr<char> downloadFile;
    g_object_get(player->m_downloadBuffer.get(), "temp-location", &downloadFile.outPtr(), nullptr);
    player->m_downloadBuffer = nullptr;

    if (UNLIKELY(!FileSystem::deleteFile(downloadFile.get()))) {
        GST_WARNING("Couldn't unlink media temporary file %s after creation", downloadFile.get());
        return;
    }

    GST_DEBUG_OBJECT(player->pipeline(), "Unlinked media temporary file %s after creation", downloadFile.get());
}

void MediaPlayerPrivateGStreamer::purgeOldDownloadFiles(const char* downloadFileTemplate)
{
    if (!downloadFileTemplate)
        return;

    GUniquePtr<char> templatePath(g_path_get_dirname(downloadFileTemplate));
    GUniquePtr<char> templateFile(g_path_get_basename(downloadFileTemplate));
    String templatePattern = String(templateFile.get()).replace("X", "?");

    for (auto& filePath : FileSystem::listDirectory(templatePath.get(), templatePattern)) {
        if (UNLIKELY(!FileSystem::deleteFile(filePath))) {
            GST_WARNING("Couldn't unlink legacy media temporary file: %s", filePath.utf8().data());
            continue;
        }

        GST_TRACE("Unlinked legacy media temporary file: %s", filePath.utf8().data());
    }
}

void MediaPlayerPrivateGStreamer::sourceSetup(GstElement* sourceElement)
{
    GST_DEBUG_OBJECT(pipeline(), "Source element set-up for %s", GST_ELEMENT_NAME(sourceElement));

    if (WEBKIT_IS_WEB_SRC(m_source.get()) && GST_OBJECT_PARENT(m_source.get()))
        g_signal_handlers_disconnect_by_func(GST_ELEMENT_PARENT(m_source.get()), reinterpret_cast<gpointer>(uriDecodeBinElementAddedCallback), this);

    m_source = sourceElement;

    if (WEBKIT_IS_WEB_SRC(m_source.get())) {
        webKitWebSrcSetMediaPlayer(WEBKIT_WEB_SRC(m_source.get()), m_player);
        g_signal_connect(GST_ELEMENT_PARENT(m_source.get()), "element-added", G_CALLBACK(uriDecodeBinElementAddedCallback), this);
#if ENABLE(MEDIA_STREAM) && GST_CHECK_VERSION(1, 10, 0)
    } else if (WEBKIT_IS_MEDIA_STREAM_SRC(sourceElement)) {
        auto stream = m_streamPrivate.get();
        ASSERT(stream);
        webkitMediaStreamSrcSetStream(WEBKIT_MEDIA_STREAM_SRC(sourceElement), stream);
#endif
    }
}

bool MediaPlayerPrivateGStreamer::hasSingleSecurityOrigin() const
{
    if (!m_source)
        return false;

    if (!WEBKIT_IS_WEB_SRC(m_source.get()))
        return true;

    GUniqueOutPtr<char> originalURI, resolvedURI;
    g_object_get(m_source.get(), "location", &originalURI.outPtr(), "resolved-location", &resolvedURI.outPtr(), nullptr);
    if (!originalURI || !resolvedURI)
        return false;
    if (!g_strcmp0(originalURI.get(), resolvedURI.get()))
        return true;

    Ref<SecurityOrigin> resolvedOrigin(SecurityOrigin::createFromString(String::fromUTF8(resolvedURI.get())));
    Ref<SecurityOrigin> requestedOrigin(SecurityOrigin::createFromString(String::fromUTF8(originalURI.get())));
    return resolvedOrigin->isSameSchemeHostPort(requestedOrigin.get());
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
            GST_DEBUG_OBJECT(pipeline(), "[Seek] seeked to %s", toString(m_seekTime).utf8().data());
            m_seeking = false;
            m_cachedPosition = MediaTime::invalidTime();
            if (m_timeOfOverlappingSeek != m_seekTime && m_timeOfOverlappingSeek.isValid()) {
                seek(m_timeOfOverlappingSeek);
                m_timeOfOverlappingSeek = MediaTime::invalidTime();
                return;
            }
            m_timeOfOverlappingSeek = MediaTime::invalidTime();

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
    GstState pending;
    GstState state;
    bool stateReallyChanged = false;

    GstStateChangeReturn getStateResult = gst_element_get_state(m_pipeline.get(), &state, &pending, 250 * GST_NSECOND);
    if (state != m_currentState) {
        m_oldState = m_currentState;
        m_currentState = state;
        stateReallyChanged = true;
    }

    bool shouldUpdatePlaybackState = false;
    switch (getStateResult) {
    case GST_STATE_CHANGE_SUCCESS: {
        GST_DEBUG_OBJECT(pipeline(), "State: %s, pending: %s", gst_element_state_get_name(m_currentState), gst_element_state_get_name(pending));

        // Do nothing if on EOS and state changed to READY to avoid recreating the player
        // on HTMLMediaElement and properly generate the video 'ended' event.
        if (m_isEndReached && m_currentState == GST_STATE_READY)
            break;

        m_resetPipeline = m_currentState <= GST_STATE_READY;

        bool didBuffering = m_buffering;

        // Update ready and network states.
        switch (m_currentState) {
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
                    GST_DEBUG_OBJECT(pipeline(), "[Buffering] Complete.");
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
        if (m_currentState == GST_STATE_PAUSED) {
            if (!m_volumeAndMuteInitialized) {
                notifyPlayerOfVolumeChange();
                notifyPlayerOfMute();
                m_volumeAndMuteInitialized = true;
            }

            if (didBuffering && !m_buffering && !m_paused && m_playbackRate) {
                GST_DEBUG_OBJECT(pipeline(), "[Buffering] Restarting playback.");
                changePipelineState(GST_STATE_PLAYING);
            }
        } else if (m_currentState == GST_STATE_PLAYING) {
            m_paused = false;

            if ((m_buffering && !isLiveStream()) || !m_playbackRate) {
                GST_DEBUG_OBJECT(pipeline(), "[Buffering] Pausing stream for buffering.");
                changePipelineState(GST_STATE_PAUSED);
            }
        } else
            m_paused = true;

        GST_DEBUG_OBJECT(pipeline(), "Old state: %s, new state: %s (requested: %s)", gst_element_state_get_name(m_oldState), gst_element_state_get_name(m_currentState), gst_element_state_get_name(m_requestedState));
        if (m_requestedState == GST_STATE_PAUSED && m_currentState == GST_STATE_PAUSED) {
            shouldUpdatePlaybackState = true;
            GST_INFO_OBJECT(pipeline(), "Requested state change to %s was completed", gst_element_state_get_name(m_currentState));
        }

        // Emit play state change notification only when going to PLAYING so that
        // the media element gets a chance to enable its page sleep disabler.
        // Emitting this notification in more cases triggers unwanted code paths
        // and test timeouts.
        if (stateReallyChanged && (m_oldState != m_currentState) && (m_oldState == GST_STATE_PAUSED && m_currentState == GST_STATE_PLAYING)) {
            GST_INFO_OBJECT(pipeline(), "Playback state changed from %s to %s. Notifying the media player client", gst_element_state_get_name(m_oldState), gst_element_state_get_name(m_currentState));
            shouldUpdatePlaybackState = true;
        }

        break;
    }
    case GST_STATE_CHANGE_ASYNC:
        GST_DEBUG_OBJECT(pipeline(), "Async: State: %s, pending: %s", gst_element_state_get_name(m_currentState), gst_element_state_get_name(pending));
        // Change in progress.
        break;
    case GST_STATE_CHANGE_FAILURE:
        GST_DEBUG_OBJECT(pipeline(), "Failure: State: %s, pending: %s", gst_element_state_get_name(m_currentState), gst_element_state_get_name(pending));
        // Change failed
        return;
    case GST_STATE_CHANGE_NO_PREROLL:
        GST_DEBUG_OBJECT(pipeline(), "No preroll: State: %s, pending: %s", gst_element_state_get_name(m_currentState), gst_element_state_get_name(pending));

        // Live pipelines go in PAUSED without prerolling.
        m_isStreaming = true;
        setDownloadBuffering();

        if (m_currentState == GST_STATE_READY)
            m_readyState = MediaPlayer::HaveNothing;
        else if (m_currentState == GST_STATE_PAUSED) {
            m_readyState = MediaPlayer::HaveEnoughData;
            m_paused = true;
        } else if (m_currentState == GST_STATE_PLAYING)
            m_paused = false;

        if (!m_paused && m_playbackRate)
            changePipelineState(GST_STATE_PLAYING);

        m_networkState = MediaPlayer::Loading;
        break;
    default:
        GST_DEBUG_OBJECT(pipeline(), "Else : %d", getStateResult);
        break;
    }

    m_requestedState = GST_STATE_VOID_PENDING;

    if (shouldUpdatePlaybackState)
        m_player->playbackStateChanged();

    if (m_networkState != oldNetworkState) {
        GST_DEBUG_OBJECT(pipeline(), "Network State Changed from %s to %s", convertEnumerationToString(oldNetworkState).utf8().data(), convertEnumerationToString(m_networkState).utf8().data());
        m_player->networkStateChanged();
    }
    if (m_readyState != oldReadyState) {
        GST_DEBUG_OBJECT(pipeline(), "Ready State Changed from %s to %s", convertEnumerationToString(oldReadyState).utf8().data(), convertEnumerationToString(m_readyState).utf8().data());
        m_player->readyStateChanged();
    }

    if (getStateResult == GST_STATE_CHANGE_SUCCESS && m_currentState >= GST_STATE_PAUSED) {
        updatePlaybackRate();
        if (m_seekIsPending) {
            GST_DEBUG_OBJECT(pipeline(), "[Seek] committing pending seek to %s", toString(m_seekTime).utf8().data());
            m_seekIsPending = false;
            m_seeking = doSeek(m_seekTime, m_player->rate(), static_cast<GstSeekFlags>(GST_SEEK_FLAG_FLUSH | GST_SEEK_FLAG_ACCURATE));
            if (!m_seeking) {
                m_cachedPosition = MediaTime::invalidTime();
                GST_DEBUG_OBJECT(pipeline(), "[Seek] seeking to %s failed", toString(m_seekTime).utf8().data());
            }
        }
    }
}

bool MediaPlayerPrivateGStreamer::handleSyncMessage(GstMessage* message)
{
#if GST_CHECK_VERSION(1, 10, 0)
    if (GST_MESSAGE_TYPE(message) == GST_MESSAGE_STREAM_COLLECTION && !m_isLegacyPlaybin) {
        GRefPtr<GstStreamCollection> collection;
        gst_message_parse_stream_collection(message, &collection.outPtr());

        if (collection) {
            m_streamCollection.swap(collection);
            m_notifier->notify(MainThreadNotification::StreamCollectionChanged, [this] {
                this->updateTracks();
            });
        }
    }
#endif

    return MediaPlayerPrivateGStreamerBase::handleSyncMessage(message);
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
    const gchar* newLocation = nullptr;

    if (!locations) {
        // Fallback on new-location string.
        newLocation = gst_structure_get_string(m_mediaLocations, "new-location");
        if (!newLocation)
            return false;
    }

    if (!newLocation) {
        if (m_mediaLocationCurrentIndex < 0) {
            m_mediaLocations = nullptr;
            return false;
        }

        const GValue* location = gst_value_list_get_value(locations, m_mediaLocationCurrentIndex);
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
        convertToInternalProtocol(newUrl);

        auto securityOrigin = SecurityOrigin::create(m_url);
        if (securityOrigin->canRequest(newUrl)) {
            GST_INFO_OBJECT(pipeline(), "New media url: %s", newUrl.string().utf8().data());

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
                setPlaybinURL(newUrl);
                changePipelineState(GST_STATE_PLAYING);
                return true;
            }
        } else
            GST_INFO_OBJECT(pipeline(), "Not allowed to load new media location: %s", newUrl.string().utf8().data());
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
    GST_INFO_OBJECT(pipeline(), "Playback ended");

    // Synchronize position and duration values to not confuse the
    // HTMLMediaElement. In some cases like reverse playback the
    // position is not always reported as 0 for instance.
    m_cachedPosition = MediaTime::invalidTime();
    MediaTime now = currentMediaTime();
    if (now > MediaTime { } && now <= durationMediaTime())
        m_player->durationChanged();

    m_isEndReached = true;
    timeChanged();

    if (!m_player->client().mediaPlayerIsLooping()) {
        m_paused = true;
        m_durationAtEOS = durationMediaTime();
        changePipelineState(GST_STATE_READY);
        m_downloadFinished = false;
    }
}

void MediaPlayerPrivateGStreamer::durationChanged()
{
    MediaTime previousDuration = durationMediaTime();

    // FIXME: Check if this method is still useful, because it's not doing its work at all
    // since bug #159458 removed a cacheDuration() call here.

    // Avoid emiting durationchanged in the case where the previous
    // duration was 0 because that case is already handled by the
    // HTMLMediaElement.
    if (previousDuration && durationMediaTime() != previousDuration)
        m_player->durationChanged();
}

void MediaPlayerPrivateGStreamer::loadingFailed(MediaPlayer::NetworkState error)
{
    GST_WARNING("Loading failed, error: %d", error);

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

void MediaPlayerPrivateGStreamer::getSupportedTypes(HashSet<String, ASCIICaseInsensitiveHash>& types)
{
    auto& gstRegistryScanner = GStreamerRegistryScanner::singleton();
    types = gstRegistryScanner.mimeTypeSet();
}

MediaPlayer::SupportsType MediaPlayerPrivateGStreamer::supportsType(const MediaEngineSupportParameters& parameters)
{
    MediaPlayer::SupportsType result = MediaPlayer::IsNotSupported;
#if ENABLE(MEDIA_SOURCE)
    // MediaPlayerPrivateGStreamerMSE is in charge of mediasource playback, not us.
    if (parameters.isMediaSource)
        return result;
#endif

#if !ENABLE(MEDIA_STREAM) || !GST_CHECK_VERSION(1, 10, 0)
    if (parameters.isMediaStream)
        return result;
#endif

    if (parameters.type.isEmpty())
        return result;

    GST_DEBUG("Checking mime-type \"%s\"", parameters.type.raw().utf8().data());
    auto containerType = parameters.type.containerType();
    auto& gstRegistryScanner = GStreamerRegistryScanner::singleton();
    if (gstRegistryScanner.isContainerTypeSupported(containerType)) {
        // Spec says we should not return "probably" if the codecs string is empty.
        Vector<String> codecs = parameters.type.codecs();
        result = codecs.isEmpty() ? MediaPlayer::MayBeSupported : (gstRegistryScanner.areAllCodecsSupported(codecs) ? MediaPlayer::IsSupported : MediaPlayer::IsNotSupported);
    }

    auto finalResult = extendedSupportsType(parameters, result);
    GST_DEBUG("Supported: %s", convertEnumerationToString(finalResult).utf8().data());
    return finalResult;
}

void MediaPlayerPrivateGStreamer::setDownloadBuffering()
{
    if (!m_pipeline)
        return;

    unsigned flags;
    g_object_get(m_pipeline.get(), "flags", &flags, nullptr);

    unsigned flagDownload = getGstPlayFlag("download");

    // We don't want to stop downloading if we already started it.
    if (flags & flagDownload && m_readyState > MediaPlayer::HaveNothing && !m_resetPipeline) {
        GST_DEBUG_OBJECT(pipeline(), "Download already started, not starting again");
        return;
    }

    bool shouldDownload = !isLiveStream() && m_preload == MediaPlayer::Auto;
    if (shouldDownload) {
        GST_INFO_OBJECT(pipeline(), "Enabling on-disk buffering");
        g_object_set(m_pipeline.get(), "flags", flags | flagDownload, nullptr);
        m_fillTimer.startRepeating(200_ms);
    } else {
        GST_INFO_OBJECT(pipeline(), "Disabling on-disk buffering");
        g_object_set(m_pipeline.get(), "flags", flags & ~flagDownload, nullptr);
        m_fillTimer.stop();
    }
}

void MediaPlayerPrivateGStreamer::setPreload(MediaPlayer::Preload preload)
{
    GST_DEBUG_OBJECT(pipeline(), "Setting preload to %s", convertEnumerationToString(preload).utf8().data());
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
    m_autoAudioSink = gst_element_factory_make("autoaudiosink", nullptr);
    if (!m_autoAudioSink) {
        GST_WARNING("GStreamer's autoaudiosink not found. Please check your gst-plugins-good installation");
        return nullptr;
    }

    g_signal_connect_swapped(m_autoAudioSink.get(), "child-added", G_CALLBACK(setAudioStreamPropertiesCallback), this);

    GstElement* audioSinkBin;

    if (webkitGstCheckVersion(1, 4, 2)) {
#if ENABLE(WEB_AUDIO)
        audioSinkBin = gst_bin_new("audio-sink");
        ensureAudioSourceProvider();
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
        ensureAudioSourceProvider();
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
    ensureAudioSourceProvider();
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

#if ENABLE(WEB_AUDIO)
void MediaPlayerPrivateGStreamer::ensureAudioSourceProvider()
{
    if (!m_audioSourceProvider)
        m_audioSourceProvider = std::make_unique<AudioSourceProviderGStreamer>();
}

AudioSourceProvider* MediaPlayerPrivateGStreamer::audioSourceProvider()
{
    ensureAudioSourceProvider();
    return m_audioSourceProvider.get();
}
#endif

void MediaPlayerPrivateGStreamer::createGSTPlayBin(const gchar* playbinName, const String& pipelineName)
{
    if (m_pipeline) {
        if (!playbinName) {
            GST_INFO_OBJECT(pipeline(), "Keeping same playbin as nothing forced");
            return;
        }

        if (!g_strcmp0(GST_OBJECT_NAME(gst_element_get_factory(m_pipeline.get())), playbinName)) {
            GST_INFO_OBJECT(pipeline(), "Already using %s", playbinName);
            return;
        }

        GST_INFO_OBJECT(pipeline(), "Tearing down as we need to use %s now.",
            playbinName);
        changePipelineState(GST_STATE_NULL);
        m_pipeline = nullptr;
    }

    ASSERT(!m_pipeline);

#if GST_CHECK_VERSION(1, 10, 0)
    if (Environment::get("USE_PLAYBIN3"))
        playbinName = "playbin3";
#else
    playbinName = "playbin";
#endif

    if (!playbinName)
        playbinName = "playbin";

    m_isLegacyPlaybin = !g_strcmp0(playbinName, "playbin");

    // gst_element_factory_make() returns a floating reference so
    // we should not adopt.
    setPipeline(gst_element_factory_make(playbinName,
        pipelineName.isEmpty() ? String::format("play_%p", this).utf8().data() : pipelineName.utf8().data()));
    setStreamVolumeElement(GST_STREAM_VOLUME(m_pipeline.get()));

    GST_INFO_OBJECT(pipeline(), "Using legacy playbin element: %s", boolForPrinting(m_isLegacyPlaybin));

    // Let also other listeners subscribe to (application) messages in this bus.
    GRefPtr<GstBus> bus = adoptGRef(gst_pipeline_get_bus(GST_PIPELINE(m_pipeline.get())));
    gst_bus_add_signal_watch_full(bus.get(), RunLoopSourcePriority::RunLoopDispatcher);
    g_signal_connect(bus.get(), "message", G_CALLBACK(busMessageCallback), this);

    g_object_set(m_pipeline.get(), "mute", m_player->muted(), nullptr);

    g_signal_connect_swapped(m_pipeline.get(), "source-setup", G_CALLBACK(sourceSetupCallback), this);
    if (m_isLegacyPlaybin) {
        g_signal_connect_swapped(m_pipeline.get(), "video-changed", G_CALLBACK(videoChangedCallback), this);
        g_signal_connect_swapped(m_pipeline.get(), "audio-changed", G_CALLBACK(audioChangedCallback), this);
    }

#if ENABLE(VIDEO_TRACK)
    if (m_isLegacyPlaybin)
        g_signal_connect_swapped(m_pipeline.get(), "text-changed", G_CALLBACK(textChangedCallback), this);

    GstElement* textCombiner = webkitTextCombinerNew();
    ASSERT(textCombiner);
    g_object_set(m_pipeline.get(), "text-stream-combiner", textCombiner, nullptr);

    m_textAppSink = webkitTextSinkNew();
    ASSERT(m_textAppSink);

    m_textAppSinkPad = adoptGRef(gst_element_get_static_pad(m_textAppSink.get(), "sink"));
    ASSERT(m_textAppSinkPad);

    GRefPtr<GstCaps> textCaps;
    if (webkitGstCheckVersion(1, 13, 0))
        textCaps = adoptGRef(gst_caps_new_empty_simple("application/x-subtitle-vtt"));
    else
        textCaps = adoptGRef(gst_caps_new_empty_simple("text/vtt"));
    g_object_set(m_textAppSink.get(), "emit-signals", TRUE, "enable-last-sample", FALSE, "caps", textCaps.get(), nullptr);
    g_signal_connect_swapped(m_textAppSink.get(), "new-sample", G_CALLBACK(newTextSampleCallback), this);

    g_object_set(m_pipeline.get(), "text-sink", m_textAppSink.get(), nullptr);
#endif

    g_object_set(m_pipeline.get(), "video-sink", createVideoSink(), "audio-sink", createAudioSink(), nullptr);

    configurePlaySink();

    // On 1.4.2 and newer we use the audio-filter property instead.
    // See https://bugzilla.gnome.org/show_bug.cgi?id=735748 for
    // the reason for using >= 1.4.2 instead of >= 1.4.0.
    if (m_preservesPitch && webkitGstCheckVersion(1, 4, 2)) {
        GstElement* scale = gst_element_factory_make("scaletempo", nullptr);

        if (!scale)
            GST_WARNING("Failed to create scaletempo");
        else
            g_object_set(m_pipeline.get(), "audio-filter", scale, nullptr);
    }

    if (!m_renderingCanBeAccelerated) {
        // If not using accelerated compositing, let GStreamer handle
        // the image-orientation tag.
        GstElement* videoFlip = gst_element_factory_make("videoflip", nullptr);
        if (videoFlip) {
            g_object_set(videoFlip, "method", 8, nullptr);
            g_object_set(m_pipeline.get(), "video-filter", videoFlip, nullptr);
        } else
            GST_WARNING("The videoflip element is missing, video rotation support is now disabled. Please check your gst-plugins-good installation.");
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

Optional<bool> MediaPlayerPrivateGStreamer::wouldTaintOrigin(const SecurityOrigin&) const
{
    // Ideally the given origin should always be verified with
    // webKitSrcWouldTaintOrigin() instead of only checking it for
    // adaptive-streaming-statistics. We can't do this yet because HLS fragments
    // are currently downloaded independently from WebKit.
    // See also https://bugs.webkit.org/show_bug.cgi?id=189967.
    return m_hasTaintedOrigin;
}


}

#endif // USE(GSTREAMER)
