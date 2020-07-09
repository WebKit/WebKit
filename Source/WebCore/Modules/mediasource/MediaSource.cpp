/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 * Copyright (C) 2013-2017 Apple Inc. All rights reserved.
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
#include "MediaSource.h"

#if ENABLE(MEDIA_SOURCE)

#include "AudioTrackList.h"
#include "ContentType.h"
#include "Event.h"
#include "EventNames.h"
#include "HTMLMediaElement.h"
#include "Logging.h"
#include "MediaSourcePrivate.h"
#include "MediaSourceRegistry.h"
#include "Settings.h"
#include "SourceBuffer.h"
#include "SourceBufferList.h"
#include "SourceBufferPrivate.h"
#include "TextTrackList.h"
#include "TimeRanges.h"
#include "VideoTrackList.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(MediaSource);

String convertEnumerationToString(MediaSourcePrivate::AddStatus enumerationValue)
{
    static const NeverDestroyed<String> values[] = {
        MAKE_STATIC_STRING_IMPL("Ok"),
        MAKE_STATIC_STRING_IMPL("NotSupported"),
        MAKE_STATIC_STRING_IMPL("ReachedIdLimit"),
    };
    static_assert(static_cast<size_t>(MediaSourcePrivate::AddStatus::Ok) == 0, "MediaSourcePrivate::AddStatus::Ok is not 0 as expected");
    static_assert(static_cast<size_t>(MediaSourcePrivate::AddStatus::NotSupported) == 1, "MediaSourcePrivate::AddStatus::NotSupported is not 1 as expected");
    static_assert(static_cast<size_t>(MediaSourcePrivate::AddStatus::ReachedIdLimit) == 2, "MediaSourcePrivate::AddStatus::ReachedIdLimit is not 2 as expected");
    ASSERT(static_cast<size_t>(enumerationValue) < WTF_ARRAY_LENGTH(values));
    return values[static_cast<size_t>(enumerationValue)];
}

String convertEnumerationToString(MediaSourcePrivate::EndOfStreamStatus enumerationValue)
{
    static const NeverDestroyed<String> values[] = {
        MAKE_STATIC_STRING_IMPL("EosNoError"),
        MAKE_STATIC_STRING_IMPL("EosNetworkError"),
        MAKE_STATIC_STRING_IMPL("EosDecodeError"),
    };
    static_assert(static_cast<size_t>(MediaSourcePrivate::EndOfStreamStatus::EosNoError) == 0, "MediaSourcePrivate::EndOfStreamStatus::EosNoError is not 0 as expected");
    static_assert(static_cast<size_t>(MediaSourcePrivate::EndOfStreamStatus::EosNetworkError) == 1, "MediaSourcePrivate::EndOfStreamStatus::EosNetworkError is not 1 as expected");
    static_assert(static_cast<size_t>(MediaSourcePrivate::EndOfStreamStatus::EosDecodeError) == 2, "MediaSourcePrivate::EndOfStreamStatus::EosDecodeError is not 2 as expected");
    ASSERT(static_cast<size_t>(enumerationValue) < WTF_ARRAY_LENGTH(values));
    return values[static_cast<size_t>(enumerationValue)];
}

URLRegistry* MediaSource::s_registry;

void MediaSource::setRegistry(URLRegistry* registry)
{
    ASSERT(!s_registry);
    s_registry = registry;
}

Ref<MediaSource> MediaSource::create(ScriptExecutionContext& context)
{
    auto mediaSource = adoptRef(*new MediaSource(context));
    mediaSource->suspendIfNeeded();
    return mediaSource;
}

MediaSource::MediaSource(ScriptExecutionContext& context)
    : ActiveDOMObject(&context)
    , m_duration(MediaTime::invalidTime())
    , m_pendingSeekTime(MediaTime::invalidTime())
    , m_asyncEventQueue(MainThreadGenericEventQueue::create(*this))
#if !RELEASE_LOG_DISABLED
    , m_logger(downcast<Document>(context).logger())
#endif
{
    m_sourceBuffers = SourceBufferList::create(scriptExecutionContext());
    m_activeSourceBuffers = SourceBufferList::create(scriptExecutionContext());
}

MediaSource::~MediaSource()
{
    ALWAYS_LOG(LOGIDENTIFIER);
    ASSERT(isClosed());
}

void MediaSource::setPrivateAndOpen(Ref<MediaSourcePrivate>&& mediaSourcePrivate)
{
    DEBUG_LOG(LOGIDENTIFIER);
    ASSERT(!m_private);
    ASSERT(m_mediaElement);
    m_private = WTFMove(mediaSourcePrivate);

    // 2.4.1 Attaching to a media element
    // https://rawgit.com/w3c/media-source/45627646344eea0170dd1cbc5a3d508ca751abb8/media-source-respec.html#mediasource-attach

    // ↳ If readyState is NOT set to "closed"
    //    Run the "If the media data cannot be fetched at all, due to network errors, causing the user agent to give up trying
    //    to fetch the resource" steps of the resource fetch algorithm's media data processing steps list.
    if (!isClosed()) {
        m_mediaElement->mediaLoadingFailedFatally(MediaPlayer::NetworkState::NetworkError);
        return;
    }

    // ↳ Otherwise
    // 1. Set the media element's delaying-the-load-event-flag to false.
    m_mediaElement->setShouldDelayLoadEvent(false);

    // 2. Set the readyState attribute to "open".
    // 3. Queue a task to fire a simple event named sourceopen at the MediaSource.
    setReadyState(ReadyState::Open);

    // 4. Continue the resource fetch algorithm by running the remaining "Otherwise (mode is local)" steps,
    // with these clarifications:
    // NOTE: This is handled in HTMLMediaElement.
}

void MediaSource::addedToRegistry()
{
    DEBUG_LOG(LOGIDENTIFIER);
    setPendingActivity(*this);
}

