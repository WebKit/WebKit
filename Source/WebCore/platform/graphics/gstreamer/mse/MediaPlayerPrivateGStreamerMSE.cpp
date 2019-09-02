/*
 * Copyright (C) 2007, 2009 Apple Inc.  All rights reserved.
 * Copyright (C) 2007 Collabora Ltd.  All rights reserved.
 * Copyright (C) 2007 Alp Toker <alp@atoker.com>
 * Copyright (C) 2009 Gustavo Noronha Silva <gns@gnome.org>
 * Copyright (C) 2009, 2010, 2011, 2012, 2013, 2016, 2017, 2018, 2019 Igalia S.L
 * Copyright (C) 2015 Sebastian Dröge <sebastian@centricular.com>
 * Copyright (C) 2015, 2016, 2017, 2018, 2019 Metrological Group B.V.
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
#include "NotImplemented.h"
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
    case WebCore::MediaPlayer::HaveNothing: return "HaveNothing";
    case WebCore::MediaPlayer::HaveMetadata: return "HaveMetadata";
    case WebCore::MediaPlayer::HaveCurrentData: return "HaveCurrentData";
    case WebCore::MediaPlayer::HaveFutureData: return "HaveFutureData";
    case WebCore::MediaPlayer::HaveEnoughData: return "HaveEnoughData";
    default: return "(unknown)";
    }
}

GST_DEBUG_CATEGORY(webkit_mse_debug);
#define GST_CAT_DEFAULT webkit_mse_debug

namespace WebCore {

void MediaPlayerPrivateGStreamerMSE::registerMediaEngine(MediaEngineRegistrar registrar)
{
    initializeGStreamerAndRegisterWebKitElements();
    GST_DEBUG_CATEGORY_INIT(webkit_mse_debug, "webkitmse", 0, "WebKit MSE media player");
    if (isAvailable()) {
        registrar([](MediaPlayer* player) { return makeUnique<MediaPlayerPrivateGStreamerMSE>(player); },
            getSupportedTypes, supportsType, nullptr, nullptr, nullptr, supportsKeySystem);
    }
}

MediaPlayerPrivateGStreamerMSE::MediaPlayerPrivateGStreamerMSE(MediaPlayer* player)
    : MediaPlayerPrivateGStreamer(player)
{
    GST_TRACE("creating the player (%p)", this);
}

MediaPlayerPrivateGStreamerMSE::~MediaPlayerPrivateGStreamerMSE()
{
    GST_TRACE("destroying the player (%p)", this);

    // Clear the AppendPipeline map. This should cause the destruction of all the AppendPipeline's since there should
    // be no alive references at this point.
#ifndef NDEBUG
    for (auto iterator : m_appendPipelinesMap)
        ASSERT(iterator.value->hasOneRef());
#endif
    m_appendPipelinesMap.clear();

    m_source.clear();
}

void MediaPlayerPrivateGStreamerMSE::load(const String& urlString)
{
    if (!urlString.startsWith("mediasource")) {
        // Properly fail so the global MediaPlayer tries to fallback to the next MediaPlayerPrivate.
        m_networkState = MediaPlayer::FormatError;
        m_player->networkStateChanged();
        return;
    }

    MediaPlayerPrivateGStreamer::load(urlString);
}

void MediaPlayerPrivateGStreamerMSE::load(const String& url, MediaSourcePrivateClient* mediaSource)
{
    m_mediaSource = mediaSource;
    load(makeString("mediasource", url));
}

void MediaPlayerPrivateGStreamerMSE::play()
{
    GST_DEBUG_OBJECT(pipeline(), "Play requested");
    m_paused = false;
    updateStates();
}

void MediaPlayerPrivateGStreamerMSE::pause()
{
    GST_DEBUG_OBJECT(pipeline(), "Pause requested");
    m_paused = true;
    updateStates();
}

MediaTime MediaPlayerPrivateGStreamerMSE::durationMediaTime() const
{
    if (UNLIKELY(!m_pipeline || m_errorOccured))
        return MediaTime();

    return m_mediaTimeDuration;
}

void MediaPlayerPrivateGStreamerMSE::seek(const MediaTime& time)
{
    GST_DEBUG_OBJECT(pipeline(), "Seeking to %s", time.toString().utf8().data());
    m_seekTime = time;
    m_seeking = true;
    m_isEndReached = false;

    webKitMediaSrcSeek(WEBKIT_MEDIA_SRC(m_source.get()), toGstClockTime(m_seekTime), m_playbackRate);

    invalidateCachedPosition();
    m_canFallBackToLastFinishedSeekPosition = true;

    // Notify MediaSource and have new frames enqueued (when they're available).
    m_mediaSource->seekToTime(time);
}

void MediaPlayerPrivateGStreamerMSE::reportSeekCompleted()
{
    m_seeking = false;
    m_player->timeChanged();
}

void MediaPlayerPrivateGStreamerMSE::setReadyState(MediaPlayer::ReadyState readyState)
{
    if (readyState == m_readyState)
        return;

    GST_DEBUG("MediaPlayerPrivateGStreamerMSE::setReadyState(%p): %s -> %s", this, dumpReadyState(m_readyState), dumpReadyState(readyState));
    m_readyState = readyState;
    updateStates();

    // Both readyStateChanged() and timeChanged() check for "seeked" condition, which requires all the following three things:
    //   1. HTMLMediaPlayer.m_seekRequested == true.
    //   2. Our seeking() method to return false (that is, we have completed the seek).
    //   3. readyState > HaveMetadata.
    //
    // We normally would set m_seeking = false in seekCompleted(), but unfortunately by that time, playback has already
    // started which means that the "playing" event is emitted before "seeked". In order to avoid that wrong order,
    // we do it here already.
    if (m_seeking && readyState > MediaPlayer::ReadyState::HaveMetadata)
        m_seeking = false;
    m_player->readyStateChanged();

    // The readyState change may be a result of monitorSourceBuffers() finding that currentTime == duration, which
    // should cause the video to be marked as ended. Let's have the player check that.
    m_player->timeChanged();
}

void MediaPlayerPrivateGStreamerMSE::setRate(float)
{
    notImplemented();
}

std::unique_ptr<PlatformTimeRanges> MediaPlayerPrivateGStreamerMSE::buffered() const
{
    return m_mediaSource ? m_mediaSource->buffered() : makeUnique<PlatformTimeRanges>();
}

void MediaPlayerPrivateGStreamerMSE::sourceSetup(GstElement* sourceElement)
{
    m_source = sourceElement;
    ASSERT(WEBKIT_IS_MEDIA_SRC(m_source.get()));
    MediaSourceGStreamer::open(*m_mediaSource.get(), *this);
}

void MediaPlayerPrivateGStreamerMSE::updateStates()
{
    bool shouldBePlaying = !m_paused && readyState() >= MediaPlayer::ReadyState::HaveFutureData;
    GST_DEBUG_OBJECT(pipeline(), "shouldBePlaying = %d, m_isPipelinePlaying = %d", static_cast<int>(shouldBePlaying), static_cast<int>(m_isPipelinePlaying));
    if (shouldBePlaying && !m_isPipelinePlaying) {
        if (!changePipelineState(GST_STATE_PLAYING))
            GST_ERROR_OBJECT(pipeline(), "Setting the pipeline to PLAYING failed");
        m_isPipelinePlaying = true;
    } else if (!shouldBePlaying && m_isPipelinePlaying) {
        if (!changePipelineState(GST_STATE_PAUSED))
            GST_ERROR_OBJECT(pipeline(), "Setting the pipeline to PAUSED failed");
        m_isPipelinePlaying = false;
    }
}

void MediaPlayerPrivateGStreamerMSE::didEnd()
{
    GST_DEBUG_OBJECT(pipeline(), "EOS received, currentTime=%s duration=%s", currentMediaTime().toString().utf8().data(), durationMediaTime().toString().utf8().data());
    m_isEndReached = true;
    invalidateCachedPosition();
    // HTMLMediaElement will emit ended if currentTime >= duration (which should now be the case).
    ASSERT(currentMediaTime() == durationMediaTime());
    m_player->timeChanged();
}

bool MediaPlayerPrivateGStreamerMSE::isTimeBuffered(const MediaTime &time) const
{
    bool result = m_mediaSource && m_mediaSource->buffered()->contain(time);
    GST_DEBUG("Time %s buffered? %s", toString(time).utf8().data(), boolForPrinting(result));
    return result;
}

void MediaPlayerPrivateGStreamerMSE::setMediaSourceClient(Ref<MediaSourceClientGStreamerMSE> client)
{
    m_mediaSourceClient = client.ptr();
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
        m_shouldReportDurationWhenUnblocking = false;
    }

    m_areDurationChangesBlocked = false;
}

void MediaPlayerPrivateGStreamerMSE::durationChanged()
{
    ASSERT(isMainThread());
    if (!m_mediaSourceClient) {
        GST_DEBUG("m_mediaSourceClient is null, doing nothing");
        return;
    }

    MediaTime previousDuration = m_mediaTimeDuration;
    m_mediaTimeDuration = m_mediaSourceClient->duration();

    GST_TRACE("previous=%s, new=%s", toString(previousDuration).utf8().data(), toString(m_mediaTimeDuration).utf8().data());

    // Avoid emiting durationchanged in the case where the previous duration was 0 because that case is already handled
    // by the HTMLMediaElement.
    if (m_mediaTimeDuration != previousDuration && m_mediaTimeDuration.isValid() && previousDuration.isValid()) {
        if (!m_areDurationChangesBlocked)
            m_player->durationChanged();
        else
            m_shouldReportDurationWhenUnblocking = true;
        m_mediaSource->durationChanged(m_mediaTimeDuration);
    }
}

void MediaPlayerPrivateGStreamerMSE::trackDetected(RefPtr<AppendPipeline> appendPipeline, RefPtr<WebCore::TrackPrivateBase> newTrack, bool firstTrackDetected)
{
    ASSERT(appendPipeline->track() == newTrack);

    GRefPtr<GstCaps> caps = appendPipeline->appsinkCaps();
    ASSERT(caps);
    GST_DEBUG("track ID: %s, caps: %" GST_PTR_FORMAT, newTrack->id().string().latin1().data(), caps.get());

    if (doCapsHaveType(caps.get(), GST_VIDEO_CAPS_TYPE_PREFIX)) {
        Optional<FloatSize> size = getVideoResolutionFromCaps(caps.get());
        if (size.hasValue())
            m_videoSize = size.value();
    }

    if (firstTrackDetected)
        webKitMediaSrcAddStream(WEBKIT_MEDIA_SRC(m_source.get()), newTrack->id(), appendPipeline->streamType(), WTFMove(caps));
}

void MediaPlayerPrivateGStreamerMSE::getSupportedTypes(HashSet<String, ASCIICaseInsensitiveHash>& types)
{
    auto& gstRegistryScanner = GStreamerRegistryScannerMSE::singleton();
    types = gstRegistryScanner.mimeTypeSet();
}

MediaPlayer::SupportsType MediaPlayerPrivateGStreamerMSE::supportsType(const MediaEngineSupportParameters& parameters)
{
    MediaPlayer::SupportsType result = MediaPlayer::IsNotSupported;
    if (!parameters.isMediaSource)
        return result;

    auto containerType = parameters.type.containerType();

    // YouTube TV provides empty types for some videos and we want to be selected as best media engine for them.
    if (containerType.isEmpty()) {
        result = MediaPlayer::MayBeSupported;
        GST_DEBUG("mime-type \"%s\" supported: %s", parameters.type.raw().utf8().data(), convertEnumerationToString(result).utf8().data());
        return result;
    }

    GST_DEBUG("Checking mime-type \"%s\"", parameters.type.raw().utf8().data());
    auto& gstRegistryScanner = GStreamerRegistryScannerMSE::singleton();
    // Spec says we should not return "probably" if the codecs string is empty.
    if (gstRegistryScanner.isContainerTypeSupported(containerType)) {
        Vector<String> codecs = parameters.type.codecs();
        result = codecs.isEmpty() ? MediaPlayer::MayBeSupported : (gstRegistryScanner.areAllCodecsSupported(codecs) ? MediaPlayer::IsSupported : MediaPlayer::IsNotSupported);
    }

    auto finalResult = extendedSupportsType(parameters, result);
    GST_DEBUG("Supported: %s", convertEnumerationToString(finalResult).utf8().data());
    return finalResult;
}

MediaTime MediaPlayerPrivateGStreamerMSE::maxMediaTimeSeekable() const
{
    if (UNLIKELY(m_errorOccured))
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
