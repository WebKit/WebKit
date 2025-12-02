/*
 * Copyright (C) 2007, 2009 Apple Inc. All rights reserved.
 * Copyright (C) 2007 Collabora Ltd. All rights reserved.
 * Copyright (C) 2007 Alp Toker <alp@atoker.com>
 * Copyright (C) 2009 Gustavo Noronha Silva <gns@gnome.org>
 * Copyright (C) 2009, 2010, 2011, 2012, 2013, 2016, 2017, 2018, 2019, 2020, 2021 Igalia S.L
 * Copyright (C) 2015 Sebastian Dröge <sebastian@centricular.com>
 * Copyright (C) 2015, 2016, 2017, 2018, 2019, 2020, 2021 Metrological Group B.V.
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
#include "MediaSourceTrackGStreamer.h"
#include "SourceBufferPrivateGStreamer.h"
#include "TimeRanges.h"
#include "VideoTrackPrivateGStreamer.h"
#include "WebKitMediaSourceGStreamer.h"

#include <gst/app/gstappsink.h>
#include <gst/app/gstappsrc.h>
#include <gst/gst.h>
#include <gst/pbutils/pbutils.h>
#include <gst/video/video.h>
#include <wtf/Condition.h>
#include <wtf/HashSet.h>
#include <wtf/NativePromise.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/RunLoop.h>
#include <wtf/StringPrintStream.h>
#include <wtf/URL.h>
#include <wtf/text/AtomString.h>
#include <wtf/text/AtomStringHash.h>
#include <wtf/text/MakeString.h>
#include <wtf/text/StringToIntegerConversion.h>

#ifndef GST_DISABLE_GST_DEBUG
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
#endif // GST_DISABLE_GST_DEBUG

GST_DEBUG_CATEGORY_STATIC(webkit_mse_player_debug);
#define GST_CAT_DEFAULT webkit_mse_player_debug

namespace WebCore {

class MediaPlayerFactoryGStreamerMSE final : public MediaPlayerFactory {
private:
    MediaPlayerEnums::MediaEngineIdentifier identifier() const final { return MediaPlayerEnums::MediaEngineIdentifier::GStreamerMSE; };

    Ref<MediaPlayerPrivateInterface> createMediaEnginePlayer(MediaPlayer& player) const final
    {
        return adoptRef(*new MediaPlayerPrivateGStreamerMSE(player));
    }

    void getSupportedTypes(HashSet<String>& types) const final
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
    GST_DEBUG_CATEGORY_INIT(webkit_mse_player_debug, "webkitmseplayer", 0, "WebKit MSE media player");
    registrar(makeUnique<MediaPlayerFactoryGStreamerMSE>());
}

MediaPlayerPrivateGStreamerMSE::MediaPlayerPrivateGStreamerMSE(MediaPlayer& player)
    : MediaPlayerPrivateGStreamer(player)
{
    GST_TRACE("creating the player (%p)", this);
}

MediaPlayerPrivateGStreamerMSE::~MediaPlayerPrivateGStreamerMSE()
{
    GST_TRACE("destroying the player (%p)", this);

    m_source.clear();
}

void MediaPlayerPrivateGStreamerMSE::load(const String&)
{
    // This media engine only supports MediaSource URLs.
    m_networkState = MediaPlayer::NetworkState::FormatError;
    if (auto player = m_player.get())
        player->networkStateChanged();
}

void MediaPlayerPrivateGStreamerMSE::load(const URL& url, const LoadOptions&, MediaSourcePrivateClient& mediaSource)
{
    auto mseBlobURI = makeString("mediasource"_s, url.string().isEmpty() ? "blob://"_s : url.string());
    GST_DEBUG("Loading %s", mseBlobURI.ascii().data());
    if (RefPtr mediaSourcePrivate = downcast<MediaSourcePrivateGStreamer>(mediaSource.mediaSourcePrivate())) {
        mediaSourcePrivate->setPlayer(this);
        m_mediaSourcePrivate = WTFMove(mediaSourcePrivate);
        mediaSource.reOpen();
    } else
        m_mediaSourcePrivate = MediaSourcePrivateGStreamer::open(mediaSource, *this);

    MediaPlayerPrivateGStreamer::load(mseBlobURI);
}

void MediaPlayerPrivateGStreamerMSE::play()
{
    GST_DEBUG_OBJECT(pipeline(), "Play requested");
    m_isPaused = false;
    if (!m_playbackRate && m_playbackRatePausedState == PlaybackRatePausedState::ManuallyPaused)
        m_playbackRatePausedState = PlaybackRatePausedState::RatePaused;
    updateStates();
}

void MediaPlayerPrivateGStreamerMSE::pause()
{
    GST_DEBUG_OBJECT(pipeline(), "Pause requested");
    if (m_playbackRatePausedState == PlaybackRatePausedState::ManuallyPaused) {
        GST_DEBUG_OBJECT(pipeline(), "Player is paused already.");
        return;
    }

    m_isPaused = true;
    m_playbackRatePausedState = PlaybackRatePausedState::ManuallyPaused;
    updateStates();

    // HTMLMediaElement::pauseInternal() synchronously schedules the pause event right after pausing
    // the player, so without a playbackStateChanged notification here we would still observe an
    // active sleep disabler right after receiving the pause event on JS side.
    RefPtr player = m_player.get();
    if (!player) [[unlikely]]
        return;
    player->playbackStateChanged();
}

void MediaPlayerPrivateGStreamerMSE::willSeekToTarget(const MediaTime& time)
{
    MediaPlayerPrivateInterface::willSeekToTarget(time);
    // Don't consider the stream as EOS anymore after a seek.
    m_isEndReached = false;
}

void MediaPlayerPrivateGStreamerMSE::checkPlayingConsistency()
{
    MediaPlayerPrivateGStreamer::checkPlayingConsistency();

    if (!m_playbackStateChangedNotificationPending)
        return;

    m_playbackStateChangedNotificationPending = false;
    RefPtr player = m_player.get();
    if (!player) [[unlikely]]
        return;

    GstState state, pendingState;
    gst_element_get_state(pipeline(), &state, &pendingState, 0);
    if (pendingState != GST_STATE_VOID_PENDING)
        return;

    if ((state == GST_STATE_PLAYING && m_playbackRatePausedState == PlaybackRatePausedState::Playing) || (state == GST_STATE_PAUSED && m_playbackRatePausedState == PlaybackRatePausedState::ManuallyPaused)) {
        GST_DEBUG_OBJECT(pipeline(), "Notifying MediaPlayer of pipeline state change to %s", gst_element_state_get_name(state));
        player->playbackStateChanged();
    }
}

#ifndef GST_DISABLE_GST_DEBUG
void MediaPlayerPrivateGStreamerMSE::setShouldDisableSleep(bool shouldDisableSleep)
{
    // This method is useful only for logging purpose. The actual sleep disabler is managed by HTMLMediaElement.
    GST_DEBUG_OBJECT(pipeline(), "%s display sleep.", shouldDisableSleep ? "Disabling" : "Enabling");
}
#endif

void MediaPlayerPrivateGStreamerMSE::mirrorEnabledVideoTrackIfNeeded(const VideoTrackPrivateGStreamer& originalVideoTrackPrivate)
{
    if (!isMediaSource())
        return;

    for (auto& pair : m_videoTracks) {
        auto& track = pair.value.get();
        if (track.id() == originalVideoTrackPrivate.id() && &track != &originalVideoTrackPrivate) {
            GST_DEBUG_OBJECT(m_pipeline.get(), "Mirrored selected state (%s) from element track %p %" PRIu64
                " to player track %p %" PRIu64, boolForPrinting(track.selected()), &originalVideoTrackPrivate,
                originalVideoTrackPrivate.id(), &track, track.id());
            track.setSelected(originalVideoTrackPrivate.selected());
            break;
        }
    }
}

MediaTime MediaPlayerPrivateGStreamerMSE::duration() const
{
    if (!m_pipeline || m_didErrorOccur) [[unlikely]]
        return MediaTime();

    return m_mediaTimeDuration.isValid() ? m_mediaTimeDuration : MediaTime::zeroTime();
}

void MediaPlayerPrivateGStreamerMSE::seekToTarget(const SeekTarget& target)
{
    if (!m_pipeline)
        return;
    GST_DEBUG_OBJECT(pipeline(), "Requested seek to %s", target.time.toString().utf8().data());
    doSeek(target, m_playbackRate);
}

bool MediaPlayerPrivateGStreamerMSE::doSeek(const SeekTarget& target, float rate, bool isAsync, bool isSegment)
{
    UNUSED_PARAM(isAsync);
    RefPtr player = m_player.get();
    if (!player)
        return false;

    // This method should only be called outside of MediaPlayerPrivateGStreamerMSE by MediaPlayerPrivateGStreamer::setRate().

    // Note: An important difference between seek with WebKitMediaSrc and regular playback is that seeking before
    // pre-roll (e.g. to start playback at a non-zero position) is supported in WebKitMediaSrc but not in regular
    // playback. This is relevant in MSE because pre-roll may never occur if the JS code never appends a range starting
    // at zero, creating a chicken and egg problem.

    // GStreamer doesn't support zero as a valid playback rate. Instead, that is implemented in WebKit by pausing
    // the pipeline.
    if (rate <= 0)
        rate = 1.0;

    m_seekTarget = target;
    m_isSeeking = true;
    m_isWaitingForPreroll = true;
    m_isEndReached = false;

    auto flag = (player->isLooping() && isSegment) ? GST_SEEK_FLAG_SEGMENT : GST_SEEK_FLAG_FLUSH;
    if (paused() && !m_isEndReached && player->isLooping()) {
        GST_DEBUG_OBJECT(pipeline(), "Segment non-flushing seek attempt not supported on a paused pipeline, enabling flush");
        m_isSegmentSeekAllowed = false;
        flag = GST_SEEK_FLAG_FLUSH;
    }
    auto seekFlags = static_cast<GstSeekFlags>(flag | GST_SEEK_FLAG_ACCURATE);
    auto seekStart = toGstClockTime(target.time);
    int64_t seekStop = 0;

    // Important: In order to ensure correct propagation whether pre-roll has happened or not, we send the seek directly
    // to the source element, rather than letting playbin do the routing.
    {
        // Take the STATE_LOCK of the __pipeline__.
        //
        // gst_element_send_event() [which is called by gst_element_seek()] already takes the STATE_LOCK of the element
        // in order to delay any state change attempts from other threads while the event is travelling the pipeline.
        //
        // Normally that would happen to both the pipeline and then recursively to the elements inside as they handle
        // the seek event, but since we're sending the event directly to the source element we need to take the
        // STATE_LOCK on the pipeline ourselves.
        auto locker = GstStateLocker(pipeline());
        gst_element_seek(m_source.get(), rate, GST_FORMAT_TIME, seekFlags,
            GST_SEEK_TYPE_SET, seekStart, GST_SEEK_TYPE_NONE, seekStop);
    }

    if (!isSegment && player->isLooping()) {
        GST_DEBUG_OBJECT(m_pipeline.get(), "Resuming segment playback");
        seekFlags = static_cast<GstSeekFlags>(GST_SEEK_FLAG_SEGMENT | GST_SEEK_FLAG_ACCURATE);
        gst_element_send_event(m_pipeline.get(), gst_event_new_seek(rate, GST_FORMAT_TIME, seekFlags, GST_SEEK_TYPE_SET, seekStart, GST_SEEK_TYPE_SET, seekStop));
    }

    invalidateCachedPosition();

    // Notify MediaSource and have new frames enqueued (when they're available).
    // Seek should only continue once the seekToTarget completionhandler has run.
    // This will also add support for fastSeek once done (see webkit.org/b/260607)
    if (!m_mediaSourcePrivate)
        return false;
    m_mediaSourcePrivate->willSeek();
    m_mediaSourcePrivate->waitForTarget(target)->whenSettled(RunLoop::currentSingleton(), [this, weakThis = ThreadSafeWeakPtr { *this }](auto&& result) {
        RefPtr self = weakThis.get();
        if (!self || !result)
            return;

        if (m_mediaSourcePrivate)
            m_mediaSourcePrivate->seekToTime(*result);

        auto player = m_player.get();
        if (player && !hasVideo() && m_audioSink) {
            gboolean audioSinkPerformsAsyncStateChanges = true;

            GRefPtr<GstElement> sink = m_audioSink;
            while (GST_IS_BIN(sink.get())) {
                GUniquePtr<GstIterator> iter(gst_bin_iterate_sinks(GST_BIN_CAST(sink.get())));
                GValue value = G_VALUE_INIT;
                auto result = gst_iterator_next(iter.get(), &value);
                ASSERT_UNUSED(result, result == GST_ITERATOR_OK);
                sink = GST_ELEMENT(g_value_get_object(&value));
                g_value_unset(&value);
            }
            if (gstObjectHasProperty(sink.get(), "async"))
                g_object_get(sink.get(), "async", &audioSinkPerformsAsyncStateChanges, nullptr);
            if (!audioSinkPerformsAsyncStateChanges) {
                // If audio-only pipeline's sink is not performing async state changes
                // we must simulate preroll right away as otherwise nothing will trigger it.

                // Post this on HTML media element queue so it will be executed
                // synchonously with media events (e.g. seeking). This will ensure
                // that HTML element attributes (like HTMLmedia.seeking) are not reseted
                // before app receives "seeking" event
                player->queueTaskOnEventLoop([this, weakThis = ThreadSafeWeakPtr { *this }] {
                    RefPtr self = weakThis.get();
                    if (!self)
                        return;
                    bool mustPreventPositionReset = m_isWaitingForPreroll && m_isSeeking;
                    if (mustPreventPositionReset)
                        m_cachedPosition = currentTime();
                    didPreroll();
                    if (mustPreventPositionReset) {
                        propagateReadyStateToPlayer();
                        invalidateCachedPosition();
                    }
                });
            }
        }
    });
    return true;
}

void MediaPlayerPrivateGStreamerMSE::setNetworkState(MediaPlayer::NetworkState networkState)
{
    if (networkState == m_mediaSourceNetworkState)
        return;

    m_mediaSourceNetworkState = networkState;
    m_networkState = networkState;
    updateStates();
    if (auto player = m_player.get())
        player->networkStateChanged();
}

void MediaPlayerPrivateGStreamerMSE::readyStateFromMediaSourceChanged()
{
    assertIsMainThread();
    // Something important to bear in mind is that the readyState we get here comes from MediaSource.
    // From MediaSource perspective, as long as the sample for currentTime exists in the sample map, we are >= HaveCurrentData.
    // This is NOT true from the player perspective though, because there needs to pass some time since we have the first frame
    // (>=HaveCurrentData for MediaSource) and we have decoded it and sent it to the sink/compositor (>=HaveCurrentData in HTMLMediaElement).
    // The way we implement this is by keeping track of the MediaSource readyState internally in m_mediaSourceReadyState but not
    // spreading states >= HaveCurrentData to the player until prerolled.
    RefPtr mediaSourcePrivate = m_mediaSourcePrivate;
    auto mediaSourceReadyState = mediaSourcePrivate ? mediaSourcePrivate->mediaPlayerReadyState() : MediaPlayer::ReadyState::HaveNothing;
    if (mediaSourceReadyState == m_mediaSourceReadyState)
        return;

    GST_DEBUG("MediaSource called setReadyState(%p): %s -> %s Current player state: %s Waiting for preroll: %s", this,
        dumpReadyState(m_mediaSourceReadyState), dumpReadyState(mediaSourceReadyState), dumpReadyState(m_readyState), boolForPrinting(m_isWaitingForPreroll));
    m_mediaSourceReadyState = mediaSourceReadyState;

    if (mediaSourceReadyState < MediaPlayer::ReadyState::HaveCurrentData || !hasVideo() || !m_isWaitingForPreroll)
        propagateReadyStateToPlayer();
}

void MediaPlayerPrivateGStreamerMSE::propagateReadyStateToPlayer()
{
    ASSERT(m_mediaSourceReadyState < MediaPlayer::ReadyState::HaveCurrentData || !hasVideo() || !m_isWaitingForPreroll);
    if (m_readyState == m_mediaSourceReadyState)
        return;
    GST_DEBUG("Propagating MediaSource readyState %s to player ready state (currently %s)", dumpReadyState(m_mediaSourceReadyState), dumpReadyState(m_readyState));

    m_readyState = m_mediaSourceReadyState;
    updateStates(); // Set the pipeline to PLAYING or PAUSED if necessary.
    RefPtr player = m_player.get();
    if (player)
        player->readyStateChanged();

    // The readyState change may be a result of monitorSourceBuffers() finding that currentTime == duration, which
    // should cause the video to be marked as ended. Let's have the player check that.
    if (player && (!m_isWaitingForPreroll || currentTime() == duration()))
        player->timeChanged();
}

void MediaPlayerPrivateGStreamerMSE::mediaSourceHasRetrievedAllData()
{
    setNetworkState(MediaPlayer::NetworkState::Loaded);
}

void MediaPlayerPrivateGStreamerMSE::didPreroll()
{
    ASSERT(GST_STATE(m_pipeline.get()) >= GST_STATE_PAUSED);
    // There are three circumstances in which a preroll can occur:
    // a) At the begining of playback. This is the point where we propagate >=HaveCurrentData to the player.
    // b) At the end of a seek. We emit the "seeked" event as well.
    // c) At the end of a flush (forced quality change). These should not produce either of these outcomes.
    // We identify (a) and (b) by setting m_isWaitingForPreroll = true at the initialization of the player and
    // at the beginning of a seek.
    GST_DEBUG("Pipeline prerolled. currentMediaTime = %s", currentTime().toString().utf8().data());
    if (!m_isWaitingForPreroll) {
        GST_DEBUG("Preroll was consequence of a flush, nothing to do at this level.");
        return;
    }
    m_isWaitingForPreroll = false;

    // The order of these sections is important. In the case of a seek, the "seeked" event must be emitted
    // before the "playing" event (which is emitted on HAVE_ENOUGH_DATA). Therefore, we take care of them in
    // that order.

    if (m_isSeeking) {
        m_isSeeking = false;
        m_canFallBackToLastFinishedSeekPosition = true;
        invalidateCachedPosition();
        GST_DEBUG("Seek complete because of preroll. currentMediaTime = %s", currentTime().toString().utf8().data());
        // By calling timeChanged(), m_isSeeking will be checked an a "seeked" event will be emitted.
        timeChanged(currentTime());
    }

    propagateReadyStateToPlayer();
}

const PlatformTimeRanges& MediaPlayerPrivateGStreamerMSE::buffered() const
{
    if (!m_source)
        return PlatformTimeRanges::emptyRanges();

    // When a MediaSource object is in use, the HTMLMediaElement retrieves the buffered ranges
    // directly from it rather than from the MediaPlayer / MediaPlayerPrivate.
    ASSERT_NOT_REACHED();
    return PlatformTimeRanges::emptyRanges();
}

void MediaPlayerPrivateGStreamerMSE::sourceSetup(GstElement* sourceElement)
{
    ASSERT(WEBKIT_IS_MEDIA_SRC(sourceElement));
    GST_DEBUG_OBJECT(pipeline(), "Source %p setup (old was: %p)", sourceElement, m_source.get());
    webKitMediaSrcSetPlayer(WEBKIT_MEDIA_SRC(sourceElement), ThreadSafeWeakPtr { *this });
    m_source = sourceElement;

    if (m_mediaSourcePrivate && m_mediaSourcePrivate->hasAllTracks()) {
        emitStreams(m_tracks);
    }
}

size_t MediaPlayerPrivateGStreamerMSE::extraMemoryCost() const
{
    return 0;
}

void MediaPlayerPrivateGStreamerMSE::updateStates()
{
    bool isWaitingPreroll = isPipelineWaitingPreroll();
    bool shouldUpdatePlaybackState = false;
    bool shouldBePlaying = (!m_isPaused && !isPausedByViewport() && readyState() >= MediaPlayer::ReadyState::HaveFutureData && m_playbackRatePausedState != PlaybackRatePausedState::RatePaused)
        || m_playbackRatePausedState == PlaybackRatePausedState::ShouldMoveToPlaying;
    GST_DEBUG_OBJECT(pipeline(), "shouldBePlaying = %s, m_isPipelinePlaying = %s, is seeking %s", boolForPrinting(shouldBePlaying),
        boolForPrinting(m_isPipelinePlaying), boolForPrinting(isWaitingPreroll));
    if (!isWaitingPreroll && shouldBePlaying && !m_isPipelinePlaying) {
        auto result = changePipelineState(GST_STATE_PLAYING);
        if (result == ChangePipelineStateResult::Failed)
            GST_ERROR_OBJECT(pipeline(), "Setting the pipeline to PLAYING failed");
        else if (result == ChangePipelineStateResult::Ok) {
            m_playbackRatePausedState = PlaybackRatePausedState::Playing;
            shouldUpdatePlaybackState = true;
        }
    } else if (!isWaitingPreroll && !shouldBePlaying && m_isPipelinePlaying) {
        auto result = changePipelineState(GST_STATE_PAUSED);
        if (result == ChangePipelineStateResult::Failed)
            GST_ERROR_OBJECT(pipeline(), "Setting the pipeline to PAUSED failed");

        shouldUpdatePlaybackState = result == ChangePipelineStateResult::Ok;
    } else if (m_isEosWithNoBuffers) {
        if (auto player = m_player.get()) {
            // Trigger playback end detection in HTMLMediaElement.
            player->timeChanged();
        }
    }

    if (!shouldUpdatePlaybackState)
        return;

    m_playbackStateChangedNotificationPending = shouldUpdatePlaybackState;
}

bool MediaPlayerPrivateGStreamerMSE::isTimeBuffered(const MediaTime &time) const
{

    bool result = m_mediaSourcePrivate && m_mediaSourcePrivate->buffered().contain(time);
    GST_DEBUG("Time %s buffered? %s", toString(time).utf8().data(), boolForPrinting(result));
    return result;
}

void MediaPlayerPrivateGStreamerMSE::durationChanged()
{
    ASSERT(isMainThread());

    MediaTime previousDuration = m_mediaTimeDuration;
    m_mediaTimeDuration = m_mediaSourcePrivate ? m_mediaSourcePrivate->duration() : MediaTime::invalidTime();

    GST_TRACE("previous=%s, new=%s", toString(previousDuration).utf8().data(), toString(m_mediaTimeDuration).utf8().data());

    // Avoid emiting durationchanged in the case where the previous duration was 0 because that case is already handled
    // by the HTMLMediaElement.
    if (m_mediaTimeDuration != previousDuration && m_mediaTimeDuration.isValid() && previousDuration.isValid()) {
        if (auto player = m_player.get())
            player->durationChanged();
    }
}

void MediaPlayerPrivateGStreamerMSE::setInitialVideoSize(const FloatSize& videoSize)
{
    ASSERT(isMainThread());

    // We set the size of the video only for the first initialization segment.
    // This is intentional: Normally the video size depends on the frames arriving
    // at the sink in the playback pipeline, not in the append pipeline; but we still
    // want to report an initial size for HAVE_METADATA (first initialization segment).

    if (!m_videoSize.isEmpty())
        return;

    GST_DEBUG("Setting initial video size: %gx%g", videoSize.width(), videoSize.height());
    m_videoSize = videoSize;
}

void MediaPlayerPrivateGStreamerMSE::startSource(const Vector<RefPtr<MediaSourceTrackGStreamer>>& tracks)
{
    emitStreams(tracks);
}

void MediaPlayerPrivateGStreamerMSE::emitStreams(const Vector<RefPtr<MediaSourceTrackGStreamer>>& tracks)
{
    Vector<RefPtr<MediaSourceTrackGStreamer>> uniqueTracks;
    Vector<RefPtr<MediaSourceTrackGStreamer>> playbackTracks;

    uniqueTracks.reserveInitialCapacity(tracks.size());
    playbackTracks.reserveInitialCapacity(tracks.size());

    for (const auto& track : tracks) {
        if (!uniqueTracks.containsIf([&track](const auto& current) { return track->id() == current->id(); })) {
            uniqueTracks.append(track);

            if (track->type() != TrackPrivateBaseGStreamer::Text)
                playbackTracks.append(track);
            else
                GST_DEBUG("Ignoring text track with id %" PRIu64, track->id());
        }
    }

    uniqueTracks.shrinkToFit();
    playbackTracks.shrinkToFit();

    m_tracks = uniqueTracks;
    webKitMediaSrcEmitStreams(WEBKIT_MEDIA_SRC(m_source.get()), playbackTracks);
}

void MediaPlayerPrivateGStreamerMSE::setEosWithNoBuffers(bool eosWithNoBuffers)
{
    m_isEosWithNoBuffers = eosWithNoBuffers;
    // Parsebin will trigger an error, instruct MediaPlayerPrivateGStreamer to ignore it.
    if (eosWithNoBuffers) {
        // On GStreamer, EOS with no buffers causes a parsebin error here:
        // https://github.com/GStreamer/gst-plugins-base/blob/1.18.6/gst/playback/gstparsebin.c#L3495

        m_ignoreErrors = true;

        GST_DEBUG_OBJECT(pipeline(), "EOS with no buffers, setting pipeline to READY state.");
        changePipelineState(GST_STATE_READY);

        unsigned errorCount = m_queuedSyncErrors.loadFullyFenced();
        if (errorCount) {
            GST_WARNING_OBJECT(pipeline(), "%u errors pending to process happened while processing EOS with"
                " no buffers and should be ignored, manually reporting EOS", errorCount);
            didEnd();
            // In the best case, m_ignoreErrors will be cleaned up (in the main thread) in MediaPlayerPrivateGStreamer::handleMessage()
            // in the future, after the last pending error is processed on the main thread. In the worst case, didEnd() will trigger
            // a pipeline teardown, which will trigger gst_bus_set_flushing() and the pending error message won't ever be processed
            // by the main thread. That's not a problem, since the player itself will be destructed and nobody will care about the
            // error count integrity anymore.
        } else
            m_ignoreErrors = false;
    }
}

void MediaPlayerPrivateGStreamerMSE::getSupportedTypes(HashSet<String>& types)
{
    GStreamerRegistryScannerMSE::getSupportedDecodingTypes(types);
}

MediaPlayer::SupportsType MediaPlayerPrivateGStreamerMSE::supportsType(const MediaEngineSupportParameters& parameters)
{
    MediaPlayer::SupportsType result = MediaPlayer::SupportsType::IsNotSupported;
    if (!parameters.isMediaSource)
        return result;

    if (!ensureGStreamerInitialized())
        return result;

    auto containerType = parameters.type.containerType();

    // YouTube TV provides empty types for some videos and we want to be selected as best media engine for them.
    if (containerType.isEmpty()) {
        result = MediaPlayer::SupportsType::MayBeSupported;
        GST_DEBUG("mime-type \"%s\" supported: %s", parameters.type.raw().utf8().data(), convertEnumerationToString(result).utf8().data());
        return result;
    }

    registerWebKitGStreamerElements();

    GST_DEBUG("Checking mime-type \"%s\"", parameters.type.raw().utf8().data());
    auto& gstRegistryScanner = GStreamerRegistryScannerMSE::singleton();
    result = gstRegistryScanner.isContentTypeSupported(GStreamerRegistryScanner::Configuration::Decoding, parameters.type, parameters.contentTypesRequiringHardwareSupport);

    GST_DEBUG("Supported: %s", convertEnumerationToString(result).utf8().data());
    return result;
}

MediaTime MediaPlayerPrivateGStreamerMSE::maxTimeSeekable() const
{
    if (m_didErrorOccur) [[unlikely]]
        return MediaTime::zeroTime();

    GST_DEBUG("maxTimeSeekable");
    MediaTime result = duration();
    // Infinite duration means live stream.
    if (result.isPositiveInfinite()) {
        MediaTime maxBufferedTime = buffered().maximumBufferedTime();
        // Return the highest end time reported by the buffered attribute.
        result = maxBufferedTime.isValid() ? maxBufferedTime : MediaTime::zeroTime();
    }

    return result;
}

bool MediaPlayerPrivateGStreamerMSE::timeIsProgressing() const
{
    if (!m_mediaSourcePrivate)
        return false;

    bool isPaused = paused();
    const auto currentTime = this->currentTime();
    bool hasFutureTime = m_mediaSourcePrivate->hasFutureTime(currentTime);
    bool isProgressing = !isPaused && hasFutureTime;
    GST_DEBUG_OBJECT(pipeline(), "Is paused: %s, has future time for %f: %s, time is progressing: %s", boolForPrinting(isPaused), currentTime.toDouble(), boolForPrinting(hasFutureTime), boolForPrinting(isProgressing));
    return isProgressing;
}

void MediaPlayerPrivateGStreamerMSE::notifyActiveSourceBuffersChanged()
{
    if (auto player = m_player.get())
        player->activeSourceBuffersChanged();
}

#undef GST_CAT_DEFAULT

} // namespace WebCore.

#endif // ENABLE(VIDEO) && USE(GSTREAMER) && ENABLE(MEDIA_SOURCE)