void MediaSource::removedFromRegistry()
{
    DEBUG_LOG(LOGIDENTIFIER);
    unsetPendingActivity(*this);
}

MediaTime MediaSource::duration() const
{
    return m_duration;
}

MediaTime MediaSource::currentTime() const
{
    return m_mediaElement ? m_mediaElement->currentMediaTime() : MediaTime::zeroTime();
}

std::unique_ptr<PlatformTimeRanges> MediaSource::buffered() const
{
    if (m_buffered && m_activeSourceBuffers->length() && std::all_of(m_activeSourceBuffers->begin(), m_activeSourceBuffers->end(), [](auto& buffer) { return !buffer->isBufferedDirty(); }))
        return makeUnique<PlatformTimeRanges>(*m_buffered);

    m_buffered = makeUnique<PlatformTimeRanges>();
    for (auto& sourceBuffer : *m_activeSourceBuffers)
        sourceBuffer->setBufferedDirty(false);

    // Implements MediaSource algorithm for HTMLMediaElement.buffered.
    // https://dvcs.w3.org/hg/html-media/raw-file/default/media-source/media-source.html#htmlmediaelement-extensions
    Vector<PlatformTimeRanges> activeRanges = this->activeRanges();

    // 1. If activeSourceBuffers.length equals 0 then return an empty TimeRanges object and abort these steps.
    if (activeRanges.isEmpty())
        return makeUnique<PlatformTimeRanges>(*m_buffered);

    // 2. Let active ranges be the ranges returned by buffered for each SourceBuffer object in activeSourceBuffers.
    // 3. Let highest end time be the largest range end time in the active ranges.
    MediaTime highestEndTime = MediaTime::zeroTime();
    for (auto& ranges : activeRanges) {
        unsigned length = ranges.length();
        if (length)
            highestEndTime = std::max(highestEndTime, ranges.end(length - 1));
    }

    // Return an empty range if all ranges are empty.
    if (!highestEndTime)
        return makeUnique<PlatformTimeRanges>(*m_buffered);

    // 4. Let intersection ranges equal a TimeRange object containing a single range from 0 to highest end time.
    m_buffered->add(MediaTime::zeroTime(), highestEndTime);

    // 5. For each SourceBuffer object in activeSourceBuffers run the following steps:
    bool ended = readyState() == ReadyState::Ended;
    for (auto& sourceRanges : activeRanges) {
        // 5.1 Let source ranges equal the ranges returned by the buffered attribute on the current SourceBuffer.
        // 5.2 If readyState is "ended", then set the end time on the last range in source ranges to highest end time.
        if (ended && sourceRanges.length())
            sourceRanges.add(sourceRanges.start(sourceRanges.length() - 1), highestEndTime);

        // 5.3 Let new intersection ranges equal the intersection between the intersection ranges and the source ranges.
        // 5.4 Replace the ranges in intersection ranges with the new intersection ranges.
        m_buffered->intersectWith(sourceRanges);
    }

    return makeUnique<PlatformTimeRanges>(*m_buffered);
}

void MediaSource::seekToTime(const MediaTime& time)
{
    if (isClosed())
        return;

    ALWAYS_LOG(LOGIDENTIFIER, time);

    // 2.4.3 Seeking
    // https://rawgit.com/w3c/media-source/45627646344eea0170dd1cbc5a3d508ca751abb8/media-source-respec.html#mediasource-seeking

    m_pendingSeekTime = time;

    // Run the following steps as part of the "Wait until the user agent has established whether or not the
    // media data for the new playback position is available, and, if it is, until it has decoded enough data
    // to play back that position" step of the seek algorithm:
    // ↳ If new playback position is not in any TimeRange of HTMLMediaElement.buffered
    if (!hasBufferedTime(time)) {
        // 1. If the HTMLMediaElement.readyState attribute is greater than HAVE_METADATA,
        // then set the HTMLMediaElement.readyState attribute to HAVE_METADATA.
        m_private->setReadyState(MediaPlayer::ReadyState::HaveMetadata);

        // 2. The media element waits until an appendBuffer() or an appendStream() call causes the coded
        // frame processing algorithm to set the HTMLMediaElement.readyState attribute to a value greater
        // than HAVE_METADATA.
        m_private->waitForSeekCompleted();
        return;
    }
    // ↳ Otherwise
    // Continue

// https://bugs.webkit.org/show_bug.cgi?id=125157 broke seek on MediaPlayerPrivateGStreamerMSE
#if !USE(GSTREAMER)
    m_private->waitForSeekCompleted();
#endif
    completeSeek();
}

void MediaSource::completeSeek()
{
    if (isClosed())
        return;

    // 2.4.3 Seeking, ctd.
    // https://dvcs.w3.org/hg/html-media/raw-file/tip/media-source/media-source.html#mediasource-seeking

    ASSERT(m_pendingSeekTime.isValid());

    ALWAYS_LOG(LOGIDENTIFIER, m_pendingSeekTime);

    // 2. The media element resets all decoders and initializes each one with data from the appropriate
    // initialization segment.
    // 3. The media element feeds coded frames from the active track buffers into the decoders starting
    // with the closest random access point before the new playback position.
    MediaTime pendingSeekTime = m_pendingSeekTime;
    m_pendingSeekTime = MediaTime::invalidTime();
    for (auto& sourceBuffer : *m_activeSourceBuffers)
        sourceBuffer->seekToTime(pendingSeekTime);

    // 4. Resume the seek algorithm at the "Await a stable state" step.
    m_private->seekCompleted();

    monitorSourceBuffers();
}

