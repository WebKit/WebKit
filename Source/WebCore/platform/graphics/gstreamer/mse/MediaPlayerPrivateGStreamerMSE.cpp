/*
 * Copyright (C) 2007, 2009 Apple Inc.  All rights reserved.
 * Copyright (C) 2007 Collabora Ltd.  All rights reserved.
 * Copyright (C) 2007 Alp Toker <alp@atoker.com>
 * Copyright (C) 2009 Gustavo Noronha Silva <gns@gnome.org>
 * Copyright (C) 2009, 2010, 2011, 2012, 2013, 2016, 2017 Igalia S.L
 * Copyright (C) 2015 Sebastian Dröge <sebastian@centricular.com>
 * Copyright (C) 2015, 2016, 2017 Metrological Group B.V.
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
#include "MediaPlayerPrivateGStreamerMSE.h"

#if ENABLE(VIDEO) && USE(GSTREAMER) && ENABLE(MEDIA_SOURCE)

#include "AppendPipeline.h"
#include "AudioTrackPrivateGStreamer.h"
#include "GStreamerCommon.h"
#include "GStreamerRegistryScannerMSE.h"
#include "InbandTextTrackPrivateGStreamer.h"
#include "MIMETypeRegistry.h"
#include "MediaDescription.h"
#include "MediaPlayer.h"
#include "PlaybackPipeline.h"
#include "SourceBufferPrivateGStreamer.h"
#include "TimeRanges.h"
#include "VideoTrackPrivateGStreamer.h"

#include <gst/app/gstappsink.h>
#include <gst/app/gstappsrc.h>
#include <gst/gst.h>
#include <gst/pbutils/pbutils.h>
#include <gst/video/video.h>
#include <wtf/Condition.h>
#include <wtf/HashSet.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/StringPrintStream.h>
#include <wtf/URL.h>
#include <wtf/text/AtomString.h>
#include <wtf/text/AtomStringHash.h>

static const char* dumpReadyState(WebCore::MediaPlayer::ReadyState readyState)
{
    switch (readyState) {
    case WebCore::MediaPlayer::ReadyState::HaveNothing: return "HaveNothing";
    case WebCore::MediaPlayer::ReadyState::HaveMetadata: return "HaveMetadata";
    case WebCore::MediaPlayer::ReadyState::HaveCurrentData: return "HaveCurrentData";
    case WebCore::MediaPlayer::ReadyState::HaveFutureData: return "HaveFutureData";
    case WebCore::MediaPlayer::ReadyState::HaveEnoughData: return "HaveEnoughData";
    default: return "(unknown)";
    }
}

GST_DEBUG_CATEGORY(webkit_mse_debug);
#define GST_CAT_DEFAULT webkit_mse_debug

namespace WebCore {

class MediaPlayerFactoryGStreamerMSE final : public MediaPlayerFactory {
private:
    MediaPlayerEnums::MediaEngineIdentifier identifier() const final { return MediaPlayerEnums::MediaEngineIdentifier::GStreamerMSE; };

    std::unique_ptr<MediaPlayerPrivateInterface> createMediaEnginePlayer(MediaPlayer* player) const final
    {
        return makeUnique<MediaPlayerPrivateGStreamerMSE>(player);
    }

    void getSupportedTypes(HashSet<String, ASCIICaseInsensitiveHash>& types) const final
    {
        return MediaPlayerPrivateGStreamerMSE::getSupportedTypes(types);
    }

    MediaPlayer::SupportsType supportsTypeAndCodecs(const MediaEngineSupportParameters& parameters) const final
    {
        return MediaPlayerPrivateGStreamerMSE::supportsType(parameters);
    }

    bool supportsKeySystem(const String& keySystem, const String& mimeType) const final
    {
        return MediaPlayerPrivateGStreamerMSE::supportsKeySystem(keySystem, mimeType);
    }
};

void MediaPlayerPrivateGStreamerMSE::registerMediaEngine(MediaEngineRegistrar registrar)
{
    initializeGStreamerAndRegisterWebKitElements();
    GST_DEBUG_CATEGORY_INIT(webkit_mse_debug, "webkitmse", 0, "WebKit MSE media player");
    if (isAvailable())
        registrar(makeUnique<MediaPlayerFactoryGStreamerMSE>());
}

MediaPlayerPrivateGStreamerMSE::MediaPlayerPrivateGStreamerMSE(MediaPlayer* player)
    : MediaPlayerPrivateGStreamer(player)
{
    GST_TRACE("creating the player (%p)", this);
}

MediaPlayerPrivateGStreamerMSE::~MediaPlayerPrivateGStreamerMSE()
{
    GST_TRACE("destroying the player (%p)", this);

    if (m_source) {
        webKitMediaSrcSetMediaPlayerPrivate(WEBKIT_MEDIA_SRC(m_source.get()), nullptr);
        g_signal_handlers_disconnect_by_data(m_source.get(), this);
    }

    if (m_playbackPipeline)
        m_playbackPipeline->setWebKitMediaSrc(nullptr);
}

void MediaPlayerPrivateGStreamerMSE::load(const String& urlString)
{
    if (!urlString.startsWith("mediasource")) {
        // Properly fail so the global MediaPlayer tries to fallback to the next MediaPlayerPrivate.
        m_networkState = MediaPlayer::NetworkState::FormatError;
        m_player->networkStateChanged();
        return;
    }

    if (!m_playbackPipeline)
        m_playbackPipeline = PlaybackPipeline::create();

    MediaPlayerPrivateGStreamer::load(urlString);
}

void MediaPlayerPrivateGStreamerMSE::load(const String& url, MediaSourcePrivateClient* mediaSource)
{
    m_mediaSource = mediaSource;
    load(makeString("mediasource", url));
}

void MediaPlayerPrivateGStreamerMSE::pause()
{
    // FIXME: Should not need direct access to this member. This override is probably not needed.
    m_isPaused = true;
    MediaPlayerPrivateGStreamer::pause();
}

MediaTime MediaPlayerPrivateGStreamerMSE::durationMediaTime() const
{
    if (UNLIKELY(!m_pipeline || m_didErrorOccur))
        return MediaTime();

    return m_mediaTimeDuration;
}

void MediaPlayerPrivateGStreamerMSE::seek(const MediaTime& time)
{
    if (UNLIKELY(!m_pipeline || m_didErrorOccur))
        return;

    GST_INFO_OBJECT(pipeline(), "[Seek] seek attempt to %s secs", toString(time).utf8().data());

    // Avoid useless seeking.
    MediaTime current = currentMediaTime();
    if (time == current) {
        if (!m_isSeeking)
            timeChanged();
        return;
    }

    if (m_isLiveStream)
        return;

    if (m_isSeeking && m_isSeekPending) {
        m_seekTime = time;
        return;
    }

    GST_DEBUG_OBJECT(pipeline(), "Seeking from %s to %s seconds", toString(current).utf8().data(), toString(time).utf8().data());

    MediaTime previousSeekTime = m_seekTime;
    m_seekTime = time;

    if (!doSeek(m_seekTime, m_playbackRate, static_cast<GstSeekFlags>(GST_SEEK_FLAG_FLUSH | GST_SEEK_FLAG_ACCURATE))) {
        m_seekTime = previousSeekTime;
        GST_WARNING_OBJECT(pipeline(), "Seeking to %s failed", toString(time).utf8().data());
        return;
    }

    m_isEndReached = false;
    GST_DEBUG_OBJECT(pipeline(), "m_isSeeking=%s, m_seekTime=%s", boolForPrinting(m_isSeeking), toString(m_seekTime).utf8().data());
}

void MediaPlayerPrivateGStreamerMSE::configurePlaySink()
{
    MediaPlayerPrivateGStreamer::configurePlaySink();

    GRefPtr<GstElement> playsink = adoptGRef(gst_bin_get_by_name(GST_BIN(m_pipeline.get()), "playsink"));
    if (playsink) {
        // The default value (0) means "send events to all the sinks", instead
        // of "only to the first that returns true". This is needed for MSE seek.
        g_object_set(G_OBJECT(playsink.get()), "send-event-mode", 0, nullptr);
    }
}

bool MediaPlayerPrivateGStreamerMSE::changePipelineState(GstState newState)
{
    if (seeking()) {
        GST_DEBUG("Rejected state change to %s while seeking",
            gst_element_state_get_name(newState));
        return true;
    }

    return MediaPlayerPrivateGStreamer::changePipelineState(newState);
}

void MediaPlayerPrivateGStreamerMSE::notifySeekNeedsDataForTime(const MediaTime& seekTime)
{
    // Reenqueue samples needed to resume playback in the new position.
    m_mediaSource->seekToTime(seekTime);

    GST_DEBUG("MSE seek to %s finished", toString(seekTime).utf8().data());

    if (!m_gstSeekCompleted) {
        m_gstSeekCompleted = true;
        maybeFinishSeek();
    }
}

static bool checkShouldDelaySeek(GstStateChangeReturn getStateResult, GstState currentState, GstState newState)
{
    if (getStateResult != GST_STATE_CHANGE_ASYNC)
        return false;
    if (GST_STATE_TRANSITION(currentState, newState) == GST_STATE_CHANGE_PLAYING_TO_PAUSED)
        return false;
    if (currentState == GST_STATE_READY && newState >= GST_STATE_PAUSED)
        return false;
    return true;
}

bool MediaPlayerPrivateGStreamerMSE::doSeek(const MediaTime& position, float rate, GstSeekFlags seekType)
{
    // FIXME: Make a copy here because in some cases below it is modified. This
    // sounds like a bad idea and should be investigated further.
    MediaTime seekTime = position;

    // Always move to seeking state to report correct 'currentTime' while pending for actual seek to complete.
    m_isSeeking = true;

    // Check if playback pipeline is ready for seek.
    GstState state, newState;
    GstStateChangeReturn getStateResult = gst_element_get_state(m_pipeline.get(), &state, &newState, 0);
    if (getStateResult == GST_STATE_CHANGE_FAILURE || getStateResult == GST_STATE_CHANGE_NO_PREROLL) {
        GST_DEBUG_OBJECT(pipeline(), "[Seek] cannot seek, current state change is %s", gst_element_state_change_return_get_name(getStateResult));
        webKitMediaSrcSetReadyForSamples(WEBKIT_MEDIA_SRC(m_source.get()), true);
        m_isSeeking = false;
        return false;
    }

    bool shouldDelaySeek = checkShouldDelaySeek(getStateResult, state, newState);
    if (shouldDelaySeek || m_isEndReached || !m_gstSeekCompleted) {
        CString reason = "Unknown reason";
        if (shouldDelaySeek) {
            reason = makeString("In async change ",
                gst_element_state_get_name(state), " --> ",
                gst_element_state_get_name(newState)).utf8();
        } else if (state < GST_STATE_PAUSED)
            reason = "State less than PAUSED";
        else if (m_isEndReached)
            reason = "End reached";
        else if (!m_gstSeekCompleted)
            reason = "Previous seek is not finished yet";

        GST_DEBUG_OBJECT(pipeline(), "[Seek] Delaying the seek: %s", reason.data());

        m_isSeekPending = true;

        if (m_isEndReached) {
            GST_DEBUG("[Seek] reset pipeline");
            m_shouldResetPipeline = true;
            m_isSeeking = false;
            if (!changePipelineState(GST_STATE_PAUSED))
                loadingFailed(MediaPlayer::NetworkState::Empty);
            else
                m_isSeeking = true;
        }

        return m_isSeeking;
    }

    // Stop accepting new samples until actual seek is finished.
    webKitMediaSrcSetReadyForSamples(WEBKIT_MEDIA_SRC(m_source.get()), false);

    // Correct seek time if it helps to fix a small gap.
    if (!isTimeBuffered(seekTime)) {
        // Look if a near future time (<0.1 sec.) is buffered and change the seek target time.
        if (m_mediaSource) {
            const MediaTime miniGap = MediaTime(1, 10);
            MediaTime nearest = m_mediaSource->buffered()->nearest(seekTime);
            if (nearest.isValid() && nearest > seekTime && (nearest - seekTime) <= miniGap && isTimeBuffered(nearest + miniGap)) {
                GST_DEBUG_OBJECT(pipeline(), "[Seek] Changed the seek target time from %s to %s, a near point in the future", toString(seekTime).utf8().data(), toString(nearest).utf8().data());
                seekTime = nearest;
            }
        }
    }

    // Check if MSE has samples for requested time and defer actual seek if needed.
    if (!isTimeBuffered(seekTime)) {
        GST_DEBUG_OBJECT(pipeline(), "[Seek] Delaying the seek: MSE is not ready");
        GstStateChangeReturn setStateResult = gst_element_set_state(m_pipeline.get(), GST_STATE_PAUSED);
        if (setStateResult == GST_STATE_CHANGE_FAILURE) {
            GST_DEBUG_OBJECT(pipeline(), "[Seek] Cannot seek, failed to pause playback pipeline.");
            webKitMediaSrcSetReadyForSamples(WEBKIT_MEDIA_SRC(m_source.get()), true);
            m_isSeeking = false;
            return false;
        }
        m_readyState = MediaPlayer::ReadyState::HaveMetadata;
        notifySeekNeedsDataForTime(seekTime);
        ASSERT(!m_mseSeekCompleted);
        return true;
    }

    // Complete previous MSE seek if needed.
    if (!m_mseSeekCompleted) {
        m_mediaSource->monitorSourceBuffers();
        ASSERT(m_mseSeekCompleted);
        // Note: seekCompleted will recursively call us.
        return m_isSeeking;
    }

    GST_DEBUG_OBJECT(pipeline(), "We can seek now");

    MediaTime startTime = seekTime, endTime = MediaTime::invalidTime();

    if (rate < 0) {
        startTime = MediaTime::zeroTime();
        endTime = seekTime;
    }

    if (!rate)
        rate = 1;

    GST_DEBUG_OBJECT(pipeline(), "Actual seek to %s, end time:  %s, rate: %f", toString(startTime).utf8().data(), toString(endTime).utf8().data(), rate);

    m_gstSeekCompleted = false;
    if (state < GST_STATE_PAUSED) {
        // Special case of initial seek. We set the right segment instead of a seek.
        webKitMediaSrcPrepareInitialSeek(WEBKIT_MEDIA_SRC(m_source.get()), rate, startTime, endTime);
        notifySeekNeedsDataForTime(seekTime);
        GST_DEBUG("Initial seek succeeded, returning true");
    } else {
        // This will call notifySeekNeedsData() after some time to tell that the pipeline is ready for sample enqueuing.
        webKitMediaSrcPrepareSeek(WEBKIT_MEDIA_SRC(m_source.get()), seekTime);

        if (!gst_element_seek(m_pipeline.get(), rate, GST_FORMAT_TIME, seekType, GST_SEEK_TYPE_SET, toGstClockTime(startTime), GST_SEEK_TYPE_SET, toGstClockTime(endTime))) {
            webKitMediaSrcSetReadyForSamples(WEBKIT_MEDIA_SRC(m_source.get()), true);
            m_isSeeking = false;
            m_gstSeekCompleted = true;
            GST_DEBUG("gst_element_seek() failed, returning false");
            return false;
        }
        // The samples will be enqueued in notifySeekNeedsData().
        GST_DEBUG("gst_element_seek() succeeded, returning true");
    }
    return true;
}

void MediaPlayerPrivateGStreamerMSE::maybeFinishSeek()
{
    if (!m_isSeeking || !m_mseSeekCompleted || !m_gstSeekCompleted)
        return;

    GstState state, newState;
    GstStateChangeReturn getStateResult = gst_element_get_state(m_pipeline.get(), &state, &newState, 0);

    if (getStateResult == GST_STATE_CHANGE_ASYNC
        && !(state == GST_STATE_PLAYING && newState == GST_STATE_PAUSED)) {
        GST_DEBUG("[Seek] Delaying seek finish");
        return;
    }

    if (m_isSeekPending) {
        GST_DEBUG("[Seek] Committing pending seek to %s", toString(m_seekTime).utf8().data());
        m_isSeekPending = false;
        if (!doSeek(m_seekTime, m_playbackRate, static_cast<GstSeekFlags>(GST_SEEK_FLAG_FLUSH | GST_SEEK_FLAG_ACCURATE))) {
            GST_WARNING("[Seek] Seeking to %s failed", toString(m_seekTime).utf8().data());
            m_cachedPosition = MediaTime::invalidTime();
        }
        return;
    }

    GST_DEBUG("[Seek] Seeked to %s", toString(m_seekTime).utf8().data());

    webKitMediaSrcSetReadyForSamples(WEBKIT_MEDIA_SRC(m_source.get()), true);
    m_isSeeking = false;
    m_cachedPosition = MediaTime::invalidTime();
    // The pipeline can still have a pending state. In this case a position query will fail.
    // Right now we can use m_seekTime as a fallback.
    m_canFallBackToLastFinishedSeekPosition = true;
    timeChanged();
}

bool MediaPlayerPrivateGStreamerMSE::seeking() const
{
    return m_isSeeking;
}

// FIXME: MediaPlayerPrivateGStreamer manages the ReadyState on its own. We shouldn't change it manually.
void MediaPlayerPrivateGStreamerMSE::setReadyState(MediaPlayer::ReadyState readyState)
{
    if (readyState == m_readyState)
        return;

    if (seeking()) {
        GST_DEBUG("Skip ready state change(%s -> %s) due to seek\n", dumpReadyState(m_readyState), dumpReadyState(readyState));
        return;
    }

    GST_DEBUG("Ready State Changed manually from %u to %u", static_cast<unsigned>(m_readyState), static_cast<unsigned>(readyState));
    MediaPlayer::ReadyState oldReadyState = m_readyState;
    m_readyState = readyState;
    GST_DEBUG("m_readyState: %s -> %s", dumpReadyState(oldReadyState), dumpReadyState(m_readyState));

    if (oldReadyState < MediaPlayer::ReadyState::HaveCurrentData && m_readyState >= MediaPlayer::ReadyState::HaveCurrentData) {
        GST_DEBUG("[Seek] Reporting load state changed to trigger seek continuation");
        loadStateChanged();
    }
    m_player->readyStateChanged();

    GstState pipelineState;
    GstStateChangeReturn getStateResult = gst_element_get_state(m_pipeline.get(), &pipelineState, nullptr, 250 * GST_NSECOND);
    bool isPlaying = (getStateResult == GST_STATE_CHANGE_SUCCESS && pipelineState == GST_STATE_PLAYING);

    if (m_readyState == MediaPlayer::ReadyState::HaveMetadata && oldReadyState > MediaPlayer::ReadyState::HaveMetadata && isPlaying) {
        GST_TRACE("Changing pipeline to PAUSED...");
        bool ok = changePipelineState(GST_STATE_PAUSED);
        GST_TRACE("Changed pipeline to PAUSED: %s", ok ? "Success" : "Error");
    }
}

void MediaPlayerPrivateGStreamerMSE::waitForSeekCompleted()
{
    if (!m_isSeeking)
        return;

    GST_DEBUG("Waiting for MSE seek completed");
    m_mseSeekCompleted = false;
}

void MediaPlayerPrivateGStreamerMSE::seekCompleted()
{
    if (m_mseSeekCompleted)
        return;

    GST_DEBUG("MSE seek completed");
    m_mseSeekCompleted = true;

    doSeek(m_seekTime, m_playbackRate, static_cast<GstSeekFlags>(GST_SEEK_FLAG_FLUSH | GST_SEEK_FLAG_ACCURATE));

    if (!seeking() && m_readyState >= MediaPlayer::ReadyState::HaveFutureData)
        changePipelineState(GST_STATE_PLAYING);

    if (!seeking())
        m_player->timeChanged();
}

std::unique_ptr<PlatformTimeRanges> MediaPlayerPrivateGStreamerMSE::buffered() const
{
    return m_mediaSource ? m_mediaSource->buffered() : makeUnique<PlatformTimeRanges>();
}

void MediaPlayerPrivateGStreamerMSE::sourceSetup(GstElement* sourceElement)
{
    m_source = sourceElement;

    ASSERT(WEBKIT_IS_MEDIA_SRC(m_source.get()));

    m_playbackPipeline->setWebKitMediaSrc(WEBKIT_MEDIA_SRC(m_source.get()));

    MediaSourcePrivateGStreamer::open(*m_mediaSource.get(), *this);
    g_signal_connect_swapped(m_source.get(), "video-changed", G_CALLBACK(videoChangedCallback), this);
    g_signal_connect_swapped(m_source.get(), "audio-changed", G_CALLBACK(audioChangedCallback), this);
    g_signal_connect_swapped(m_source.get(), "text-changed", G_CALLBACK(textChangedCallback), this);
    webKitMediaSrcSetMediaPlayerPrivate(WEBKIT_MEDIA_SRC(m_source.get()), this);
}

void MediaPlayerPrivateGStreamerMSE::updateStates()
{
    if (UNLIKELY(!m_pipeline || m_didErrorOccur))
        return;

    MediaPlayer::NetworkState oldNetworkState = m_networkState;
    MediaPlayer::ReadyState oldReadyState = m_readyState;
    GstState state, pending;

    GstStateChangeReturn getStateResult = gst_element_get_state(m_pipeline.get(), &state, &pending, 250 * GST_NSECOND);

    bool shouldUpdatePlaybackState = false;
    switch (getStateResult) {
    case GST_STATE_CHANGE_SUCCESS: {
        GST_DEBUG("State: %s, pending: %s", gst_element_state_get_name(state), gst_element_state_get_name(pending));

        // Do nothing if on EOS and state changed to READY to avoid recreating the player
        // on HTMLMediaElement and properly generate the video 'ended' event.
        if (m_isEndReached && state == GST_STATE_READY)
            break;

        m_shouldResetPipeline = (state <= GST_STATE_READY);
        if (m_shouldResetPipeline)
            m_mediaTimeDuration = MediaTime::zeroTime();

        // Update ready and network states.
        switch (state) {
        case GST_STATE_NULL:
            m_readyState = MediaPlayer::ReadyState::HaveNothing;
            GST_DEBUG("m_readyState=%s", dumpReadyState(m_readyState));
            m_networkState = MediaPlayer::NetworkState::Empty;
            break;
        case GST_STATE_READY:
            m_readyState = MediaPlayer::ReadyState::HaveMetadata;
            GST_DEBUG("m_readyState=%s", dumpReadyState(m_readyState));
            m_networkState = MediaPlayer::NetworkState::Empty;
            break;
        case GST_STATE_PAUSED:
        case GST_STATE_PLAYING:
            if (seeking()) {
                m_readyState = MediaPlayer::ReadyState::HaveMetadata;
                // FIXME: Should we manage NetworkState too?
                GST_DEBUG("m_readyState=%s", dumpReadyState(m_readyState));
            } else {
                if (m_readyState < MediaPlayer::ReadyState::HaveFutureData)
                    m_readyState = MediaPlayer::ReadyState::HaveFutureData;
                GST_DEBUG("m_readyState=%s", dumpReadyState(m_readyState));
                m_networkState = MediaPlayer::NetworkState::Loading;
            }

            if (m_eosMarked && state == GST_STATE_PLAYING)
                m_eosPending = true;

            break;
        default:
            ASSERT_NOT_REACHED();
            break;
        }

        // Sync states where needed.
        if (state == GST_STATE_PAUSED) {
            if (!m_areVolumeAndMuteInitialized) {
                notifyPlayerOfVolumeChange();
                notifyPlayerOfMute();
                m_areVolumeAndMuteInitialized = true;
            }

            if (!seeking() && !m_isPaused && m_playbackRate) {
                GST_DEBUG("[Buffering] Restarting playback.");
                changePipelineState(GST_STATE_PLAYING);
            }
        } else if (state == GST_STATE_PLAYING) {
            m_isPaused = false;

            if (!m_playbackRate) {
                GST_DEBUG("[Buffering] Pausing stream for buffering.");
                changePipelineState(GST_STATE_PAUSED);
            }
        } else
            m_isPaused = true;

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
        GST_WARNING("Failure: State: %s, pending: %s", gst_element_state_get_name(state), gst_element_state_get_name(pending));
        // Change failed.
        return;
    case GST_STATE_CHANGE_NO_PREROLL:
        GST_DEBUG("No preroll: State: %s, pending: %s", gst_element_state_get_name(state), gst_element_state_get_name(pending));

        // Live pipelines go in PAUSED without prerolling.
        m_isLiveStream = true;

        if (state == GST_STATE_READY) {
            m_readyState = MediaPlayer::ReadyState::HaveNothing;
            GST_DEBUG("m_readyState=%s", dumpReadyState(m_readyState));
        } else if (state == GST_STATE_PAUSED) {
            m_readyState = MediaPlayer::ReadyState::HaveEnoughData;
            GST_DEBUG("m_readyState=%s", dumpReadyState(m_readyState));
            m_isPaused = true;
        } else if (state == GST_STATE_PLAYING)
            m_isPaused = false;

        if (!m_isPaused && m_playbackRate)
            changePipelineState(GST_STATE_PLAYING);

        m_networkState = MediaPlayer::NetworkState::Loading;
        break;
    default:
        GST_DEBUG("Else : %d", getStateResult);
        break;
    }

    m_requestedState = GST_STATE_VOID_PENDING;

    if (shouldUpdatePlaybackState)
        m_player->playbackStateChanged();

    if (m_networkState != oldNetworkState) {
        GST_DEBUG("Network State Changed from %u to %u", static_cast<unsigned>(oldNetworkState), static_cast<unsigned>(m_networkState));
        m_player->networkStateChanged();
    }
    if (m_readyState != oldReadyState) {
        GST_DEBUG("Ready State Changed from %u to %u", static_cast<unsigned>(oldReadyState), static_cast<unsigned>(m_readyState));
        m_player->readyStateChanged();
    }

    if (getStateResult == GST_STATE_CHANGE_SUCCESS && state >= GST_STATE_PAUSED) {
        updatePlaybackRate();
        maybeFinishSeek();
    }
}
void MediaPlayerPrivateGStreamerMSE::asyncStateChangeDone()
{
    if (UNLIKELY(!m_pipeline || m_didErrorOccur))
        return;

    if (m_isSeeking)
        maybeFinishSeek();
    else
        updateStates();
}

bool MediaPlayerPrivateGStreamerMSE::isTimeBuffered(const MediaTime &time) const
{
    bool result = m_mediaSource && m_mediaSource->buffered()->contain(time);
    GST_DEBUG("Time %s buffered? %s", toString(time).utf8().data(), boolForPrinting(result));
    return result;
}

void MediaPlayerPrivateGStreamerMSE::blockDurationChanges()
{
    ASSERT(isMainThread());
    m_areDurationChangesBlocked = true;
    m_shouldReportDurationWhenUnblocking = false;
}

void MediaPlayerPrivateGStreamerMSE::unblockDurationChanges()
{
    ASSERT(isMainThread());
    if (m_shouldReportDurationWhenUnblocking) {
        m_player->durationChanged();
        m_playbackPipeline->notifyDurationChanged();
        m_shouldReportDurationWhenUnblocking = false;
    }

    m_areDurationChangesBlocked = false;
}

void MediaPlayerPrivateGStreamerMSE::durationChanged()
{
    ASSERT(isMainThread());

    MediaTime previousDuration = m_mediaTimeDuration;
    m_mediaTimeDuration = m_mediaSource->duration();

    GST_TRACE("previous=%s, new=%s", toString(previousDuration).utf8().data(), toString(m_mediaTimeDuration).utf8().data());

    // Avoid emiting durationchanged in the case where the previous duration was 0 because that case is already handled
    // by the HTMLMediaElement.
    if (m_mediaTimeDuration != previousDuration && m_mediaTimeDuration.isValid() && previousDuration.isValid()) {
        if (!m_areDurationChangesBlocked) {
            m_player->durationChanged();
            m_playbackPipeline->notifyDurationChanged();
        } else
            m_shouldReportDurationWhenUnblocking = true;
    }
}

void MediaPlayerPrivateGStreamerMSE::trackDetected(AppendPipeline& appendPipeline, RefPtr<WebCore::TrackPrivateBase> newTrack, bool firstTrackDetected)
{
    ASSERT(appendPipeline.track() == newTrack);

    GstCaps* caps = appendPipeline.appsinkCaps();
    ASSERT(caps);
    GST_DEBUG("track ID: %s, caps: %" GST_PTR_FORMAT, newTrack->id().string().latin1().data(), caps);

    if (doCapsHaveType(caps, GST_VIDEO_CAPS_TYPE_PREFIX)) {
        Optional<FloatSize> size = getVideoResolutionFromCaps(caps);
        if (size.hasValue())
            m_videoSize = size.value();
    }

    if (firstTrackDetected)
        m_playbackPipeline->attachTrack(appendPipeline.sourceBufferPrivate(), newTrack, caps);
    else
        m_playbackPipeline->reattachTrack(appendPipeline.sourceBufferPrivate(), newTrack, caps);
}

void MediaPlayerPrivateGStreamerMSE::getSupportedTypes(HashSet<String, ASCIICaseInsensitiveHash>& types)
{
    auto& gstRegistryScanner = GStreamerRegistryScannerMSE::singleton();
    types = gstRegistryScanner.mimeTypeSet();
}

MediaPlayer::SupportsType MediaPlayerPrivateGStreamerMSE::supportsType(const MediaEngineSupportParameters& parameters)
{
    MediaPlayer::SupportsType result = MediaPlayer::SupportsType::IsNotSupported;
    if (!parameters.isMediaSource)
        return result;

    auto containerType = parameters.type.containerType();

    // YouTube TV provides empty types for some videos and we want to be selected as best media engine for them.
    if (containerType.isEmpty()) {
        result = MediaPlayer::SupportsType::MayBeSupported;
        GST_DEBUG("mime-type \"%s\" supported: %s", parameters.type.raw().utf8().data(), convertEnumerationToString(result).utf8().data());
        return result;
    }

    GST_DEBUG("Checking mime-type \"%s\"", parameters.type.raw().utf8().data());
    auto& gstRegistryScanner = GStreamerRegistryScannerMSE::singleton();
    result = gstRegistryScanner.isContentTypeSupported(parameters.type, parameters.contentTypesRequiringHardwareSupport);

    auto finalResult = extendedSupportsType(parameters, result);
    GST_DEBUG("Supported: %s", convertEnumerationToString(finalResult).utf8().data());
    return finalResult;
}

void MediaPlayerPrivateGStreamerMSE::markEndOfStream(MediaSourcePrivate::EndOfStreamStatus status)
{
    if (status != MediaSourcePrivate::EosNoError)
        return;

    GST_DEBUG("Marking end of stream");
    m_eosMarked = true;
    updateStates();
}

MediaTime MediaPlayerPrivateGStreamerMSE::currentMediaTime() const
{
    MediaTime position = MediaPlayerPrivateGStreamer::currentMediaTime();

    if (m_eosPending && position >= durationMediaTime()) {
        if (m_networkState != MediaPlayer::NetworkState::Loaded) {
            m_networkState = MediaPlayer::NetworkState::Loaded;
            m_player->networkStateChanged();
        }

        m_eosPending = false;
        m_isEndReached = true;
        m_cachedPosition = m_mediaTimeDuration;
        m_player->timeChanged();
    }
    return position;
}

MediaTime MediaPlayerPrivateGStreamerMSE::maxMediaTimeSeekable() const
{
    if (UNLIKELY(m_didErrorOccur))
        return MediaTime::zeroTime();

    GST_DEBUG("maxMediaTimeSeekable");
    MediaTime result = durationMediaTime();
    // Infinite duration means live stream.
    if (result.isPositiveInfinite()) {
        MediaTime maxBufferedTime = buffered()->maximumBufferedTime();
        // Return the highest end time reported by the buffered attribute.
        result = maxBufferedTime.isValid() ? maxBufferedTime : MediaTime::zeroTime();
    }

    return result;
}

} // namespace WebCore.

#endif // USE(GSTREAMER)
