/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 * Copyright (C) 2013 Orange
 * Copyright (C) 2014 Sebastian Dröge <sebastian@centricular.com>
 * Copyright (C) 2015, 2016 Metrological Group B.V.
 * Copyright (C) 2015, 2016 Igalia, S.L
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "MediaSourcePrivateGStreamer.h"

#if ENABLE(MEDIA_SOURCE) && USE(GSTREAMER)

#include "ContentType.h"
#include "Logging.h"
#include "MediaPlayerPrivateGStreamer.h"
#include "MediaPlayerPrivateGStreamerMSE.h"
#include "MediaSourceTrackGStreamer.h"
#include "NotImplemented.h"
#include "SourceBufferPrivateGStreamer.h"
#include "TimeRanges.h"
#include "WebKitMediaSourceGStreamer.h"
#include <wtf/NativePromise.h>
#include <wtf/RefPtr.h>
#include <wtf/glib/GRefPtr.h>

GST_DEBUG_CATEGORY_STATIC(webkit_mse_private_debug);
#define GST_CAT_DEFAULT webkit_mse_private_debug

namespace WebCore {

Ref<MediaSourcePrivateGStreamer> MediaSourcePrivateGStreamer::open(MediaSourcePrivateClient& mediaSource, MediaPlayerPrivateGStreamerMSE& playerPrivate)
{
    auto mediaSourcePrivate = adoptRef(*new MediaSourcePrivateGStreamer(mediaSource, playerPrivate));
    mediaSource.setPrivateAndOpen(mediaSourcePrivate.copyRef());
    return mediaSourcePrivate;
}

MediaSourcePrivateGStreamer::MediaSourcePrivateGStreamer(MediaSourcePrivateClient& mediaSource, MediaPlayerPrivateGStreamerMSE& playerPrivate)
    : MediaSourcePrivate(mediaSource)
    , m_playerPrivate(playerPrivate)
#if !RELEASE_LOG_DISABLED
    , m_logger(playerPrivate.mediaPlayerLogger())
    , m_logIdentifier(playerPrivate.mediaPlayerLogIdentifier())
#endif
{
    static std::once_flag debugRegisteredFlag;
    std::call_once(debugRegisteredFlag, [] {
        GST_DEBUG_CATEGORY_INIT(webkit_mse_private_debug, "webkitmseprivate", 0, "WebKit MSE Private");
    });
}

MediaSourcePrivateGStreamer::~MediaSourcePrivateGStreamer()
{
    ALWAYS_LOG(LOGIDENTIFIER);
}

MediaSourcePrivateGStreamer::AddStatus MediaSourcePrivateGStreamer::addSourceBuffer(const ContentType& contentType, const MediaSourceConfiguration&, RefPtr<SourceBufferPrivate>& sourceBufferPrivate)
{
    DEBUG_LOG(LOGIDENTIFIER, contentType);

    // Once every SourceBuffer has had an initialization segment appended playback starts and it's too late to add new SourceBuffers.
    if (m_hasAllTracks)
        return MediaSourcePrivateGStreamer::AddStatus::ReachedIdLimit;

    if (!SourceBufferPrivateGStreamer::isContentTypeSupported(contentType))
        return MediaSourcePrivateGStreamer::AddStatus::NotSupported;

    m_sourceBuffers.append(SourceBufferPrivateGStreamer::create(*this, contentType));
    sourceBufferPrivate = m_sourceBuffers.last();
    sourceBufferPrivate->setMediaSourceDuration(duration());
    return MediaSourcePrivateGStreamer::AddStatus::Ok;
}

RefPtr<MediaPlayerPrivateInterface> MediaSourcePrivateGStreamer::player() const
{
    return m_playerPrivate.get();
}

void MediaSourcePrivateGStreamer::setPlayer(MediaPlayerPrivateInterface* player)
{
    m_playerPrivate = downcast<MediaPlayerPrivateGStreamerMSE>(player);
}

RefPtr<MediaPlayerPrivateGStreamerMSE> MediaSourcePrivateGStreamer::platformPlayer() const
{
    return m_playerPrivate.get();
}

void MediaSourcePrivateGStreamer::durationChanged(const MediaTime& duration)
{
    ASSERT(isMainThread());

    RefPtr player = platformPlayer();
    if (!player)
        return;
    MediaSourcePrivate::durationChanged(duration);
    GST_TRACE_OBJECT(player->pipeline(), "Duration: %" GST_TIME_FORMAT, GST_TIME_ARGS(toGstClockTime(duration)));
    if (!duration.isValid() || duration.isNegativeInfinite())
        return;

    player->durationChanged();
}

void MediaSourcePrivateGStreamer::markEndOfStream(EndOfStreamStatus endOfStreamStatus)
{
    ASSERT(isMainThread());

    MediaSourcePrivate::markEndOfStream(endOfStreamStatus);
    RefPtr player = platformPlayer();
    if (!player)
        return;

#ifndef GST_DISABLE_GST_DEBUG
    const char* statusString = nullptr;
    switch (endOfStreamStatus) {
    case EndOfStreamStatus::NoError:
        statusString = "no-error";
        break;
    case EndOfStreamStatus::DecodeError:
        statusString = "decode-error";
        break;
    case EndOfStreamStatus::NetworkError:
        statusString = "network-error";
        break;
    }
    GST_DEBUG_OBJECT(player->pipeline(), "Marking EOS, status is %s", statusString);
#endif
    if (endOfStreamStatus == EndOfStreamStatus::NoError) {
        auto bufferedRanges = buffered();
        if (!bufferedRanges.length()) {
            GST_DEBUG("EOS with no buffers");
            player->setEosWithNoBuffers(true);
        }
    }
}

void MediaSourcePrivateGStreamer::unmarkEndOfStream()
{
    ASSERT(isMainThread());
    RefPtr player = platformPlayer();
    if (!player)
        return;

    player->setEosWithNoBuffers(false);
    MediaSourcePrivate::unmarkEndOfStream();
}

void MediaSourcePrivateGStreamer::startPlaybackIfHasAllTracks()
{
    RefPtr player = platformPlayer();
    if (!player)
        return;

    if (m_hasAllTracks) {
        // Already started, nothing to do.
        return;
    }

    for (auto& sourceBuffer : m_sourceBuffers) {
        if (!sourceBuffer->hasReceivedFirstInitializationSegment()) {
            GST_DEBUG_OBJECT(player->pipeline(), "There are still SourceBuffers without an initialization segment, not starting source yet.");
            return;
        }
    }

    GST_DEBUG_OBJECT(player->pipeline(), "All SourceBuffers have an initialization segment, starting source.");
    m_hasAllTracks = true;

    Vector<RefPtr<MediaSourceTrackGStreamer>> tracks;
    for (auto& privateSourceBuffer : m_sourceBuffers) {
        auto sourceBuffer = downcast<SourceBufferPrivateGStreamer>(privateSourceBuffer);
        for (auto& [_, track] : sourceBuffer->tracks())
            tracks.append(track);
    }
    player->startSource(tracks);
}

MediaSourcePrivateGStreamer::RegisteredTrack MediaSourcePrivateGStreamer::registerTrack(TrackID preferredId, StreamType streamType)
{
    ASSERT(isMainThread());
    RefPtr player = platformPlayer();

    TrackID assignedId = preferredId;

    // If the ID is already known, assign one starting at 100 - this helps avoid a snowball effect
    // where each following ID would now need to be offset by 1.
    if (m_trackRegistry.contains(assignedId)) {
        auto maxRegisteredId = std::max_element(m_trackRegistry.keys().begin(), m_trackRegistry.keys().end());
        assignedId = std::max((TrackID) 100, *maxRegisteredId + 1);
    }

    // Ensure that indices are sequential by track type.
    size_t assignedIndex = std::count_if(m_trackRegistry.values().begin(), m_trackRegistry.values().end(), [streamType](RegisteredTrack& other) -> bool {
        return other.streamType == streamType;
    });

    RegisteredTrack info = RegisteredTrack(assignedId, assignedIndex, streamType);
    [[maybe_unused]] auto result = m_trackRegistry.add(assignedId, info);
    ASSERT(result.isNewEntry);

    if (player)
        GST_DEBUG_OBJECT(player->pipeline(), "Registered new Track with index %" PRIu64 " and ID %" PRIu64 " (preferred ID was %" PRIu64 ")", static_cast<uint64_t>(assignedIndex), static_cast<uint64_t>(assignedId), static_cast<uint64_t>(preferredId));

    return info;
}

void MediaSourcePrivateGStreamer::willSeek()
{
    for (auto* sourceBuffer : m_activeSourceBuffers)
        downcast<SourceBufferPrivateGStreamer>(sourceBuffer)->willSeek();
}

void MediaSourcePrivateGStreamer::unregisterTrack(TrackID trackId)
{
    ASSERT(isMainThread());
    ASSERT(m_trackRegistry.contains(trackId));
    m_trackRegistry.remove(trackId);

    if (RefPtr player = platformPlayer())
        GST_DEBUG_OBJECT(player->pipeline(), "Unregistered Track ID: %" PRIu64 "", trackId);
}

void MediaSourcePrivateGStreamer::notifyActiveSourceBuffersChanged()
{
    if (RefPtr player = platformPlayer())
        player->notifyActiveSourceBuffersChanged();
}

void MediaSourcePrivateGStreamer::detach()
{
    m_hasAllTracks = false;
}

#if !RELEASE_LOG_DISABLED
WTFLogChannel& MediaSourcePrivateGStreamer::logChannel() const
{
    return LogMediaSource;
}

#endif

#undef GST_CAT_DEFAULT

} // namespace WebCore

#endif // ENABLE(MEDIA_SOURCE) && USE(GSTREAMER)