Ref<TimeRanges> MediaSource::seekable()
{
    // 6. HTMLMediaElement Extensions, seekable
    // W3C Editor's Draft 16 September 2016
    // https://rawgit.com/w3c/media-source/45627646344eea0170dd1cbc5a3d508ca751abb8/media-source-respec.html#htmlmediaelement-extensions

    // ↳ If duration equals NaN:
    // Return an empty TimeRanges object.
    if (m_duration.isInvalid())
        return TimeRanges::create();

    // ↳ If duration equals positive Infinity:
    if (m_duration.isPositiveInfinite()) {
        auto buffered = this->buffered();
        // If live seekable range is not empty:
        if (m_liveSeekable && m_liveSeekable->length()) {
            // Let union ranges be the union of live seekable range and the HTMLMediaElement.buffered attribute.
            buffered->unionWith(*m_liveSeekable);
            // Return a single range with a start time equal to the earliest start time in union ranges
            // and an end time equal to the highest end time in union ranges and abort these steps.
            buffered->add(buffered->start(0), buffered->maximumBufferedTime());
            return TimeRanges::create(*buffered);
        }

        // If the HTMLMediaElement.buffered attribute returns an empty TimeRanges object, then return
        // an empty TimeRanges object and abort these steps.
        if (!buffered->length())
            return TimeRanges::create();

        // Return a single range with a start time of 0 and an end time equal to the highest end time
        // reported by the HTMLMediaElement.buffered attribute.
        return TimeRanges::create({MediaTime::zeroTime(), buffered->maximumBufferedTime()});
    }

    // ↳ Otherwise:
    // Return a single range with a start time of 0 and an end time equal to duration.
    return TimeRanges::create({MediaTime::zeroTime(), m_duration});
}

ExceptionOr<void> MediaSource::setLiveSeekableRange(double start, double end)
{
    // W3C Editor's Draft 16 September 2016
    // https://rawgit.com/w3c/media-source/45627646344eea0170dd1cbc5a3d508ca751abb8/media-source-respec.html#dom-mediasource-setliveseekablerange

    ALWAYS_LOG(LOGIDENTIFIER, "start = ", start, ", end = ", end);

    // If the readyState attribute is not "open" then throw an InvalidStateError exception and abort these steps.
    if (!isOpen())
        return Exception { InvalidStateError };

    // If start is negative or greater than end, then throw a TypeError exception and abort these steps.
    if (start < 0 || start > end)
        return Exception { TypeError };

    // Set live seekable range to be a new normalized TimeRanges object containing a single range
    // whose start position is start and end position is end.
    m_liveSeekable = makeUnique<PlatformTimeRanges>(MediaTime::createWithDouble(start), MediaTime::createWithDouble(end));

    return { };
}

ExceptionOr<void> MediaSource::clearLiveSeekableRange()
{
    // W3C Editor's Draft 16 September 2016
    // https://rawgit.com/w3c/media-source/45627646344eea0170dd1cbc5a3d508ca751abb8/media-source-respec.html#dom-mediasource-clearliveseekablerange

    ALWAYS_LOG(LOGIDENTIFIER);

    // If the readyState attribute is not "open" then throw an InvalidStateError exception and abort these steps.
    if (!isOpen())
        return Exception { InvalidStateError };
    m_liveSeekable = nullptr;
    return { };
}

const MediaTime& MediaSource::currentTimeFudgeFactor()
{
    // Allow hasCurrentTime() to be off by as much as the length of two 24fps video frames
    static NeverDestroyed<MediaTime> fudgeFactor(2002, 24000);
    return fudgeFactor;
}

bool MediaSource::contentTypeShouldGenerateTimestamps(const ContentType& contentType)
{
    return contentType.containerType() == "audio/aac" || contentType.containerType() == "audio/mpeg";
}

bool MediaSource::hasBufferedTime(const MediaTime& time)
{
    if (time > duration())
        return false;

    auto ranges = buffered();
    if (!ranges->length())
        return false;

    return abs(ranges->nearest(time) - time) <= currentTimeFudgeFactor();
}

bool MediaSource::hasCurrentTime()
{
    return hasBufferedTime(currentTime());
}

bool MediaSource::hasFutureTime()
{
    MediaTime currentTime = this->currentTime();
    MediaTime duration = this->duration();

    if (currentTime >= duration)
        return true;

    auto ranges = buffered();
    MediaTime nearest = ranges->nearest(currentTime);
    if (abs(nearest - currentTime) > currentTimeFudgeFactor())
        return false;

    size_t found = ranges->find(nearest);
    if (found == notFound)
        return false;

    MediaTime localEnd = ranges->end(found);
    if (localEnd == duration)
        return true;

    return localEnd - currentTime > currentTimeFudgeFactor();
}

