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
#include <wtf/RefPtr.h>
#include <wtf/glib/GRefPtr.h>

GST_DEBUG_CATEGORY_EXTERN(webkit_mse_debug);
#define GST_CAT_DEFAULT webkit_mse_debug

namespace WebCore {

Ref<MediaSourcePrivateGStreamer> MediaSourcePrivateGStreamer::open(MediaSourcePrivateClient& mediaSource, MediaPlayerPrivateGStreamerMSE& playerPrivate)
{
    auto mediaSourcePrivate = adoptRef(*new MediaSourcePrivateGStreamer(mediaSource, playerPrivate));
    mediaSource.setPrivateAndOpen(mediaSourcePrivate.copyRef());
    return mediaSourcePrivate;
}

MediaSourcePrivateGStreamer::MediaSourcePrivateGStreamer(MediaSourcePrivateClient& mediaSource, MediaPlayerPrivateGStreamerMSE& playerPrivate)
    : MediaSourcePrivate()
    , m_mediaSource(mediaSource)
    , m_playerPrivate(playerPrivate)
#if !RELEASE_LOG_DISABLED
    , m_logger(m_playerPrivate.mediaPlayerLogger())
    , m_logIdentifier(m_playerPrivate.mediaPlayerLogIdentifier())
#endif
{
}

MediaSourcePrivateGStreamer::~MediaSourcePrivateGStreamer()
{
    ALWAYS_LOG(LOGIDENTIFIER);
    for (auto& sourceBufferPrivate : m_sourceBuffers)
        sourceBufferPrivate->clearMediaSource();
}

MediaSourcePrivateGStreamer::AddStatus MediaSourcePrivateGStreamer::addSourceBuffer(const ContentType& contentType, bool, RefPtr<SourceBufferPrivate>& sourceBufferPrivate)
{
    DEBUG_LOG(LOGIDENTIFIER, contentType);

    // Once every SourceBuffer has had an initialization segment appended playback starts and it's too late to add new SourceBuffers.
    if (m_playerPrivate.hasAllTracks())
        return MediaSourcePrivateGStreamer::AddStatus::ReachedIdLimit;

    if (!SourceBufferPrivateGStreamer::isContentTypeSupported(contentType))
        return MediaSourcePrivateGStreamer::AddStatus::NotSupported;

    sourceBufferPrivate = SourceBufferPrivateGStreamer::create(this, contentType, m_playerPrivate);
    RefPtr<SourceBufferPrivateGStreamer> sourceBufferPrivateGStreamer = static_cast<SourceBufferPrivateGStreamer*>(sourceBufferPrivate.get());
    m_sourceBuffers.add(sourceBufferPrivateGStreamer);
    return MediaSourcePrivateGStreamer::AddStatus::Ok;
}

void MediaSourcePrivateGStreamer::removeSourceBuffer(SourceBufferPrivate* sourceBufferPrivate)
{
    RefPtr<SourceBufferPrivateGStreamer> sourceBufferPrivateGStreamer = static_cast<SourceBufferPrivateGStreamer*>(sourceBufferPrivate);
    ASSERT(m_sourceBuffers.contains(sourceBufferPrivateGStreamer));

    sourceBufferPrivateGStreamer->clearMediaSource();
    m_sourceBuffers.remove(sourceBufferPrivateGStreamer);
    m_activeSourceBuffers.remove(sourceBufferPrivateGStreamer.get());
}

void MediaSourcePrivateGStreamer::durationChanged(const MediaTime&)
{
    ASSERT(isMainThread());

    MediaTime duration = m_mediaSource ? m_mediaSource->duration() : MediaTime::invalidTime();
    GST_TRACE_OBJECT(m_playerPrivate.pipeline(), "Duration: %" GST_TIME_FORMAT, GST_TIME_ARGS(toGstClockTime(duration)));
    if (!duration.isValid() || duration.isNegativeInfinite())
        return;

    m_playerPrivate.durationChanged();
}

void MediaSourcePrivateGStreamer::markEndOfStream(EndOfStreamStatus endOfStreamStatus)
{
    ASSERT(isMainThread());
#ifndef GST_DISABLE_GST_DEBUG
    const char* statusString = nullptr;
    switch (endOfStreamStatus) {
    case EndOfStreamStatus::EosNoError:
        statusString = "no-error";
        break;
    case EndOfStreamStatus::EosDecodeError:
        statusString = "decode-error";
        break;
    case EndOfStreamStatus::EosNetworkError:
        statusString = "network-error";
        break;
    }
    GST_DEBUG_OBJECT(m_playerPrivate.pipeline(), "Marking EOS, status is %s", statusString);
#endif
    if (endOfStreamStatus == EosNoError)
        m_playerPrivate.setNetworkState(MediaPlayer::NetworkState::Loaded);
    m_isEnded = true;
}

void MediaSourcePrivateGStreamer::unmarkEndOfStream()
{
    ASSERT(isMainThread());
    GST_DEBUG_OBJECT(m_playerPrivate.pipeline(), "Un-marking EOS");
    m_isEnded = false;
}

MediaPlayer::ReadyState MediaSourcePrivateGStreamer::readyState() const
{
    return m_playerPrivate.readyState();
}

void MediaSourcePrivateGStreamer::setReadyState(MediaPlayer::ReadyState state)
{
    m_playerPrivate.setReadyState(state);
}

void MediaSourcePrivateGStreamer::seekCompleted()
{
    // This call just informs us that the seek has been completed as far as MediaSource is concerned: that is,
    // the samples for `currentTime` have been fed. This doesn't mean the seek is complete for the player, as
    // they still have to be decoded and preroll has to occur before we let the "seeked" event happen.
    // See MediaPlayerPrivateGStreamerMSE::asyncStateChangeDone().
}

MediaTime MediaSourcePrivateGStreamer::duration() const
{
    return m_mediaSource ? m_mediaSource->duration() : MediaTime::invalidTime();
}

MediaTime MediaSourcePrivateGStreamer::currentMediaTime() const
{
    return m_playerPrivate.currentMediaTime();
}

void MediaSourcePrivateGStreamer::sourceBufferPrivateDidChangeActiveState(SourceBufferPrivateGStreamer* sourceBufferPrivate, bool isActive)
{
    if (!isActive)
        m_activeSourceBuffers.remove(sourceBufferPrivate);
    else if (!m_activeSourceBuffers.contains(sourceBufferPrivate))
        m_activeSourceBuffers.add(sourceBufferPrivate);
}

void MediaSourcePrivateGStreamer::startPlaybackIfHasAllTracks()
{
    if (m_hasAllTracks) {
        // Already started, nothing to do.
        return;
    }

    for (auto& sourceBuffer : m_sourceBuffers) {
        if (!sourceBuffer->hasReceivedInitializationSegment()) {
            GST_DEBUG_OBJECT(m_playerPrivate.pipeline(), "There are still SourceBuffers without an initialization segment, not starting source yet.");
            return;
        }
    }

    GST_DEBUG_OBJECT(m_playerPrivate.pipeline(), "All SourceBuffers have an initialization segment, starting source.");
    m_hasAllTracks = true;

    Vector<RefPtr<MediaSourceTrackGStreamer>> tracks;
    for (auto& sourceBuffer : m_sourceBuffers)
        tracks.appendRange(sourceBuffer->tracks().begin(), sourceBuffer->tracks().end());
    m_playerPrivate.startSource(tracks);
}

std::unique_ptr<PlatformTimeRanges> MediaSourcePrivateGStreamer::buffered()
{
    if (m_mediaSource)
        return m_mediaSource->buffered();
    return nullptr;
}

#if !RELEASE_LOG_DISABLED
WTFLogChannel& MediaSourcePrivateGStreamer::logChannel() const
{
    return LogMediaSource;
}

#endif

}
#endif
