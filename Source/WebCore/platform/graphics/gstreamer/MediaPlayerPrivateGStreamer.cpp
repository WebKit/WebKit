/*
 * Copyright (C) 2007, 2009 Apple Inc.  All rights reserved.
 * Copyright (C) 2007 Collabora Ltd.  All rights reserved.
 * Copyright (C) 2007 Alp Toker <alp@atoker.com>
 * Copyright (C) 2009 Gustavo Noronha Silva <gns@gnome.org>
 * Copyright (C) 2014 Cable Television Laboratories, Inc.
 * Copyright (C) 2009, 2019 Igalia S.L
 * Copyright (C) 2015, 2019 Metrological Group B.V.
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
#include "VideoFrameGStreamer.h"

#if ENABLE(VIDEO) && USE(GSTREAMER)

#include "AudioTrackPrivateGStreamer.h"
#include "GStreamerAudioMixer.h"
#include "GStreamerCommon.h"
#include "GStreamerQuirks.h"
#include "GStreamerRegistryScanner.h"
#include "GraphicsContext.h"
#include "HTTPHeaderNames.h"
#include "ImageGStreamer.h"
#include "ImageOrientation.h"
#include "InbandMetadataTextTrackPrivateGStreamer.h"
#include "InbandTextTrackPrivateGStreamer.h"
#include "IntRect.h"
#include "Logging.h"
#include "MIMETypeRegistry.h"
#include "MediaPlayer.h"
#include "NotImplemented.h"
#include "OriginAccessPatterns.h"
#include "SecurityOrigin.h"
#include "TextCombinerGStreamer.h"
#include "TextSinkGStreamer.h"
#include "TimeRanges.h"
#include "VideoFrameMetadataGStreamer.h"
#include "VideoSinkGStreamer.h"
#include "VideoTrackPrivateGStreamer.h"
#include "WebKitAudioSinkGStreamer.h"
#include "WebKitWebSourceGStreamer.h"

#if ENABLE(MEDIA_STREAM)
#include "GStreamerMediaStreamSource.h"
#include "MediaStreamPrivate.h"
#endif

#if ENABLE(MEDIA_SOURCE)
#include "MediaSource.h"
#include "WebKitMediaSourceGStreamer.h"
#endif

#if ENABLE(ENCRYPTED_MEDIA)
#include "CDMInstance.h"
#include "GStreamerEMEUtilities.h"
#include "SharedBuffer.h"
#include "WebKitCommonEncryptionDecryptorGStreamer.h"
#endif

#if ENABLE(WEB_AUDIO)
#include "AudioSourceProviderGStreamer.h"
#endif

#include <cstring>
#include <glib.h>
#include <gst/audio/streamvolume.h>
#include <gst/gst.h>
#include <gst/pbutils/missing-plugins.h>
#include <gst/pbutils/pbutils.h>
#include <gst/video/gstvideometa.h>
#include <limits>
#include <wtf/FileSystem.h>
#include <wtf/MathExtras.h>
#include <wtf/MediaTime.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/Scope.h>
#include <wtf/StdLibExtras.h>
#include <wtf/StringPrintStream.h>
#include <wtf/URL.h>
#include <wtf/UniStdExtras.h>
#include <wtf/WallTime.h>
#include <wtf/glib/GUniquePtr.h>
#include <wtf/glib/RunLoopSourcePriority.h>
#include <wtf/text/AtomString.h>
#include <wtf/text/CString.h>
#include <wtf/text/StringConcatenateNumbers.h>
#include <wtf/text/StringToIntegerConversion.h>

#if USE(GSTREAMER_MPEGTS)
#define GST_USE_UNSTABLE_API
#include <gst/mpegts/mpegts.h>
#undef GST_USE_UNSTABLE_API
#endif // ENABLE(VIDEO) && USE(GSTREAMER_MPEGTS)

#if USE(GSTREAMER_GL)
#include "GLVideoSinkGStreamer.h"
#endif // USE(GSTREAMER_GL)

#if USE(TEXTURE_MAPPER)
#include "BitmapTexture.h"
#include "BitmapTexturePool.h"
#include "GStreamerVideoFrameHolder.h"
#include "TextureMapperPlatformLayerBuffer.h"
#include "TextureMapperPlatformLayerProxyGL.h"
#endif // USE(TEXTURE_MAPPER)

#if USE(TEXTURE_MAPPER_DMABUF)
#include "DMABufFormat.h"
#include "DMABufObject.h"
#include "DMABufVideoSinkGStreamer.h"
#include "GBMBufferSwapchain.h"
#include "TextureMapperPlatformLayerProxyDMABuf.h"
#include <gbm.h>
#include <gst/allocators/gstdmabuf.h>
#endif // USE(TEXTURE_MAPPER_DMABUF)

#if USE(EXTERNAL_HOLEPUNCH)
#include "MediaPlayerPrivateHolePunch.h"
#endif

GST_DEBUG_CATEGORY(webkit_media_player_debug);
#define GST_CAT_DEFAULT webkit_media_player_debug

namespace WebCore {
using namespace std;

static const FloatSize s_holePunchDefaultFrameSize(1280, 720);

MediaPlayerPrivateGStreamer::MediaPlayerPrivateGStreamer(MediaPlayer* player)
    : m_notifier(MainThreadNotifier<MainThreadNotification>::create())
    , m_player(player)
    , m_referrer(player->referrer())
    , m_cachedDuration(MediaTime::invalidTime())
    , m_timeOfOverlappingSeek(MediaTime::invalidTime())
    , m_fillTimer(*this, &MediaPlayerPrivateGStreamer::fillTimerFired)
    , m_maxTimeLoaded(MediaTime::zeroTime())
    , m_preload(player->preload())
    , m_maxTimeLoadedAtLastDidLoadingProgress(MediaTime::zeroTime())
    , m_drawTimer(RunLoop::main(), this, &MediaPlayerPrivateGStreamer::repaint)
    , m_pausedTimerHandler(RunLoop::main(), this, &MediaPlayerPrivateGStreamer::pausedTimerFired)
#if USE(TEXTURE_MAPPER) && !USE(NICOSIA)
    , m_platformLayerProxy(adoptRef(new TextureMapperPlatformLayerProxyGL(TextureMapperPlatformLayerProxy::ContentType::Video)))
#endif
#if !RELEASE_LOG_DISABLED
    , m_logger(player->mediaPlayerLogger())
    , m_logIdentifier(player->mediaPlayerLogIdentifier())
#endif
#if USE(TEXTURE_MAPPER_DMABUF)
    , m_swapchain(adoptRef(new GBMBufferSwapchain(GBMBufferSwapchain::BufferSwapchainSize::Eight)))
#endif
    , m_loader(player->createResourceLoader())
{
#if USE(GLIB)
    m_pausedTimerHandler.setPriority(G_PRIORITY_DEFAULT_IDLE);
#endif
    m_isPlayerShuttingDown.store(false);

    if (player->isGStreamerHolePunchingEnabled()) {
        m_quirksManagerForTesting = GStreamerQuirksManager::createForTesting();
        m_quirksManagerForTesting->setHolePunchEnabledForTesting(true);
    }

#if USE(TEXTURE_MAPPER) && USE(NICOSIA)
    m_nicosiaLayer = Nicosia::ContentLayer::create(*this,
        [&]() -> Ref<TextureMapperPlatformLayerProxy> {
            if (isHolePunchRenderingEnabled())
                return adoptRef(*new TextureMapperPlatformLayerProxyGL(TextureMapperPlatformLayerProxy::ContentType::HolePunch));

#if USE(TEXTURE_MAPPER_DMABUF)
            if (webKitDMABufVideoSinkIsEnabled() && webKitDMABufVideoSinkProbePlatform())
                return adoptRef(*new TextureMapperPlatformLayerProxyDMABuf(TextureMapperPlatformLayerProxy::ContentType::Video));
#endif
            return adoptRef(*new TextureMapperPlatformLayerProxyGL(TextureMapperPlatformLayerProxy::ContentType::Video));
        }());
#endif

    ensureGStreamerInitialized();
    m_audioSink = createAudioSink();
    ensureSeekFlags();
}

MediaPlayerPrivateGStreamer::~MediaPlayerPrivateGStreamer()
{
    GST_DEBUG_OBJECT(pipeline(), "Disposing player");
    m_isPlayerShuttingDown.store(true);

    m_sinkTaskQueue.startAborting();

    for (auto& track : m_audioTracks.values())
        track->disconnect();

    for (auto& track : m_textTracks.values())
        track->disconnect();

    for (auto& track : m_videoTracks.values())
        track->disconnect();

    if (m_fillTimer.isActive())
        m_fillTimer.stop();

    m_pausedTimerHandler.stop();

    if (m_videoSink) {
        GRefPtr<GstPad> videoSinkPad = adoptGRef(gst_element_get_static_pad(m_videoSink.get(), "sink"));
        g_signal_handlers_disconnect_matched(videoSinkPad.get(), G_SIGNAL_MATCH_DATA, 0, 0, nullptr, nullptr, this);
    }

    if (m_pipeline) {
        auto bus = adoptGRef(gst_pipeline_get_bus(GST_PIPELINE(m_pipeline.get())));
        gst_bus_disable_sync_message_emission(bus.get());
        disconnectSimpleBusMessageCallback(m_pipeline.get());
        g_signal_handlers_disconnect_matched(m_pipeline.get(), G_SIGNAL_MATCH_DATA, 0, 0, nullptr, nullptr, this);
    }

#if USE(GSTREAMER_GL)
    if (m_videoDecoderPlatform == GstVideoDecoderPlatform::Video4Linux)
        flushCurrentBuffer();
#endif
#if USE(TEXTURE_MAPPER) && USE(NICOSIA)
    m_nicosiaLayer->invalidateClient();
#endif

    if (m_videoSink)
        g_signal_handlers_disconnect_matched(m_videoSink.get(), G_SIGNAL_MATCH_DATA, 0, 0, nullptr, nullptr, this);

    if (m_volumeElement)
        g_signal_handlers_disconnect_matched(m_volumeElement.get(), G_SIGNAL_MATCH_DATA, 0, 0, nullptr, nullptr, this);

    // This will release the GStreamer thread from m_drawCondition in non AC mode in case there's an ongoing triggerRepaint call
    // waiting there, and ensure that any triggerRepaint call reaching the lock won't wait on m_drawCondition.
    cancelRepaint(true);

#if ENABLE(ENCRYPTED_MEDIA)
    {
        Locker cdmAttachmentLocker { m_cdmAttachmentLock };
        m_cdmAttachmentCondition.notifyAll();
    }
#endif

    // The change to GST_STATE_NULL state is always synchronous. So after this gets executed we don't need to worry
    // about handlers running in the GStreamer thread.
    if (m_pipeline) {
        unregisterPipeline(m_pipeline);
        gst_element_set_state(m_pipeline.get(), GST_STATE_NULL);
    }

    m_player = nullptr;
    m_notifier->invalidate();
}

bool MediaPlayerPrivateGStreamer::isAvailable()
{
    return true;
}

class MediaPlayerFactoryGStreamer final : public MediaPlayerFactory {
private:
    MediaPlayerEnums::MediaEngineIdentifier identifier() const final { return MediaPlayerEnums::MediaEngineIdentifier::GStreamer; };

    Ref<MediaPlayerPrivateInterface> createMediaEnginePlayer(MediaPlayer* player) const final
    {
        return adoptRef(*new MediaPlayerPrivateGStreamer(player));
    }

    void getSupportedTypes(HashSet<String>& types) const final
    {
        return MediaPlayerPrivateGStreamer::getSupportedTypes(types);
    }

    MediaPlayer::SupportsType supportsTypeAndCodecs(const MediaEngineSupportParameters& parameters) const final
    {
        return MediaPlayerPrivateGStreamer::supportsType(parameters);
    }

    bool supportsKeySystem(const String& keySystem, const String& mimeType) const final
    {
        return MediaPlayerPrivateGStreamer::supportsKeySystem(keySystem, mimeType);
    }
};

void MediaPlayerPrivateGStreamer::registerMediaEngine(MediaEngineRegistrar registrar)
{
    static std::once_flag onceFlag;
    std::call_once(onceFlag, [] {
        GST_DEBUG_CATEGORY_INIT(webkit_media_player_debug, "webkitmediaplayer", 0, "WebKit media player");
    });
    registrar(makeUnique<MediaPlayerFactoryGStreamer>());
}

void MediaPlayerPrivateGStreamer::load(const String& urlString)
{
    URL url { urlString };
    if (url.protocolIsAbout()) {
        loadingFailed(MediaPlayer::NetworkState::FormatError, MediaPlayer::ReadyState::HaveNothing, true);
        return;
    }

    if (!ensureGStreamerInitialized()) {
        loadingFailed(MediaPlayer::NetworkState::FormatError, MediaPlayer::ReadyState::HaveNothing, true);
        return;
    }

    RefPtr player = m_player.get();
    if (!player) {
        loadingFailed(MediaPlayer::NetworkState::FormatError, MediaPlayer::ReadyState::HaveNothing, true);
        return;
    }

    registerWebKitGStreamerElements();

    if (!m_pipeline)
        createGSTPlayBin(url);
    syncOnClock(true);
    if (m_fillTimer.isActive())
        m_fillTimer.stop();

    ASSERT(m_pipeline);
    setPlaybinURL(url);
    setVisibleInViewport(player->isVisibleInViewport());

    GST_DEBUG_OBJECT(pipeline(), "preload: %s", convertEnumerationToString(m_preload).utf8().data());
    if (m_preload == MediaPlayer::Preload::None && !isMediaSource()) {
        GST_INFO_OBJECT(pipeline(), "Delaying load.");
        m_isDelayingLoad = true;
    }

    // Reset network and ready states. Those will be set properly once
    // the pipeline pre-rolled.
    m_networkState = MediaPlayer::NetworkState::Loading;
    player->networkStateChanged();
    m_readyState = MediaPlayer::ReadyState::HaveNothing;
    player->readyStateChanged();
    m_areVolumeAndMuteInitialized = false;

    if (!m_isDelayingLoad)
        commitLoad();
}

#if ENABLE(MEDIA_SOURCE)
void MediaPlayerPrivateGStreamer::load(const URL&, const ContentType&, MediaSourcePrivateClient&)
{
    // Properly fail so the global MediaPlayer tries to fallback to the next MediaPlayerPrivate.
    m_networkState = MediaPlayer::NetworkState::FormatError;
    if (RefPtr player = m_player.get())
        player->networkStateChanged();
}
#endif

#if ENABLE(MEDIA_STREAM)
void MediaPlayerPrivateGStreamer::load(MediaStreamPrivate& stream)
{
    m_streamPrivate = &stream;
    load(makeString("mediastream://", stream.id()));
    syncOnClock(false);

    if (RefPtr player = m_player.get())
        player->play();
}
#endif

void MediaPlayerPrivateGStreamer::cancelLoad()
{
    if (m_networkState < MediaPlayer::NetworkState::Loading || m_networkState == MediaPlayer::NetworkState::Loaded)
        return;

    if (m_pipeline)
        changePipelineState(GST_STATE_READY);
}

void MediaPlayerPrivateGStreamer::prepareToPlay()
{
    GST_DEBUG_OBJECT(pipeline(), "Prepare to play");
    m_preload = MediaPlayer::Preload::Auto;
    if (m_isDelayingLoad) {
        m_isDelayingLoad = false;
        commitLoad();
    }
}

bool MediaPlayerPrivateGStreamer::isPipelineWaitingPreroll(GstState current, GstState pending, GstStateChangeReturn change) const
{
    return change == GST_STATE_CHANGE_ASYNC && current == GST_STATE_PAUSED && pending >= GST_STATE_PAUSED;
}

bool MediaPlayerPrivateGStreamer::isPipelineWaitingPreroll() const
{
    GstState current, pending;
    GstStateChangeReturn change = gst_element_get_state(m_pipeline.get(), &current, &pending, 0);
    return isPipelineWaitingPreroll(current, pending, change);
}

void MediaPlayerPrivateGStreamer::play()
{
    if (isMediaStreamPlayer()) {
        m_pausedTime = MediaTime::invalidTime();
        if (m_startTime.isInvalid())
            m_startTime = MediaTime::createWithDouble(MonotonicTime::now().secondsSinceEpoch().value());
    }

    if (!m_playbackRate) {
        if (m_playbackRatePausedState == PlaybackRatePausedState::ManuallyPaused)
            m_playbackRatePausedState = PlaybackRatePausedState::RatePaused;
        return;
    }

    if (isPipelineWaitingPreroll()) {
        GST_DEBUG_OBJECT(pipeline(), "pipeline is waiting preroll (after seek or flush), let's delay moving the pipeline to playing right now");
        return;
    }

    if (changePipelineState(GST_STATE_PLAYING) == ChangePipelineStateResult::Ok) {
        m_isEndReached = false;
        m_isDelayingLoad = false;
        m_preload = MediaPlayer::Preload::Auto;
        GST_INFO_OBJECT(pipeline(), "Play");
        RefPtr player = m_player.get();
        if (player) {
            if (player->isLooping()) {
                GST_DEBUG_OBJECT(pipeline(), "Scheduling initial SEGMENT seek");
                doSeek(SeekTarget { playbackPosition() }, m_playbackRate);
            } else
                updateDownloadBufferingFlag();
        }
    } else
        loadingFailed(MediaPlayer::NetworkState::Empty);
}

void MediaPlayerPrivateGStreamer::pause()
{
    if (isMediaStreamPlayer())
        m_pausedTime = currentTime();

    m_playbackRatePausedState = PlaybackRatePausedState::ManuallyPaused;
    GstState currentState, pendingState;
    gst_element_get_state(m_pipeline.get(), &currentState, &pendingState, 0);
    if (currentState < GST_STATE_PAUSED && pendingState <= GST_STATE_PAUSED)
        return;

    auto result = changePipelineState(GST_STATE_PAUSED);
    if (result == ChangePipelineStateResult::Ok)
        GST_INFO_OBJECT(pipeline(), "Pause");
    else if (result == ChangePipelineStateResult::Failed)
        loadingFailed(MediaPlayer::NetworkState::Empty);
}

bool MediaPlayerPrivateGStreamer::paused() const
{
    if (!m_pipeline)
        return true;

    if (m_isEndReached) {
        GST_DEBUG_OBJECT(pipeline(), "Ignoring pause at EOS");
        return true;
    }

    if (m_playbackRatePausedState == PlaybackRatePausedState::RatePaused
        || m_playbackRatePausedState == PlaybackRatePausedState::ShouldMoveToPlaying) {
        GST_DEBUG_OBJECT(pipeline(), "Playback rate is 0, simulating PAUSED state");
        return false;
    }

    // For debug mode (either GStreamer of WebKit) we make some extra check to ensure there is no desynchronization
    // between pipeline and player. In the case of media stream, we just return the result of the pipeline as there are
    // nuances regarding the prerolling creating some regressions in the tests.
#if !defined(GST_DISABLE_GST_DEBUG) || !defined(NDEBUG) || (defined(ENABLE_MEDIA_STREAM) && ENABLE_MEDIA_STREAM)
    GstState state, pending;
    auto stateChange = gst_element_get_state(m_pipeline.get(), &state, &pending, 0);
    bool isPipelinePaused = state <= GST_STATE_PAUSED;

    if (isMediaStreamPlayer())
        return isPipelinePaused;

#if !defined(GST_DISABLE_GST_DEBUG) || !defined(NDEBUG)
    if (!isPipelineWaitingPreroll(state, pending, stateChange) && isPipelinePaused != !m_isPipelinePlaying
        && (stateChange == GST_STATE_CHANGE_SUCCESS || stateChange == GST_STATE_CHANGE_NO_PREROLL)) {
        GST_WARNING_OBJECT(pipeline(), "states are not synchronized, player paused %s, pipeline paused %s",
            boolForPrinting(!m_isPipelinePlaying), boolForPrinting(isPipelinePaused));
        ASSERT_NOT_REACHED_WITH_MESSAGE("pipeline and player states are not synchronized");
    }
#else
    UNUSED_VARIABLE(stateChange);
#endif
#endif

    GST_DEBUG_OBJECT(pipeline(), "paused %s", boolForPrinting(!m_isPipelinePlaying));
    return !m_isPipelinePlaying;
}

bool MediaPlayerPrivateGStreamer::doSeek(const SeekTarget& target, float rate)
{
    RefPtr player = m_player.get();

    // Default values for rate >= 0.
    MediaTime startTime = target.time, endTime = MediaTime::invalidTime();

    if (rate < 0) {
        startTime = MediaTime::zeroTime();
        // If we are at beginning of media, start from the end to avoid immediate EOS.
        endTime = target.time <= MediaTime::zeroTime() ? duration() : target.time;
    }

    if (!rate)
        rate = 1.0;

    if (m_hasWebKitWebSrcSentEOS && m_downloadBuffer) {
        GST_DEBUG_OBJECT(pipeline(), "Setting high-percent=0 on GstDownloadBuffer to force 100%% buffered reporting");
        g_object_set(m_downloadBuffer.get(), "high-percent", 0, nullptr);
    }

    if (paused() && !m_isEndReached && player && player->isLooping()) {
        GST_DEBUG_OBJECT(pipeline(), "Segment non-flushing seek attempt not supported on a paused pipeline, enabling flush");
        m_seekFlags = static_cast<GstSeekFlags>((m_seekFlags | GST_SEEK_FLAG_FLUSH) & ~GST_SEEK_FLAG_SEGMENT);
    }

    if (rate && player && player->isLooping() && startTime >= duration()) {
        didEnd();
        return true;
    }

    auto seekStart = toGstClockTime(startTime);
    auto seekStop = toGstClockTime(endTime);
    GST_DEBUG_OBJECT(pipeline(), "[Seek] Performing actual seek to %" GST_TIMEP_FORMAT " (endTime: %" GST_TIMEP_FORMAT ") at rate %f", &seekStart, &seekStop, rate);
    return gst_element_seek(m_pipeline.get(), rate, GST_FORMAT_TIME, m_seekFlags, GST_SEEK_TYPE_SET, seekStart, GST_SEEK_TYPE_SET, seekStop);
}

void MediaPlayerPrivateGStreamer::seekToTarget(const SeekTarget& inTarget)
{
    if (!m_pipeline || m_didErrorOccur || isMediaStreamPlayer())
        return;

    GST_INFO_OBJECT(pipeline(), "[Seek] seek attempt to %s", toString(inTarget.time).utf8().data());

    // Avoid useless seeking.
    if (inTarget.time == currentTime()) {
        GST_DEBUG_OBJECT(pipeline(), "[Seek] Already at requested position. Aborting.");
        timeChanged(inTarget.time);
        return;
    }

    if (m_isLiveStream.value_or(false)) {
        GST_DEBUG_OBJECT(pipeline(), "[Seek] Live stream seek unhandled");
        return;
    }

    RefPtr player = m_player.get();
    if (!player) {
        GST_DEBUG_OBJECT(pipeline(), "[Seek] m_player is nullptr");
        return;
    }

    auto target = inTarget;
    target.time = std::min(inTarget.time, duration());
    GST_INFO_OBJECT(pipeline(), "[Seek] seeking to %s", toString(target.time).utf8().data());

    if (m_isSeeking) {
        m_timeOfOverlappingSeek = target.time;
        if (m_isSeekPending) {
            m_seekTarget = target;
            return;
        }
    }

    GstState state;
    GstStateChangeReturn getStateResult = gst_element_get_state(m_pipeline.get(), &state, nullptr, 0);
    if (getStateResult == GST_STATE_CHANGE_FAILURE || getStateResult == GST_STATE_CHANGE_NO_PREROLL) {
        GST_DEBUG_OBJECT(pipeline(), "[Seek] cannot seek, current state change is %s", gst_element_state_change_return_get_name(getStateResult));
        return;
    }

    if (player->isLooping() && isSeamlessSeekingEnabled() && state > GST_STATE_PAUSED) {
        // Segment seeking is synchronous, the pipeline state has not changed, no flush is done.
        GST_DEBUG_OBJECT(pipeline(), "Performing segment seek");
        m_isSeeking = true;
        if (!doSeek(target, player->rate())) {
            GST_DEBUG_OBJECT(pipeline(), "[Seek] seeking to %s failed", toString(target.time).utf8().data());
            return;
        }
        m_isEndReached = false;
        m_isSeeking = false;
        m_cachedPosition = MediaTime::zeroTime();
        timeChanged(target.time);
        return;
    }

    if (getStateResult == GST_STATE_CHANGE_ASYNC || state < GST_STATE_PAUSED || m_isEndReached) {
        m_isSeekPending = true;
        if (m_isEndReached && (!player->isLooping() || !isSeamlessSeekingEnabled())) {
            GST_DEBUG_OBJECT(pipeline(), "[Seek] reset pipeline");
            m_shouldResetPipeline = true;
            if (changePipelineState(GST_STATE_PAUSED) == ChangePipelineStateResult::Failed)
                loadingFailed(MediaPlayer::NetworkState::Empty);
        }
    } else {
        // We can seek now.
        if (!doSeek(target, player->rate())) {
            GST_DEBUG_OBJECT(pipeline(), "[Seek] seeking to %s failed", toString(target.time).utf8().data());
            return;
        }
    }

    m_isSeeking = true;
    m_seekTarget = target;
    m_isEndReached = false;
}

void MediaPlayerPrivateGStreamer::updatePlaybackRate()
{
    if (isMediaStreamPlayer() || !m_isChangingRate)
        return;

    GST_INFO_OBJECT(pipeline(), "Set playback rate to %f", m_playbackRate);

    // Mute the sound if the playback rate is negative or too extreme and audio pitch is not adjusted.
    bool mute = m_playbackRate <= 0 || (!m_shouldPreservePitch && (m_playbackRate < 0.8 || m_playbackRate > 2));

    GST_INFO_OBJECT(pipeline(), mute ? "Need to mute audio" : "Do not need to mute audio");

    if (m_lastPlaybackRate != m_playbackRate) {
        if (doSeek(SeekTarget { playbackPosition() }, m_playbackRate)) {
            g_object_set(m_pipeline.get(), "mute", mute, nullptr);
            m_lastPlaybackRate = m_playbackRate;
        } else {
            GST_ERROR_OBJECT(pipeline(), "Set rate to %f failed", m_playbackRate);
            m_playbackRate = m_lastPlaybackRate;
        }
    }

    m_isChangingRate = false;
    if (RefPtr player = m_player.get())
        player->rateChanged();
}

MediaTime MediaPlayerPrivateGStreamer::duration() const
{
    if (isMediaStreamPlayer())
        return MediaTime::positiveInfiniteTime();

    GST_TRACE_OBJECT(pipeline(), "Cached duration: %s", m_cachedDuration.toString().utf8().data());
    if (m_cachedDuration.isValid())
        return m_cachedDuration;

    MediaTime duration = platformDuration();
    if (!duration || duration.isInvalid())
        return MediaTime::zeroTime();

    m_cachedDuration = duration;

    return m_cachedDuration;
}

MediaTime MediaPlayerPrivateGStreamer::currentTime() const
{
    if (isMediaStreamPlayer()) {
        if (m_pausedTime)
            return m_pausedTime;

        return MediaTime::createWithDouble(MonotonicTime::now().secondsSinceEpoch().value()) - m_startTime;
    }

    if (!m_pipeline || m_didErrorOccur)
        return MediaTime::invalidTime();

    GST_TRACE_OBJECT(pipeline(), "seeking: %s, seekTarget: %s", boolForPrinting(m_isSeeking), m_seekTarget.toString().utf8().data());
    if (m_isSeeking)
        return m_seekTarget.time;

    return playbackPosition();
}

void MediaPlayerPrivateGStreamer::setRate(float rate)
{
    RefPtr player = m_player.get();

    float rateClamped = clampTo(rate, -20.0, 20.0);
    if (rateClamped != rate)
        GST_WARNING_OBJECT(pipeline(), "Clamping original rate (%f) to [-20, 20] (%f), higher rates cause crashes", rate, rateClamped);

    GST_DEBUG_OBJECT(pipeline(), "Setting playback rate to %f", rateClamped);
    // Avoid useless playback rate update.
    if (m_playbackRate == rateClamped) {
        // And make sure that upper layers were notified if rate was set.

        if (!m_isChangingRate && player && player->rate() != m_playbackRate)
            player->rateChanged();
        return;
    }

    if (m_isLiveStream.value_or(false)) {
        // Notify upper layers that we cannot handle passed rate.
        m_isChangingRate = false;
        if (player)
            player->rateChanged();
        return;
    }

    m_playbackRate = rateClamped;
    m_isChangingRate = true;

    if (!rateClamped) {
        m_isChangingRate = false;
        if (m_playbackRatePausedState == PlaybackRatePausedState::Playing || m_playbackRatePausedState == PlaybackRatePausedState::ShouldMoveToPlaying) {
            m_playbackRatePausedState = PlaybackRatePausedState::RatePaused;
            updateStates();
        }
        if (m_currentState == GST_STATE_PLAYING && !m_playbackRate
            && m_playbackRatePausedState != PlaybackRatePausedState::RatePaused) {
            GST_INFO_OBJECT(pipeline(), "Pausing stream because of zero playback rate in setRate");
            m_playbackRatePausedState = PlaybackRatePausedState::RatePaused;
            changePipelineState(GST_STATE_PAUSED);
            updatePlaybackRate();
        }
        return;
    } else if (m_playbackRatePausedState == PlaybackRatePausedState::RatePaused) {
        m_playbackRatePausedState = PlaybackRatePausedState::ShouldMoveToPlaying;
        updateStates();
    }

    GstState state, pending;
    gst_element_get_state(m_pipeline.get(), &state, &pending, 0);
    if ((state != GST_STATE_PLAYING && state != GST_STATE_PAUSED)
        || (pending == GST_STATE_PAUSED))
        return;
    if (m_currentState == GST_STATE_PAUSED && m_playbackRate
        && m_playbackRatePausedState != PlaybackRatePausedState::Playing) {
        m_playbackRatePausedState = PlaybackRatePausedState::Playing;
        GST_INFO_OBJECT(pipeline(), "[Buffering] Restarting playback (because of resuming from zero playback rate) in setRate");
        changePipelineState(GST_STATE_PLAYING);
    }
    updatePlaybackRate();
}

double MediaPlayerPrivateGStreamer::rate() const
{
    return m_playbackRate;
}

void MediaPlayerPrivateGStreamer::setPreservesPitch(bool preservesPitch)
{
    GST_DEBUG_OBJECT(pipeline(), "Preserving audio pitch: %s", boolForPrinting(preservesPitch));
    m_shouldPreservePitch = preservesPitch;
}

void MediaPlayerPrivateGStreamer::setPreload(MediaPlayer::Preload preload)
{
    if (isMediaStreamPlayer())
        return;

    GST_DEBUG_OBJECT(pipeline(), "Setting preload to %s", convertEnumerationToString(preload).utf8().data());
    if (preload == MediaPlayer::Preload::Auto && m_isLiveStream.value_or(false))
        return;

    m_preload = preload;
    updateDownloadBufferingFlag();

    if (m_isDelayingLoad && m_preload != MediaPlayer::Preload::None) {
        m_isDelayingLoad = false;
        commitLoad();
    }
}

const PlatformTimeRanges& MediaPlayerPrivateGStreamer::buffered() const
{
    if (m_didErrorOccur || m_isLiveStream.value_or(false))
        return PlatformTimeRanges::emptyRanges();

    MediaTime mediaDuration = duration();
    if (!mediaDuration || mediaDuration.isPositiveInfinite())
        return PlatformTimeRanges::emptyRanges();

    GRefPtr<GstQuery> query = adoptGRef(gst_query_new_buffering(GST_FORMAT_PERCENT));

    if (!gst_element_query(m_pipeline.get(), query.get()))
        return PlatformTimeRanges::emptyRanges();

    m_buffered.clear();
    unsigned numBufferingRanges = gst_query_get_n_buffering_ranges(query.get());
    for (unsigned index = 0; index < numBufferingRanges; index++) {
        gint64 rangeStart = 0, rangeStop = 0;
        if (gst_query_parse_nth_buffering_range(query.get(), index, &rangeStart, &rangeStop)) {
            uint64_t startTime = gst_util_uint64_scale_int_round(toGstUnsigned64Time(mediaDuration), rangeStart, GST_FORMAT_PERCENT_MAX);
            uint64_t stopTime = gst_util_uint64_scale_int_round(toGstUnsigned64Time(mediaDuration), rangeStop, GST_FORMAT_PERCENT_MAX);
            m_buffered.add(MediaTime(startTime, GST_SECOND), MediaTime(stopTime, GST_SECOND));
        }
    }

    // Fallback to the more general maxTimeLoaded() if no range has been found.
    if (!m_buffered.length()) {
        MediaTime loaded = maxTimeLoaded();
        if (loaded.isValid() && loaded)
            m_buffered.add(MediaTime::zeroTime(), loaded);
    }

    return m_buffered;
}

MediaTime MediaPlayerPrivateGStreamer::maxTimeSeekable() const
{
    GST_TRACE_OBJECT(pipeline(), "errorOccured: %s, isLiveStream: %s", boolForPrinting(m_didErrorOccur), boolForPrinting(m_isLiveStream));
    if (m_didErrorOccur)
        return MediaTime::zeroTime();

    if (m_isLiveStream.value_or(false))
        return MediaTime::zeroTime();

    if (isMediaStreamPlayer())
        return MediaTime::zeroTime();

    MediaTime duration = this->duration();
    GST_DEBUG_OBJECT(pipeline(), "maxTimeSeekable, duration: %s", toString(duration).utf8().data());
    // Infinite duration means live stream.
    if (duration.isPositiveInfinite())
        return MediaTime::zeroTime();

    return duration;
}

MediaTime MediaPlayerPrivateGStreamer::maxTimeLoaded() const
{
    if (m_didErrorOccur)
        return MediaTime::zeroTime();

    MediaTime loaded = m_maxTimeLoaded;
    if (m_isEndReached)
        loaded = duration();
    GST_LOG_OBJECT(pipeline(), "maxTimeLoaded: %s", toString(loaded).utf8().data());
    return loaded;
}

bool MediaPlayerPrivateGStreamer::didLoadingProgress() const
{
    if (m_didErrorOccur || m_loadingStalled)
        return false;

    if (WEBKIT_IS_WEB_SRC(m_source.get())) {
        GST_LOG_OBJECT(pipeline(), "Last network read position: %" G_GUINT64_FORMAT ", current: %" G_GUINT64_FORMAT, m_readPositionAtLastDidLoadingProgress, m_networkReadPosition);
        bool didLoadingProgress = m_readPositionAtLastDidLoadingProgress < m_networkReadPosition;
        m_readPositionAtLastDidLoadingProgress = m_networkReadPosition;
        GST_LOG_OBJECT(pipeline(), "didLoadingProgress: %s", boolForPrinting(didLoadingProgress));
        return didLoadingProgress;
    }

    if (UNLIKELY(!m_pipeline || !duration() || (!isMediaSource() && !totalBytes())))
        return false;

    MediaTime currentMaxTimeLoaded = maxTimeLoaded();
    bool didLoadingProgress = currentMaxTimeLoaded != m_maxTimeLoadedAtLastDidLoadingProgress;
    m_maxTimeLoadedAtLastDidLoadingProgress = currentMaxTimeLoaded;
    GST_LOG_OBJECT(pipeline(), "didLoadingProgress: %s", boolForPrinting(didLoadingProgress));
    return didLoadingProgress;
}

unsigned long long MediaPlayerPrivateGStreamer::totalBytes() const
{
    if (m_didErrorOccur || !m_source || m_isLiveStream.value_or(false) || isMediaStreamPlayer())
        return 0;

    if (m_totalBytes)
        return m_totalBytes;

    GstFormat fmt = GST_FORMAT_BYTES;
    gint64 length = 0;
    if (gst_element_query_duration(m_source.get(), fmt, &length)) {
        GST_INFO_OBJECT(pipeline(), "totalBytes %" G_GINT64_FORMAT, length);
        m_totalBytes = static_cast<unsigned long long>(length);
        m_isLiveStream = !length;
        return m_totalBytes;
    }

    // Fall back to querying the source pads manually. See also https://bugzilla.gnome.org/show_bug.cgi?id=638749
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
    m_isLiveStream = !length;
    return m_totalBytes;
}

std::optional<bool> MediaPlayerPrivateGStreamer::isCrossOrigin(const SecurityOrigin& origin) const
{
    if (WEBKIT_IS_WEB_SRC(m_source.get()))
        return webKitSrcIsCrossOrigin(WEBKIT_WEB_SRC(m_source.get()), origin);
    return false;
}

void MediaPlayerPrivateGStreamer::simulateAudioInterruption()
{
    GstMessage* message = gst_message_new_request_state(GST_OBJECT(m_pipeline.get()), GST_STATE_PAUSED);
    gst_element_post_message(m_pipeline.get(), message);
}

#if ENABLE(WEB_AUDIO)
void MediaPlayerPrivateGStreamer::ensureAudioSourceProvider()
{
    if (!m_audioSourceProvider)
        m_audioSourceProvider = AudioSourceProviderGStreamer::create();
}

AudioSourceProvider* MediaPlayerPrivateGStreamer::audioSourceProvider()
{
    ensureAudioSourceProvider();
    return m_audioSourceProvider.get();
}
#endif

void MediaPlayerPrivateGStreamer::durationChanged()
{
    MediaTime previousDuration = duration();
    m_cachedDuration = MediaTime::invalidTime();

    // Avoid emitting durationChanged in the case where the previous
    // duration was 0 because that case is already handled by the
    // HTMLMediaElement.
    if (previousDuration && duration() != previousDuration) {
        if (RefPtr player = m_player.get())
            player->durationChanged();
    }
}

void MediaPlayerPrivateGStreamer::sourceSetup(GstElement* sourceElement)
{
    GST_DEBUG_OBJECT(pipeline(), "Source element set-up for %s", GST_ELEMENT_NAME(sourceElement));

    m_source = sourceElement;

    if (WEBKIT_IS_WEB_SRC(m_source.get())) {
        auto* source = WEBKIT_WEB_SRC_CAST(m_source.get());
        webKitWebSrcSetReferrer(source, m_referrer);
        webKitWebSrcSetResourceLoader(source, m_loader);
#if ENABLE(MEDIA_STREAM)
    } else if (WEBKIT_IS_MEDIA_STREAM_SRC(sourceElement)) {
        RefPtr player = m_player.get();
        auto stream = m_streamPrivate.get();
        ASSERT(stream);
        webkitMediaStreamSrcSetStream(WEBKIT_MEDIA_STREAM_SRC(sourceElement), stream, player && player->isVideoPlayer());
#endif
    }
}

void MediaPlayerPrivateGStreamer::sourceSetupCallback(MediaPlayerPrivateGStreamer* player, GstElement* sourceElement)
{
    player->sourceSetup(sourceElement);
}

MediaPlayerPrivateGStreamer::ChangePipelineStateResult MediaPlayerPrivateGStreamer::changePipelineState(GstState newState)
{
    ASSERT(m_pipeline);

    if (!m_isVisibleInViewport && newState > GST_STATE_PAUSED) {
        GST_DEBUG_OBJECT(pipeline(), "Saving state for when player becomes visible: %s", gst_element_state_get_name(newState));
        m_invisiblePlayerState = newState;
        return ChangePipelineStateResult::Ok;
    }

    GstState currentState, pending;
    GstStateChangeReturn change = gst_element_get_state(m_pipeline.get(), &currentState, &pending, 0);
    if (isPipelineWaitingPreroll(currentState, pending, change)) {
        GST_DEBUG_OBJECT(pipeline(), "rejected state change during preroll");
        return ChangePipelineStateResult::Rejected;
    }

    GST_DEBUG_OBJECT(pipeline(), "Changing state change to %s from %s with %s pending", gst_element_state_get_name(newState),
        gst_element_state_get_name(currentState), gst_element_state_get_name(pending));

    change = gst_element_set_state(m_pipeline.get(), newState);
    GstState pausedOrPlaying = newState == GST_STATE_PLAYING ? GST_STATE_PAUSED : GST_STATE_PLAYING;
    if (currentState != pausedOrPlaying && change == GST_STATE_CHANGE_FAILURE)
        return ChangePipelineStateResult::Failed;

    m_isPipelinePlaying = newState == GST_STATE_PLAYING;

    // Create a timer when entering the READY state so that we can free resources if we stay for too long on READY.
    // Also lets remove the timer if we request a state change for any state other than READY. See also https://bugs.webkit.org/show_bug.cgi?id=117354
    if (RefPtr player = m_player.get(); newState == GST_STATE_PAUSED && m_isEndReached && player && !player->isLooping()
        && !isMediaSource() && !m_pausedTimerHandler.isActive()) {
        // Max interval in seconds to stay in the PAUSED state after video finished on manual state change requests.
        static const Seconds readyStateTimerDelay { 5_min };
        m_pausedTimerHandler.startOneShot(readyStateTimerDelay);
    } else if (newState != GST_STATE_PAUSED)
        m_pausedTimerHandler.stop();

    return ChangePipelineStateResult::Ok;
}

void MediaPlayerPrivateGStreamer::setPlaybinURL(const URL& url)
{
    // Clean out everything after file:// url path.
    String cleanURLString(url.string());
    if (url.protocolIsFile())
        cleanURLString = cleanURLString.left(url.pathEnd());

    m_url = URL { cleanURLString };
    GST_INFO_OBJECT(pipeline(), "Load %s", m_url.string().utf8().data());
    g_object_set(m_pipeline.get(), "uri", m_url.string().utf8().data(), nullptr);
}

static void setSyncOnClock(GstElement* element, bool sync)
{
    if (!element)
        return;

    if (!GST_IS_BIN(element)) {
        g_object_set(element, "sync", sync, nullptr);
        return;
    }

    GUniquePtr<GstIterator> iterator(gst_bin_iterate_sinks(GST_BIN_CAST(element)));
    while (gst_iterator_foreach(iterator.get(), static_cast<GstIteratorForeachFunction>([](const GValue* item, void* syncPtr) {
        bool* sync = static_cast<bool*>(syncPtr);
        setSyncOnClock(GST_ELEMENT_CAST(g_value_get_object(item)), *sync);
    }), &sync) == GST_ITERATOR_RESYNC)
        gst_iterator_resync(iterator.get());
}

void MediaPlayerPrivateGStreamer::syncOnClock(bool sync)
{
    auto& quirksManager = GStreamerQuirksManager::singleton();
    if (quirksManager.supportsVideoHolePunchRendering() && !quirksManager.sinksRequireClockSynchronization())
        return;

    setSyncOnClock(videoSink(), sync);
    setSyncOnClock(audioSink(), sync);
}

template <typename TrackPrivateType>
void MediaPlayerPrivateGStreamer::notifyPlayerOfTrack()
{
    if (UNLIKELY(!m_pipeline || !m_source))
        return;

    RefPtr player = m_player.get();
    if (!player)
        return;

    ASSERT(m_isLegacyPlaybin);

    using TrackType = TrackPrivateBaseGStreamer::TrackType;
    std::variant<HashMap<AtomString, Ref<AudioTrackPrivateGStreamer>>*, HashMap<AtomString, Ref<VideoTrackPrivateGStreamer>>*, HashMap<AtomString, Ref<InbandTextTrackPrivateGStreamer>>*> variantTracks = static_cast<HashMap<AtomString, Ref<TrackPrivateType>>*>(0);
    auto type(static_cast<TrackType>(variantTracks.index()));
    const char* typeName;
    bool* hasType;
    switch (type) {
    case TrackType::Audio:
        typeName = "audio";
        hasType = &m_hasAudio;
        variantTracks = &m_audioTracks;
        break;
    case TrackType::Video:
        typeName = "video";
        hasType = &m_hasVideo;
        variantTracks = &m_videoTracks;
        break;
    case TrackType::Text:
        typeName = "text";
        hasType = nullptr;
        variantTracks = &m_textTracks;
        break;
    default:
        ASSERT_NOT_REACHED();
    }
    auto& tracks = *std::get<HashMap<AtomString, Ref<TrackPrivateType>>*>(variantTracks);

    // Ignore notifications after a EOS. We don't want the tracks to disappear when the video is finished.
    if (m_isEndReached && (type == TrackType::Audio || type == TrackType::Video))
        return;

    unsigned numberOfTracks = 0;
    StringPrintStream numberOfTracksProperty;
    numberOfTracksProperty.printf("n-%s", typeName);
    g_object_get(m_pipeline.get(), numberOfTracksProperty.toCString().data(), &numberOfTracks, nullptr);

    GST_INFO_OBJECT(pipeline(), "Media has %d %s tracks", numberOfTracks, typeName);

    if (hasType) {
        bool oldHasType = *hasType;
        *hasType = numberOfTracks > 0;
        if (oldHasType != *hasType)
            player->characteristicChanged();

        if (*hasType && type == TrackType::Video)
            player->sizeChanged();
    }

    Vector<AtomString> validStreams;
    StringPrintStream getPadProperty;
    getPadProperty.printf("get-%s-pad", typeName);

    bool changed = false;
    for (unsigned i = 0; i < numberOfTracks; ++i) {
        GRefPtr<GstPad> pad;
        g_signal_emit_by_name(m_pipeline.get(), getPadProperty.toCString().data(), i, &pad.outPtr(), nullptr);
        ASSERT(pad);
        if (!pad)
            continue;

        AtomString streamId(TrackPrivateBaseGStreamer::trackIdFromPadStreamStartOrUniqueID(type, i, pad));
        validStreams.append(streamId);

        if (i < tracks.size()) {
            RefPtr<TrackPrivateType> existingTrack = tracks.get(streamId);
            if (existingTrack) {
                ASSERT(existingTrack->index() == i);
                // TODO: Position of index should remain the same on replay.
                existingTrack->setIndex(i);
                // If the video has been played twice, the track is still there, but we need
                // to update the pad pointer.
                if (existingTrack->pad() != pad)
                    existingTrack->setPad(GRefPtr(pad));
                continue;
            }
        }

        auto track = TrackPrivateType::create(*this, i, GRefPtr(pad));
        ASSERT(track->stringId() == streamId);
        if (!track->trackIndex() && (type == TrackType::Audio || type == TrackType::Video))
            track->setActive(true);

        std::variant<AudioTrackPrivate*, VideoTrackPrivate*, InbandTextTrackPrivate*> variantTrack(&track.get());
        switch (variantTrack.index()) {
        case TrackType::Audio:
            player->addAudioTrack(*std::get<AudioTrackPrivate*>(variantTrack));
            break;
        case TrackType::Video:
            player->addVideoTrack(*std::get<VideoTrackPrivate*>(variantTrack));
            break;
        case TrackType::Text:
            player->addTextTrack(*std::get<InbandTextTrackPrivate*>(variantTrack));
            break;
        }
        tracks.add(track->stringId(), WTFMove(track));
        changed = true;
    }

    // Purge invalid tracks
    changed = changed || tracks.removeIf([validStreams](auto& keyAndValue) {
        return !validStreams.contains(keyAndValue.key);
    });

    if (changed)
        player->mediaEngineUpdated();
}

bool MediaPlayerPrivateGStreamer::hasFirstVideoSampleReachedSink() const
{
    Locker sampleLocker { m_sampleMutex };
    return !!m_sample;
}

void MediaPlayerPrivateGStreamer::videoSinkCapsChanged(GstPad* videoSinkPad)
{
    GRefPtr<GstCaps> caps = adoptGRef(gst_pad_get_current_caps(videoSinkPad));
    if (!caps) {
        // This can happen when downgrading the state of the pipeline, which causes the caps to be unset.
        return;
    }
    // We're in videoSinkPad streaming thread.
    ASSERT(!isMainThread());
    GST_DEBUG_OBJECT(videoSinkPad, "Received new caps: %" GST_PTR_FORMAT, caps.get());

    if (!hasFirstVideoSampleReachedSink()) {
        // We want to wait for the sink to receive the first buffer before emitting dimensions, since only by then we
        // are guaranteed that any potential tag event with a rotation has been handled.
        GST_DEBUG_OBJECT(videoSinkPad, "Ignoring notify::caps until the first buffer reaches the sink.");
        return;
    }

    RunLoop::main().dispatch([weakThis = ThreadSafeWeakPtr { *this }, this, caps = WTFMove(caps)] {
        RefPtr self = weakThis.get();
        if (!self)
            return;
        updateVideoSizeAndOrientationFromCaps(caps.get());
    });
}

void MediaPlayerPrivateGStreamer::handleTextSample(GstSample* sample, const char* streamId)
{
    for (auto& track : m_textTracks.values()) {
        if (!strcmp(track->stringId().string().utf8().data(), streamId)) {
            track->handleSample(sample);
            return;
        }
    }

    GST_WARNING_OBJECT(m_pipeline.get(), "Got sample with unknown stream ID %s.", streamId);
}

MediaTime MediaPlayerPrivateGStreamer::platformDuration() const
{
    if (!m_pipeline)
        return MediaTime::invalidTime();

    if (isMediaStreamPlayer())
        return MediaTime::positiveInfiniteTime();

    GST_TRACE_OBJECT(pipeline(), "errorOccured: %s, pipeline state: %s", boolForPrinting(m_didErrorOccur), gst_element_state_get_name(GST_STATE(m_pipeline.get())));
    if (m_didErrorOccur)
        return MediaTime::invalidTime();

    // The duration query would fail on a not-prerolled pipeline.
    if (GST_STATE(m_pipeline.get()) < GST_STATE_PAUSED)
        return MediaTime::invalidTime();

    int64_t duration = 0;
    if (!gst_element_query_duration(m_pipeline.get(), GST_FORMAT_TIME, &duration) || !GST_CLOCK_TIME_IS_VALID(duration)) {
        GST_DEBUG_OBJECT(pipeline(), "Time duration query failed for %s", m_url.string().utf8().data());
        // https://www.w3.org/TR/2011/WD-html5-20110113/video.html#getting-media-metadata
        // In order to be strict with the spec, consider that not "enough of the media data has been fetched to determine
        // the duration of the media resource" and therefore return invalidTime only when we know for sure that the
        // stream isn't live (treating empty value as unsure).
        return m_isLiveStream.value_or(true) ? MediaTime::positiveInfiniteTime() : MediaTime::invalidTime();
    }

    GST_LOG_OBJECT(pipeline(), "Duration: %" GST_TIME_FORMAT, GST_TIME_ARGS(duration));
    return MediaTime(duration, GST_SECOND);
}

bool MediaPlayerPrivateGStreamer::isMuted() const
{
    GST_INFO_OBJECT(pipeline(), "Player is muted: %s", boolForPrinting(m_isMuted));
    return m_isMuted;
}

void MediaPlayerPrivateGStreamer::commitLoad()
{
    ASSERT(!m_isDelayingLoad);
    GST_DEBUG_OBJECT(pipeline(), "Committing load.");

    // GStreamer needs to have the pipeline set to a paused state to
    // start providing anything useful.
    changePipelineState(GST_STATE_PAUSED);

    updateDownloadBufferingFlag();
    updateStates();
}

void MediaPlayerPrivateGStreamer::fillTimerFired()
{
    if (m_didErrorOccur) {
        GST_DEBUG_OBJECT(pipeline(), "[Buffering] An error occurred, disabling the fill timer");
        m_fillTimer.stop();
        return;
    }

    GRefPtr<GstQuery> query = adoptGRef(gst_query_new_buffering(GST_FORMAT_PERCENT));
    double fillStatus = 100.0;
    GstBufferingMode mode = GST_BUFFERING_DOWNLOAD;

    if (gst_element_query(pipeline(), query.get())) {
        gst_query_parse_buffering_stats(query.get(), &mode, nullptr, nullptr, nullptr);

        int percentage;
        gst_query_parse_buffering_percent(query.get(), nullptr, &percentage);
        fillStatus = percentage;
    } else if (m_httpResponseTotalSize) {
        GST_DEBUG_OBJECT(pipeline(), "[Buffering] Query failed, falling back to network read position estimation");
        fillStatus = 100.0 * (static_cast<double>(m_networkReadPosition) / static_cast<double>(m_httpResponseTotalSize));
    } else {
        GST_DEBUG_OBJECT(pipeline(), "[Buffering] Unable to determine on-disk buffering status");
        return;
    }

    updateBufferingStatus(mode, fillStatus);
}

void MediaPlayerPrivateGStreamer::loadStateChanged()
{
    updateStates();
}

void MediaPlayerPrivateGStreamer::timeChanged(const MediaTime& seekedTime)
{
    updateStates();
    GST_DEBUG_OBJECT(pipeline(), "Emitting timeChanged notification (seekCompleted:%d)", seekedTime.isValid());
    if (RefPtr player = m_player.get()) {
        if (seekedTime.isValid())
            player->seeked(seekedTime);
        player->timeChanged();
    }
}

void MediaPlayerPrivateGStreamer::loadingFailed(MediaPlayer::NetworkState networkError, MediaPlayer::ReadyState readyState, bool forceNotifications)
{
    GST_WARNING("Loading failed, error: %s", convertEnumerationToString(networkError).utf8().data());

    RefPtr player = m_player.get();

    m_didErrorOccur = true;
    if (forceNotifications || m_networkState != networkError) {
        m_networkState = networkError;
        if (player)
            player->networkStateChanged();
    }
    if (forceNotifications || m_readyState != readyState) {
        m_readyState = readyState;
        if (player)
            player->readyStateChanged();
    }

    // Loading failed, remove ready timer.
    m_pausedTimerHandler.stop();
}

GstElement* MediaPlayerPrivateGStreamer::createAudioSink()
{
    auto& quirksManager = GStreamerQuirksManager::singleton();

    // If audio is being controlled by an another pipeline, creating sink here may interfere with
    // audio playback. Instead, check if an audio sink was setup in handleMessage and use it.
    if (quirksManager.isEnabled())
        return nullptr;

    RefPtr player = m_player.get();
    if (!player)
        return nullptr;

    // For platform specific audio sinks, they need to be properly upranked so that they get properly autoplugged.

    auto role = player->isVideoPlayer() ? "video"_s : "music"_s;
    GstElement* audioSink = createPlatformAudioSink(role);
    RELEASE_ASSERT(audioSink);
    if (!audioSink)
        return nullptr;

#if ENABLE(WEB_AUDIO)
    GstElement* audioSinkBin = gst_bin_new("audio-sink");
    ensureAudioSourceProvider();
    m_audioSourceProvider->configureAudioBin(audioSinkBin, audioSink);
    return audioSinkBin;
#else
    return audioSink;
#endif
}

bool MediaPlayerPrivateGStreamer::isMediaStreamPlayer() const
{
#if ENABLE(MEDIA_STREAM)
    if (m_source)
        return WEBKIT_IS_MEDIA_STREAM_SRC(m_source.get());
#endif
    return m_url.protocolIs("mediastream"_s);
}

GstClockTime MediaPlayerPrivateGStreamer::gstreamerPositionFromSinks() const
{
    gint64 gstreamerPosition = GST_CLOCK_TIME_NONE;
    // Asking directly to the sinks and choosing the highest value is faster than asking to the pipeline.
    GST_TRACE_OBJECT(pipeline(), "Querying position to audio sink (if any).");
    GRefPtr<GstQuery> query = adoptGRef(gst_query_new_position(GST_FORMAT_TIME));
    if (m_audioSink && gst_element_query(m_audioSink.get(), query.get())) {
        gint64 audioPosition = GST_CLOCK_TIME_NONE;
        gst_query_parse_position(query.get(), 0, &audioPosition);
        if (GST_CLOCK_TIME_IS_VALID(audioPosition))
            gstreamerPosition = audioPosition;
        GST_TRACE_OBJECT(pipeline(), "Audio position %" GST_TIME_FORMAT, GST_TIME_ARGS(audioPosition));
        query = adoptGRef(gst_query_new_position(GST_FORMAT_TIME));
    }
    GST_TRACE_OBJECT(pipeline(), "Querying position to video sink (if any).");
    RefPtr player = m_player.get();
    if (player && player->isVideoPlayer() && m_videoSink && gst_element_query(m_videoSink.get(), query.get())) {
        gint64 videoPosition = GST_CLOCK_TIME_NONE;
        gst_query_parse_position(query.get(), 0, &videoPosition);
        GST_TRACE_OBJECT(pipeline(), "Video position %" GST_TIME_FORMAT, GST_TIME_ARGS(videoPosition));
        if (GST_CLOCK_TIME_IS_VALID(videoPosition) && (!GST_CLOCK_TIME_IS_VALID(gstreamerPosition)
            || (m_playbackRate >= 0 && videoPosition > gstreamerPosition)
            || (m_playbackRate < 0 && videoPosition < gstreamerPosition)))
            gstreamerPosition = videoPosition;
    }
    return static_cast<GstClockTime>(gstreamerPosition);
}

MediaTime MediaPlayerPrivateGStreamer::playbackPosition() const
{
    GST_TRACE_OBJECT(pipeline(), "isEndReached: %s, seeking: %s, seekTime: %s", boolForPrinting(m_isEndReached), boolForPrinting(m_isSeeking), m_seekTarget.time.toString().utf8().data());

#if ENABLE(MEDIA_STREAM)
    RefPtr player = m_player.get();
    if (m_streamPrivate && player && player->isVideoPlayer() && !hasFirstVideoSampleReachedSink())
        return MediaTime::zeroTime();
#endif

    if (m_isSeeking)
        return m_seekTarget.time;
    if (m_isEndReached)
        return m_playbackRate > 0 ? duration() : MediaTime::zeroTime();

    if (m_isCachedPositionValid) {
        GST_TRACE_OBJECT(pipeline(), "Returning cached position: %s", m_cachedPosition.toString().utf8().data());
        return m_cachedPosition;
    }

    // We can't trust sinks position when pipeline is flushed (e.g. after MSE samples removal).
    GstClockTime gstreamerPosition = isPipelineWaitingPreroll() ? GST_CLOCK_TIME_NONE : gstreamerPositionFromSinks();
    GST_TRACE_OBJECT(pipeline(), "Position %" GST_TIME_FORMAT ", canFallBackToLastFinishedSeekPosition: %s", GST_TIME_ARGS(gstreamerPosition), boolForPrinting(m_canFallBackToLastFinishedSeekPosition));

    // Cached position is marked as non valid here but we might fail to get a new one so initializing to this as "educated guess".
    MediaTime playbackPosition = m_cachedPosition;

    if (GST_CLOCK_TIME_IS_VALID(gstreamerPosition))
        playbackPosition = MediaTime(gstreamerPosition, GST_SECOND);
    else if (m_canFallBackToLastFinishedSeekPosition)
        playbackPosition = m_seekTarget.time;

    setCachedPosition(playbackPosition);
    invalidateCachedPositionOnNextIteration();
    return playbackPosition;
}

void MediaPlayerPrivateGStreamer::updateEnabledVideoTrack()
{
    VideoTrackPrivateGStreamer* wantedTrack = nullptr;
    for (auto& pair : m_videoTracks) {
        auto& track = pair.value.get();
        if (track.selected()) {
            wantedTrack = &track;
            break;
        }
    }

    // No active track, no changes.
    if (!wantedTrack)
        return;

    if (m_isLegacyPlaybin) {
        GST_DEBUG_OBJECT(m_pipeline.get(), "Setting playbin2 current-video=%d", wantedTrack->trackIndex());
        g_object_set(m_pipeline.get(), "current-video", wantedTrack->trackIndex(), nullptr);
    } else {
        m_wantedVideoStreamId = wantedTrack->stringId();
        playbin3SendSelectStreamsIfAppropriate();
    }
}

void MediaPlayerPrivateGStreamer::updateEnabledAudioTrack()
{
    AudioTrackPrivateGStreamer* wantedTrack = nullptr;
    for (auto& pair : m_audioTracks) {
        auto& track = pair.value.get();
        if (track.enabled()) {
            wantedTrack = &track;
            break;
        }
    }

    // No active track, no changes.
    if (!wantedTrack)
        return;

    if (m_isLegacyPlaybin) {
        GST_DEBUG_OBJECT(m_pipeline.get(), "Setting playbin2 current-audio=%d", wantedTrack->trackIndex());
        g_object_set(m_pipeline.get(), "current-audio", wantedTrack->trackIndex(), nullptr);
    } else {
        m_wantedAudioStreamId = wantedTrack->stringId();
        playbin3SendSelectStreamsIfAppropriate();
    }
}

void MediaPlayerPrivateGStreamer::playbin3SendSelectStreamsIfAppropriate()
{
    ASSERT(!m_isLegacyPlaybin);

    bool haveDifferentStreamIds = (m_wantedAudioStreamId != m_currentAudioStreamId || m_wantedVideoStreamId != m_currentVideoStreamId);
    bool shouldSendSelectStreams = !m_waitingForStreamsSelectedEvent && haveDifferentStreamIds && m_currentState == GST_STATE_PLAYING;
    GST_DEBUG_OBJECT(m_pipeline.get(), "Checking if to send SELECT_STREAMS, m_waitingForStreamsSelectedEvent = %s, haveDifferentStreamIds = %s, m_currentState = %s... shouldSendSelectStreams = %s",
        boolForPrinting(m_waitingForStreamsSelectedEvent), boolForPrinting(haveDifferentStreamIds), gst_element_state_get_name(m_currentState), boolForPrinting(shouldSendSelectStreams));
    if (!shouldSendSelectStreams)
        return;

    GList* streams = nullptr;
    if (!m_wantedVideoStreamId.isNull()) {
        m_requestedVideoStreamId = m_wantedVideoStreamId;
        streams = g_list_append(streams, g_strdup(m_wantedVideoStreamId.string().utf8().data()));
    }
    if (!m_wantedAudioStreamId.isNull()) {
        m_requestedAudioStreamId = m_wantedAudioStreamId;
        streams = g_list_append(streams, g_strdup(m_wantedAudioStreamId.string().utf8().data()));
    }
    if (!m_wantedTextStreamId.isNull()) {
        m_requestedTextStreamId = m_wantedTextStreamId;
        streams = g_list_append(streams, g_strdup(m_wantedTextStreamId.string().utf8().data()));
    }

    if (!streams)
        return;

    m_waitingForStreamsSelectedEvent = true;
    gst_element_send_event(m_pipeline.get(), gst_event_new_select_streams(streams));
    g_list_free_full(streams, reinterpret_cast<GDestroyNotify>(g_free));
}

void MediaPlayerPrivateGStreamer::updateTracks(const GRefPtr<GstObject>& collectionOwner)
{
    ASSERT(!m_isLegacyPlaybin);

    bool oldHasAudio = m_hasAudio;
    bool oldHasVideo = m_hasVideo;

    RefPtr player = m_player.get();

    // fast/mediastream/MediaStream-video-element-remove-track.html expects audio tracks gone, not deactivated.
    if (player) {
        for (auto& track : m_audioTracks.values())
            player->removeAudioTrack(track);
    }
    m_audioTracks.clear();

    for (auto& track : m_videoTracks.values())
        track->setActive(false);
    for (auto& track : m_textTracks.values())
        track->setActive(false);

    auto scopeExit = makeScopeExit([oldHasAudio, oldHasVideo, protectedThis = ThreadSafeWeakPtr { *this }, this] {
        RefPtr self = protectedThis.get();
        if (!self)
            return;

        RefPtr player = m_player.get();

        m_hasAudio = !m_audioTracks.isEmpty();
        m_hasVideo = false;

        for (auto& track : m_videoTracks.values()) {
            if (track->selected()) {
                m_hasVideo = true;
                break;
            }
        }

        if (player) {
            if (oldHasVideo != m_hasVideo || oldHasAudio != m_hasAudio)
                player->characteristicChanged();

            if (!oldHasVideo && m_hasVideo)
                player->sizeChanged();

            player->mediaEngineUpdated();
        }

        if (!m_hasAudio && !m_hasVideo)
            didEnd();
    });

    if (!m_streamCollection)
        return;

    using TextTrackPrivateGStreamer = InbandTextTrackPrivateGStreamer;
#define CREATE_OR_SELECT_TRACK(type, Type) G_STMT_START { \
        bool isTrackCached = m_##type##Tracks.contains(streamId);       \
        if (!isTrackCached) { \
            auto track = Type##TrackPrivateGStreamer::create(*this, type##TrackIndex, stream); \
            if (player)                                                 \
                player->add##Type##Track(track);                        \
            m_##type##Tracks.add(streamId, WTFMove(track));             \
        }                                                               \
        auto track = m_##type##Tracks.get(streamId);                    \
        if (isTrackCached)                                              \
            track->updateConfigurationFromCaps(WTFMove(caps));          \
        auto trackId = track->stringId();                               \
        if (!type##TrackIndex) { \
            m_wanted##Type##StreamId = trackId;                         \
            m_requested##Type##StreamId = trackId;                      \
            track->setActive(true);                                     \
        }                                                               \
        type##TrackIndex++;                                             \
    } G_STMT_END

    bool useMediaSource = isMediaSource();
    unsigned audioTrackIndex = 0;
    unsigned videoTrackIndex = 0;
    unsigned textTrackIndex = 0;
    unsigned length = gst_stream_collection_get_size(m_streamCollection.get());
    GST_DEBUG_OBJECT(pipeline(), "Received STREAM_COLLECTION message with upstream id \"%s\" from %" GST_PTR_FORMAT " defining the following streams:", gst_stream_collection_get_upstream_id(m_streamCollection.get()), collectionOwner.get());
    for (unsigned i = 0; i < length; i++) {
        auto* stream = gst_stream_collection_get_stream(m_streamCollection.get(), i);
        RELEASE_ASSERT(stream);
        auto streamId = AtomString::fromLatin1(gst_stream_get_stream_id(stream));
        auto type = gst_stream_get_stream_type(stream);
        auto caps = adoptGRef(gst_stream_get_caps(stream));

        GST_DEBUG_OBJECT(pipeline(), "#%u %s track with ID %s and caps %" GST_PTR_FORMAT, i, gst_stream_type_get_name(type), streamId.string().ascii().data(), caps.get());

        if (type & GST_STREAM_TYPE_AUDIO) {
            CREATE_OR_SELECT_TRACK(audio, Audio);
            configureMediaStreamAudioTracks();
        } else if (type & GST_STREAM_TYPE_VIDEO && player && player->isVideoPlayer())
            CREATE_OR_SELECT_TRACK(video, Video);
        else if (type & GST_STREAM_TYPE_TEXT && !useMediaSource)
            CREATE_OR_SELECT_TRACK(text, Text);
        else
            GST_WARNING("Unknown track type found for stream %s", streamId.string().ascii().data());
    }
#undef CREATE_OR_SELECT_TRACK
}

void MediaPlayerPrivateGStreamer::handleStreamCollectionMessage(GstMessage* message)
{
    if (m_isLegacyPlaybin)
        return;

    if (!m_source)
        return;

    // GStreamer workaround: Unfortunately, when we have a stream-collection aware source (like
    // WebKitMediaSrc) parsebin and decodebin3 emit their own stream-collection messages, but late,
    // and sometimes with duplicated streams. Let's only listen for stream-collection messages from
    // the source to avoid these issues.
    if (!(g_str_has_prefix(GST_OBJECT_NAME(m_source.get()), "filesrc") || WEBKIT_IS_WEB_SRC(m_source.get())) && GST_MESSAGE_SRC(message) != GST_OBJECT(m_source.get())) {
        GST_DEBUG_OBJECT(pipeline(), "Ignoring redundant STREAM_COLLECTION from %" GST_PTR_FORMAT, message->src);
        return;
    }

    ASSERT(GST_MESSAGE_TYPE(message) == GST_MESSAGE_STREAM_COLLECTION);
    gst_message_parse_stream_collection(message, &m_streamCollection.outPtr());

    auto callback = [weakThis = ThreadSafeWeakPtr { *this }, owner = GRefPtr<GstObject>(GST_MESSAGE_SRC(message))] {
        RefPtr player = weakThis.get();
        if (player)
            player->updateTracks(owner);
    };

    GST_DEBUG_OBJECT(pipeline(), "Updating tracks");
    callOnMainThreadAndWait(WTFMove(callback));
    GST_DEBUG_OBJECT(pipeline(), "Updating tracks DONE");
}

bool MediaPlayerPrivateGStreamer::handleNeedContextMessage(GstMessage* message)
{
    ASSERT(GST_MESSAGE_TYPE(message) == GST_MESSAGE_NEED_CONTEXT);

    const gchar* contextType;
    if (!gst_message_parse_context_type(message, &contextType))
        return false;

    GST_DEBUG_OBJECT(pipeline(), "Handling %s need-context message for %s", contextType, GST_MESSAGE_SRC_NAME(message));

    if (!g_strcmp0(contextType, WEBKIT_WEB_SRC_RESOURCE_LOADER_CONTEXT_TYPE_NAME)) {
        auto context = adoptGRef(gst_context_new(WEBKIT_WEB_SRC_RESOURCE_LOADER_CONTEXT_TYPE_NAME, FALSE));
        GstStructure* contextStructure = gst_context_writable_structure(context.get());

        gst_structure_set(contextStructure, "loader", G_TYPE_POINTER, m_loader.get(), nullptr);
        gst_element_set_context(GST_ELEMENT(GST_MESSAGE_SRC(message)), context.get());
        return true;
    }

#if ENABLE(ENCRYPTED_MEDIA)
    if (!g_strcmp0(contextType, "drm-preferred-decryption-system-id")) {
        initializationDataEncountered(parseInitDataFromProtectionMessage(message));
        bool isCDMAttached = waitForCDMAttachment();
        if (isCDMAttached && !isPlayerShuttingDown() && !m_cdmInstance->keySystem().isEmpty()) {
            const char* preferredKeySystemUuid = GStreamerEMEUtilities::keySystemToUuid(m_cdmInstance->keySystem());
            GST_INFO_OBJECT(pipeline(), "working with key system %s, continuing with key system %s on %s", m_cdmInstance->keySystem().utf8().data(), preferredKeySystemUuid, GST_MESSAGE_SRC_NAME(message));

            GRefPtr<GstContext> context = adoptGRef(gst_context_new("drm-preferred-decryption-system-id", FALSE));
            GstStructure* contextStructure = gst_context_writable_structure(context.get());
            gst_structure_set(contextStructure, "decryption-system-id", G_TYPE_STRING, preferredKeySystemUuid, nullptr);
            gst_element_set_context(GST_ELEMENT(GST_MESSAGE_SRC(message)), context.get());
            return true;
        }

        GST_WARNING_OBJECT(pipeline(), "waiting for a CDM failed, no CDM available");
        return false;
    }
#endif // ENABLE(ENCRYPTED_MEDIA)

    GST_DEBUG_OBJECT(pipeline(), "Unhandled %s need-context message for %s", contextType, GST_MESSAGE_SRC_NAME(message));
    return false;
}

// Returns the size of the video.
FloatSize MediaPlayerPrivateGStreamer::naturalSize() const
{
    if (!hasVideo())
        return FloatSize();

    if (!m_videoSize.isEmpty())
        return m_videoSize;

    // When using the holepunch we may not be able to get the video frames size, so we can't use
    // it. But we need to report some non empty naturalSize for the player's GraphicsLayer
    // to be properly created.
    if (isHolePunchRenderingEnabled())
        return s_holePunchDefaultFrameSize;

    return m_videoSize;
}

void MediaPlayerPrivateGStreamer::configureMediaStreamAudioTracks()
{
#if ENABLE(MEDIA_STREAM)
    if (WEBKIT_IS_MEDIA_STREAM_SRC(m_source.get()))
        webkitMediaStreamSrcConfigureAudioTracks(WEBKIT_MEDIA_STREAM_SRC(m_source.get()), volume(), isMuted(), !m_isPaused);
#endif
}

void MediaPlayerPrivateGStreamer::setVolume(float volume)
{
    if (!m_volumeElement)
        return;

    GST_DEBUG_OBJECT(pipeline(), "Setting volume: %f", volume);
    gst_stream_volume_set_volume(m_volumeElement.get(), GST_STREAM_VOLUME_FORMAT_LINEAR, static_cast<double>(volume));
    configureMediaStreamAudioTracks();
}

float MediaPlayerPrivateGStreamer::volume() const
{
    if (!m_volumeElement)
        return 0;

    auto volume = gst_stream_volume_get_volume(m_volumeElement.get(), GST_STREAM_VOLUME_FORMAT_LINEAR);
    GST_DEBUG_OBJECT(pipeline(), "Volume: %f", volume);
    return volume;
}

void MediaPlayerPrivateGStreamer::notifyPlayerOfVolumeChange()
{
    RefPtr player = m_player.get();
    if (!player || !m_volumeElement)
        return;

    // get_volume() can return values superior to 1.0 if the user applies software user gain via
    // third party application (GNOME volume control for instance).
    auto oldVolume = this->volume();
    auto volume = CLAMP(oldVolume, 0.0, 1.0);

    if (volume != oldVolume)
        GST_DEBUG_OBJECT(pipeline(), "Volume value (%f) was not in [0,1] range. Clamped to %f", oldVolume, volume);
    player->volumeChanged(volume);
}

void MediaPlayerPrivateGStreamer::volumeChangedCallback(MediaPlayerPrivateGStreamer* player)
{
    if (player->isPlayerShuttingDown())
        return;

    // This is called when m_volumeElement receives the notify::volume signal.
    GST_DEBUG_OBJECT(player->pipeline(), "Volume changed to: %f", player->volume());

    player->m_notifier->notify(MainThreadNotification::VolumeChanged, [player] {
        player->notifyPlayerOfVolumeChange();
    });
}

MediaPlayer::NetworkState MediaPlayerPrivateGStreamer::networkState() const
{
    return m_networkState;
}

MediaPlayer::ReadyState MediaPlayerPrivateGStreamer::readyState() const
{
    return m_readyState;
}

void MediaPlayerPrivateGStreamer::setMuted(bool shouldMute)
{
    GST_DEBUG_OBJECT(pipeline(), "Attempting to set muted state to %s", boolForPrinting(shouldMute));

    if (!m_volumeElement || shouldMute == isMuted())
        return;

    GST_INFO_OBJECT(pipeline(), "Setting muted state to %s", boolForPrinting(shouldMute));
    g_object_set(m_volumeElement.get(), "mute", static_cast<gboolean>(shouldMute), nullptr);
    configureMediaStreamAudioTracks();
}

void MediaPlayerPrivateGStreamer::notifyPlayerOfMute()
{
    RefPtr player = m_player.get();
    if (!player || !m_volumeElement)
        return;

    gboolean value;
    bool isMuted;
    g_object_get(m_volumeElement.get(), "mute", &value, nullptr);
    isMuted = value;
    if (isMuted == m_isMuted)
        return;

    m_isMuted = isMuted;
    GST_DEBUG_OBJECT(pipeline(), "Notifying player of new mute value: %s", boolForPrinting(isMuted));
    player->muteChanged(m_isMuted);
}

void MediaPlayerPrivateGStreamer::muteChangedCallback(MediaPlayerPrivateGStreamer* player)
{
    // This is called when m_volumeElement receives the notify::mute signal.
    player->m_notifier->notify(MainThreadNotification::MuteChanged, [player] {
        player->notifyPlayerOfMute();
    });
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

    RefPtr player = m_player.get();

    // We ignore state changes from internal elements. They are forwarded to playbin2 anyway.
    bool messageSourceIsPlaybin = GST_MESSAGE_SRC(message) == reinterpret_cast<GstObject*>(m_pipeline.get());

    GST_LOG_OBJECT(pipeline(), "Message %s received from element %s", GST_MESSAGE_TYPE_NAME(message), GST_MESSAGE_SRC_NAME(message));
    switch (GST_MESSAGE_TYPE(message)) {
    case GST_MESSAGE_ERROR:
        gst_message_parse_error(message, &err.outPtr(), &debug.outPtr());
        GST_ERROR_OBJECT(pipeline(), "%s (url=%s) (code=%d)", err->message, m_url.string().utf8().data(), err->code);

        if (m_shouldResetPipeline || m_didErrorOccur)
            break;

        m_errorMessage = String::fromLatin1(err->message);

        error = MediaPlayer::NetworkState::Empty;
        if (g_error_matches(err.get(), GST_STREAM_ERROR, GST_STREAM_ERROR_CODEC_NOT_FOUND)
            || g_error_matches(err.get(), GST_STREAM_ERROR, GST_STREAM_ERROR_DECRYPT)
            || g_error_matches(err.get(), GST_STREAM_ERROR, GST_STREAM_ERROR_DECRYPT_NOKEY)
            || g_error_matches(err.get(), GST_STREAM_ERROR, GST_STREAM_ERROR_WRONG_TYPE)
            || g_error_matches(err.get(), GST_STREAM_ERROR, GST_STREAM_ERROR_FAILED)
            || g_error_matches(err.get(), GST_CORE_ERROR, GST_CORE_ERROR_MISSING_PLUGIN)
            || g_error_matches(err.get(), GST_CORE_ERROR, GST_CORE_ERROR_PAD)
            || g_error_matches(err.get(), GST_RESOURCE_ERROR, GST_RESOURCE_ERROR_NOT_FOUND))
            error = MediaPlayer::NetworkState::FormatError;
        else if (g_error_matches(err.get(), GST_STREAM_ERROR, GST_STREAM_ERROR_TYPE_NOT_FOUND)) {
            GST_ERROR_OBJECT(pipeline(), "Decode error, let the Media element emit a stalled event.");
            m_loadingStalled = true;
            error = MediaPlayer::NetworkState::DecodeError;
            attemptNextLocation = true;
        } else if (err->domain == GST_STREAM_ERROR
            || g_error_matches(err.get(), GST_STREAM_ERROR, GST_STREAM_ERROR_DECODE)) {
            error = MediaPlayer::NetworkState::DecodeError;
            attemptNextLocation = true;
        } else if (err->domain == GST_RESOURCE_ERROR)
            error = MediaPlayer::NetworkState::NetworkError;

        if (attemptNextLocation)
            issueError = !loadNextLocation();
        if (issueError) {
            m_didErrorOccur = true;
            if (m_networkState != error) {
                m_networkState = error;
                if (player)
                    player->networkStateChanged();
            }
        }
        break;
    case GST_MESSAGE_WARNING:
        gst_message_parse_warning(message, &err.outPtr(), &debug.outPtr());
        GST_WARNING_OBJECT(pipeline(), "%s (url=%s) (code=%d)", err->message, m_url.string().utf8().data(), err->code);
        break;
    case GST_MESSAGE_EOS: {
        // In some specific cases, an EOS GstEvent can happen right before a seek. The event is translated
        // by playbin as an EOS GstMessage and posted to the bus, waiting to be forwarded to the main thread.
        // The EOS message (now irrelevant after the seek) is received and processed right after the seek,
        // causing the termination of the media at the player private and upper levels. This can even happen
        // after the seek has completed (m_isSeeking already false).
        // The code below detects that condition by ensuring that the playback is coherent with the EOS message,
        // that is, if we're still playing somewhere inside the playable ranges, there should be no EOS at
        // all. If that's the case, it's considered to be one of those spureous EOS and is ignored.
        // Live streams (infinite duration) are special and we still have to detect legitimate EOS there, so
        // this message bailout isn't done in those cases.
        MediaTime playbackPosition = MediaTime::invalidTime();
        MediaTime duration = this->duration();
        GstClockTime gstreamerPosition = gstreamerPositionFromSinks();
        bool eosFlagIsSetInSink = false;
        if (player && player->isVideoPlayer()) {
            GRefPtr<GstPad> sinkPad = adoptGRef(gst_element_get_static_pad(m_videoSink.get(), "sink"));
            eosFlagIsSetInSink = sinkPad && GST_PAD_IS_EOS(sinkPad.get());
        }

        if (!eosFlagIsSetInSink && m_audioSink) {
            GRefPtr<GstPad> sinkPad = adoptGRef(gst_element_get_static_pad(m_audioSink.get(), "sink"));
            eosFlagIsSetInSink = sinkPad && GST_PAD_IS_EOS(sinkPad.get());
        }

        if (GST_CLOCK_TIME_IS_VALID(gstreamerPosition))
            playbackPosition = MediaTime(gstreamerPosition, GST_SECOND);
        if (!player->isLooping() && !eosFlagIsSetInSink && playbackPosition.isValid() && duration.isValid()
            && ((m_playbackRate >= 0 && playbackPosition < duration && duration.isFinite())
            || (m_playbackRate < 0 && playbackPosition > MediaTime::zeroTime()))) {
            GST_DEBUG_OBJECT(pipeline(), "EOS received but position %s is still in the finite playable limits [%s, %s], ignoring it",
                playbackPosition.toString().utf8().data(), MediaTime::zeroTime().toString().utf8().data(), duration.toString().utf8().data());
            break;
        }
        didEnd();
        break;
    }
    case GST_MESSAGE_ASYNC_DONE:
        if (!messageSourceIsPlaybin || m_isDelayingLoad)
            break;

        // The MediaPlayerPrivateGStreamer superclass now processes what it needs by calling updateStates() in handleMessage() for
        // GST_MESSAGE_STATE_CHANGED. However, subclasses still need to override asyncStateChangeDone() to do their own stuff.
        didPreroll();
        break;
    case GST_MESSAGE_STATE_CHANGED: {
        GstState newState;
        gst_message_parse_state_changed(message, &currentState, &newState, nullptr);

        if (isHolePunchRenderingEnabled() && currentState <= GST_STATE_READY && newState >= GST_STATE_READY) {
            // If we didn't create a video sink, store a reference to the created one.
            if (!m_videoSink) {
                // Detect the videoSink element. Getting the video-sink property of the pipeline requires
                // locking some elements, which may lead to deadlocks during playback. Instead, identify
                // the videoSink based on its metadata.
                GstElement* element = GST_ELEMENT(GST_MESSAGE_SRC(message));
                if (GST_OBJECT_FLAG_IS_SET(element, GST_ELEMENT_FLAG_SINK)) {
                    const gchar* klassStr = gst_element_get_metadata(element, "klass");
                    if (strstr(klassStr, "Sink") && strstr(klassStr, "Video")) {
                        m_videoSink = element;

                        // Ensure that there's a buffer with the transparent rectangle available when playback is going to start.
                        pushNextHolePunchBuffer();
                    }
                }
            }
        }

        auto& quirksManager = GStreamerQuirksManager::singleton();
        if (quirksManager.isEnabled() && currentState <= GST_STATE_READY && newState >= GST_STATE_READY) {
            // Detect an audio sink element and store reference to it if it supersedes what we currently have.
            GstElement* element = GST_ELEMENT(GST_MESSAGE_SRC(message));
            if (GST_OBJECT_FLAG_IS_SET(element, GST_ELEMENT_FLAG_SINK)) {
                const gchar* klassStr = gst_element_get_metadata(element, "klass");
                if (g_strrstr(klassStr, "Sink") && g_strrstr(klassStr, "Audio")
                    && (!m_audioSink || (m_audioSink.get() != element && GST_STATE(m_audioSink.get()) == GST_STATE_NULL)))
                    m_audioSink = element;
            }
        }

        if (!messageSourceIsPlaybin || m_isDelayingLoad)
            break;

        GST_DEBUG_OBJECT(pipeline(), "Changed state from %s to %s", gst_element_state_get_name(currentState), gst_element_state_get_name(newState));

        if (!m_isLegacyPlaybin && currentState == GST_STATE_PAUSED && newState == GST_STATE_PLAYING)
            playbin3SendSelectStreamsIfAppropriate();
        updateStates();
        checkPlayingConsistency();

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
            if (changePipelineState(requestedState) == ChangePipelineStateResult::Failed)
                loadingFailed(MediaPlayer::NetworkState::Empty);
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
#if USE(GSTREAMER_MPEGTS)
        if (GstMpegtsSection* section = gst_message_parse_mpegts_section(message)) {
            processMpegTsSection(section);
            gst_mpegts_section_unref(section);
        } else
#endif
        if (gst_structure_has_name(structure, "http-headers")) {
            GST_DEBUG_OBJECT(pipeline(), "Processing HTTP headers: %" GST_PTR_FORMAT, structure);
            if (const char* uri = gst_structure_get_string(structure, "uri")) {
                URL url { String::fromLatin1(uri) };

                if (url != m_url) {
                    GST_DEBUG_OBJECT(pipeline(), "Ignoring HTTP response headers for non-main URI.");
                    break;
                }
            }

            bool isRangeRequest = false;
            GUniqueOutPtr<GstStructure> requestHeaders;
            if (gst_structure_get(structure, "request-headers", GST_TYPE_STRUCTURE, &requestHeaders.outPtr(), nullptr))
                isRangeRequest = gst_structure_has_field(requestHeaders.get(), "Range");

            GST_DEBUG_OBJECT(pipeline(), "Is range request: %s", boolForPrinting(isRangeRequest));

            GUniqueOutPtr<GstStructure> responseHeaders;
            if (gst_structure_get(structure, "response-headers", GST_TYPE_STRUCTURE, &responseHeaders.outPtr(), nullptr)) {
                auto contentLengthHeaderName = httpHeaderNameString(HTTPHeaderName::ContentLength);
                uint64_t contentLength = 0;
                if (!gst_structure_get_uint64(responseHeaders.get(), contentLengthHeaderName.characters(), &contentLength)) {
                    // souphttpsrc sets a string for Content-Length, so
                    // handle it here, until we remove the webkit+ protocol
                    // prefix from webkitwebsrc.
                    if (const char* contentLengthAsString = gst_structure_get_string(responseHeaders.get(), contentLengthHeaderName.characters())) {
                        contentLength = g_ascii_strtoull(contentLengthAsString, nullptr, 10);
                        if (contentLength == G_MAXUINT64)
                            contentLength = 0;
                    }
                }
                if (!isRangeRequest) {
                    m_isLiveStream = !contentLength;
                    GST_INFO_OBJECT(pipeline(), "%s stream detected", m_isLiveStream.value_or(false) ? "Live" : "Non-live");
                    updateDownloadBufferingFlag();
                }
            }
        } else if (gst_structure_has_name(structure, "webkit-network-statistics")) {
            if (gst_structure_get(structure, "read-position", G_TYPE_UINT64, &m_networkReadPosition, "size", G_TYPE_UINT64, &m_httpResponseTotalSize, nullptr)) {
                GST_LOG_OBJECT(pipeline(), "Updated network read position %" G_GUINT64_FORMAT ", size: %" G_GUINT64_FORMAT, m_networkReadPosition, m_httpResponseTotalSize);

                MediaTime mediaDuration = duration();

                // Update maxTimeLoaded only if the media duration is available. Otherwise we can't compute it.
                if (mediaDuration && m_httpResponseTotalSize) {
                    const double fillStatus = 100.0 * (static_cast<double>(m_networkReadPosition) / static_cast<double>(m_httpResponseTotalSize));
                    updateMaxTimeLoaded(fillStatus);
                    GST_DEBUG("Updated maxTimeLoaded base on network read position: %s", m_maxTimeLoaded.toString().utf8().data());
                }
            }
        } else if (gst_structure_has_name(structure, "GstCacheDownloadComplete")) {
            GST_INFO_OBJECT(pipeline(), "Stream is fully downloaded, stopping monitoring downloading progress.");
            m_fillTimer.stop();
            m_bufferingPercentage = 100;
            updateStates();
        } else if (gst_structure_has_name(structure, "webkit-web-src-has-eos")) {
            GST_DEBUG_OBJECT(pipeline(), "WebKitWebSrc has EOS");
            m_hasWebKitWebSrcSentEOS = true;
        } else
            GST_DEBUG_OBJECT(pipeline(), "Unhandled element message: %" GST_PTR_FORMAT, structure);
        break;
    case GST_MESSAGE_TOC:
        processTableOfContents(message);
        break;
    case GST_MESSAGE_STREAMS_SELECTED: {
        if (m_isLegacyPlaybin)
            break;

#ifndef GST_DISABLE_DEBUG
        GST_DEBUG_OBJECT(m_pipeline.get(), "Received STREAMS_SELECTED message selecting the following streams:");
        unsigned numStreams = gst_message_streams_selected_get_size(message);
        for (unsigned i = 0; i < numStreams; i++) {
            auto stream = adoptGRef(gst_message_streams_selected_get_stream(message, i));
            GST_DEBUG_OBJECT(pipeline(), "#%u %s %s", i, gst_stream_type_get_name(gst_stream_get_stream_type(stream.get())), gst_stream_get_stream_id(stream.get()));
        }
#endif
        GST_DEBUG_OBJECT(m_pipeline.get(), "Setting m_waitingForStreamsSelectedEvent to false.");
        m_waitingForStreamsSelectedEvent = false;

        // Unfortunately, STREAMS_SELECTED messages from playbin3 are highly unreliable, often only including the audio
        // stream or only the video stream when both are present and going to be played.
        // Therefore, instead of reading the event data, we will just assume our previously requested selection was honored.
        m_currentAudioStreamId = m_requestedAudioStreamId;
        m_currentVideoStreamId = m_requestedVideoStreamId;
        m_currentTextStreamId = m_requestedTextStreamId;

        // It's possible the user made a track switch before the initial STREAMS_SELECED. Now it's a good moment to
        // request it being attended. Note that it's not possible to send a SELECT_STREAMS before the first
        // STREAMS_SELECTED message because at that point the pipeline is not compeletely constructed.
        playbin3SendSelectStreamsIfAppropriate();
        break;
    }
    case GST_MESSAGE_STREAM_START: {
        // Real track id configuration in MSE is managed by AppendPipeline. In MediaStream we don't support native stream ids.
        if (!m_isLegacyPlaybin)
            break;

        notifyPlayerOfTrack<VideoTrackPrivateGStreamer>();
        notifyPlayerOfTrack<AudioTrackPrivateGStreamer>();
        notifyPlayerOfTrack<InbandTextTrackPrivateGStreamer>();
        break;
    }
    default:
        GST_DEBUG_OBJECT(pipeline(), "Unhandled GStreamer message type: %s", GST_MESSAGE_TYPE_NAME(message));
        break;
    }
}

void MediaPlayerPrivateGStreamer::processBufferingStats(GstMessage* message)
{
    GstBufferingMode mode;
    gst_message_parse_buffering_stats(message, &mode, nullptr, nullptr, nullptr);

    int percentage;
    gst_message_parse_buffering(message, &percentage);

    updateBufferingStatus(mode, percentage);
}

void MediaPlayerPrivateGStreamer::updateMaxTimeLoaded(double percentage)
{
    MediaTime mediaDuration = duration();
    if (!mediaDuration)
        return;

    m_maxTimeLoaded = MediaTime(percentage * static_cast<double>(toGstUnsigned64Time(mediaDuration)) / 100, GST_SECOND);
    GST_DEBUG_OBJECT(pipeline(), "[Buffering] Updated maxTimeLoaded: %s", toString(m_maxTimeLoaded).utf8().data());
}

void MediaPlayerPrivateGStreamer::updateBufferingStatus(GstBufferingMode mode, double percentage)
{
    bool wasBuffering = m_isBuffering;

#ifndef GST_DISABLE_GST_DEBUG
    GUniquePtr<char> modeString(g_enum_to_string(GST_TYPE_BUFFERING_MODE, mode));
    GST_DEBUG_OBJECT(pipeline(), "[Buffering] mode: %s, status: %f%%", modeString.get(), percentage);
#endif

    m_didDownloadFinish = percentage == 100;

    if (!m_didDownloadFinish)
        m_isBuffering = true;
    else
        m_fillTimer.stop();

    m_bufferingPercentage = percentage;
    switch (mode) {
    case GST_BUFFERING_STREAM: {
        updateMaxTimeLoaded(percentage);

        m_bufferingPercentage = percentage;
        if (m_didDownloadFinish || !wasBuffering)
            updateStates();

        break;
    }
    case GST_BUFFERING_DOWNLOAD: {
        updateMaxTimeLoaded(percentage);
        updateStates();
        break;
    }
    default:
#ifndef GST_DISABLE_GST_DEBUG
        GST_DEBUG_OBJECT(pipeline(), "Unhandled buffering mode: %s", modeString.get());
#endif
        break;
    }
}

#if USE(GSTREAMER_MPEGTS)
void MediaPlayerPrivateGStreamer::processMpegTsSection(GstMpegtsSection* section)
{
    ASSERT(section);

    if (section->section_type == GST_MPEGTS_SECTION_PMT) {
        const GstMpegtsPMT* pmt = gst_mpegts_section_get_pmt(section);
        m_metadataTracks.clear();
        for (unsigned i = 0; i < pmt->streams->len; ++i) {
            const GstMpegtsPMTStream* stream = static_cast<const GstMpegtsPMTStream*>(g_ptr_array_index(pmt->streams, i));
            if (stream->stream_type == 0x05 || stream->stream_type >= 0x80) {
                AtomString pid = AtomString::number(stream->pid);
                RefPtr<InbandMetadataTextTrackPrivateGStreamer> track = InbandMetadataTextTrackPrivateGStreamer::create(
                    InbandTextTrackPrivate::Kind::Metadata, InbandTextTrackPrivate::CueFormat::Data, pid);

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
                StringBuilder inbandMetadataTrackDispatchType;
                inbandMetadataTrackDispatchType.append(hex(stream->stream_type, 2));
                for (unsigned j = 0; j < stream->descriptors->len; ++j) {
                    const GstMpegtsDescriptor* descriptor = static_cast<const GstMpegtsDescriptor*>(g_ptr_array_index(stream->descriptors, j));
                    for (unsigned k = 0; k < descriptor->length; ++k)
                        inbandMetadataTrackDispatchType.append(hex(descriptor->data[k], 2));
                }
                track->setInBandMetadataTrackDispatchType(inbandMetadataTrackDispatchType.toAtomString());

                m_metadataTracks.add(pid, track);
                if (RefPtr player = m_player.get())
                    player->addTextTrack(*track);
            }
        }
    } else {
        AtomString pid = AtomString::number(section->pid);
        RefPtr<InbandMetadataTextTrackPrivateGStreamer> track = m_metadataTracks.get(pid);
        if (!track)
            return;

        GRefPtr<GBytes> data = gst_mpegts_section_get_data(section);
        gsize size;
        const void* bytes = g_bytes_get_data(data.get(), &size);

        track->addDataCue(currentTime(), currentTime(), bytes, size);
    }
}
#endif

void MediaPlayerPrivateGStreamer::processTableOfContents(GstMessage* message)
{
    RefPtr player = m_player.get();

    if (player && m_chaptersTrack)
        player->removeTextTrack(*m_chaptersTrack);

    m_chaptersTrack = InbandMetadataTextTrackPrivateGStreamer::create(InbandTextTrackPrivate::Kind::Chapters, InbandTextTrackPrivate::CueFormat::Generic);
    if (player)
        player->addTextTrack(*m_chaptersTrack);

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

    auto cue = InbandGenericCue::create();

    gint64 start = -1, stop = -1;
    gst_toc_entry_get_start_stop_times(entry, &start, &stop);

    uint32_t truncatedGstSecond = static_cast<uint32_t>(GST_SECOND);
    if (start != -1)
        cue->setStartTime(MediaTime(static_cast<int64_t>(start), truncatedGstSecond));
    if (stop != -1)
        cue->setEndTime(MediaTime(static_cast<int64_t>(stop), truncatedGstSecond));

    GstTagList* tags = gst_toc_entry_get_tags(entry);
    if (tags) {
        gchar* title = nullptr;
        gst_tag_list_get_string(tags, GST_TAG_TITLE, &title);
        if (title) {
            cue->setContent(String::fromUTF8(title));
            g_free(title);
        }
    }

    m_chaptersTrack->addGenericCue(cue);

    for (GList* i = gst_toc_entry_get_sub_entries(entry); i; i = i->next)
        processTableOfContentsEntry(static_cast<GstTocEntry*>(i->data));
}

void MediaPlayerPrivateGStreamer::configureElement(GstElement* element)
{
    configureElementPlatformQuirks(element);

    GUniquePtr<char> elementName(gst_element_get_name(element));
    auto elementClass = makeString(gst_element_get_metadata(element, GST_ELEMENT_METADATA_KLASS));
    auto classifiers = elementClass.split('/');

    // In GStreamer 1.20 and older urisourcebin mishandles source elements with dynamic pads. This
    // is not an issue in 1.22. Streams parsing is not needed for MediaStream cases because we do it
    // upfront for incoming WebRTC MediaStreams. It is however needed for MSE, otherwise decodebin3
    // might not auto-plug hardware decoders.
    if (webkitGstCheckVersion(1, 22, 0) && g_str_has_prefix(elementName.get(), "urisourcebin") && (isMediaSource() || isMediaStreamPlayer()))
        g_object_set(element, "use-buffering", FALSE, "parse-streams", !isMediaStreamPlayer(), nullptr);

    // In case of playbin3 with <video ... preload="auto">, instantiate
    // downloadbuffer element, otherwise the playbin3 would instantiate
    // a queue element instead .
    if (g_str_has_prefix(elementName.get(), "urisourcebin") && !m_isLegacyPlaybin && !isMediaSource() && !isMediaStreamPlayer() && m_preload == MediaPlayer::Preload::Auto)
        g_object_set(element, "download", TRUE, nullptr);

    // Collect processing time metrics for video decoders and converters.
    if ((classifiers.contains("Converter"_s) || classifiers.contains("Decoder"_s)) && classifiers.contains("Video"_s) && !classifiers.contains("Parser"_s) && !classifiers.contains("Sink"_s))
        webkitGstTraceProcessingTimeForElement(element);

    // This will set the multiqueue size to the default value.
    if (g_str_has_prefix(elementName.get(), "uridecodebin"))
        g_object_set(element, "buffer-size", 2 * MB, nullptr);

    if (classifiers.contains("Decoder"_s)) {
        if (classifiers.contains("Video"_s))
            configureVideoDecoder(element);
        else if (classifiers.contains("Audio"_s))
            configureAudioDecoder(element);
        return;
    }

    if (isMediaStreamPlayer())
        return;

    if (g_str_has_prefix(elementName.get(), "downloadbuffer")) {
        configureDownloadBuffer(element);
        return;
    }

    if (!g_strcmp0(G_OBJECT_TYPE_NAME(G_OBJECT(element)), "GstQueue2"))
        g_object_set(G_OBJECT(element), "high-watermark", 0.10, nullptr);
}

void MediaPlayerPrivateGStreamer::configureElementPlatformQuirks(GstElement* element)
{
    GST_DEBUG_OBJECT(pipeline(), "Element set-up for %s", GST_ELEMENT_NAME(element));

    OptionSet<ElementRuntimeCharacteristics> characteristics;
    if (isMediaStreamPlayer())
        characteristics.add({ ElementRuntimeCharacteristics::IsMediaStream });
    if (hasVideo())
        characteristics.add({ ElementRuntimeCharacteristics::HasVideo });
    if (hasAudio())
        characteristics.add({ ElementRuntimeCharacteristics::HasAudio });
    if (m_isLiveStream.value_or(false))
        characteristics.add({ ElementRuntimeCharacteristics::IsLiveStream });

    GStreamerQuirksManager::singleton().configureElement(element, WTFMove(characteristics));
}

void MediaPlayerPrivateGStreamer::configureDownloadBuffer(GstElement* element)
{
    GUniquePtr<char> elementName(gst_element_get_name(element));
    RELEASE_ASSERT(g_str_has_prefix(elementName.get(), "downloadbuffer"));

    m_downloadBuffer = element;
    g_signal_connect_swapped(element, "notify::temp-location", G_CALLBACK(downloadBufferFileCreatedCallback), this);

    // Set the GstDownloadBuffer size to our preferred value controls the thresholds for buffering events.
    auto cacheSizeBytesStr = StringView::fromLatin1(std::getenv("WPE_SHELL_MEDIA_DISK_CACHE_SIZE_BYTES"));
    auto cacheSizeBytes = parseInteger<unsigned long>(cacheSizeBytesStr).value_or(100 * KB);
    auto cacheSizeNSecStr = StringView::fromLatin1(std::getenv("WPE_SHELL_MEDIA_DISK_CACHE_SIZE_NSEC"));
    auto cacheSizeNSec = parseInteger<unsigned long long>(cacheSizeNSecStr).value_or(5000000000);
    g_object_set(element, "max-size-bytes", cacheSizeBytes, "max-size-time", cacheSizeNSec, nullptr);

    GUniqueOutPtr<char> oldDownloadTemplate;
    g_object_get(element, "temp-template", &oldDownloadTemplate.outPtr(), nullptr);

#if PLATFORM(WPE)
    GUniquePtr<char> mediaDiskCachePath(g_strdup(std::getenv("WPE_SHELL_MEDIA_DISK_CACHE_PATH")));
    if (!mediaDiskCachePath || !*mediaDiskCachePath) {
        GUniquePtr<char> defaultValue(g_build_filename(G_DIR_SEPARATOR_S, "var", "tmp", nullptr));
        mediaDiskCachePath.swap(defaultValue);
    }
#else
    GUniquePtr<char> mediaDiskCachePath(g_build_filename(G_DIR_SEPARATOR_S, "var", "tmp", nullptr));
#endif

    GUniquePtr<char> newDownloadTemplate(g_build_filename(G_DIR_SEPARATOR_S, mediaDiskCachePath.get(), "WebKit-Media-XXXXXX", nullptr));
    g_object_set(element, "temp-template", newDownloadTemplate.get(), nullptr);
    GST_DEBUG_OBJECT(pipeline(), "Reconfigured file download template from '%s' to '%s'", oldDownloadTemplate.get(), newDownloadTemplate.get());

    auto newDownloadPrefixPath = makeStringByReplacingAll(String::fromLatin1(newDownloadTemplate.get()), "XXXXXX"_s, ""_s);
    purgeOldDownloadFiles(newDownloadPrefixPath);
}

void MediaPlayerPrivateGStreamer::downloadBufferFileCreatedCallback(MediaPlayerPrivateGStreamer* player)
{
    ASSERT(player->m_downloadBuffer);

    g_signal_handlers_disconnect_by_func(player->m_downloadBuffer.get(), reinterpret_cast<gpointer>(downloadBufferFileCreatedCallback), player);

    GUniqueOutPtr<char> downloadFile;
    g_object_get(player->m_downloadBuffer.get(), "temp-location", &downloadFile.outPtr(), nullptr);

    if (UNLIKELY(!FileSystem::deleteFile(String::fromUTF8(downloadFile.get())))) {
        GST_WARNING("Couldn't unlink media temporary file %s after creation", downloadFile.get());
        return;
    }

    GST_DEBUG_OBJECT(player->pipeline(), "Unlinked media temporary file %s after creation", downloadFile.get());
}

void MediaPlayerPrivateGStreamer::purgeOldDownloadFiles(const String& downloadFilePrefixPath)
{
    if (downloadFilePrefixPath.isEmpty())
        return;

    auto templateDirectory = FileSystem::parentPath(downloadFilePrefixPath);
    auto templatePrefix = FileSystem::pathFileName(downloadFilePrefixPath);
    for (auto& fileName : FileSystem::listDirectory(templateDirectory)) {
        if (!fileName.startsWith(templatePrefix))
            continue;

        auto filePath = FileSystem::pathByAppendingComponent(templateDirectory, fileName);
        if (UNLIKELY(!FileSystem::deleteFile(filePath))) {
            GST_WARNING("Couldn't unlink legacy media temporary file: %s", filePath.utf8().data());
            continue;
        }

        GST_TRACE("Unlinked legacy media temporary file: %s", filePath.utf8().data());
    }
}

void MediaPlayerPrivateGStreamer::finishSeek()
{
    GST_DEBUG_OBJECT(pipeline(), "[Seek] seeked to %s", toString(m_seekTarget.time).utf8().data());
    m_isSeeking = false;
    invalidateCachedPosition();
    if (m_timeOfOverlappingSeek != m_seekTarget.time && m_timeOfOverlappingSeek.isValid()) {
        seekToTarget(SeekTarget { m_timeOfOverlappingSeek });
        m_timeOfOverlappingSeek = MediaTime::invalidTime();
        return;
    }
    m_timeOfOverlappingSeek = MediaTime::invalidTime();

    // The pipeline can still have a pending state. In this case a position query will fail.
    // Right now we can use m_seekTarget as a fallback.
    m_canFallBackToLastFinishedSeekPosition = true;
    timeChanged(m_seekTarget.time);
}

void MediaPlayerPrivateGStreamer::updateStates()
{
    if (!m_pipeline || m_didErrorOccur)
        return;

    MediaPlayer::NetworkState oldNetworkState = m_networkState;
    MediaPlayer::ReadyState oldReadyState = m_readyState;
    bool oldIsPaused = m_isPaused;
    GstState pending, state;
    bool stateReallyChanged = false;
    RefPtr player = m_player.get();

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

        m_shouldResetPipeline = m_currentState <= GST_STATE_READY;

        bool didBuffering = m_isBuffering;

        // Update ready and network states.
        switch (m_currentState) {
        case GST_STATE_NULL:
            m_readyState = MediaPlayer::ReadyState::HaveNothing;
            m_networkState = MediaPlayer::NetworkState::Empty;
            break;
        case GST_STATE_READY:
            m_readyState = MediaPlayer::ReadyState::HaveMetadata;
            m_networkState = MediaPlayer::NetworkState::Empty;
            break;
        case GST_STATE_PAUSED:
            FALLTHROUGH;
        case GST_STATE_PLAYING: {
            bool isLooping = player && player->isLooping();
            if (m_isBuffering) {
                GRefPtr<GstQuery> query = adoptGRef(gst_query_new_buffering(GST_FORMAT_PERCENT));

                m_isBuffering = m_bufferingPercentage < 100;
                if ((m_audioSink && gst_element_query(m_audioSink.get(), query.get()))
                    || (m_videoSink && gst_element_query(m_videoSink.get(), query.get()))
                    || gst_element_query(m_pipeline.get(), query.get())) {
                    gboolean isBuffering = m_isBuffering;
                    gst_query_parse_buffering_percent(query.get(), &isBuffering, nullptr);
                    GST_TRACE_OBJECT(pipeline(), "[Buffering] m_isBuffering forcefully updated from %d to %d", m_isBuffering, isBuffering);
                    m_isBuffering = isBuffering;
                }

                if (!m_isBuffering) {
                    GST_INFO_OBJECT(pipeline(), "[Buffering] Complete.");
                    m_readyState = MediaPlayer::ReadyState::HaveEnoughData;
                    m_networkState = m_didDownloadFinish ? MediaPlayer::NetworkState::Idle : MediaPlayer::NetworkState::Loading;
                } else {
                    m_readyState = MediaPlayer::ReadyState::HaveCurrentData;
                    m_networkState = MediaPlayer::NetworkState::Loading;
                }
            } else if (m_didDownloadFinish || isLooping) {
                m_readyState = MediaPlayer::ReadyState::HaveEnoughData;
                m_networkState = MediaPlayer::NetworkState::Loaded;
            } else {
                m_readyState = MediaPlayer::ReadyState::HaveFutureData;
                m_networkState = MediaPlayer::NetworkState::Loading;
            }

            break;
        }
        default:
            ASSERT_NOT_REACHED();
            break;
        }

        bool shouldPauseForBuffering = false;
        // Sync states where needed.
        if (m_currentState == GST_STATE_PAUSED) {
            if (!m_areVolumeAndMuteInitialized) {
                notifyPlayerOfVolumeChange();
                notifyPlayerOfMute();
                m_areVolumeAndMuteInitialized = true;
            }

            if ((didBuffering && !m_isBuffering && !m_isPaused && m_playbackRate)
                || m_playbackRatePausedState == PlaybackRatePausedState::ShouldMoveToPlaying) {
                m_playbackRatePausedState = PlaybackRatePausedState::Playing;
                GST_INFO_OBJECT(pipeline(), "[Buffering] Restarting playback (because of buffering or resuming from zero playback rate)");
                changePipelineState(GST_STATE_PLAYING);
            }
        } else if (m_currentState == GST_STATE_PLAYING) {
            m_isPaused = false;

            shouldPauseForBuffering = (m_isBuffering && !m_isLiveStream.value_or(false));
            if (shouldPauseForBuffering || !m_playbackRate) {
                GST_INFO_OBJECT(pipeline(), "[Buffering] Pausing stream for buffering or because of zero playback rate.");
                changePipelineState(GST_STATE_PAUSED);
            }
        } else
            m_isPaused = true;

        GST_DEBUG_OBJECT(pipeline(), "Old state: %s, new state: %s (requested: %s)", gst_element_state_get_name(m_oldState), gst_element_state_get_name(m_currentState), gst_element_state_get_name(m_requestedState));
        if (m_requestedState == GST_STATE_PAUSED && m_currentState == GST_STATE_PAUSED) {
            shouldUpdatePlaybackState = true;
            GST_INFO_OBJECT(pipeline(), "Requested state change to %s was completed", gst_element_state_get_name(m_currentState));
        }

        // Emit play state change notification only when going to PLAYING so that
        // the media element gets a chance to enable its page sleep disabler.
        // Emitting this notification in more cases triggers unwanted code paths
        // and test timeouts.
        if (stateReallyChanged && (m_oldState != m_currentState) && (m_oldState == GST_STATE_PAUSED && m_currentState == GST_STATE_PLAYING) && !shouldPauseForBuffering
            && !m_isSeeking) {
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
        // Change failed.
        return;
    case GST_STATE_CHANGE_NO_PREROLL:
        GST_DEBUG_OBJECT(pipeline(), "No preroll: State: %s, pending: %s", gst_element_state_get_name(m_currentState), gst_element_state_get_name(pending));

        // Live pipelines go in PAUSED without prerolling.
        m_isLiveStream = true;
        updateDownloadBufferingFlag();

        if (m_currentState == GST_STATE_READY)
            m_readyState = MediaPlayer::ReadyState::HaveNothing;
        else if (m_currentState == GST_STATE_PAUSED)
            m_isPaused = true;
        else if (m_currentState == GST_STATE_PLAYING)
            m_isPaused = false;

        if (!m_isPaused && m_playbackRate)
            changePipelineState(GST_STATE_PLAYING);

        m_networkState = MediaPlayer::NetworkState::Loading;

        if (!isMediaStreamPlayer() && m_isLiveStream.value_or(false) && m_readyState < MediaPlayer::ReadyState::HaveEnoughData
            && m_currentState >= GST_STATE_PAUSED) {
            GST_DEBUG_OBJECT(pipeline(), "live stream reached %s", gst_element_state_get_name(m_currentState));
            m_readyState = MediaPlayer::ReadyState::HaveEnoughData;
            if (player)
                player->readyStateChanged();
        }
        break;
    default:
        GST_DEBUG_OBJECT(pipeline(), "Else : %d", getStateResult);
        break;
    }

    m_requestedState = GST_STATE_VOID_PENDING;

    if (player && shouldUpdatePlaybackState)
        player->playbackStateChanged();

    if (m_networkState != oldNetworkState) {
        GST_DEBUG_OBJECT(pipeline(), "Network State Changed from %s to %s", convertEnumerationToString(oldNetworkState).utf8().data(), convertEnumerationToString(m_networkState).utf8().data());
        if (player)
            player->networkStateChanged();
    }
    if (m_readyState != oldReadyState) {
        GST_DEBUG_OBJECT(pipeline(), "Ready State Changed from %s to %s", convertEnumerationToString(oldReadyState).utf8().data(), convertEnumerationToString(m_readyState).utf8().data());
        if (player)
            player->readyStateChanged();
    }

    if (getStateResult == GST_STATE_CHANGE_SUCCESS && m_currentState >= GST_STATE_PAUSED) {
        updatePlaybackRate();
        if (player && m_isSeekPending) {
            GST_DEBUG_OBJECT(pipeline(), "[Seek] committing pending seek to %s", toString(m_seekTarget.time).utf8().data());
            m_isSeekPending = false;
            m_isSeeking = doSeek(m_seekTarget, player->rate());
            if (!m_isSeeking) {
                invalidateCachedPosition();
                GST_DEBUG_OBJECT(pipeline(), "[Seek] seeking to %s failed", toString(m_seekTarget.time).utf8().data());
            }
        } else if (m_isSeeking && !(state == GST_STATE_PLAYING && pending == GST_STATE_PAUSED))
            finishSeek();
    }

    if (oldIsPaused != m_isPaused)
        configureMediaStreamAudioTracks();
}

void MediaPlayerPrivateGStreamer::mediaLocationChanged(GstMessage* message)
{
    const GstStructure* structure = gst_message_get_structure(message);
    if (!structure)
        return;

    // This structure can contain:
    // - both a new-location string and embedded locations structure
    // - or only a new-location string.
    m_mediaLocations.reset(gst_structure_copy(structure));
    const GValue* locations = gst_structure_get_value(m_mediaLocations.get(), "locations");

    if (locations)
        m_mediaLocationCurrentIndex = static_cast<int>(gst_value_list_get_size(locations)) - 1;

    loadNextLocation();
}

bool MediaPlayerPrivateGStreamer::loadNextLocation()
{
    if (!m_mediaLocations)
        return false;

    const GValue* locations = gst_structure_get_value(m_mediaLocations.get(), "locations");
    const char* newLocation = nullptr;

    if (!locations) {
        // Fallback on new-location string.
        newLocation = gst_structure_get_string(m_mediaLocations.get(), "new-location");
        if (!newLocation)
            return false;
    }

    if (!newLocation) {
        if (m_mediaLocationCurrentIndex < 0) {
            m_mediaLocations.reset();
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
        URL newUrl = URL(baseUrl, String::fromLatin1(newLocation));

        GUniqueOutPtr<gchar> playbinUrlStr;
        g_object_get(m_pipeline.get(), "current-uri", &playbinUrlStr.outPtr(), nullptr);
        URL playbinUrl { String::fromLatin1(playbinUrlStr.get()) };

        if (playbinUrl == newUrl) {
            GST_DEBUG_OBJECT(pipeline(), "Playbin already handled redirection.");

            m_url = playbinUrl;

            return true;
        }

        changePipelineState(GST_STATE_READY);
        auto securityOrigin = SecurityOrigin::create(m_url);
        if (securityOrigin->canRequest(newUrl, originAccessPatternsForWebProcessOrEmpty())) {
            GST_INFO_OBJECT(pipeline(), "New media url: %s", newUrl.string().utf8().data());

            RefPtr player = m_player.get();

            // Reset player states.
            m_networkState = MediaPlayer::NetworkState::Loading;
            m_readyState = MediaPlayer::ReadyState::HaveNothing;
            if (player) {
                player->networkStateChanged();
                player->readyStateChanged();
            }

            // Reset pipeline state.
            m_shouldResetPipeline = true;

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

bool MediaPlayerPrivateGStreamer::ended() const
{
#if ENABLE(MEDIA_STREAM)
    if (isMediaStreamPlayer())
        return !m_streamPrivate->active();
#endif
    return m_isEndReached;
}

void MediaPlayerPrivateGStreamer::didEnd()
{
    invalidateCachedPosition();
    MediaTime now = currentTime();
    GST_INFO_OBJECT(pipeline(), "Playback ended, currentMediaTime = %s, duration = %s", now.toString().utf8().data(), duration().toString().utf8().data());
    m_isEndReached = true;
    RefPtr player = m_player.get();

    if (!duration().isFinite()) {
        // From the HTMLMediaElement spec.
        // If an "infinite" stream ends for some reason, then the duration would change from positive Infinity to the
        // time of the last frame or sample in the stream, and the durationchange event would be fired.
        GST_DEBUG_OBJECT(pipeline(), "HTMLMediaElement duration previously infinite or unknown (e.g. live stream), setting it to current position.");
        m_cachedDuration = now;
        if (player)
            player->durationChanged();
    }

    if (!isMediaStreamPlayer()) {
        // Synchronize position and duration values to not confuse the
        // HTMLMediaElement. In some cases like reverse playback the
        // position is not always reported as 0 for instance.
        if (!m_isSeeking) {
            setCachedPosition(m_playbackRate > 0 ? duration() : MediaTime::zeroTime());
            GST_DEBUG("Position adjusted: %s", currentTime().toString().utf8().data());
        }
    }

    // Now that playback has ended it's NOT a safe time to send a SELECT_STREAMS event. In fact, as of GStreamer 1.16,
    // playbin3 will crash on a GStreamer assertion (combine->sinkpad being unexpectedly null) if we try. Instead, wait
    // until we get the initial STREAMS_SELECTED message one more time.
    m_waitingForStreamsSelectedEvent = true;

    if (player && !player->isLooping() && !isMediaSource()) {
        m_isPaused = true;
        changePipelineState(GST_STATE_PAUSED);
        m_didDownloadFinish = false;
        configureMediaStreamAudioTracks();
    }

    timeChanged(MediaTime::invalidTime());
}

void MediaPlayerPrivateGStreamer::getSupportedTypes(HashSet<String>& types)
{
    GStreamerRegistryScanner::getSupportedDecodingTypes(types);
}

MediaPlayer::SupportsType MediaPlayerPrivateGStreamer::supportsType(const MediaEngineSupportParameters& parameters)
{
    MediaPlayer::SupportsType result = MediaPlayer::SupportsType::IsNotSupported;
#if ENABLE(MEDIA_SOURCE)
    // MediaPlayerPrivateGStreamerMSE is in charge of mediasource playback, not us.
    if (parameters.isMediaSource)
        return result;
#endif

    if (parameters.isMediaStream) {
#if ENABLE(MEDIA_STREAM)
        return MediaPlayer::SupportsType::IsSupported;
#else
        return result;
#endif
    }

    if (parameters.type.isEmpty())
        return result;

    // This player doesn't support pictures rendering.
    if (parameters.type.raw().startsWith("image"_s))
        return result;

#if USE(EXTERNAL_HOLEPUNCH)
    HashSet<String> externalHolePunchTypes;
    MediaPlayerPrivateHolePunch::getSupportedTypes(externalHolePunchTypes);
    if (externalHolePunchTypes.contains(parameters.type.containerType()))
        return result;
#endif

    if (!ensureGStreamerInitialized())
        return result;

    GST_DEBUG("Checking mime-type \"%s\"", parameters.type.raw().utf8().data());

    registerWebKitGStreamerElements();

    auto& gstRegistryScanner = GStreamerRegistryScanner::singleton();
    result = gstRegistryScanner.isContentTypeSupported(GStreamerRegistryScanner::Configuration::Decoding, parameters.type, parameters.contentTypesRequiringHardwareSupport);

    GST_DEBUG("Supported: %s", convertEnumerationToString(result).utf8().data());
    return result;
}

bool isMediaDiskCacheDisabled()
{
    static bool result = false;
#if PLATFORM(WPE)
    static std::once_flag once;
    std::call_once(once, []() {
        auto shouldDisableMediaDiskCache = StringView::fromLatin1(std::getenv("WPE_SHELL_DISABLE_MEDIA_DISK_CACHE"));
        if (!shouldDisableMediaDiskCache.isEmpty()) {
            result = shouldDisableMediaDiskCache == "1"_s || equalLettersIgnoringASCIICase(shouldDisableMediaDiskCache, "true"_s)
                || equalLettersIgnoringASCIICase(shouldDisableMediaDiskCache, "t"_s);
        }
    });
#endif
    GST_DEBUG("Should disable media disk cache: %s", boolForPrinting(result));
    return result;
}

void MediaPlayerPrivateGStreamer::updateDownloadBufferingFlag()
{
    if (!m_pipeline)
        return;

    unsigned flags;
    g_object_get(m_pipeline.get(), "flags", &flags, nullptr);

    unsigned flagDownload = getGstPlayFlag("download");

    auto disableDownloading = [&] {
        GST_INFO_OBJECT(m_pipeline.get(), "Disabling on-disk buffering");
        g_object_set(m_pipeline.get(), "flags", flags & ~flagDownload, nullptr);
        m_fillTimer.stop();
    };

    RefPtr player = m_player.get();
    if (player && player->isLooping()) {
        // See also: https://gitlab.freedesktop.org/gstreamer/gstreamer/-/issues/3129
        GST_DEBUG_OBJECT(pipeline(), "Media is looping. Disabling deadlock-prone on-disk buffering");
        disableDownloading();
        return;
    }

    if (m_url.protocolIsBlob()) {
        GST_DEBUG_OBJECT(pipeline(), "Blob URI detected. Disabling on-disk buffering");
        disableDownloading();
        return;
    }

    // We don't want to stop downloading if we already started it.
    if (flags & flagDownload && m_readyState > MediaPlayer::ReadyState::HaveNothing && !m_shouldResetPipeline) {
        GST_DEBUG_OBJECT(pipeline(), "Download already started, not starting again");
        return;
    }

    bool diskCacheDisabled = isMediaDiskCacheDisabled();
    GST_DEBUG_OBJECT(pipeline(), "Media on-disk cache is %s", (diskCacheDisabled) ? "disabled" : "enabled");

    bool shouldDownload = !m_isLiveStream.value_or(false) && m_preload == MediaPlayer::Preload::Auto && !diskCacheDisabled;
    if (shouldDownload) {
        GST_INFO_OBJECT(pipeline(), "Enabling on-disk buffering");
        g_object_set(m_pipeline.get(), "flags", flags | flagDownload, nullptr);
        m_fillTimer.startRepeating(200_ms);
    } else
        disableDownloading();
}

void MediaPlayerPrivateGStreamer::setPlaybackFlags(bool isMediaStream)
{
    unsigned hasAudio = getGstPlayFlag("audio");
    unsigned hasVideo = getGstPlayFlag("video");
    unsigned hasText = getGstPlayFlag("text");
    unsigned hasNativeVideo = getGstPlayFlag("native-video");
    unsigned hasNativeAudio = getGstPlayFlag("native-audio");
    unsigned hasSoftwareColorBalance = getGstPlayFlag("soft-colorbalance");

    unsigned flags = 0;
    g_object_get(pipeline(), "flags", &flags, nullptr);
    GST_TRACE_OBJECT(pipeline(), "default flags %x", flags);
    flags = flags & ~hasText;
    flags = flags & ~hasNativeAudio;
    flags = flags & ~hasNativeVideo;
    flags = flags & ~hasSoftwareColorBalance;

    if (isMediaStream)
        flags = flags & ~getGstPlayFlag("buffering");

    unsigned additionalFlags = GStreamerQuirksManager::singleton().getAdditionalPlaybinFlags();
    hasText &= additionalFlags;
    hasSoftwareColorBalance &= additionalFlags;
    hasNativeVideo &= additionalFlags;
    hasNativeAudio &= additionalFlags;

    GST_INFO_OBJECT(pipeline(), "text %s, audio %s (native %s), video %s (native %s, software color balance %s)", boolForPrinting(hasText),
        boolForPrinting(hasAudio), boolForPrinting(hasNativeAudio), boolForPrinting(hasVideo), boolForPrinting(hasNativeVideo),
        boolForPrinting(hasSoftwareColorBalance));
    flags |= hasText | hasAudio | hasVideo | hasNativeVideo | hasNativeAudio | hasSoftwareColorBalance;
    g_object_set(pipeline(), "flags", flags, nullptr);
    GST_DEBUG_OBJECT(pipeline(), "current pipeline flags %x", flags);

    if (m_shouldPreservePitch && hasAudio && hasNativeAudio) {
        GST_WARNING_OBJECT(pipeline(), "can't preserve pitch with native audio");
        setPreservesPitch(false);
    }
}

void MediaPlayerPrivateGStreamer::createGSTPlayBin(const URL& url)
{
    RefPtr player = m_player.get();
    if (!player)
        return;

    GST_INFO("Creating pipeline for %s player", player->isVideoPlayer() ? "video" : "audio");
    const char* playbinName = "playbin";

    // MSE and Mediastream require playbin3. Regular playback can use playbin3 on-demand with the
    // WEBKIT_GST_USE_PLAYBIN3 environment variable.
    const char* usePlaybin3 = g_getenv("WEBKIT_GST_USE_PLAYBIN3");
    bool isMediaStream = url.protocolIs("mediastream"_s);
    if (isMediaSource() || isMediaStream || (usePlaybin3 && !strcmp(usePlaybin3, "1")))
        playbinName = "playbin3";

    ASSERT(!m_pipeline);

    auto elementId = player->elementId();
    if (elementId.isEmpty())
        elementId = "media-player"_s;

    const char* type = isMediaSource() ? "MSE-" : isMediaStream ? "mediastream-" : "";

    m_isLegacyPlaybin = !g_strcmp0(playbinName, "playbin");

    static Atomic<uint32_t> pipelineId;

    m_pipeline = makeGStreamerElement(playbinName, makeString(type, elementId, '-', pipelineId.exchangeAdd(1)).ascii().data());
    if (!m_pipeline) {
        GST_WARNING("%s not found, make sure to install gst-plugins-base", playbinName);
        loadingFailed(MediaPlayer::NetworkState::FormatError, MediaPlayer::ReadyState::HaveNothing, true);
        return;
    }

    registerActivePipeline(m_pipeline);

    setStreamVolumeElement(GST_STREAM_VOLUME(m_pipeline.get()));

    GST_INFO_OBJECT(pipeline(), "Using legacy playbin element: %s", boolForPrinting(m_isLegacyPlaybin));

    setPlaybackFlags(isMediaStream);

    // Let also other listeners subscribe to (application) messages in this bus.
    auto bus = adoptGRef(gst_pipeline_get_bus(GST_PIPELINE(m_pipeline.get())));
    gst_bus_enable_sync_message_emission(bus.get());
    connectSimpleBusMessageCallback(m_pipeline.get(), [this](GstMessage* message) {
        handleMessage(message);
    });

    g_signal_connect_swapped(bus.get(), "sync-message::need-context", G_CALLBACK(+[](MediaPlayerPrivateGStreamer* player, GstMessage* message) {
        player->handleNeedContextMessage(message);
    }), this);

    g_signal_connect_swapped(bus.get(), "message::segment-done", G_CALLBACK(+[](MediaPlayerPrivateGStreamer* player, GstMessage*) {
        callOnMainThread([weakThis = ThreadSafeWeakPtr { *player }, player] {
            RefPtr self = weakThis.get();
            if (!self)
                return;
            RefPtr mediaPlayer = player->m_player.get();
            if (!mediaPlayer || !mediaPlayer->isLooping())
                return;
            GST_DEBUG_OBJECT(player->pipeline(), "Handling segment-done message");
            player->didEnd();
        });
    }), this);

    // In the MSE case stream collection messages are emitted from the main thread right before the
    // initilization segment is parsed and "updateend" is fired. We need therefore to handle these
    // synchronously in the same main thread tick to make the tracks information available to JS no
    // later than "updateend".
    g_signal_connect_swapped(bus.get(), "sync-message::stream-collection", G_CALLBACK(+[](MediaPlayerPrivateGStreamer* player, GstMessage* message) {
        player->handleStreamCollectionMessage(message);
    }), this);

    g_object_set(m_pipeline.get(), "mute", static_cast<gboolean>(player->muted()), nullptr);

    g_signal_connect(GST_BIN_CAST(m_pipeline.get()), "element-setup", G_CALLBACK(+[](GstBin*, GstElement* element, MediaPlayerPrivateGStreamer* player) {
        player->configureElement(element);
    }), this);

    g_signal_connect_swapped(m_pipeline.get(), "source-setup", G_CALLBACK(sourceSetupCallback), this);

    if (auto* textCombiner = webkitTextCombinerNew())
        g_object_set(m_pipeline.get(), "text-stream-combiner", textCombiner, nullptr);

    m_textSink = webkitTextSinkNew(*this);
    ASSERT(m_textSink);

    g_object_set(m_pipeline.get(), "text-sink", m_textSink.get(), nullptr);

    if (!m_audioSink)
        m_audioSink = createAudioSink();

    g_object_set(m_pipeline.get(), "audio-sink", m_audioSink.get(), "video-sink", createVideoSink(), nullptr);

    if (m_shouldPreservePitch && !isMediaStream) {
        if (auto* scale = makeGStreamerElement("scaletempo", nullptr))
            g_object_set(m_pipeline.get(), "audio-filter", scale, nullptr);
    }

    if (!player->isVideoPlayer())
        return;

    if (isHolePunchRenderingEnabled())
        return;

    GRefPtr<GstPad> videoSinkPad = adoptGRef(gst_element_get_static_pad(m_videoSink.get(), "sink"));
    if (videoSinkPad)
        g_signal_connect(videoSinkPad.get(), "notify::caps", G_CALLBACK(+[](GstPad* videoSinkPad, GParamSpec*, MediaPlayerPrivateGStreamer* player) {
            player->videoSinkCapsChanged(videoSinkPad);
        }), this);
}

void MediaPlayerPrivateGStreamer::setupCodecProbe(GstElement* element)
{
#if GST_CHECK_VERSION(1, 20, 0)
    auto sinkPad = adoptGRef(gst_element_get_static_pad(element, "sink"));
    gst_pad_add_probe(sinkPad.get(), GST_PAD_PROBE_TYPE_EVENT_DOWNSTREAM, reinterpret_cast<GstPadProbeCallback>(+[](GstPad* pad, GstPadProbeInfo* info, MediaPlayerPrivateGStreamer* player) -> GstPadProbeReturn {
        auto* event = gst_pad_probe_info_get_event(info);
        if (GST_EVENT_TYPE(event) != GST_EVENT_CAPS)
            return GST_PAD_PROBE_OK;

        GstCaps* caps;
        gst_event_parse_caps(event, &caps);
        GUniquePtr<char> codec(gst_codec_utils_caps_get_mime_codec(caps));
        if (!codec)
            return GST_PAD_PROBE_REMOVE;

        GUniquePtr<char> streamId(gst_pad_get_stream_id(pad));
        if (UNLIKELY(!streamId)) {
            // FIXME: This is a workaround for https://bugs.webkit.org/show_bug.cgi?id=256428.
            GST_WARNING_OBJECT(player->pipeline(), "Caps event received before stream-start. This shouldn't happen!");
            return GST_PAD_PROBE_REMOVE;
        }

        GST_INFO_OBJECT(player->pipeline(), "Setting codec for stream %s to %s", streamId.get(), codec.get());
        {
            Locker locker { player->m_codecsLock };
            player->m_codecs.add(String::fromLatin1(streamId.get()), String::fromLatin1(codec.get()));
        }
        return GST_PAD_PROBE_REMOVE;
    }), this, nullptr);
#else
    UNUSED_PARAM(element);
#endif
}

void MediaPlayerPrivateGStreamer::configureAudioDecoder(GstElement* decoder)
{
    setupCodecProbe(decoder);
}

void MediaPlayerPrivateGStreamer::configureVideoDecoder(GstElement* decoder)
{
    GUniquePtr<char> name(gst_element_get_name(decoder));
    if (g_str_has_prefix(name.get(), "v4l2"))
        m_videoDecoderPlatform = GstVideoDecoderPlatform::Video4Linux;
    else if (g_str_has_prefix(name.get(), "imxvpudec"))
        m_videoDecoderPlatform = GstVideoDecoderPlatform::ImxVPU;
    else if (g_str_has_prefix(name.get(), "omx"))
        m_videoDecoderPlatform = GstVideoDecoderPlatform::OpenMAX;
    else if (g_str_has_prefix(name.get(), "avdec")) {
        // Set the decoder maximum number of threads to a low, fixed value, not depending on the
        // platform. This also helps with processing metrics gathering. When using the default value
        // the decoder introduces artificial processing latency reflecting the maximum number of threads.
        if (gstObjectHasProperty(decoder, "max-threads"))
            g_object_set(decoder, "max-threads", 2, nullptr);
    }

    if (gstObjectHasProperty(decoder, "max-errors"))
        g_object_set(decoder, "max-errors", 0, nullptr);

#if USE(TEXTURE_MAPPER)
    updateTextureMapperFlags();
#endif

    setupCodecProbe(decoder);

    if (!isMediaStreamPlayer())
        return;

    configureMediaStreamVideoDecoder(decoder);

    auto pad = adoptGRef(gst_element_get_static_pad(decoder, "src"));
    gst_pad_add_probe(pad.get(), static_cast<GstPadProbeType>(GST_PAD_PROBE_TYPE_QUERY_DOWNSTREAM | GST_PAD_PROBE_TYPE_BUFFER), [](GstPad*, GstPadProbeInfo* info, gpointer userData) -> GstPadProbeReturn {
        auto* player = static_cast<MediaPlayerPrivateGStreamer*>(userData);
        if (GST_PAD_PROBE_INFO_TYPE(info) & GST_PAD_PROBE_TYPE_BUFFER) {
            player->incrementDecodedVideoFramesCount();
            return GST_PAD_PROBE_OK;
        }

        if (GST_QUERY_TYPE(GST_PAD_PROBE_INFO_QUERY(info)) == GST_QUERY_CUSTOM) {
            auto* query = GST_QUERY_CAST(GST_PAD_PROBE_INFO_DATA(info));
            auto* structure = gst_query_writable_structure(query);
            if (gst_structure_has_name(structure, "webkit-video-decoder-stats")) {
                gst_structure_set(structure, "frames-decoded", G_TYPE_UINT64, player->decodedVideoFramesCount(), nullptr);

                if (player->updateVideoSinkStatistics())
                    gst_structure_set(structure, "frames-dropped", G_TYPE_UINT64, player->m_droppedVideoFrames, nullptr);

                auto naturalSize = roundedIntSize(player->naturalSize());
                if (naturalSize.width() && naturalSize.height())
                    gst_structure_set(structure, "frame-width", G_TYPE_UINT, naturalSize.width(), "frame-height", G_TYPE_UINT, naturalSize.height(), nullptr);

                GST_PAD_PROBE_INFO_DATA(info) = query;
                return GST_PAD_PROBE_HANDLED;
            }
        }

        return GST_PAD_PROBE_OK;
    }, this, nullptr);
}

bool MediaPlayerPrivateGStreamer::didPassCORSAccessCheck() const
{
    if (WEBKIT_IS_WEB_SRC(m_source.get()))
        return webKitSrcPassedCORSAccessCheck(WEBKIT_WEB_SRC_CAST(m_source.get()));
    return false;
}

bool MediaPlayerPrivateGStreamer::canSaveMediaData() const
{
    if (m_isLiveStream.value_or(false))
        return false;

    if (m_url.protocolIsFile())
        return true;

    if (m_url.protocolIsInHTTPFamily())
        return true;

    return false;
}

void MediaPlayerPrivateGStreamer::pausedTimerFired()
{
    GST_DEBUG_OBJECT(pipeline(), "In PAUSED for too long. Releasing pipeline resources.");
    changePipelineState(GST_STATE_NULL);
}

void MediaPlayerPrivateGStreamer::acceleratedRenderingStateChanged()
{
    RefPtr player = m_player.get();
    m_canRenderingBeAccelerated = player && player->acceleratedCompositingEnabled();
}

bool MediaPlayerPrivateGStreamer::performTaskAtTime(Function<void()>&& task, const MediaTime& time)
{
    ASSERT(isMainThread());

    // Ignore the cases when the time isn't marching on or the position is unknown.
    MediaTime currentTime = playbackPosition();
    if (!m_pipeline || m_didErrorOccur || m_isSeeking || m_isPaused || !m_playbackRate || !currentTime.isValid())
        return false;

    std::optional<Function<void()>> taskToSchedule;
    {
        DataMutexLocker taskAtMediaTimeScheduler { m_TaskAtMediaTimeSchedulerDataMutex };
        taskAtMediaTimeScheduler->setTask(WTFMove(task), time,
            m_playbackRate >= 0 ? TaskAtMediaTimeScheduler::Forward : TaskAtMediaTimeScheduler::Backward);
        taskToSchedule = taskAtMediaTimeScheduler->checkTaskForScheduling(currentTime);
    }

    // Dispatch the task if the time is already reached. Dispatching instead of directly running the
    // task prevents infinite recursion in case the task calls performTaskAtTime() internally.
    if (taskToSchedule)
        RunLoop::main().dispatch(WTFMove(taskToSchedule.value()));

    return true;
}

void MediaPlayerPrivateGStreamer::isLoopingChanged()
{
    RefPtr player = m_player.get();
    GST_DEBUG_OBJECT(m_pipeline.get(), "Looping attribute changed to: %s", player ? boolForPrinting(player->isLooping()) : nullptr);
    ensureSeekFlags();
}

#if USE(TEXTURE_MAPPER)
PlatformLayer* MediaPlayerPrivateGStreamer::platformLayer() const
{
#if USE(NICOSIA)
    return m_nicosiaLayer.get();
#else
    return const_cast<MediaPlayerPrivateGStreamer*>(this);
#endif
}

#if USE(NICOSIA)
void MediaPlayerPrivateGStreamer::swapBuffersIfNeeded()
{
}
#else
RefPtr<TextureMapperPlatformLayerProxy> MediaPlayerPrivateGStreamer::proxy() const
{
    return m_platformLayerProxy.copyRef();
}

void MediaPlayerPrivateGStreamer::swapBuffersIfNeeded()
{
    if (isHolePunchRenderingEnabled())
        pushNextHolePunchBuffer();
}
#endif

void MediaPlayerPrivateGStreamer::pushTextureToCompositor()
{
    Locker sampleLocker { m_sampleMutex };
    if (!GST_IS_SAMPLE(m_sample.get()))
        return;

    ++m_sampleCount;

    auto internalCompositingOperation = [this](TextureMapperPlatformLayerProxyGL& proxy, std::unique_ptr<GstVideoFrameHolder>&& frameHolder) {
        std::unique_ptr<TextureMapperPlatformLayerBuffer> layerBuffer;
        if (frameHolder->hasMappedTextures()) {
            layerBuffer = frameHolder->platformLayerBuffer();
            if (!layerBuffer)
                return;
            layerBuffer->setUnmanagedBufferDataHolder(WTFMove(frameHolder));
        } else {
            layerBuffer = proxy.getAvailableBuffer(frameHolder->size(), GL_DONT_CARE);
            if (UNLIKELY(!layerBuffer)) {
                OptionSet<BitmapTexture::Flags> flags;
                if (frameHolder->hasAlphaChannel())
                    flags.add(BitmapTexture::Flags::SupportsAlpha);
                auto texture = BitmapTexture::create(frameHolder->size(), flags);
                layerBuffer = makeUnique<TextureMapperPlatformLayerBuffer>(WTFMove(texture));
            }
            frameHolder->updateTexture(layerBuffer->texture());
            auto extraFlags = m_textureMapperFlags;
            if (frameHolder->hasAlphaChannel())
                extraFlags.add({ TextureMapperFlags::ShouldBlend, TextureMapperFlags::ShouldPremultiply });
            layerBuffer->setExtraFlags(extraFlags);
        }
        proxy.pushNextBuffer(WTFMove(layerBuffer));
    };

    auto proxyOperation =
        [this, internalCompositingOperation](TextureMapperPlatformLayerProxyGL& proxy)
        {
            Locker locker { proxy.lock() };

            if (!proxy.isActive())
                return;

            auto frameHolder = makeUnique<GstVideoFrameHolder>(m_sample.get(), m_videoDecoderPlatform, m_textureMapperFlags, !m_isUsingFallbackVideoSink);
            internalCompositingOperation(proxy, WTFMove(frameHolder));
        };

#if USE(NICOSIA)
    auto& proxy = m_nicosiaLayer->proxy();
    ASSERT(is<TextureMapperPlatformLayerProxyGL>(proxy));
    proxyOperation(downcast<TextureMapperPlatformLayerProxyGL>(proxy));
#else
    proxyOperation(*m_platformLayerProxy);
#endif
}
#endif // USE(TEXTURE_MAPPER)

#if USE(TEXTURE_MAPPER_DMABUF)
// GStreamer's gst_video_format_to_fourcc() doesn't cover RGB-like formats, so we
// provide the appropriate FourCC values for those through this funcion.
static uint32_t fourccValue(GstVideoFormat format)
{
    switch (format) {
    case GST_VIDEO_FORMAT_RGBx:
        return uint32_t(DMABufFormat::FourCC::XBGR8888);
    case GST_VIDEO_FORMAT_BGRx:
        return uint32_t(DMABufFormat::FourCC::XRGB8888);
    case GST_VIDEO_FORMAT_xRGB:
        return uint32_t(DMABufFormat::FourCC::BGRX8888);
    case GST_VIDEO_FORMAT_xBGR:
        return uint32_t(DMABufFormat::FourCC::RGBX8888);
    case GST_VIDEO_FORMAT_RGBA:
        return uint32_t(DMABufFormat::FourCC::ABGR8888);
    case GST_VIDEO_FORMAT_BGRA:
        return uint32_t(DMABufFormat::FourCC::ARGB8888);
    case GST_VIDEO_FORMAT_ARGB:
        return uint32_t(DMABufFormat::FourCC::BGRA8888);
    case GST_VIDEO_FORMAT_ABGR:
        return uint32_t(DMABufFormat::FourCC::RGBA8888);
    case GST_VIDEO_FORMAT_P010_10LE:
    case GST_VIDEO_FORMAT_P010_10BE:
        return uint32_t(DMABufFormat::FourCC::P010);
    case GST_VIDEO_FORMAT_P016_LE:
    case GST_VIDEO_FORMAT_P016_BE:
        return uint32_t(DMABufFormat::FourCC::P016);
    default:
        break;
    }

    return gst_video_format_to_fourcc(format);
}

static DMABufColorSpace colorSpaceForColorimetry(const GstVideoColorimetry* cinfo)
{
    if (gst_video_colorimetry_matches(cinfo, GST_VIDEO_COLORIMETRY_SRGB))
        return DMABufColorSpace::SRGB;
    if (gst_video_colorimetry_matches(cinfo, GST_VIDEO_COLORIMETRY_BT601))
        return DMABufColorSpace::BT601;
    if (gst_video_colorimetry_matches(cinfo, GST_VIDEO_COLORIMETRY_BT709))
        return DMABufColorSpace::BT709;
    if (gst_video_colorimetry_matches(cinfo, GST_VIDEO_COLORIMETRY_BT2020))
        return DMABufColorSpace::BT2020;
    if (gst_video_colorimetry_matches(cinfo, GST_VIDEO_COLORIMETRY_SMPTE240M))
        return DMABufColorSpace::SMPTE240M;
    return DMABufColorSpace::Invalid;
}

struct DMABufMemoryQuarkData {
    DMABufReleaseFlag releaseFlag;
};

G_DEFINE_QUARK(DMABufMemoryQuarkData, dmabuf_memory);

void MediaPlayerPrivateGStreamer::pushDMABufToCompositor()
{
    m_dmabufMemory.removeIf([](auto& memory) -> bool {
        auto* quarkData = static_cast<DMABufMemoryQuarkData*>(gst_mini_object_get_qdata(GST_MINI_OBJECT_CAST(memory.get()), dmabuf_memory_quark()));
        if (!quarkData)
            return true;
        return quarkData->releaseFlag.released();
    });

    Locker sampleLocker { m_sampleMutex };
    if (!GST_IS_SAMPLE(m_sample.get()))
        return;

    auto* caps = gst_sample_get_caps(m_sample.get());
    if (!caps)
        return;

    GstVideoInfo videoInfo;
    gst_video_info_init(&videoInfo);
    if (!gst_video_info_from_caps(&videoInfo, caps))
        return;

    auto* buffer = gst_sample_get_buffer(m_sample.get());
    if (!buffer)
        return;

    auto* meta = gst_buffer_get_video_meta(buffer);
    if (meta) {
        GST_VIDEO_INFO_WIDTH(&videoInfo) = meta->width;
        GST_VIDEO_INFO_HEIGHT(&videoInfo) = meta->height;

        for (unsigned i = 0; i < meta->n_planes; ++i) {
            GST_VIDEO_INFO_PLANE_OFFSET(&videoInfo, i) = meta->offset[i];
            GST_VIDEO_INFO_PLANE_STRIDE(&videoInfo, i) = meta->stride[i];
        }
    }

    auto textureMapperFlags = m_textureMapperFlags;
    if (GST_VIDEO_INFO_HAS_ALPHA(&videoInfo))
        textureMapperFlags.add({ TextureMapperFlags::ShouldBlend, TextureMapperFlags::ShouldPremultiply });

    ++m_sampleCount;

    auto& proxy = m_nicosiaLayer->proxy();
    ASSERT(is<TextureMapperPlatformLayerProxyDMABuf>(proxy));

    Locker locker { proxy.lock() };
    if (!proxy.isActive())
        return;

    // Currently we have to cover two ways of detecting a DMABuf memory. The most reliable is by detecting
    // the memory:DMABuf feature on the GstCaps object. All sensible decoders yielding DMABufs specify this.
    // For all other decoders, another option is peeking the zero-index GstMemory and testing whether it's
    // a DMABuf memory, i.e. allocated by a DMABuf-capable allocator. If it is, we can proceed the same way.
    bool isDMABufMemory = gst_caps_features_contains(gst_caps_get_features(caps, 0), GST_CAPS_FEATURE_MEMORY_DMABUF)
        || gst_is_dmabuf_memory(gst_buffer_peek_memory(buffer, 0));
    if (isDMABufMemory) {
        // In case of a hardware decoder that's yielding dmabuf memory, we can take the relevant data and
        // push it into the composition process.
        auto memory = adoptGRef(gst_buffer_get_memory(buffer, 0));

        // Provide the DMABufObject with a relevant handle (memory address). When provided for the first time,
        // the lambda will be invoked and all dmabuf data is filled in.
        downcast<TextureMapperPlatformLayerProxyDMABuf>(proxy).pushDMABuf(
            DMABufObject(reinterpret_cast<uintptr_t>(memory.get())),
            [&](auto&& object) {
                bool infoHasDrmFormat = false;
                uint32_t fourcc = 0;
#if GST_CHECK_VERSION(1, 24, 0)
                GstVideoInfoDmaDrm drmInfo;
                infoHasDrmFormat = gst_video_info_dma_drm_from_caps(&drmInfo, caps);
                if (infoHasDrmFormat)
                    fourcc = drmInfo.drm_fourcc;
#endif
                if (!fourcc)
                    fourcc = fourccValue(GST_VIDEO_INFO_FORMAT(&videoInfo));

                object.format = DMABufFormat::create(fourcc);
                object.colorSpace = colorSpaceForColorimetry(&GST_VIDEO_INFO_COLORIMETRY(&videoInfo));

                if (m_videoSize.isEmpty()) {
                    object.width = GST_VIDEO_INFO_WIDTH(&videoInfo);
                    object.height = GST_VIDEO_INFO_HEIGHT(&videoInfo);
                } else {
                    object.width = m_videoSize.width();
                    object.height = m_videoSize.height();
                }

                // The dmabuf object itself doesn't provide anything useful, but the decoder won't
                // reuse the dmabuf until the relevant GstSample reference is dropped by the
                // downstream pipeline. So for this to work, we associate the GstMemory reference
                // count with this release flag so that the reference is dropped once the release
                // flag is signalled.
                object.releaseFlag = DMABufReleaseFlag(DMABufReleaseFlag::Initialize);

                gst_mini_object_set_qdata(GST_MINI_OBJECT_CAST(memory.get()), dmabuf_memory_quark(),
                    new DMABufMemoryQuarkData { object.releaseFlag.dup() },
                    [](gpointer data) {
                        delete static_cast<DMABufMemoryQuarkData*>(data);
                    });

                // For each plane, the relevant data (stride, offset, skip, dmabuf fd) is retrieved and assigned
                // as appropriate.
                for (unsigned i = 0; i < object.format.numPlanes; ++i) {
                    gsize offset = GST_VIDEO_INFO_PLANE_OFFSET(&videoInfo, i);
                    guint memid = 0;
                    guint length = 0;
                    gsize skip = 0;
                    if (gst_buffer_find_memory(buffer, offset, 1, &memid, &length, &skip)) {
                        auto* mem = gst_buffer_peek_memory(buffer, memid);
                        object.fd[i] = { gst_dmabuf_memory_get_fd(mem), UnixFileDescriptor::Duplicate };
                        offset = mem->offset + skip;
                    } else
                        object.fd[i] = { };

                    gint comp[GST_VIDEO_MAX_COMPONENTS];
                    gst_video_format_info_component(videoInfo.finfo, i, comp);
                    object.offset[i] = offset;
                    object.stride[i] = GST_VIDEO_INFO_PLANE_STRIDE(&videoInfo, i);
                    object.modifierPresent[i] = infoHasDrmFormat;
#if GST_CHECK_VERSION(1, 24, 0)
                    if (infoHasDrmFormat)
                        object.modifierValue[i] = drmInfo.drm_modifier;
#endif
                }
                return WTFMove(object);
            }, textureMapperFlags);

        auto* quarkData = static_cast<DMABufMemoryQuarkData*>(gst_mini_object_get_qdata(GST_MINI_OBJECT_CAST(memory.get()), dmabuf_memory_quark()));
        if (quarkData)
            m_dmabufMemory.add(WTFMove(memory));

        return;
    }

    // If the decoder is exporting raw memory, we have to use the swapchain to allocate appropriate buffers
    // and copy over the data for each plane. For that to work, linear-storage buffer is required.
    GBMBufferSwapchain::BufferDescription bufferDescription {
        .format = DMABufFormat::create(fourccValue(GST_VIDEO_INFO_FORMAT(&videoInfo))),
        .width = static_cast<uint32_t>GST_VIDEO_INFO_WIDTH(&videoInfo),
        .height = static_cast<uint32_t>GST_VIDEO_INFO_HEIGHT(&videoInfo),
        .flags = GBMBufferSwapchain::BufferDescription::LinearStorage,
    };
    if (bufferDescription.format.fourcc == DMABufFormat::FourCC::Invalid)
        return;

    auto swapchainBuffer = m_swapchain->getBuffer(bufferDescription);
    if (!swapchainBuffer)
        return;

    // Destination helper struct, maps the gbm_bo object into CPU-memory space and copies from the accompanying Source in fill().
    struct Destination {
        static Lock& mappingLock()
        {
            static Lock s_mappingLock;
            return s_mappingLock;
        }

        Destination(struct gbm_bo* bo, uint32_t width, uint32_t height)
            : bo(bo)
            , height(height)
        {
            map = gbm_bo_map(bo, 0, 0, width, height, GBM_BO_TRANSFER_WRITE, &stride, &mapData);
            if (!map)
                return;

            valid = true;
            data = reinterpret_cast<uint8_t*>(map);
        }

        ~Destination()
        {
            if (valid)
                gbm_bo_unmap(bo, mapData);
        }

        void copyPlaneData(GstVideoFrame* sourceVideoFrame, unsigned planeIndex)
        {
            auto sourceStride = GST_VIDEO_FRAME_PLANE_STRIDE(sourceVideoFrame, planeIndex);
            auto* planeData = reinterpret_cast<uint8_t*>(GST_VIDEO_FRAME_PLANE_DATA(sourceVideoFrame, planeIndex));
            for (uint32_t y = 0; y < height; ++y) {
                auto* destinationData = &data[y * stride];
                auto* sourceData = &planeData[y * sourceStride];
                memcpy(destinationData, sourceData, std::min(static_cast<uint32_t>(sourceStride), stride));
            }
        }

        bool valid { false };
        struct gbm_bo* bo { nullptr };
        void* map { nullptr };
        void* mapData { nullptr };
        uint8_t* data { nullptr };
        uint32_t height { 0 };
        uint32_t stride { 0 };
    };

    GstMappedFrame sourceFrame(m_sample, GST_MAP_READ);
    if (!sourceFrame)
        return;

    auto* sourceVideoFrame = sourceFrame.get();
    for (unsigned i = 0; i < GST_VIDEO_FRAME_N_PLANES(sourceVideoFrame); ++i) {
        auto& planeData = swapchainBuffer->planeData(i);

        Locker locker { Destination::mappingLock() };
        Destination destination(planeData.bo, planeData.width, planeData.height);
        if (destination.valid)
            destination.copyPlaneData(sourceVideoFrame, i);
    }

    // The updated buffer is pushed into the composition stage. The DMABufObject handle uses the swapchain address as the handle base.
    // When the buffer is pushed for the first time, the lambda will be invoked to retrieve a more complete DMABufObject for the
    // given GBMBufferSwapchain::Buffer object.
    downcast<TextureMapperPlatformLayerProxyDMABuf>(proxy).pushDMABuf(
        DMABufObject(reinterpret_cast<uintptr_t>(m_swapchain.get()) + swapchainBuffer->handle()),
        [&](auto&& initialObject) {
            auto object = swapchainBuffer->createDMABufObject(initialObject.handle);
            object.colorSpace = colorSpaceForColorimetry(&GST_VIDEO_INFO_COLORIMETRY(&videoInfo));
            return object;
        }, textureMapperFlags);
}
#endif // USE(TEXTURE_MAPPER_DMABUF)

void MediaPlayerPrivateGStreamer::repaint()
{
    ASSERT(m_sample);
    ASSERT(isMainThread());

    if (RefPtr player = m_player.get())
        player->repaint();

    Locker locker { m_drawLock };
    m_drawCondition.notifyOne();
}

static ImageOrientation getVideoOrientation(const GstTagList* tagList)
{
    ASSERT(tagList);
    GUniqueOutPtr<gchar> tag;
    if (!gst_tag_list_get_string(tagList, GST_TAG_IMAGE_ORIENTATION, &tag.outPtr())) {
        GST_DEBUG("No image_orientation tag, applying no rotation.");
        return ImageOrientation::Orientation::None;
    }

    GST_DEBUG("Found image_orientation tag: %s", tag.get());
    if (!g_strcmp0(tag.get(), "flip-rotate-0"))
        return ImageOrientation::Orientation::OriginTopRight;
    if (!g_strcmp0(tag.get(), "rotate-180"))
        return ImageOrientation::Orientation::OriginBottomRight;
    if (!g_strcmp0(tag.get(), "flip-rotate-180"))
        return ImageOrientation::Orientation::OriginBottomLeft;
    if (!g_strcmp0(tag.get(), "flip-rotate-270"))
        return ImageOrientation::Orientation::OriginLeftTop;
    if (!g_strcmp0(tag.get(), "rotate-90"))
        return ImageOrientation::Orientation::OriginRightTop;
    if (!g_strcmp0(tag.get(), "flip-rotate-90"))
        return ImageOrientation::Orientation::OriginRightBottom;
    if (!g_strcmp0(tag.get(), "rotate-270"))
        return ImageOrientation::Orientation::OriginLeftBottom;

    // Default rotation.
    return ImageOrientation::Orientation::None;
}

void MediaPlayerPrivateGStreamer::updateVideoOrientation(const GstTagList* tagList)
{
    GST_DEBUG_OBJECT(pipeline(), "Updating orientation from %" GST_PTR_FORMAT, tagList);
    auto sizeActuallyChanged = setVideoSourceOrientation(getVideoOrientation(tagList));

    if (!sizeActuallyChanged)
        return;

    // If the video is tagged as rotated 90 or 270 degrees, swap width and height.
    if (m_videoSourceOrientation.usesWidthAsHeight())
        m_videoSize = m_videoSize.transposedSize();

    GST_DEBUG_OBJECT(pipeline(), "Enqueuing and waiting for main-thread task to call sizeChanged()...");
    bool sizeChangedProcessed = m_sinkTaskQueue.enqueueTaskAndWait<AbortableTaskQueue::Void>([weakThis = ThreadSafeWeakPtr { *this }, this] {
        RefPtr self = weakThis.get();
        if (!self)
            return AbortableTaskQueue::Void();
        RefPtr player = m_player.get();
        if (!player)
            return AbortableTaskQueue::Void();

        player->sizeChanged();
        return AbortableTaskQueue::Void();
    }).has_value();
    GST_DEBUG_OBJECT(pipeline(), "Finished waiting for main-thread task to call sizeChanged()... %s", sizeChangedProcessed ? "sizeChanged() was called." : "task queue aborted by flush");
}

void MediaPlayerPrivateGStreamer::updateVideoSizeAndOrientationFromCaps(const GstCaps* caps)
{
    ASSERT(isMainThread());

    // TODO: handle possible clean aperture data. See https://bugzilla.gnome.org/show_bug.cgi?id=596571
    // TODO: handle possible transformation matrix. See https://bugzilla.gnome.org/show_bug.cgi?id=596326

    // Get the video PAR and original size, if this fails the
    // video-sink has likely not yet negotiated its caps.
    int pixelAspectRatioNumerator, pixelAspectRatioDenominator, stride;
    IntSize originalSize;
    GstVideoFormat format;
    if (!getVideoSizeAndFormatFromCaps(caps, originalSize, format, pixelAspectRatioNumerator, pixelAspectRatioDenominator, stride)) {
        GST_WARNING("Failed to get size and format from caps: %" GST_PTR_FORMAT, caps);
        return;
    }

    auto pad = adoptGRef(gst_element_get_static_pad(m_videoSink.get(), "sink"));
    ASSERT(pad);
    auto tagsEvent = adoptGRef(gst_pad_get_sticky_event(pad.get(), GST_EVENT_TAG, 0));
    auto orientation = ImageOrientation::Orientation::None;
    if (tagsEvent) {
        GstTagList* tagList;
        gst_event_parse_tag(tagsEvent.get(), &tagList);
        orientation = getVideoOrientation(tagList).orientation();
    }

    setVideoSourceOrientation(orientation);
    // If the video is tagged as rotated 90 or 270 degrees, swap width and height.
    if (m_videoSourceOrientation.usesWidthAsHeight())
        originalSize = originalSize.transposedSize();

    GST_DEBUG_OBJECT(pipeline(), "Original video size: %dx%d", originalSize.width(), originalSize.height());
    GST_DEBUG_OBJECT(pipeline(), "Pixel aspect ratio: %d/%d", pixelAspectRatioNumerator, pixelAspectRatioDenominator);

    // Calculate DAR based on PAR and video size.
    int displayWidth = originalSize.width() * pixelAspectRatioNumerator;
    int displayHeight = originalSize.height() * pixelAspectRatioDenominator;

    // Divide display width and height by their GCD to avoid possible overflows.
    int displayAspectRatioGCD = gst_util_greatest_common_divisor(displayWidth, displayHeight);
    displayWidth /= displayAspectRatioGCD;
    displayHeight /= displayAspectRatioGCD;

    // Apply DAR to original video size. This is the same behavior as in xvimagesink's setcaps function.
    uint64_t width = 0, height = 0;
    if (!(originalSize.height() % displayHeight)) {
        GST_DEBUG_OBJECT(pipeline(), "Keeping video original height");
        width = gst_util_uint64_scale_int(originalSize.height(), displayWidth, displayHeight);
        height = originalSize.height();
    } else if (!(originalSize.width() % displayWidth)) {
        GST_DEBUG_OBJECT(pipeline(), "Keeping video original width");
        height = gst_util_uint64_scale_int(originalSize.width(), displayHeight, displayWidth);
        width = originalSize.width();
    } else {
        GST_DEBUG_OBJECT(pipeline(), "Approximating while keeping original video height");
        width = gst_util_uint64_scale_int(originalSize.height(), displayWidth, displayHeight);
        height = originalSize.height();
    }

    GST_DEBUG_OBJECT(pipeline(), "Saving natural size: %" G_GUINT64_FORMAT "x%" G_GUINT64_FORMAT, width, height);
    m_videoSize = FloatSize(static_cast<int>(width), static_cast<int>(height));
    if (RefPtr player = m_player.get())
        player->sizeChanged();
}

void MediaPlayerPrivateGStreamer::setCachedPosition(const MediaTime& cachedPosition) const
{
    m_cachedPosition = cachedPosition;
    m_isCachedPositionValid = true;
}

void MediaPlayerPrivateGStreamer::invalidateCachedPosition() const
{
    m_isCachedPositionValid = false;
}

void MediaPlayerPrivateGStreamer::invalidateCachedPositionOnNextIteration() const
{
    RunLoop::main().dispatch([weakThis = ThreadSafeWeakPtr { *this }, this] {
        RefPtr player = weakThis.get();
        if (!player)
            return;
        invalidateCachedPosition();
    });
}

void MediaPlayerPrivateGStreamer::ensureSeekFlags()
{
    RefPtr player = m_player.get();
    auto flag = (player && player->isLooping()) ? GST_SEEK_FLAG_SEGMENT : GST_SEEK_FLAG_FLUSH;
    m_seekFlags = static_cast<GstSeekFlags>(flag | GST_SEEK_FLAG_ACCURATE);
}

void MediaPlayerPrivateGStreamer::triggerRepaint(GRefPtr<GstSample>&& sample)
{
    ASSERT(!isMainThread());

    GstBuffer* buffer = gst_sample_get_buffer(sample.get());
    if (buffer && GST_BUFFER_PTS_IS_VALID(buffer)) {
        // Heuristic to avoid asking for playbackPosition() from a non-main thread.
        MediaTime currentTime = MediaTime(gst_segment_to_stream_time(gst_sample_get_segment(sample.get()), GST_FORMAT_TIME, GST_BUFFER_PTS(buffer)), GST_SECOND);
        DataMutexLocker taskAtMediaTimeScheduler { m_TaskAtMediaTimeSchedulerDataMutex };
        if (auto task = taskAtMediaTimeScheduler->checkTaskForScheduling(currentTime))
            RunLoop::main().dispatch(WTFMove(task.value()));
    }

    bool shouldTriggerResize;
    {
        Locker sampleLocker { m_sampleMutex };
        shouldTriggerResize = !m_sample;
        m_sample = WTFMove(sample);
    }

    if (shouldTriggerResize) {
        GST_DEBUG_OBJECT(pipeline(), "First sample reached the sink, triggering video dimensions update");
        GRefPtr<GstCaps> caps;
        {
            Locker sampleLocker { m_sampleMutex };
            caps = gst_sample_get_caps(m_sample.get());
            if (!caps) {
                GST_ERROR_OBJECT(pipeline(), "Received sample without caps: %" GST_PTR_FORMAT, m_sample.get());
                return;
            }
        }
        RunLoop::main().dispatch([weakThis = ThreadSafeWeakPtr { *this }, this, caps = WTFMove(caps)] {
            RefPtr self = weakThis.get();
            if (!self)
                return;
            RefPtr player = m_player.get();

            updateVideoSizeAndOrientationFromCaps(caps.get());

            // Live streams start without pre-rolling, that means they can reach PAUSED while sinks
            // still haven't received a sample to render. So we need to notify the media element in
            // such cases only after pre-rolling has completed. Otherwise the media element might
            // emit a play event too early, before pre-rolling has been completed.
            if (m_isLiveStream.value_or(false) && m_readyState < MediaPlayer::ReadyState::HaveEnoughData) {
                m_readyState = MediaPlayer::ReadyState::HaveEnoughData;
                if (player)
                    player->readyStateChanged();
            }
        });
    }

    if (!m_canRenderingBeAccelerated) {
        Locker locker { m_drawLock };
        if (m_isBeingDestroyed)
            return;
        m_drawTimer.startOneShot(0_s);
        m_drawCondition.wait(m_drawLock);
        return;
    }

#if USE(TEXTURE_MAPPER)
    if (m_isUsingFallbackVideoSink) {
        Locker locker { m_drawLock };
        auto proxyOperation =
            [this](TextureMapperPlatformLayerProxyGL& proxy)
            {
                return proxy.scheduleUpdateOnCompositorThread([this] { this->pushTextureToCompositor(); });
            };
#if USE(NICOSIA)
        auto& proxy = m_nicosiaLayer->proxy();
        ASSERT(is<TextureMapperPlatformLayerProxyGL>(proxy));
        if (!proxyOperation(downcast<TextureMapperPlatformLayerProxyGL>(proxy)))
            return;
#else
        if (!proxyOperation(*m_platformLayerProxy))
            return;
#endif
        m_drawTimer.startOneShot(0_s);
        m_drawCondition.wait(m_drawLock);
    } else {
#if USE(NICOSIA) && USE(TEXTURE_MAPPER_DMABUF)
        if (is<TextureMapperPlatformLayerProxyDMABuf>(m_nicosiaLayer->proxy())) {
            pushDMABufToCompositor();
            return;
        }
#endif

        pushTextureToCompositor();
    }
#endif // USE(TEXTURE_MAPPER)
}

void MediaPlayerPrivateGStreamer::cancelRepaint(bool destroying)
{
    // The goal of this function is to release the GStreamer thread from m_drawCondition in triggerRepaint() in non-AC case,
    // to avoid a deadlock if the player gets paused while waiting for drawing (see https://bugs.webkit.org/show_bug.cgi?id=170003):
    // the main thread is waiting for the GStreamer thread to pause, but the GStreamer thread is locked waiting for the
    // main thread to draw. This deadlock doesn't happen when using AC because the sample is processed (not painted) in the compositor
    // thread, so the main thread can request the pause and wait if the GStreamer thread is waiting for the compositor thread.
    //
    // This function is also used when destroying the player (destroying parameter is true), to release the gstreamer thread from
    // m_drawCondition and to ensure that new triggerRepaint calls won't wait on m_drawCondition.
    if (m_isUsingFallbackVideoSink) {
        Locker locker { m_drawLock };
        m_drawTimer.stop();
        m_isBeingDestroyed = destroying;
        m_drawCondition.notifyOne();
    }
}

void MediaPlayerPrivateGStreamer::repaintCancelledCallback(MediaPlayerPrivateGStreamer* player)
{
    player->cancelRepaint();
}

#if USE(GSTREAMER_GL)
void MediaPlayerPrivateGStreamer::flushCurrentBuffer()
{
    Locker sampleLocker { m_sampleMutex };

    if (m_sample && gst_sample_get_buffer(m_sample.get())) {
        // Allocate a new copy of the sample which has to be released. The copy is necessary so that
        // the video dimensions can still be fetched and also for canvas rendering. The release is
        // necessary because the sample might have been allocated by a hardware decoder and memory
        // might have to be reclaimed by a non-sysmem buffer pool.
        const GstStructure* info = gst_sample_get_info(m_sample.get());
        auto buffer = adoptGRef(gst_buffer_copy_deep(gst_sample_get_buffer(m_sample.get())));
        m_sample = adoptGRef(gst_sample_new(buffer.get(), gst_sample_get_caps(m_sample.get()),
            gst_sample_get_segment(m_sample.get()), info ? gst_structure_copy(info) : nullptr));
    }

    bool shouldWait = m_videoDecoderPlatform == GstVideoDecoderPlatform::Video4Linux;
    auto proxyOperation = [shouldWait, pipeline = pipeline()](TextureMapperPlatformLayerProxyGL& proxy) {
        GST_DEBUG_OBJECT(pipeline, "Flushing video sample %s", shouldWait ? "synchronously" : "");
        if (shouldWait) {
            if (proxy.isActive())
                proxy.dropCurrentBufferWhilePreservingTexture(shouldWait);
        } else {
            Locker locker { proxy.lock() };
            if (proxy.isActive())
                proxy.dropCurrentBufferWhilePreservingTexture(shouldWait);
        }
    };

#if USE(NICOSIA)
    auto& proxy = m_nicosiaLayer->proxy();
    if (is<TextureMapperPlatformLayerProxyGL>(proxy))
        proxyOperation(downcast<TextureMapperPlatformLayerProxyGL>(proxy));
#else
    proxyOperation(*m_platformLayerProxy);
#endif
}
#endif

void MediaPlayerPrivateGStreamer::setVisibleInViewport(bool isVisible)
{
    if (isMediaStreamPlayer())
        return;

    // Some layout tests (webgl) expect playback of invisible videos to not be suspended, so allow
    // this using an environment variable, set from the webkitpy glib port sub-classes.
    const char* allowPlaybackOfInvisibleVideos = g_getenv("WEBKIT_GST_ALLOW_PLAYBACK_OF_INVISIBLE_VIDEOS");
    if (!isVisible && allowPlaybackOfInvisibleVideos && !strcmp(allowPlaybackOfInvisibleVideos, "1"))
        return;

    RefPtr player = m_player.get();

    GST_INFO_OBJECT(m_pipeline.get(), "%s %s player %svisible in viewport", m_isMuted ? "Muted" : "Un-muted", (player && player->isVideoPlayer()) ? "video" : "audio", isVisible ? "" : "no longer ");
    if ((player && !player->isVideoPlayer()) || !m_isMuted)
        return;

    if (!isVisible) {
        GstState currentState;
        gst_element_get_state(m_pipeline.get(), &currentState, nullptr, 0);
        // WebKitMediaSrc cannot properly handle PAUSED -> READY -> PAUSED currently, so we have to avoid transitioning
        // back to READY when the player becomes visible.
        GstState minimumState = isMediaSource() ? GST_STATE_PAUSED : GST_STATE_READY;
        if (currentState >= minimumState)
            m_invisiblePlayerState = currentState;
        m_isVisibleInViewport = false;
        // Avoid setting the pipeline to PAUSED unless the playbin URL has already been set,
        // otherwise it will fail, and may leave the pipeline stuck on READY with PAUSE pending.
        if (!m_url.isValid())
            return;
        [[maybe_unused]] auto setStateResult = gst_element_set_state(m_pipeline.get(), GST_STATE_PAUSED);
        ASSERT(setStateResult != GST_STATE_CHANGE_FAILURE);
    } else {
        m_isVisibleInViewport = true;
        if (m_invisiblePlayerState != GST_STATE_VOID_PENDING)
            changePipelineState(m_invisiblePlayerState);
    }
}

void MediaPlayerPrivateGStreamer::setPresentationSize(const IntSize& size)
{
    m_size = size;
}

void MediaPlayerPrivateGStreamer::paint(GraphicsContext& context, const FloatRect& rect)
{
    if (context.paintingDisabled())
        return;

    if (!m_visible || !m_isVisibleInViewport)
        return;

    Locker sampleLocker { m_sampleMutex };
    if (!GST_IS_SAMPLE(m_sample.get()))
        return;

    auto frame = VideoFrameGStreamer::createWrappedSample(m_sample);
    frame->paintInContext(context, rect, m_videoSourceOrientation, false);
}

DestinationColorSpace MediaPlayerPrivateGStreamer::colorSpace()
{
    return DestinationColorSpace::SRGB();
}

RefPtr<VideoFrame> MediaPlayerPrivateGStreamer::videoFrameForCurrentTime()
{
    Locker sampleLocker { m_sampleMutex };

    if (!GST_IS_SAMPLE(m_sample.get()))
        return nullptr;

    auto frame = VideoFrameGStreamer::createWrappedSample(m_sample);
    auto convertedSample = frame->downloadSample(GST_VIDEO_FORMAT_BGRA);
    if (!convertedSample)
        return nullptr;

    auto size = getVideoResolutionFromCaps(gst_sample_get_caps(m_sample.get())).value_or(FloatSize { 0, 0 });
    return VideoFrameGStreamer::create(WTFMove(convertedSample), size);
}

bool MediaPlayerPrivateGStreamer::setVideoSourceOrientation(ImageOrientation orientation)
{
    if (m_videoSourceOrientation == orientation)
        return false;

    m_videoSourceOrientation = orientation;
#if USE(TEXTURE_MAPPER)
    updateTextureMapperFlags();
#endif
    return true;
}

#if USE(TEXTURE_MAPPER)
void MediaPlayerPrivateGStreamer::updateTextureMapperFlags()
{
    switch (m_videoSourceOrientation.orientation()) {
    case ImageOrientation::Orientation::OriginTopLeft:
        m_textureMapperFlags = { };
        break;
    case ImageOrientation::Orientation::OriginRightTop:
        m_textureMapperFlags = { TextureMapperFlags::ShouldRotateTexture90 };
        break;
    case ImageOrientation::Orientation::OriginBottomRight:
        m_textureMapperFlags = { TextureMapperFlags::ShouldRotateTexture180 };
        break;
    case ImageOrientation::Orientation::OriginLeftBottom:
        m_textureMapperFlags = { TextureMapperFlags::ShouldRotateTexture270 };
        break;
    case ImageOrientation::Orientation::OriginBottomLeft:
        m_textureMapperFlags = { TextureMapperFlags::ShouldFlipTexture };
        break;
    default:
        // FIXME: Handle OriginTopRight, OriginLeftTop and OriginRightBottom.
        m_textureMapperFlags = { };
        break;
    }
}
#endif

bool MediaPlayerPrivateGStreamer::supportsFullscreen() const
{
    return true;
}

MediaPlayer::MovieLoadType MediaPlayerPrivateGStreamer::movieLoadType() const
{
    if (m_readyState == MediaPlayer::ReadyState::HaveNothing)
        return MediaPlayer::MovieLoadType::Unknown;

    if (m_isLiveStream.value_or(false))
        return MediaPlayer::MovieLoadType::LiveStream;

    return MediaPlayer::MovieLoadType::Download;
}

#if USE(TEXTURE_MAPPER_DMABUF)
GstElement* MediaPlayerPrivateGStreamer::createVideoSinkDMABuf()
{
    if (!webKitDMABufVideoSinkIsEnabled())
        return nullptr;
    if (!webKitDMABufVideoSinkProbePlatform()) {
        g_warning("WebKit wasn't able to find the DMABuf video sink dependencies. Hardware-accelerated zero-copy video rendering won't be achievable with this plugin.");
        return nullptr;
    }

    GstElement* sink = gst_element_factory_make("webkitdmabufvideosink", nullptr);
    ASSERT(sink);
    webKitDMABufVideoSinkSetMediaPlayerPrivate(WEBKIT_DMABUF_VIDEO_SINK(sink), this);
    return sink;
}
#endif

#if USE(GSTREAMER_GL)
GstElement* MediaPlayerPrivateGStreamer::createVideoSinkGL()
{
    const char* disableGLSink = g_getenv("WEBKIT_GST_DISABLE_GL_SINK");
    if (disableGLSink && !strcmp(disableGLSink, "1")) {
        GST_INFO("Disabling hardware-accelerated rendering per user request.");
        return nullptr;
    }

    const char* desiredVideoSink = g_getenv("WEBKIT_GST_CUSTOM_VIDEO_SINK");
    if (desiredVideoSink)
        return makeGStreamerElement(desiredVideoSink, nullptr);

    if (!webKitGLVideoSinkProbePlatform()) {
        g_warning("WebKit wasn't able to find the GL video sink dependencies. Hardware-accelerated zero-copy video rendering can't be enabled without this plugin.");
        return nullptr;
    }

    GstElement* sink = gst_element_factory_make("webkitglvideosink", nullptr);
    ASSERT(sink);
    webKitGLVideoSinkSetMediaPlayerPrivate(WEBKIT_GL_VIDEO_SINK(sink), this);

    return sink;
}
#endif // USE(GSTREAMER_GL)

class GStreamerHolePunchClient : public TextureMapperPlatformLayerBuffer::HolePunchClient {
public:
    GStreamerHolePunchClient(GRefPtr<GstElement>&& videoSink, RefPtr<GStreamerQuirksManager>&& quirksManagerForTesting)
        : m_videoSink(WTFMove(videoSink))
        , m_quirksManagerForTesting(WTFMove(quirksManagerForTesting))
    { };
    void setVideoRectangle(const IntRect& rect) final
    {
        if (!m_videoSink)
            return;

        if (m_quirksManagerForTesting) {
            m_quirksManagerForTesting->setHolePunchVideoRectangle(m_videoSink.get(), rect);
            return;
        }

        auto& quirksManager = GStreamerQuirksManager::singleton();
        quirksManager.setHolePunchVideoRectangle(m_videoSink.get(), rect);
    }

private:
    GRefPtr<GstElement> m_videoSink;
    RefPtr<GStreamerQuirksManager> m_quirksManagerForTesting;
};

bool MediaPlayerPrivateGStreamer::isHolePunchRenderingEnabled() const
{
    if (m_quirksManagerForTesting)
        return m_quirksManagerForTesting->supportsVideoHolePunchRendering();

    auto& quirksManager = GStreamerQuirksManager::singleton();
    return quirksManager.supportsVideoHolePunchRendering();
}

GstElement* MediaPlayerPrivateGStreamer::createHolePunchVideoSink()
{
    if (!isHolePunchRenderingEnabled())
        return nullptr;

    auto player = m_player.get();
    if (!player)
        return nullptr;

    GstElement* sink = nullptr;
    if (m_quirksManagerForTesting)
        sink = m_quirksManagerForTesting->createHolePunchVideoSink(m_isLegacyPlaybin, player.get());
    else
        sink = GStreamerQuirksManager::singleton().createHolePunchVideoSink(m_isLegacyPlaybin, player.get());

    // Configure sink before it allocates resources.
    if (sink)
        configureElement(sink);
    return sink;
}

void MediaPlayerPrivateGStreamer::pushNextHolePunchBuffer()
{
    auto proxyOperation =
        [this](TextureMapperPlatformLayerProxyGL& proxy)
        {
            Locker locker { proxy.lock() };
            std::unique_ptr<TextureMapperPlatformLayerBuffer> layerBuffer = makeUnique<TextureMapperPlatformLayerBuffer>(0, m_size, TextureMapperFlags::ShouldNotBlend, GL_DONT_CARE);
            std::unique_ptr<GStreamerHolePunchClient> holePunchClient = makeUnique<GStreamerHolePunchClient>(m_videoSink.get(), RefPtr { m_quirksManagerForTesting });
            layerBuffer->setHolePunchClient(WTFMove(holePunchClient));
            proxy.pushNextBuffer(WTFMove(layerBuffer));
        };

    ASSERT(isHolePunchRenderingEnabled());
#if USE(NICOSIA)
    auto& proxy = m_nicosiaLayer->proxy();
    ASSERT(is<TextureMapperPlatformLayerProxyGL>(proxy));
    proxyOperation(downcast<TextureMapperPlatformLayerProxyGL>(proxy));
#else
    proxyOperation(*m_platformLayerProxy);
#endif
}

bool MediaPlayerPrivateGStreamer::shouldIgnoreIntrinsicSize()
{
    return isHolePunchRenderingEnabled();
}

GstElement* MediaPlayerPrivateGStreamer::createVideoSink()
{
    acceleratedRenderingStateChanged();

    // Ensure the sink has the max-lateness property set.
    auto exit = makeScopeExit([this] {
        if (!m_videoSink || isMediaStreamPlayer())
            return;

        GstElement* sink = m_videoSink.get();
        while (GST_IS_BIN(sink)) {
            GUniquePtr<GstIterator> iter(gst_bin_iterate_sinks(GST_BIN_CAST(sink)));
            GValue value = G_VALUE_INIT;
            auto result = gst_iterator_next(iter.get(), &value);
            ASSERT_UNUSED(result, result == GST_ITERATOR_OK);
            sink = GST_ELEMENT(g_value_get_object(&value));
            g_value_unset(&value);
        }

        uint64_t maxLateness = 100 * GST_MSECOND;
        g_object_set(sink, "max-lateness", maxLateness, nullptr);
    });

    RefPtr player = m_player.get();
    if (player && !player->isVideoPlayer()) {
        m_videoSink = makeGStreamerElement("fakevideosink", nullptr);
        if (!m_videoSink) {
            GST_DEBUG_OBJECT(m_pipeline.get(), "Falling back to fakesink for video rendering");
            m_videoSink = gst_element_factory_make("fakesink", nullptr);
            g_object_set(m_videoSink.get(), "sync", TRUE, nullptr);
        }
        player->renderingModeChanged();

        return m_videoSink.get();
    }

    if (isHolePunchRenderingEnabled()) {
        m_videoSink = createHolePunchVideoSink();
        // Do not check the m_videoSink value. The nullptr case will trigger auto-plugging in playbin.
        pushNextHolePunchBuffer();
        return m_videoSink.get();
    }

#if USE(TEXTURE_MAPPER_DMABUF)
    if (!m_videoSink && m_canRenderingBeAccelerated)
        m_videoSink = createVideoSinkDMABuf();
#endif
#if USE(GSTREAMER_GL)
    if (!m_videoSink && m_canRenderingBeAccelerated)
        m_videoSink = createVideoSinkGL();
#endif

    if (!m_videoSink) {
        m_isUsingFallbackVideoSink = true;
        m_videoSink = webkitVideoSinkNew();
        g_signal_connect_swapped(m_videoSink.get(), "repaint-requested", G_CALLBACK(+[](MediaPlayerPrivateGStreamer* player, GstSample* sample) {
            player->triggerRepaint(GRefPtr<GstSample>(sample));
        }), this);
        g_signal_connect_swapped(m_videoSink.get(), "repaint-cancelled", G_CALLBACK(repaintCancelledCallback), this);
    }

    return m_videoSink.get();
}

void MediaPlayerPrivateGStreamer::setStreamVolumeElement(GstStreamVolume* volume)
{
    RefPtr player = m_player.get();
    if (!player)
        return;

    ASSERT(!m_volumeElement);
    m_volumeElement = volume;

    // We don't set the initial volume because we trust the sink to keep it for us. See
    // https://bugs.webkit.org/show_bug.cgi?id=118974 for more information.
    if (!player->platformVolumeConfigurationRequired()) {
        GST_DEBUG_OBJECT(pipeline(), "Setting stream volume to %f", player->volume());
        gst_stream_volume_set_volume(m_volumeElement.get(), GST_STREAM_VOLUME_FORMAT_LINEAR, static_cast<double>(player->volume()));
    } else
        GST_DEBUG_OBJECT(pipeline(), "Not setting stream volume, trusting system one");

    m_isMuted = player->muted();
    GST_DEBUG_OBJECT(pipeline(), "Setting stream muted %s", boolForPrinting(m_isMuted));
    g_object_set(m_volumeElement.get(), "mute", static_cast<gboolean>(m_isMuted), nullptr);

    g_signal_connect_swapped(m_volumeElement.get(), "notify::volume", G_CALLBACK(volumeChangedCallback), this);
    g_signal_connect_swapped(m_volumeElement.get(), "notify::mute", G_CALLBACK(muteChangedCallback), this);
}

bool MediaPlayerPrivateGStreamer::updateVideoSinkStatistics()
{
    if (!m_videoSink)
        return false;

    uint64_t totalVideoFrames = 0;
    uint64_t droppedVideoFrames = 0;
    GUniqueOutPtr<GstStructure> stats;
    g_object_get(m_videoSink.get(), "stats", &stats.outPtr(), nullptr);

    if (!gst_structure_get_uint64(stats.get(), "rendered", &totalVideoFrames))
        return false;

    if (!gst_structure_get_uint64(stats.get(), "dropped", &droppedVideoFrames))
        return false;

    // Caching is required so that metrics queries performed after EOS still return valid values.
    if (totalVideoFrames)
        m_totalVideoFrames = totalVideoFrames;
    if (droppedVideoFrames)
        m_droppedVideoFrames = droppedVideoFrames;
    return true;
}

std::optional<VideoPlaybackQualityMetrics> MediaPlayerPrivateGStreamer::videoPlaybackQualityMetrics()
{
    if (!updateVideoSinkStatistics())
        return std::nullopt;

    uint32_t corruptedVideoFrames = 0;
    double totalFrameDelay = 0;
    uint32_t displayCompositedVideoFrames = 0;
    return VideoPlaybackQualityMetrics {
        static_cast<uint32_t>(m_totalVideoFrames),
        static_cast<uint32_t>(m_droppedVideoFrames),
        corruptedVideoFrames,
        totalFrameDelay,
        displayCompositedVideoFrames,
    };
}

unsigned MediaPlayerPrivateGStreamer::decodedFrameCount() const
{
    return const_cast<MediaPlayerPrivateGStreamer*>(this)->videoPlaybackQualityMetrics().value_or(VideoPlaybackQualityMetrics()).totalVideoFrames;
}

unsigned MediaPlayerPrivateGStreamer::droppedFrameCount() const
{
    return const_cast<MediaPlayerPrivateGStreamer*>(this)->videoPlaybackQualityMetrics().value_or(VideoPlaybackQualityMetrics()).droppedVideoFrames;
}

#if ENABLE(ENCRYPTED_MEDIA)
InitData MediaPlayerPrivateGStreamer::parseInitDataFromProtectionMessage(GstMessage* message)
{
    ASSERT(!isMainThread());

    Locker locker { m_protectionMutex };
    ProtectionSystemEvents protectionSystemEvents(message);
    GST_TRACE_OBJECT(pipeline(), "found %zu protection events, %zu decryptors available", protectionSystemEvents.events().size(), protectionSystemEvents.availableSystems().size());

    String systemId;
    SharedBufferBuilder payloadBuilder;
    for (auto& event : protectionSystemEvents.events()) {
        const char* eventKeySystemId = nullptr;
        GstBuffer* data = nullptr;
        gst_event_parse_protection(event.get(), &eventKeySystemId, &data, nullptr);

        // FIXME: There is some confusion here about how to detect the
        // correct "initialization data type", if the system ID is
        // GST_PROTECTION_UNSPECIFIED_SYSTEM_ID, then we know it came
        // from WebM. If the system id is specified with one of the
        // defined ClearKey / Playready / Widevine / etc UUIDs, then
        // we know it's MP4. For the latter case, it does not matter
        // which of the UUIDs it is, so we just overwrite it. This is
        // a quirk of how GStreamer provides protection events, and
        // it's not very robust, so be careful here!
        systemId = GStreamerEMEUtilities::uuidToKeySystem(String::fromLatin1(eventKeySystemId));
        InitData initData { systemId, data };
        payloadBuilder.append(*initData.payload());
        m_handledProtectionEvents.add(GST_EVENT_SEQNUM(event.get()));
    }

    return { systemId, payloadBuilder.takeAsContiguous() };
}

bool MediaPlayerPrivateGStreamer::waitForCDMAttachment()
{
    if (isMainThread()) {
        GST_ERROR_OBJECT(pipeline(), "can't block the main thread waiting for a CDM instance");
        ASSERT_NOT_REACHED();
        return false;
    }

    GST_INFO_OBJECT(pipeline(), "waiting for a CDM instance");

    bool didCDMAttach = false;
    {
        Locker cdmAttachmentLocker { m_cdmAttachmentLock };
        didCDMAttach = m_cdmAttachmentCondition.waitFor(m_cdmAttachmentLock, 4_s, [this]() {
            assertIsHeld(m_cdmAttachmentLock);
            return isCDMAttached();
        });
    }

    return didCDMAttach;
}

void MediaPlayerPrivateGStreamer::initializationDataEncountered(InitData&& initData)
{
    if (!initData.payload()) {
        GST_DEBUG("initializationDataEncountered No payload");
        return;
    }

    RunLoop::main().dispatch([weakThis = ThreadSafeWeakPtr { *this }, initData = WTFMove(initData)] {
        RefPtr self = weakThis.get();
        if (!self)
            return;
        RefPtr player = self->m_player.get();

        GST_DEBUG("scheduling initializationDataEncountered %s event of size %zu", initData.payloadContainerType().utf8().data(),
            initData.payload()->size());
        GST_MEMDUMP("init datas", reinterpret_cast<const uint8_t*>(initData.payload()->makeContiguous()->data()), initData.payload()->size());
        if (player)
            player->initializationDataEncountered(initData.payloadContainerType(), initData.payload()->tryCreateArrayBuffer());
    });
}

void MediaPlayerPrivateGStreamer::cdmInstanceAttached(CDMInstance& instance)
{
    ASSERT(isMainThread());

    if (m_cdmInstance == &instance)
        return;

    if (!m_pipeline) {
        GST_ERROR("no pipeline yet");
        ASSERT_NOT_REACHED();
        return;
    }

    m_cdmInstance = reinterpret_cast<CDMInstanceProxy*>(&instance);
    RELEASE_ASSERT(m_cdmInstance);
    m_cdmInstance->setPlayer(m_player.get());

    GRefPtr<GstContext> context = adoptGRef(gst_context_new("drm-cdm-proxy", FALSE));
    GstStructure* contextStructure = gst_context_writable_structure(context.get());
    gst_structure_set(contextStructure, "cdm-proxy", G_TYPE_POINTER, m_cdmInstance->proxy().get(), nullptr);
    gst_element_set_context(GST_ELEMENT(m_pipeline.get()), context.get());

    GST_DEBUG_OBJECT(m_pipeline.get(), "CDM proxy instance %p dispatched as context", m_cdmInstance->proxy().get());

    Locker cdmAttachmentLocker { m_cdmAttachmentLock };
    // We must notify all waiters, since several demuxers can be simultaneously waiting for a CDM.
    m_cdmAttachmentCondition.notifyAll();
}

void MediaPlayerPrivateGStreamer::cdmInstanceDetached(CDMInstance& instance)
{
    UNUSED_PARAM(instance);
    ASSERT(isMainThread());
    ASSERT(m_pipeline);

    if (!m_cdmInstance)
        return;

    ASSERT(m_cdmInstance == &instance);
    GST_DEBUG_OBJECT(m_pipeline.get(), "detaching CDM instance %p, setting empty context", m_cdmInstance.get());
    m_cdmInstance = nullptr;
    GRefPtr<GstContext> context = adoptGRef(gst_context_new("drm-cdm-proxy", FALSE));
    gst_element_set_context(GST_ELEMENT(m_pipeline.get()), context.get());
}

void MediaPlayerPrivateGStreamer::attemptToDecryptWithInstance(CDMInstance& instance)
{
    ASSERT(m_cdmInstance == &instance);
    GST_TRACE("instance %p, current stored %p", &instance, m_cdmInstance.get());
    attemptToDecryptWithLocalInstance();
}

void MediaPlayerPrivateGStreamer::attemptToDecryptWithLocalInstance()
{
    bool wasEventHandled = gst_element_send_event(pipeline(), gst_event_new_custom(GST_EVENT_CUSTOM_DOWNSTREAM_OOB, gst_structure_new_empty("attempt-to-decrypt")));
    GST_DEBUG("attempting to decrypt, event handled %s", boolForPrinting(wasEventHandled));
}

void MediaPlayerPrivateGStreamer::handleProtectionEvent(GstEvent* event)
{
    {
        Locker locker { m_protectionMutex };
        if (m_handledProtectionEvents.contains(GST_EVENT_SEQNUM(event))) {
            GST_DEBUG_OBJECT(pipeline(), "event %u already handled", GST_EVENT_SEQNUM(event));
            return;
        }
    }
    GST_DEBUG_OBJECT(pipeline(), "handling event %u from MSE", GST_EVENT_SEQNUM(event));
    const char* eventKeySystemUUID = nullptr;
    GstBuffer* initData = nullptr;
    gst_event_parse_protection(event, &eventKeySystemUUID, &initData, nullptr);
    initializationDataEncountered({ GStreamerEMEUtilities::uuidToKeySystem(String::fromLatin1(eventKeySystemUUID)), initData });
}

bool MediaPlayerPrivateGStreamer::waitingForKey() const
{
    if (!m_pipeline || !m_cdmInstance)
        return false;

    return m_cdmInstance->isWaitingForKey();
}
#endif

bool MediaPlayerPrivateGStreamer::supportsKeySystem(const String& keySystem, const String& mimeType)
{
    bool result = false;

#if ENABLE(ENCRYPTED_MEDIA)
    result = GStreamerEMEUtilities::isClearKeyKeySystem(keySystem);
#endif

    GST_DEBUG("checking for KeySystem support with %s and type %s: %s", keySystem.utf8().data(), mimeType.utf8().data(), boolForPrinting(result));
    return result;
}

#if !RELEASE_LOG_DISABLED
WTFLogChannel& MediaPlayerPrivateGStreamer::logChannel() const
{
    return WebCore::LogMedia;
}
#endif

std::optional<VideoFrameMetadata> MediaPlayerPrivateGStreamer::videoFrameMetadata()
{
    if (m_sampleCount == m_lastVideoFrameMetadataSampleCount)
        return { };

    m_lastVideoFrameMetadataSampleCount = m_sampleCount;

    Locker sampleLocker { m_sampleMutex };
    if (!GST_IS_SAMPLE(m_sample.get()))
        return { };

    auto* buffer = gst_sample_get_buffer(m_sample.get());
    auto metadata = webkitGstBufferGetVideoFrameMetadata(buffer);
    auto size = naturalSize();
    metadata.width = size.width();
    metadata.height = size.height();
    metadata.presentedFrames = m_sampleCount;

    // FIXME: presentationTime and expectedDisplayTime might not always have the same value, we should try getting more precise values.
    metadata.presentationTime = MonotonicTime::now().secondsSinceEpoch().seconds();
    metadata.expectedDisplayTime = metadata.presentationTime;

    return metadata;
}

static bool areAllSinksPlayingForBin(GstBin* bin)
{
    for (auto* element : GstIteratorAdaptor<GstElement>(GUniquePtr<GstIterator>(gst_bin_iterate_sinks(bin)))) {
        if (GST_IS_BIN(element) && !areAllSinksPlayingForBin(GST_BIN_CAST(element)))
            return false;

        GstState state, pending;
        gst_element_get_state(element, &state, &pending, 0);
        if (state != GST_STATE_PLAYING && pending != GST_STATE_PLAYING)
            return false;
    }
    return true;
}

void MediaPlayerPrivateGStreamer::checkPlayingConsistency()
{
    if (!pipeline())
        return;

    GstState state, pending;
    gst_element_get_state(pipeline(), &state, &pending, 0);
    if (state == GST_STATE_PLAYING && pending == GST_STATE_VOID_PENDING) {
        if (!areAllSinksPlayingForBin(GST_BIN(pipeline()))) {
            if (!m_didTryToRecoverPlayingState) {
                GST_WARNING_OBJECT(pipeline(), "Playbin is in PLAYING state but some sinks aren't, trying to recover.");
                ASSERT_NOT_REACHED_WITH_MESSAGE("Playbin is in PLAYING state but some sinks aren't. This should not happen.");
                m_didTryToRecoverPlayingState = true;
                gst_element_set_state(pipeline(), GST_STATE_PAUSED);
                gst_element_set_state(pipeline(), GST_STATE_PLAYING);
            }
        } else
            m_didTryToRecoverPlayingState = false;
    }
}

String MediaPlayerPrivateGStreamer::codecForStreamId(const String& streamId)
{
    Locker locker { m_codecsLock };
    if (UNLIKELY(!m_codecs.contains(streamId)))
        return emptyString();

    return m_codecs.get(streamId);
}

#undef GST_CAT_DEFAULT

} // namespace WebCore

#endif //  ENABLE(VIDEO) && USE(GSTREAMER)