void MediaSource::monitorSourceBuffers()
{
    if (isClosed())
        return;

    // 2.4.4 SourceBuffer Monitoring
    // https://rawgit.com/w3c/media-source/45627646344eea0170dd1cbc5a3d508ca751abb8/media-source-respec.html#buffer-monitoring

    // Note, the behavior if activeSourceBuffers is empty is undefined.
    if (!m_activeSourceBuffers) {
        m_private->setReadyState(MediaPlayer::ReadyState::HaveNothing);
        return;
    }

    // ↳ If the HTMLMediaElement.readyState attribute equals HAVE_NOTHING:
    if (mediaElement()->readyState() == HTMLMediaElement::HAVE_NOTHING) {
        // 1. Abort these steps.
        return;
    }

    // ↳ If HTMLMediaElement.buffered does not contain a TimeRange for the current playback position:
    if (!hasCurrentTime()) {
        // 1. Set the HTMLMediaElement.readyState attribute to HAVE_METADATA.
        // 2. If this is the first transition to HAVE_METADATA, then queue a task to fire a simple event
        // named loadedmetadata at the media element.
        m_private->setReadyState(MediaPlayer::ReadyState::HaveMetadata);

        // 3. Abort these steps.
        return;
    }

    // ↳ If HTMLMediaElement.buffered contains a TimeRange that includes the current
    //  playback position and enough data to ensure uninterrupted playback:
    auto ranges = buffered();
    if (std::all_of(m_activeSourceBuffers->begin(), m_activeSourceBuffers->end(), [&](auto& sourceBuffer) {
        return sourceBuffer->canPlayThroughRange(*ranges);
    })) {
        // 1. Set the HTMLMediaElement.readyState attribute to HAVE_ENOUGH_DATA.
        // 2. Queue a task to fire a simple event named canplaythrough at the media element.
        // 3. Playback may resume at this point if it was previously suspended by a transition to HAVE_CURRENT_DATA.
        m_private->setReadyState(MediaPlayer::ReadyState::HaveEnoughData);

        if (m_pendingSeekTime.isValid())
            completeSeek();

        // 4. Abort these steps.
        return;
    }

    // ↳ If HTMLMediaElement.buffered contains a TimeRange that includes the current playback
    //  position and some time beyond the current playback position, then run the following steps:
    if (hasFutureTime()) {
        // 1. Set the HTMLMediaElement.readyState attribute to HAVE_FUTURE_DATA.
        // 2. If the previous value of HTMLMediaElement.readyState was less than HAVE_FUTURE_DATA, then queue a task to fire a simple event named canplay at the media element.
        // 3. Playback may resume at this point if it was previously suspended by a transition to HAVE_CURRENT_DATA.
        m_private->setReadyState(MediaPlayer::ReadyState::HaveFutureData);

        if (m_pendingSeekTime.isValid())
            completeSeek();

        // 4. Abort these steps.
        return;
    }

    // ↳ If HTMLMediaElement.buffered contains a TimeRange that ends at the current playback position and does not have a range covering the time immediately after the current position:
    // NOTE: Logically, !(all objects do not contain currentTime) == (some objects contain current time)

    // 1. Set the HTMLMediaElement.readyState attribute to HAVE_CURRENT_DATA.
    // 2. If this is the first transition to HAVE_CURRENT_DATA, then queue a task to fire a simple
    // event named loadeddata at the media element.
    // 3. Playback is suspended at this point since the media element doesn't have enough data to
    // advance the media timeline.
    m_private->setReadyState(MediaPlayer::ReadyState::HaveCurrentData);

    if (m_pendingSeekTime.isValid())
        completeSeek();

    // 4. Abort these steps.
}

ExceptionOr<void> MediaSource::setDuration(double duration)
{
    // 2.1 Attributes - Duration
    // https://www.w3.org/TR/2016/REC-media-source-20161117/#attributes

    ALWAYS_LOG(LOGIDENTIFIER, duration);

    // On setting, run the following steps:
    // 1. If the value being set is negative or NaN then throw a TypeError exception and abort these steps.
    if (duration < 0.0 || std::isnan(duration))
        return Exception { TypeError };

    // 2. If the readyState attribute is not "open" then throw an InvalidStateError exception and abort these steps.
    if (!isOpen())
        return Exception { InvalidStateError };

    // 3. If the updating attribute equals true on any SourceBuffer in sourceBuffers, then throw an InvalidStateError
    // exception and abort these steps.
    for (auto& sourceBuffer : *m_sourceBuffers) {
        if (sourceBuffer->updating())
            return Exception { InvalidStateError };
    }

    // 4. Run the duration change algorithm with new duration set to the value being assigned to this attribute.
    return setDurationInternal(MediaTime::createWithDouble(duration));
}

ExceptionOr<void> MediaSource::setDurationInternal(const MediaTime& duration)
{
    // 2.4.6 Duration Change
    // https://www.w3.org/TR/2016/REC-media-source-20161117/#duration-change-algorithm

    MediaTime newDuration = duration;

    // 1. If the current value of duration is equal to new duration, then return.
    if (newDuration == m_duration)
        return { };

    // 2. If new duration is less than the highest presentation timestamp of any buffered coded frames
    // for all SourceBuffer objects in sourceBuffers, then throw an InvalidStateError exception and
    // abort these steps.
    // 3. Let highest end time be the largest track buffer ranges end time across all the track buffers
    // across all SourceBuffer objects in sourceBuffers.
    MediaTime highestPresentationTimestamp;
    MediaTime highestEndTime;
    for (auto& sourceBuffer : *m_sourceBuffers) {
        highestPresentationTimestamp = std::max(highestPresentationTimestamp, sourceBuffer->highestPresentationTimestamp());
        highestEndTime = std::max(highestEndTime, sourceBuffer->bufferedInternal().ranges().maximumBufferedTime());
    }
    if (highestPresentationTimestamp.isValid() && newDuration < highestPresentationTimestamp)
        return Exception { InvalidStateError };

    // 4. If new duration is less than highest end time, then
    // 4.1. Update new duration to equal highest end time.
    if (highestEndTime.isValid() && newDuration < highestEndTime)
        newDuration = highestEndTime;

    // 5. Update duration to new duration.
    m_duration = newDuration;
    ALWAYS_LOG(LOGIDENTIFIER, duration);

    // 6. Update the media duration to new duration and run the HTMLMediaElement duration change algorithm.
    m_private->durationChanged();

    return { };
}

void MediaSource::setReadyState(ReadyState state)
{
    auto oldState = readyState();
    if (oldState == state)
        return;

    m_readyState = state;

    onReadyStateChange(oldState, state);
}

ExceptionOr<void> MediaSource::endOfStream(Optional<EndOfStreamError> error)
{
    ALWAYS_LOG(LOGIDENTIFIER);

    // 2.2 https://dvcs.w3.org/hg/html-media/raw-file/tip/media-source/media-source.html#widl-MediaSource-endOfStream-void-EndOfStreamError-error
    // 1. If the readyState attribute is not in the "open" state then throw an
    // InvalidStateError exception and abort these steps.
    if (!isOpen())
        return Exception { InvalidStateError };

    // 2. If the updating attribute equals true on any SourceBuffer in sourceBuffers, then throw an
    // InvalidStateError exception and abort these steps.
    if (std::any_of(m_sourceBuffers->begin(), m_sourceBuffers->end(), [](auto& sourceBuffer) { return sourceBuffer->updating(); }))
        return Exception { InvalidStateError };

    // 3. Run the end of stream algorithm with the error parameter set to error.
    streamEndedWithError(error);

    return { };
}

void MediaSource::streamEndedWithError(Optional<EndOfStreamError> error)
{
#if !RELEASE_LOG_DISABLED
    if (error)
        ALWAYS_LOG(LOGIDENTIFIER, error.value());
    else
        ALWAYS_LOG(LOGIDENTIFIER);
#endif

    if (isClosed())
        return;

    // 2.4.7 https://dvcs.w3.org/hg/html-media/raw-file/tip/media-source/media-source.html#end-of-stream-algorithm

    // 1. Change the readyState attribute value to "ended".
    // 2. Queue a task to fire a simple event named sourceended at the MediaSource.
    setReadyState(ReadyState::Ended);

    // 3.
    if (!error) {
        // ↳ If error is not set, is null, or is an empty string
        // 1. Run the duration change algorithm with new duration set to the highest end time reported by
        // the buffered attribute across all SourceBuffer objects in sourceBuffers.
        MediaTime maxEndTime;
        for (auto& sourceBuffer : *m_sourceBuffers) {
            if (auto length = sourceBuffer->bufferedInternal().length())
                maxEndTime = std::max(sourceBuffer->bufferedInternal().ranges().end(length - 1), maxEndTime);
        }
        setDurationInternal(maxEndTime);

        // 2. Notify the media element that it now has all of the media data.
        for (auto& sourceBuffer : *m_sourceBuffers)
            sourceBuffer->trySignalAllSamplesEnqueued();
        m_private->markEndOfStream(MediaSourcePrivate::EosNoError);
    } else if (error == EndOfStreamError::Network) {
        // ↳ If error is set to "network"
        ASSERT(m_mediaElement);
        if (m_mediaElement->readyState() == HTMLMediaElement::HAVE_NOTHING) {
            //  ↳ If the HTMLMediaElement.readyState attribute equals HAVE_NOTHING
            //    Run the "If the media data cannot be fetched at all, due to network errors, causing
            //    the user agent to give up trying to fetch the resource" steps of the resource fetch algorithm.
            //    NOTE: This step is handled by HTMLMediaElement::mediaLoadingFailed().
            m_mediaElement->mediaLoadingFailed(MediaPlayer::NetworkState::NetworkError);
        } else {
            //  ↳ If the HTMLMediaElement.readyState attribute is greater than HAVE_NOTHING
            //    Run the "If the connection is interrupted after some media data has been received, causing the
            //    user agent to give up trying to fetch the resource" steps of the resource fetch algorithm.
            //    NOTE: This step is handled by HTMLMediaElement::mediaLoadingFailedFatally().
            m_mediaElement->mediaLoadingFailedFatally(MediaPlayer::NetworkState::NetworkError);
        }
    } else {
        // ↳ If error is set to "decode"
        ASSERT(error == EndOfStreamError::Decode);
        ASSERT(m_mediaElement);
        if (m_mediaElement->readyState() == HTMLMediaElement::HAVE_NOTHING) {
            //  ↳ If the HTMLMediaElement.readyState attribute equals HAVE_NOTHING
            //    Run the "If the media data can be fetched but is found by inspection to be in an unsupported
            //    format, or can otherwise not be rendered at all" steps of the resource fetch algorithm.
            //    NOTE: This step is handled by HTMLMediaElement::mediaLoadingFailed().
            m_mediaElement->mediaLoadingFailed(MediaPlayer::NetworkState::FormatError);
        } else {
            //  ↳ If the HTMLMediaElement.readyState attribute is greater than HAVE_NOTHING
            //    Run the media data is corrupted steps of the resource fetch algorithm.
            //    NOTE: This step is handled by HTMLMediaElement::mediaLoadingFailedFatally().
            m_mediaElement->mediaLoadingFailedFatally(MediaPlayer::NetworkState::DecodeError);
        }
    }
}

ExceptionOr<Ref<SourceBuffer>> MediaSource::addSourceBuffer(const String& type)
{
    DEBUG_LOG(LOGIDENTIFIER, type);

    // 2.2 http://www.w3.org/TR/media-source/#widl-MediaSource-addSourceBuffer-SourceBuffer-DOMString-type
    // When this method is invoked, the user agent must run the following steps:

    // 1. If type is an empty string then throw a TypeError exception and abort these steps.
    if (type.isEmpty())
        return Exception { TypeError };

    // 2. If type contains a MIME type that is not supported ..., then throw a
    // NotSupportedError exception and abort these steps.
    Vector<ContentType> mediaContentTypesRequiringHardwareSupport;
    if (m_mediaElement)
        mediaContentTypesRequiringHardwareSupport.appendVector(m_mediaElement->document().settings().mediaContentTypesRequiringHardwareSupport());

    if (!isTypeSupported(type, WTFMove(mediaContentTypesRequiringHardwareSupport)))
        return Exception { NotSupportedError };

    // 4. If the readyState attribute is not in the "open" state then throw an
    // InvalidStateError exception and abort these steps.
    if (!isOpen())
        return Exception { InvalidStateError };

    // 5. Create a new SourceBuffer object and associated resources.
    ContentType contentType(type);
    auto sourceBufferPrivate = createSourceBufferPrivate(contentType);

    if (sourceBufferPrivate.hasException()) {
        // 2. If type contains a MIME type that is not supported ..., then throw a NotSupportedError exception and abort these steps.
        // 3. If the user agent can't handle any more SourceBuffer objects then throw a QuotaExceededError exception and abort these steps
        return sourceBufferPrivate.releaseException();
    }

    auto buffer = SourceBuffer::create(sourceBufferPrivate.releaseReturnValue(), this);
    DEBUG_LOG(LOGIDENTIFIER, "created SourceBuffer");

    // 6. Set the generate timestamps flag on the new object to the value in the "Generate Timestamps Flag"
    // column of the byte stream format registry [MSE-REGISTRY] entry that is associated with type.
    // NOTE: In the current byte stream format registry <http://www.w3.org/2013/12/byte-stream-format-registry/>
    // only the "MPEG Audio Byte Stream Format" has the "Generate Timestamps Flag" value set.
    bool shouldGenerateTimestamps = contentTypeShouldGenerateTimestamps(contentType);
    buffer->setShouldGenerateTimestamps(shouldGenerateTimestamps);

    // 7. If the generate timestamps flag equals true:
    // ↳ Set the mode attribute on the new object to "sequence".
    // Otherwise:
    // ↳ Set the mode attribute on the new object to "segments".
    buffer->setMode(shouldGenerateTimestamps ? SourceBuffer::AppendMode::Sequence : SourceBuffer::AppendMode::Segments);

    // 8. Add the new object to sourceBuffers and fire a addsourcebuffer on that object.
    m_sourceBuffers->add(buffer.copyRef());
    regenerateActiveSourceBuffers();

    // 9. Return the new object to the caller.
    return buffer;
}

ExceptionOr<void> MediaSource::removeSourceBuffer(SourceBuffer& buffer)
{
    DEBUG_LOG(LOGIDENTIFIER);

    Ref<SourceBuffer> protect(buffer);

    // 2. If sourceBuffer specifies an object that is not in sourceBuffers then
    // throw a NotFoundError exception and abort these steps.
    if (!m_sourceBuffers->length() || !m_sourceBuffers->contains(buffer))
        return Exception { NotFoundError };

    // 3. If the sourceBuffer.updating attribute equals true, then run the following steps: ...
    buffer.abortIfUpdating();

    ASSERT(scriptExecutionContext());
    if (!scriptExecutionContext()->activeDOMObjectsAreStopped()) {
        // 4. Let SourceBuffer audioTracks list equal the AudioTrackList object returned by sourceBuffer.audioTracks.
        auto* audioTracks = buffer.audioTracksIfExists();

        // 5. If the SourceBuffer audioTracks list is not empty, then run the following steps:
        if (audioTracks && audioTracks->length()) {
            // 5.1 Let HTMLMediaElement audioTracks list equal the AudioTrackList object returned by the audioTracks
            // attribute on the HTMLMediaElement.
            // 5.2 Let the removed enabled audio track flag equal false.
            bool removedEnabledAudioTrack = false;

            // 5.3 For each AudioTrack object in the SourceBuffer audioTracks list, run the following steps:
            while (audioTracks->length()) {
                auto& track = *audioTracks->lastItem();

                // 5.3.1 Set the sourceBuffer attribute on the AudioTrack object to null.
                track.setSourceBuffer(nullptr);

                // 5.3.2 If the enabled attribute on the AudioTrack object is true, then set the removed enabled
                // audio track flag to true.
                if (track.enabled())
                    removedEnabledAudioTrack = true;

                // 5.3.3 Remove the AudioTrack object from the HTMLMediaElement audioTracks list.
                // 5.3.4 Queue a task to fire a trusted event named removetrack, that does not bubble and is not
                // cancelable, and that uses the TrackEvent interface, at the HTMLMediaElement audioTracks list.
                if (mediaElement())
                    mediaElement()->removeAudioTrack(track);

                // 5.3.5 Remove the AudioTrack object from the SourceBuffer audioTracks list.
                // 5.3.6 Queue a task to fire a trusted event named removetrack, that does not bubble and is not
                // cancelable, and that uses the TrackEvent interface, at the SourceBuffer audioTracks list.
                audioTracks->remove(track);
            }

            // 5.4 If the removed enabled audio track flag equals true, then queue a task to fire a simple event
            // named change at the HTMLMediaElement audioTracks list.
            if (removedEnabledAudioTrack)
                mediaElement()->ensureAudioTracks().scheduleChangeEvent();
        }

        // 6. Let SourceBuffer videoTracks list equal the VideoTrackList object returned by sourceBuffer.videoTracks.
        auto* videoTracks = buffer.videoTracksIfExists();

        // 7. If the SourceBuffer videoTracks list is not empty, then run the following steps:
        if (videoTracks && videoTracks->length()) {
            // 7.1 Let HTMLMediaElement videoTracks list equal the VideoTrackList object returned by the videoTracks
            // attribute on the HTMLMediaElement.
            // 7.2 Let the removed selected video track flag equal false.
            bool removedSelectedVideoTrack = false;

            // 7.3 For each VideoTrack object in the SourceBuffer videoTracks list, run the following steps:
            while (videoTracks->length()) {
                auto& track = *videoTracks->lastItem();

                // 7.3.1 Set the sourceBuffer attribute on the VideoTrack object to null.
                track.setSourceBuffer(nullptr);

                // 7.3.2 If the selected attribute on the VideoTrack object is true, then set the removed selected
                // video track flag to true.
                if (track.selected())
                    removedSelectedVideoTrack = true;

                // 7.3.3 Remove the VideoTrack object from the HTMLMediaElement videoTracks list.
                // 7.3.4 Queue a task to fire a trusted event named removetrack, that does not bubble and is not
                // cancelable, and that uses the TrackEvent interface, at the HTMLMediaElement videoTracks list.
                if (mediaElement())
                    mediaElement()->removeVideoTrack(track);

                // 7.3.5 Remove the VideoTrack object from the SourceBuffer videoTracks list.
                // 7.3.6 Queue a task to fire a trusted event named removetrack, that does not bubble and is not
                // cancelable, and that uses the TrackEvent interface, at the SourceBuffer videoTracks list.
                videoTracks->remove(track);
            }

            // 7.4 If the removed selected video track flag equals true, then queue a task to fire a simple event
            // named change at the HTMLMediaElement videoTracks list.
            if (removedSelectedVideoTrack)
                mediaElement()->ensureVideoTracks().scheduleChangeEvent();
        }

        // 8. Let SourceBuffer textTracks list equal the TextTrackList object returned by sourceBuffer.textTracks.
        auto* textTracks = buffer.textTracksIfExists();

        // 9. If the SourceBuffer textTracks list is not empty, then run the following steps:
        if (textTracks && textTracks->length()) {
            // 9.1 Let HTMLMediaElement textTracks list equal the TextTrackList object returned by the textTracks
            // attribute on the HTMLMediaElement.
            // 9.2 Let the removed enabled text track flag equal false.
            bool removedEnabledTextTrack = false;

            // 9.3 For each TextTrack object in the SourceBuffer textTracks list, run the following steps:
            while (textTracks->length()) {
                auto& track = *textTracks->lastItem();

                // 9.3.1 Set the sourceBuffer attribute on the TextTrack object to null.
                track.setSourceBuffer(nullptr);

                // 9.3.2 If the mode attribute on the TextTrack object is set to "showing" or "hidden", then
                // set the removed enabled text track flag to true.
                if (track.mode() == TextTrack::Mode::Showing || track.mode() == TextTrack::Mode::Hidden)
                    removedEnabledTextTrack = true;

                // 9.3.3 Remove the TextTrack object from the HTMLMediaElement textTracks list.
                // 9.3.4 Queue a task to fire a trusted event named removetrack, that does not bubble and is not
                // cancelable, and that uses the TrackEvent interface, at the HTMLMediaElement textTracks list.
                if (mediaElement())
                    mediaElement()->removeTextTrack(track);

                // 9.3.5 Remove the TextTrack object from the SourceBuffer textTracks list.
                // 9.3.6 Queue a task to fire a trusted event named removetrack, that does not bubble and is not
                // cancelable, and that uses the TrackEvent interface, at the SourceBuffer textTracks list.
                textTracks->remove(track);
            }

            // 9.4 If the removed enabled text track flag equals true, then queue a task to fire a simple event
            // named change at the HTMLMediaElement textTracks list.
            if (removedEnabledTextTrack)
                mediaElement()->ensureTextTracks().scheduleChangeEvent();
        }
    }

    // 10. If sourceBuffer is in activeSourceBuffers, then remove sourceBuffer from activeSourceBuffers ...
    m_activeSourceBuffers->remove(buffer);

    // 11. Remove sourceBuffer from sourceBuffers and fire a removesourcebuffer event
    // on that object.
    m_sourceBuffers->remove(buffer);

    // 12. Destroy all resources for sourceBuffer.
    buffer.removedFromMediaSource();

    return { };
}

bool MediaSource::isTypeSupported(ScriptExecutionContext& context, const String& type)
{
    Vector<ContentType> mediaContentTypesRequiringHardwareSupport;
    if (context.isDocument()) {
        auto& document = downcast<Document>(context);
        mediaContentTypesRequiringHardwareSupport.appendVector(document.settings().mediaContentTypesRequiringHardwareSupport());
    }

    return isTypeSupported(type, WTFMove(mediaContentTypesRequiringHardwareSupport));
}

bool MediaSource::isTypeSupported(const String& type, Vector<ContentType>&& contentTypesRequiringHardwareSupport)
{
    // Section 2.2 isTypeSupported() method steps.
    // https://dvcs.w3.org/hg/html-media/raw-file/tip/media-source/media-source.html#widl-MediaSource-isTypeSupported-boolean-DOMString-type
    // 1. If type is an empty string, then return false.
    if (type.isNull() || type.isEmpty())
        return false;

    ContentType contentType(type);
    String codecs = contentType.parameter("codecs");

    // 2. If type does not contain a valid MIME type string, then return false.
    if (contentType.containerType().isEmpty())
        return false;

    // 3. If type contains a media type or media subtype that the MediaSource does not support, then return false.
    // 4. If type contains at a codec that the MediaSource does not support, then return false.
    // 5. If the MediaSource does not support the specified combination of media type, media subtype, and codecs then return false.
    // 6. Return true.
    MediaEngineSupportParameters parameters;
    parameters.type = contentType;
    parameters.isMediaSource = true;
    parameters.contentTypesRequiringHardwareSupport = WTFMove(contentTypesRequiringHardwareSupport);

    MediaPlayer::SupportsType supported = MediaPlayer::supportsType(parameters);

    if (codecs.isEmpty())
        return supported != MediaPlayer::SupportsType::IsNotSupported;

    return supported == MediaPlayer::SupportsType::IsSupported;
}

bool MediaSource::isOpen() const
{
    return readyState() == ReadyState::Open;
}

bool MediaSource::isClosed() const
{
    return readyState() == ReadyState::Closed;
}

bool MediaSource::isEnded() const
{
    return readyState() == ReadyState::Ended;
}

void MediaSource::detachFromElement(HTMLMediaElement& element)
{
    ALWAYS_LOG(LOGIDENTIFIER);

    ASSERT_UNUSED(element, m_mediaElement == &element);

    // 2.4.2 Detaching from a media element
    // https://rawgit.com/w3c/media-source/45627646344eea0170dd1cbc5a3d508ca751abb8/media-source-respec.html#mediasource-detach

    // 1. Set the readyState attribute to "closed".
    // 7. Queue a task to fire a simple event named sourceclose at the MediaSource.
    setReadyState(ReadyState::Closed);

    // 2. Update duration to NaN.
    m_duration = MediaTime::invalidTime();

    // 3. Remove all the SourceBuffer objects from activeSourceBuffers.
    // 4. Queue a task to fire a simple event named removesourcebuffer at activeSourceBuffers.
    while (m_activeSourceBuffers->length())
        removeSourceBuffer(*m_activeSourceBuffers->item(0));

    // 5. Remove all the SourceBuffer objects from sourceBuffers.
    // 6. Queue a task to fire a simple event named removesourcebuffer at sourceBuffers.
    while (m_sourceBuffers->length())
        removeSourceBuffer(*m_sourceBuffers->item(0));

    m_private = nullptr;
    m_mediaElement = nullptr;
}

void MediaSource::sourceBufferDidChangeActiveState(SourceBuffer&, bool)
{
    regenerateActiveSourceBuffers();
}

bool MediaSource::attachToElement(HTMLMediaElement& element)
{
    if (m_mediaElement)
        return false;

    ASSERT(isClosed());

    m_mediaElement = makeWeakPtr(&element);
    return true;
}

void MediaSource::openIfInEndedState()
{
    if (m_readyState != ReadyState::Ended)
        return;

    ALWAYS_LOG(LOGIDENTIFIER);

    setReadyState(ReadyState::Open);
    m_private->unmarkEndOfStream();
}

bool MediaSource::virtualHasPendingActivity() const
{
    return m_private || m_asyncEventQueue->hasPendingActivity();
}

void MediaSource::stop()
{
    ALWAYS_LOG(LOGIDENTIFIER);

    if (m_mediaElement)
        m_mediaElement->detachMediaSource();
    m_readyState = ReadyState::Closed;
    m_private = nullptr;
}

const char* MediaSource::activeDOMObjectName() const
{
    return "MediaSource";
}

void MediaSource::onReadyStateChange(ReadyState oldState, ReadyState newState)
{
    ALWAYS_LOG(LOGIDENTIFIER, "old state = ", oldState, ", new state = ", newState);

    for (auto& buffer : *m_sourceBuffers)
        buffer->readyStateChanged();

    if (isOpen()) {
        scheduleEvent(eventNames().sourceopenEvent);
        return;
    }

    if (oldState == ReadyState::Open && newState == ReadyState::Ended) {
        scheduleEvent(eventNames().sourceendedEvent);
        return;
    }

    ASSERT(isClosed());
    scheduleEvent(eventNames().sourcecloseEvent);
}

Vector<PlatformTimeRanges> MediaSource::activeRanges() const
{
    Vector<PlatformTimeRanges> activeRanges;
    for (auto& sourceBuffer : *m_activeSourceBuffers)
        activeRanges.append(sourceBuffer->bufferedInternal().ranges());
    return activeRanges;
}

ExceptionOr<Ref<SourceBufferPrivate>> MediaSource::createSourceBufferPrivate(const ContentType& type)
{
    RefPtr<SourceBufferPrivate> sourceBufferPrivate;
    switch (m_private->addSourceBuffer(type, sourceBufferPrivate)) {
    case MediaSourcePrivate::Ok:
        return sourceBufferPrivate.releaseNonNull();
    case MediaSourcePrivate::NotSupported:
        // 2.2 https://dvcs.w3.org/hg/html-media/raw-file/default/media-source/media-source.html#widl-MediaSource-addSourceBuffer-SourceBuffer-DOMString-type
        // Step 2: If type contains a MIME type ... that is not supported with the types
        // specified for the other SourceBuffer objects in sourceBuffers, then throw
        // a NotSupportedError exception and abort these steps.
        return Exception { NotSupportedError };
    case MediaSourcePrivate::ReachedIdLimit:
        // 2.2 https://dvcs.w3.org/hg/html-media/raw-file/default/media-source/media-source.html#widl-MediaSource-addSourceBuffer-SourceBuffer-DOMString-type
        // Step 3: If the user agent can't handle any more SourceBuffer objects then throw
        // a QuotaExceededError exception and abort these steps.
        return Exception { QuotaExceededError };
    }

    ASSERT_NOT_REACHED();
    return Exception { QuotaExceededError };
}

void MediaSource::scheduleEvent(const AtomString& eventName)
{
    DEBUG_LOG(LOGIDENTIFIER, "scheduling '", eventName, "'");

    auto event = Event::create(eventName, Event::CanBubble::No, Event::IsCancelable::No);
    event->setTarget(this);

    m_asyncEventQueue->enqueueEvent(WTFMove(event));
}

ScriptExecutionContext* MediaSource::scriptExecutionContext() const
{
    return ActiveDOMObject::scriptExecutionContext();
}

EventTargetInterface MediaSource::eventTargetInterface() const
{
    return MediaSourceEventTargetInterfaceType;
}

URLRegistry& MediaSource::registry() const
{
    return MediaSourceRegistry::registry();
}

void MediaSource::regenerateActiveSourceBuffers()
{
    Vector<RefPtr<SourceBuffer>> newList;
    for (auto& sourceBuffer : *m_sourceBuffers) {
        if (sourceBuffer->active())
            newList.append(sourceBuffer);
    }
    m_activeSourceBuffers->swap(newList);
    for (auto& sourceBuffer : *m_activeSourceBuffers)
        sourceBuffer->setBufferedDirty(true);
}

#if !RELEASE_LOG_DISABLED
void MediaSource::setLogIdentifier(const void* identifier)
{
    m_logIdentifier = identifier;
    ALWAYS_LOG(LOGIDENTIFIER);
}

WTFLogChannel& MediaSource::logChannel() const
{
    return LogMediaSource;
}
#endif

void MediaSource::failedToCreateRenderer(RendererType type)
{
    if (auto context = scriptExecutionContext())
        context->addConsoleMessage(MessageSource::JS, MessageLevel::Error, makeString("MediaSource ", type == RendererType::Video ? "video" : "audio", " renderer creation failed."));
}

}

#endif
