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

#include "AudioTrack.h"
#include "AudioTrackList.h"
#include "AudioTrackPrivate.h"
#include "ContentType.h"
#include "ContentTypeUtilities.h"
#include "DeprecatedGlobalSettings.h"
#include "DocumentInlines.h"
#include "Event.h"
#include "EventNames.h"
#include "HTMLMediaElement.h"
#include "InbandTextTrack.h"
#include "InbandTextTrackPrivate.h"
#include "Logging.h"
#include "ManagedMediaSource.h"
#include "ManagedSourceBuffer.h"
#if ENABLE(MEDIA_SOURCE_IN_WORKERS)
#include "MediaSourceHandle.h"
#endif
#include "MediaSourcePrivate.h"
#include "MediaSourceRegistry.h"
#include "MediaStrategy.h"
#include "PlatformStrategies.h"
#include "Quirks.h"
#include "ScriptExecutionContext.h"
#include "Settings.h"
#include "SourceBuffer.h"
#include "SourceBufferList.h"
#include "SourceBufferPrivate.h"
#include "TextTrackList.h"
#include "TimeRanges.h"
#include "VideoTrack.h"
#include "VideoTrackList.h"
#include "VideoTrackPrivate.h"
#include <wtf/NativePromise.h>
#include <wtf/RunLoop.h>
#include <wtf/Scope.h>
#include <wtf/TZoneMallocInlines.h>
#include <wtf/text/MakeString.h>

namespace WebCore {

WTF_MAKE_TZONE_OR_ISO_ALLOCATED_IMPL(MediaSource);

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
    ASSERT(static_cast<size_t>(enumerationValue) < std::size(values));
    return values[static_cast<size_t>(enumerationValue)];
}

String convertEnumerationToString(MediaSourcePrivate::EndOfStreamStatus enumerationValue)
{
    static const NeverDestroyed<String> values[] = {
        MAKE_STATIC_STRING_IMPL("NoError"),
        MAKE_STATIC_STRING_IMPL("NetworkError"),
        MAKE_STATIC_STRING_IMPL("DecodeError"),
    };
    static_assert(static_cast<size_t>(MediaSourcePrivate::EndOfStreamStatus::NoError) == 0, "MediaSourcePrivate::EndOfStreamStatus::NoError is not 0 as expected");
    static_assert(static_cast<size_t>(MediaSourcePrivate::EndOfStreamStatus::NetworkError) == 1, "MediaSourcePrivate::EndOfStreamStatus::NetworkError is not 1 as expected");
    static_assert(static_cast<size_t>(MediaSourcePrivate::EndOfStreamStatus::DecodeError) == 2, "MediaSourcePrivate::EndOfStreamStatus::DecodeError is not 2 as expected");
    ASSERT(static_cast<size_t>(enumerationValue) < std::size(values));
    return values[static_cast<size_t>(enumerationValue)];
}

class MediaSourceClientImpl final : public MediaSourcePrivateClient {
public:
    static Ref<MediaSourceClientImpl> create(MediaSource& parent) { return adoptRef(*new MediaSourceClientImpl(parent)); }

    void ensureWeakOnDispatcher(Function<void(MediaSource&)>&& function, bool forceRun = false) const
    {
        auto weakWrapper = [function = WTFMove(function), weakParent = m_parent, forceRun] {
            if (RefPtr parent = weakParent.get(); parent && (!parent->isClosed() || forceRun))
                function(*parent);
        };
        ScriptExecutionContext::ensureOnContextThread(m_identifier, [wrapper = WTFMove(weakWrapper)](auto&) {
            wrapper();
        });
    }

    void setMediaSourcePrivate(MediaSourcePrivate* source)
    {
        Locker locker { m_lock };
        m_private = source;
    }

private:
    explicit MediaSourceClientImpl(MediaSource& parent)
        : m_parent(parent)
        , m_identifier(parent.scriptExecutionContext()->identifier())
#if !RELEASE_LOG_DISABLED
        , m_logger(parent.logger())
#endif
        , m_private(parent.m_private)
        {
        }

    void setPrivateAndOpen(Ref<MediaSourcePrivate>&& mediaSourcePrivate) final
    {
        ensureWeakOnDispatcher([mediaSourcePrivate = WTFMove(mediaSourcePrivate)](MediaSource& parent) mutable {
            parent.setPrivateAndOpen(WTFMove(mediaSourcePrivate));
        }, true);
    }

    void reOpen() final
    {
        ensureWeakOnDispatcher([](MediaSource& parent) mutable {
            parent.reOpen();
        }, true);
    }

    Ref<MediaTimePromise> waitForTarget(const SeekTarget& target) final
    {
        MediaTimePromise::AutoRejectProducer producer(PlatformMediaError::SourceRemoved);
        auto promise = producer.promise();

        ensureWeakOnDispatcher([producer = WTFMove(producer), target](MediaSource& parent) mutable {
            parent.waitForTarget(target)->chainTo(WTFMove(producer));
        });
        return promise;
    }

    Ref<MediaPromise> seekToTime(const MediaTime& time) final
    {
        MediaPromise::AutoRejectProducer producer(PlatformMediaError::SourceRemoved);
        auto promise = producer.promise();

        ensureWeakOnDispatcher([producer = WTFMove(producer), time](MediaSource& parent) mutable {
            parent.seekToTime(time)->chainTo(WTFMove(producer));
        });
        return promise;
    }

    RefPtr<MediaSourcePrivate> mediaSourcePrivate() const final
    {
        Locker locker { m_lock };
        return m_private;
    }

    void failedToCreateRenderer(RendererType type)
    {
        ensureWeakOnDispatcher([type](MediaSource& parent) {
            parent.failedToCreateRenderer(type);
        });
    }

#if !RELEASE_LOG_DISABLED
    void setLogIdentifier(uint64_t identifier)
    {
        ensureWeakOnDispatcher([identifier](MediaSource& parent) {
            parent.setLogIdentifier(identifier);
        });
    }
#endif

    WeakPtr<MediaSource> m_parent;
    const ScriptExecutionContextIdentifier m_identifier;
#if !RELEASE_LOG_DISABLED
    Ref<const Logger> m_logger;
#endif
    mutable Lock m_lock;
    RefPtr<MediaSourcePrivate> m_private WTF_GUARDED_BY_LOCK(m_lock);
};

URLRegistry* MediaSource::s_registry;

void MediaSource::setRegistry(URLRegistry* registry)
{
    ASSERT(isMainThread());
    ASSERT(!s_registry);
    s_registry = registry;
}

Ref<MediaSource> MediaSource::create(ScriptExecutionContext& context, MediaSourceInit&& options)
{
    auto mediaSource = adoptRef(*new MediaSource(context, WTFMove(options)));
    mediaSource->suspendIfNeeded();
    return mediaSource;
}

MediaSource::MediaSource(ScriptExecutionContext& context, MediaSourceInit&& options)
    : ActiveDOMObject(&context)
    , m_detachable(context.settingsValues().detachableMediaSourceEnabled ? options.detachable : false)
    , m_sourceBuffers(SourceBufferList::create(scriptExecutionContext()))
    , m_activeSourceBuffers(SourceBufferList::create(scriptExecutionContext()))
#if !RELEASE_LOG_DISABLED
    , m_logger(logger(context))
#endif
    , m_client(MediaSourceClientImpl::create(*this))
{
}

MediaSource::~MediaSource()
{
    ALWAYS_LOG(LOGIDENTIFIER);

    m_detachable = false;

#if ENABLE(MEDIA_SOURCE_IN_WORKERS)
    if (!isMainThread()) {
        // When deleted on a worker; the HTMLMediaElement wouldn't have started the deletion.
        // We need to manually detach ourselves and notify the HTMLMediaElement.
        ensureWeakOnHTMLMediaElementContext([](auto& mediaElement) {
            mediaElement.mediaSourceWasDetached();
        });
    }
#endif
    detachFromElement();

    ASSERT(isClosed());
}

#if !RELEASE_LOG_DISABLED

Ref<Logger> MediaSource::logger(ScriptExecutionContext& context)
{
    if (RefPtr document = dynamicDowncast<Document>(context))
        return document->logger();

    Ref logger = Logger::create(this);
    logger->addObserver(*this);
    return logger;
}

void MediaSource::didLogMessage(const WTFLogChannel&, WTFLogLevel, Vector<JSONLogValue>&&)
{
    // FIXME: Add logging for when MediaSource is running in worker.
}

#endif

void MediaSource::setPrivate(RefPtr<MediaSourcePrivate>&& mediaSourcePrivate)
{
    m_client->setMediaSourcePrivate(mediaSourcePrivate.get());
    m_private = WTFMove(mediaSourcePrivate);
}

void MediaSource::setPrivateAndOpen(Ref<MediaSourcePrivate>&& mediaSourcePrivate)
{
    DEBUG_LOG(LOGIDENTIFIER);
    ASSERT(!m_private);

    setPrivate(WTFMove(mediaSourcePrivate));
    m_private->setTimeFudgeFactor(currentTimeFudgeFactor());

    open();
}

void MediaSource::reOpen()
{
    DEBUG_LOG(LOGIDENTIFIER);
    ASSERT(detachable());
    ASSERT(m_private);

    open();

    for (auto& sourceBuffer : m_sourceBuffers.get())
        sourceBuffer->attach();
}

void MediaSource::open()
{
    // 2.4.1 Attaching to a media element
    // https://rawgit.com/w3c/media-source/45627646344eea0170dd1cbc5a3d508ca751abb8/media-source-respec.html#mediasource-attach

    // ↳ If readyState is NOT set to "closed"
    //    Run the "If the media data cannot be fetched at all, due to network errors, causing the user agent to give up trying
    //    to fetch the resource" steps of the resource fetch algorithm's media data processing steps list.
    if (!isClosed()) {
        ensureWeakOnHTMLMediaElementContext([](auto& mediaElement) {
            mediaElement.mediaLoadingFailedFatally(MediaPlayer::NetworkState::NetworkError);
        });
        return;
    }

    // ↳ Otherwise
    // 1. Set the MediaSource's [[has ever been attached]] internal slot to true.
#if ENABLE(MEDIA_SOURCE_IN_WORKERS)
    if (m_handle) {
        m_handle->setHasEverBeenAssignedAsSrcObject();
        m_handle->mediaSourceDidOpen(*m_private);
    }
#endif

    // 2. Set the media element's delaying-the-load-event-flag to false.
    ensureWeakOnHTMLMediaElementContext([](auto& mediaElement) {
        mediaElement.setShouldDelayLoadEvent(false);
    });

    if (isManaged())
        m_openDeferred = true;

    // 2. Set the readyState attribute to "open".
    // 3. Queue a task to fire a simple event named sourceopen at the MediaSource.
    setReadyState(m_readyStateBeforeDetached.value_or(ReadyState::Open));

    // 4. Continue the resource fetch algorithm by running the remaining "Otherwise (mode is local)" steps,
    // with these clarifications:
    // NOTE: This is handled in HTMLMediaElement.

    openIfDeferredOpen();
}

void MediaSource::addedToRegistry()
{
    DEBUG_LOG(LOGIDENTIFIER);
    ++m_associatedRegistryCount;
}

void MediaSource::removedFromRegistry()
{
    DEBUG_LOG(LOGIDENTIFIER);
    ASSERT(m_associatedRegistryCount);
    --m_associatedRegistryCount;
}

MediaTime MediaSource::duration() const
{
    // 1. If the readyState attribute is "closed" then return NaN and abort these steps.
    // 2. Return the current value of the attribute.

    return m_private ? m_private->duration() : MediaTime::invalidTime();
}

MediaTime MediaSource::currentTime() const
{
    if (m_pendingSeekTarget)
        return m_pendingSeekTarget->time;

    return m_private ? m_private->currentTime() : MediaTime::zeroTime();
}

PlatformTimeRanges MediaSource::buffered() const
{
    return isClosed() ? PlatformTimeRanges::emptyRanges() : m_private->buffered();
}

Ref<MediaTimePromise> MediaSource::waitForTarget(const SeekTarget& target)
{
    ALWAYS_LOG(LOGIDENTIFIER, target.time);

    // 2.4.3 Seeking
    // https://rawgit.com/w3c/media-source/45627646344eea0170dd1cbc5a3d508ca751abb8/media-source-respec.html#mediasource-seeking

    if (m_seekTargetPromise) {
        ALWAYS_LOG(LOGIDENTIFIER, "Previous seeking to ", m_pendingSeekTarget->time, "pending, cancelling it");
        m_seekTargetPromise->reject(PlatformMediaError::Cancelled);
    }
    m_seekTargetPromise.emplace(PlatformMediaError::SourceRemoved);
    m_pendingSeekTarget = target;

    // Run the following steps as part of the "Wait until the user agent has established whether or not the
    // media data for the new playback position is available, and, if it is, until it has decoded enough data
    // to play back that position" step of the seek algorithm:
    // ↳ If new playback position is not in any TimeRange of HTMLMediaElement.buffered
    if (!hasBufferedTime(target.time)) {
        ALWAYS_LOG(LOGIDENTIFIER, "No data at seeked time, waiting");
        // 1. If the HTMLMediaElement.readyState attribute is greater than HAVE_METADATA,
        // then set the HTMLMediaElement.readyState attribute to HAVE_METADATA.
        m_private->setMediaPlayerReadyState(MediaPlayer::ReadyState::HaveMetadata);

        // 2. The media element waits until an appendBuffer() or an appendStream() call causes the coded
        // frame processing algorithm to set the HTMLMediaElement.readyState attribute to a value greater
        // than HAVE_METADATA.
        monitorSourceBuffers();

        return m_seekTargetPromise->promise();
    }
    // ↳ Otherwise
    // Continue
    auto promise = m_seekTargetPromise->promise();
    completeSeek();
    return promise;
}

void MediaSource::completeSeek()
{
    if (isClosed())
        return;
    // 2.4.3 Seeking, ctd.
    // https://dvcs.w3.org/hg/html-media/raw-file/tip/media-source/media-source.html#mediasource-seeking

    ASSERT(m_pendingSeekTarget && m_seekTargetPromise);

    ALWAYS_LOG(LOGIDENTIFIER, m_pendingSeekTarget->time);

    // 2. The media element resets all decoders and initializes each one with data from the appropriate
    // initialization segment.
    // 3. The media element feeds coded frames from the active track buffers into the decoders starting
    // with the closest random access point before the new playback position.
    auto seekTarget = *m_pendingSeekTarget;
    m_pendingSeekTarget.reset();

    MediaTimePromise::AutoRejectProducer producer(PlatformMediaError::SourceRemoved);
    Ref promise = producer.promise();

    scriptExecutionContext()->enqueueTaskWhenSettled(SourceBuffer::ComputeSeekPromise::all(WTF::map(m_activeSourceBuffers.get(), [&](auto&& sourceBuffer) {
        return sourceBuffer->computeSeekTime(seekTarget);
    })), TaskSource::MediaElement, [producer = WTFMove(producer), weakThis = WeakPtr { *this }, this, time = seekTarget.time](auto&& results) {
        RefPtr protectedThis = weakThis.get();
        if (!protectedThis || isClosed())
            return;

        if (!results)
            return producer.reject(results.error());

        auto seekTime = time;
        for (auto& result : *results) {
            if (abs(time - result) > abs(time - seekTime))
                seekTime = result;
        }

        // 4. Resume the seek algorithm at the "Await a stable state" step.
        monitorSourceBuffers();

        producer.resolve(seekTime);
    });
    promise->chainTo(WTFMove(*m_seekTargetPromise));
    m_seekTargetPromise.reset();
}

Ref<MediaPromise> MediaSource::seekToTime(const MediaTime& time)
{
    for (auto& sourceBuffer : m_activeSourceBuffers.get())
        sourceBuffer->seekToTime(time);
    return MediaPromise::createAndResolve();
}

Ref<TimeRanges> MediaSource::seekable()
{
    return m_private ? TimeRanges::create(m_private->seekable()) : TimeRanges::create();
}

ExceptionOr<void> MediaSource::setLiveSeekableRange(double start, double end)
{
    // W3C Editor's Draft 16 September 2016
    // https://rawgit.com/w3c/media-source/45627646344eea0170dd1cbc5a3d508ca751abb8/media-source-respec.html#dom-mediasource-setliveseekablerange

    ALWAYS_LOG(LOGIDENTIFIER, "start = ", start, ", end = ", end);

    // If the readyState attribute is not "open" then throw an InvalidStateError exception and abort these steps.
    if (!isOpen())
        return Exception { ExceptionCode::InvalidStateError };

    // If start is negative or greater than end, then throw a TypeError exception and abort these steps.
    if (start < 0 || start > end)
        return Exception { ExceptionCode::TypeError };

    // Set live seekable range to be a new normalized TimeRanges object containing a single range
    // whose start position is start and end position is end.
    m_private->setLiveSeekableRange({ MediaTime::createWithDouble(start), MediaTime::createWithDouble(end) });

    return { };
}

ExceptionOr<void> MediaSource::clearLiveSeekableRange()
{
    // W3C Editor's Draft 16 September 2016
    // https://rawgit.com/w3c/media-source/45627646344eea0170dd1cbc5a3d508ca751abb8/media-source-respec.html#dom-mediasource-clearliveseekablerange

    ALWAYS_LOG(LOGIDENTIFIER);

    // If the readyState attribute is not "open" then throw an InvalidStateError exception and abort these steps.
    if (!isOpen())
        return Exception { ExceptionCode::InvalidStateError };
    m_private->clearLiveSeekableRange();
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
    return contentType.containerType() == "audio/aac"_s || contentType.containerType() == "audio/mpeg"_s;
}

bool MediaSource::hasBufferedTime(const MediaTime& time)
{
    if (isClosed())
        return false;

    if (time.isInvalid())
        return false;

    if (time > duration())
        return false;

    auto ranges = m_private->buffered();
    if (!ranges.length())
        return false;

    return abs(ranges.nearest(time) - time) <= m_private->timeFudgeFactor();
}

bool MediaSource::hasCurrentTime()
{
    return hasBufferedTime(currentTime());
}

bool MediaSource::hasFutureTime()
{
    if (isClosed())
        return false;

    return m_private->hasFutureTime(currentTime(), m_private->timeIsProgressing() ? MediaTime::zeroTime() : MediaSourcePrivate::futureDataThreshold());
}

bool MediaSource::isBuffered(const PlatformTimeRanges& ranges) const
{
    if (ranges.length() < 1 || isClosed())
        return true;

    ASSERT(ranges.length() == 1);

    auto bufferedRanges = m_private->buffered();
    if (!bufferedRanges.length())
        return false;
    bufferedRanges.intersectWith(ranges);

    if (!bufferedRanges.length())
        return false;

    auto hasBufferedTime = [&] (const MediaTime& time) {
        return abs(bufferedRanges.nearest(time) - time) <= m_private->timeFudgeFactor();
    };

    if (!hasBufferedTime(ranges.minimumBufferedTime()) || !hasBufferedTime(ranges.maximumBufferedTime()))
        return false;

    if (bufferedRanges.length() == 1)
        return true;

    // Ensure that if we have a gap in the buffered range, it is smaller than the fudge factor;
    for (unsigned i = 1; i < bufferedRanges.length(); i++) {
        if (bufferedRanges.end(i) - bufferedRanges.start(i-1) > m_private->timeFudgeFactor())
            return false;
    }

    return true;
}

void MediaSource::monitorSourceBuffers()
{
    // 2.4.4 SourceBuffer Monitoring
    // https://rawgit.com/w3c/media-source/45627646344eea0170dd1cbc5a3d508ca751abb8/media-source-respec.html#buffer-monitoring

    // Note, the behavior if activeSourceBuffers is empty is undefined.

    // ↳ If the HTMLMediaElement.readyState attribute equals HAVE_NOTHING:
    if (m_private->mediaPlayerReadyState() == MediaPlayer::ReadyState::HaveNothing) {
        // 1. Abort these steps.
        return;
    }

    // ↳ If HTMLMediaElement.buffered does not contain a TimeRange for the current playback position:
    if (!hasCurrentTime()) {
        // 1. Set the HTMLMediaElement.readyState attribute to HAVE_METADATA.
        // 2. If this is the first transition to HAVE_METADATA, then queue a task to fire a simple event
        // named loadedmetadata at the media element.
        m_private->setMediaPlayerReadyState(MediaPlayer::ReadyState::HaveMetadata);

        // 3. Abort these steps.
        return;
    }

    // ↳ If HTMLMediaElement.buffered contains a TimeRange that includes the current
    //  playback position and enough data to ensure uninterrupted playback:

    // If we have data up to 3s ahead, we can assume that we can play without interruption.
    constexpr double kHaveEnoughDataThreshold = 3;
    auto currentTime = this->currentTime();
    auto limitAhead = [&] (double upper) {
        MediaTime aheadTime = currentTime + MediaTime::createWithDouble(upper);
        return isEnded() ? std::min(duration(), aheadTime) : aheadTime;
    };
    PlatformTimeRanges neededBufferedRange { currentTime, std::max(currentTime, limitAhead(kHaveEnoughDataThreshold)) };

    if (isBuffered(neededBufferedRange)) {
        // 1. Set the HTMLMediaElement.readyState attribute to HAVE_ENOUGH_DATA.
        // 2. Queue a task to fire a simple event named canplaythrough at the media element.
        // 3. Playback may resume at this point if it was previously suspended by a transition to HAVE_CURRENT_DATA.
        m_private->setMediaPlayerReadyState(MediaPlayer::ReadyState::HaveEnoughData);

        if (m_pendingSeekTarget)
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
        m_private->setMediaPlayerReadyState(MediaPlayer::ReadyState::HaveFutureData);

        if (m_pendingSeekTarget)
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
    m_private->setMediaPlayerReadyState(MediaPlayer::ReadyState::HaveCurrentData);

    if (m_pendingSeekTarget)
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
        return Exception { ExceptionCode::TypeError };

    // 2. If the readyState attribute is not "open" then throw an InvalidStateError exception and abort these steps.
    if (!isOpen())
        return Exception { ExceptionCode::InvalidStateError };

    // 3. If the updating attribute equals true on any SourceBuffer in sourceBuffers, then throw an InvalidStateError
    // exception and abort these steps.
    for (auto& sourceBuffer : m_sourceBuffers.get()) {
        if (sourceBuffer->updating())
            return Exception { ExceptionCode::InvalidStateError };
    }

    // 4. Run the duration change algorithm with new duration set to the value being assigned to this attribute.
    return setDurationInternal(MediaTime::createWithDouble(duration));
}

ExceptionOr<void> MediaSource::setDurationInternal(const MediaTime& newDuration)
{
    // 2.4.6 Duration Change
    // https://www.w3.org/TR/2016/REC-media-source-20161117/#duration-change-algorithm

    // 1. If the current value of duration is equal to new duration, then return.
    if (newDuration == duration())
        return { };

    // 2. If new duration is less than the highest presentation timestamp of any buffered coded frames
    // for all SourceBuffer objects in sourceBuffers, then throw an InvalidStateError exception and
    // abort these steps.
    // 3. Let highest end time be the largest track buffer ranges end time across all the track buffers
    // across all SourceBuffer objects in sourceBuffers.
    MediaTime highestPresentationTimestamp;
    MediaTime highestEndTime;
    for (auto& sourceBuffer : m_sourceBuffers.get()) {
        highestPresentationTimestamp = std::max(highestPresentationTimestamp, sourceBuffer->highestPresentationTimestamp());
        highestEndTime = std::max(highestEndTime, sourceBuffer->bufferedInternal().maximumBufferedTime());
    }
    if (highestPresentationTimestamp.isValid() && newDuration < highestPresentationTimestamp)
        return Exception { ExceptionCode::InvalidStateError };

    // 4. If new duration is less than highest end time, then
    // 4.1. Update new duration to equal highest end time.
    auto duration = highestEndTime.isValid() && newDuration < highestEndTime ? highestEndTime : newDuration;

    ALWAYS_LOG(LOGIDENTIFIER, duration);

    // 5. Update duration to new duration.
    // 6. Update the media duration to new duration and run the HTMLMediaElement duration change algorithm.
    m_private->durationChanged(duration);

    // Changing the duration affects the buffered range.
    monitorSourceBuffers();

    return { };
}

void MediaSource::setReadyState(ReadyState state)
{
    auto oldState = readyState();
    if (oldState == state)
        return;

    if (m_private)
        m_private->setReadyState(state);

    onReadyStateChange(oldState, readyState());
}

ExceptionOr<void> MediaSource::endOfStream(std::optional<EndOfStreamError> error)
{
    ALWAYS_LOG(LOGIDENTIFIER);

    // 2.2 https://dvcs.w3.org/hg/html-media/raw-file/tip/media-source/media-source.html#widl-MediaSource-endOfStream-void-EndOfStreamError-error
    // 1. If the readyState attribute is not in the "open" state then throw an
    // InvalidStateError exception and abort these steps.
    if (!isOpen())
        return Exception { ExceptionCode::InvalidStateError };

    // 2. If the updating attribute equals true on any SourceBuffer in sourceBuffers, then throw an
    // InvalidStateError exception and abort these steps.
    if (std::any_of(m_sourceBuffers->begin(), m_sourceBuffers->end(), [](auto& sourceBuffer) { return sourceBuffer->updating(); }))
        return Exception { ExceptionCode::InvalidStateError };

    // 3. Run the end of stream algorithm with the error parameter set to error.
    streamEndedWithError(error);

    return { };
}

void MediaSource::streamEndedWithError(std::optional<EndOfStreamError> error)
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
        for (auto& sourceBuffer : m_sourceBuffers.get()) {
            if (auto length = sourceBuffer->bufferedInternal().length())
                maxEndTime = std::max(sourceBuffer->bufferedInternal().end(length - 1), maxEndTime);
        }
        setDurationInternal(maxEndTime);

        // 2. Notify the media element that it now has all of the media data.
        m_private->markEndOfStream(MediaSourcePrivate::EndOfStreamStatus::NoError);
        return;
    }

    bool failedFatally = false;
    MediaPlayer::NetworkState mediaElementNextState = MediaPlayer::NetworkState::NetworkError;

    if (error == EndOfStreamError::Network) {
        m_private->markEndOfStream(MediaSourcePrivate::EndOfStreamStatus::NetworkError);
        // ↳ If error is set to "network"
        if (m_private->mediaPlayerReadyState() == MediaPlayerReadyState::HaveNothing) {
            //  ↳ If the HTMLMediaElement.readyState attribute equals HAVE_NOTHING
            //    Run the "If the media data cannot be fetched at all, due to network errors, causing
            //    the user agent to give up trying to fetch the resource" steps of the resource fetch algorithm.
            //    NOTE: This step is handled by HTMLMediaElement::mediaLoadingFailed().
            mediaElementNextState = MediaPlayer::NetworkState::NetworkError;
        } else {
            //  ↳ If the HTMLMediaElement.readyState attribute is greater than HAVE_NOTHING
            //    Run the "If the connection is interrupted after some media data has been received, causing the
            //    user agent to give up trying to fetch the resource" steps of the resource fetch algorithm.
            //    NOTE: This step is handled by HTMLMediaElement::mediaLoadingFailedFatally().
            mediaElementNextState = MediaPlayer::NetworkState::NetworkError;
            failedFatally = true;
        }
    } else {
        // ↳ If error is set to "decode"
        ASSERT(error == EndOfStreamError::Decode);
        m_private->markEndOfStream(MediaSourcePrivate::EndOfStreamStatus::DecodeError);

        if (m_private->mediaPlayerReadyState() == MediaPlayerReadyState::HaveNothing) {
            //  ↳ If the HTMLMediaElement.readyState attribute equals HAVE_NOTHING
            //    Run the "If the media data can be fetched but is found by inspection to be in an unsupported
            //    format, or can otherwise not be rendered at all" steps of the resource fetch algorithm.
            //    NOTE: This step is handled by HTMLMediaElement::mediaLoadingFailed().
            mediaElementNextState = MediaPlayer::NetworkState::FormatError;
        } else {
            //  ↳ If the HTMLMediaElement.readyState attribute is greater than HAVE_NOTHING
            //    Run the media data is corrupted steps of the resource fetch algorithm.
            //    NOTE: This step is handled by HTMLMediaElement::mediaLoadingFailedFatally().
            mediaElementNextState = MediaPlayer::NetworkState::DecodeError;
            failedFatally = true;
        }
    }

    ensureWeakOnHTMLMediaElementContext([mediaElementNextState, failedFatally](auto& mediaElement) {
        if (failedFatally)
            mediaElement.mediaLoadingFailedFatally(mediaElementNextState);
        else
            mediaElement.mediaLoadingFailed(mediaElementNextState);
    });
}

static ContentType addVP9FullRangeVideoFlagToContentType(const ContentType& type)
{
    auto countPeriods = [] (const String& codec) {
        unsigned count = 0;
        unsigned position = 0;

        while (codec.find('.', position) != notFound) {
            ++count;
            ++position;
        }

        return count;
    };

    for (auto codec : type.codecs()) {
        if (!codec.startsWith("vp09"_s) || countPeriods(codec) != 7)
            continue;

        auto rawType = type.raw();
        auto position = rawType.find(codec);
        ASSERT(position != notFound);
        if (position == notFound)
            continue;

        return ContentType(makeStringByInserting(rawType, ".00"_s, position + codec.length()));
    }
    return type;
}

ExceptionOr<Ref<SourceBuffer>> MediaSource::addSourceBuffer(const String& type)
{
    DEBUG_LOG(LOGIDENTIFIER, type);

    // 2.2 http://www.w3.org/TR/media-source/#widl-MediaSource-addSourceBuffer-SourceBuffer-DOMString-type
    // When this method is invoked, the user agent must run the following steps:

    // 1. If type is an empty string then throw a TypeError exception and abort these steps.
    if (type.isEmpty())
        return Exception { ExceptionCode::TypeError };

    auto context = scriptExecutionContext();
    if (!context)
        return Exception { ExceptionCode::NotAllowedError };

    // 2. If type contains a MIME type that is not supported ..., then throw a
    // NotSupportedError exception and abort these steps.
    Vector<ContentType> mediaContentTypesRequiringHardwareSupport;
    if (RefPtr document = dynamicDowncast<Document>(context))
        mediaContentTypesRequiringHardwareSupport.appendVector(document->settings().mediaContentTypesRequiringHardwareSupport());

    if (!isTypeSupported(*context, type, WTFMove(mediaContentTypesRequiringHardwareSupport)))
        return Exception { ExceptionCode::NotSupportedError };

    // 4. If the readyState attribute is not in the "open" state then throw an
    // InvalidStateError exception and abort these steps.
    if (!isOpen())
        return Exception { ExceptionCode::InvalidStateError };

    // 5. Create a new SourceBuffer object and associated resources.
    ContentType contentType(type);
    RefPtr document = dynamicDowncast<Document>(context);
    if (document && document->quirks().needsVP9FullRangeFlagQuirk())
        contentType = addVP9FullRangeVideoFlagToContentType(contentType);

    auto sourceBufferPrivate = createSourceBufferPrivate(contentType);

    if (sourceBufferPrivate.hasException()) {
        // 2. If type contains a MIME type that is not supported ..., then throw a NotSupportedError exception and abort these steps.
        // 3. If the user agent can't handle any more SourceBuffer objects then throw a QuotaExceededError exception and abort these steps
        return sourceBufferPrivate.releaseException();
    }

    Ref<SourceBuffer> buffer =
        isManaged() ? ManagedSourceBuffer::create(sourceBufferPrivate.releaseReturnValue(), downcast<ManagedMediaSource>(*this)).get() : SourceBuffer::create(sourceBufferPrivate.releaseReturnValue(), *this).get();

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
        return Exception { ExceptionCode::NotFoundError };

    // 3. If the sourceBuffer.updating attribute equals true, then run the following steps: ...
    buffer.abortIfUpdating();

    removeSourceBufferWithOptionalDestruction(buffer, true);

    // 10. If sourceBuffer is in activeSourceBuffers, then remove sourceBuffer from activeSourceBuffers ...
    m_activeSourceBuffers->remove(buffer);

    // 11. Remove sourceBuffer from sourceBuffers and fire a removesourcebuffer event
    // on that object.
    m_sourceBuffers->remove(buffer);

    // 12. Destroy all resources for sourceBuffer.
    buffer.removedFromMediaSource();

    return { };
}

void MediaSource::removeSourceBufferWithOptionalDestruction(SourceBuffer& buffer, bool withDestruction)
{
    ASSERT(scriptExecutionContext());
    if (!scriptExecutionContext()->activeDOMObjectsAreStopped()) {
        // 4. Let SourceBuffer audioTracks list equal the AudioTrackList object returned by sourceBuffer.audioTracks.
        RefPtr audioTracks = buffer.audioTracksIfExists();

        // 5. If the SourceBuffer audioTracks list is not empty, then run the following steps:
        if (audioTracks && audioTracks->length()) {
            // 5.1 Let HTMLMediaElement audioTracks list equal the AudioTrackList object returned by the audioTracks
            // attribute on the HTMLMediaElement.
            // 5.2 Let the removed enabled audio track flag equal false.
            bool removedEnabledAudioTrack = false;

            // 5.3 For each AudioTrack object in the SourceBuffer audioTracks list, run the following steps:
            for (ssize_t index = audioTracks->length() - 1; index >= 0; index--) {
                auto& track = *audioTracks->item(index);

                if (withDestruction) {
                    // 5.3.1 Set the sourceBuffer attribute on the AudioTrack object to null.
                    track.setSourceBuffer(nullptr);
                }

                // 5.3.2 If the enabled attribute on the AudioTrack object is true, then set the removed enabled
                // audio track flag to true.
                if (track.enabled())
                    removedEnabledAudioTrack = true;

                // 5.3.3 Remove the AudioTrack object from the HTMLMediaElement audioTracks list.
                // 5.3.4 Queue a task to fire a trusted event named removetrack, that does not bubble and is not
                // cancelable, and that uses the TrackEvent interface, at the HTMLMediaElement audioTracks list.
                if (isMainThread()) {
                    ensureWeakOnHTMLMediaElementContext([track = Ref { track }](auto& mediaElement) mutable {
                        // FIXME: Need to send a mirror when we are in a worker.
                        mediaElement.removeAudioTrack(WTFMove(track));
                    });
                } else {
                    ensureWeakOnHTMLMediaElementContext([trackID = track.trackId()](auto& mediaElement) mutable {
                        mediaElement.removeAudioTrack(trackID);
                    });
                }

                if (withDestruction) {
                    // 5.3.5 Remove the AudioTrack object from the SourceBuffer audioTracks list.
                    // 5.3.6 Queue a task to fire a trusted event named removetrack, that does not bubble and is not
                    // cancelable, and that uses the TrackEvent interface, at the SourceBuffer audioTracks list.
                    audioTracks->remove(track);
                }
            }

            // 5.4 If the removed enabled audio track flag equals true, then queue a task to fire a simple event
            // named change at the HTMLMediaElement audioTracks list.
            if (removedEnabledAudioTrack) {
                ensureWeakOnHTMLMediaElementContext([](auto& mediaElement) {
                    mediaElement.ensureAudioTracks().scheduleChangeEvent();
                });
            }
        }

        // 6. Let SourceBuffer videoTracks list equal the VideoTrackList object returned by sourceBuffer.videoTracks.
        RefPtr videoTracks = buffer.videoTracksIfExists();

        // 7. If the SourceBuffer videoTracks list is not empty, then run the following steps:
        if (videoTracks && videoTracks->length()) {
            // 7.1 Let HTMLMediaElement videoTracks list equal the VideoTrackList object returned by the videoTracks
            // attribute on the HTMLMediaElement.
            // 7.2 Let the removed selected video track flag equal false.
            bool removedSelectedVideoTrack = false;

            // 7.3 For each VideoTrack object in the SourceBuffer videoTracks list, run the following steps:
            for (ssize_t index = videoTracks->length() - 1; index >= 0; index--) {
                auto& track = *videoTracks->item(index);

                if (withDestruction) {
                    // 7.3.1 Set the sourceBuffer attribute on the VideoTrack object to null.
                    track.setSourceBuffer(nullptr);
                }

                // 7.3.2 If the selected attribute on the VideoTrack object is true, then set the removed selected
                // video track flag to true.
                if (track.selected())
                    removedSelectedVideoTrack = true;

                // 7.3.3 Remove the VideoTrack object from the HTMLMediaElement videoTracks list.
                // 7.3.4 Queue a task to fire a trusted event named removetrack, that does not bubble and is not
                // cancelable, and that uses the TrackEvent interface, at the HTMLMediaElement videoTracks list.
                if (isMainThread()) {
                    ensureWeakOnHTMLMediaElementContext([track = Ref { track }](auto& mediaElement) mutable {
                        // FIXME: Need to send a mirror when we are in a worker.
                        mediaElement.removeVideoTrack(WTFMove(track));
                    });
                } else {
                    ensureWeakOnHTMLMediaElementContext([trackID = track.trackId()](auto& mediaElement) mutable {
                        mediaElement.removeVideoTrack(trackID);
                    });
                }

                if (withDestruction) {
                    // 7.3.5 Remove the VideoTrack object from the SourceBuffer videoTracks list.
                    // 7.3.6 Queue a task to fire a trusted event named removetrack, that does not bubble and is not
                    // cancelable, and that uses the TrackEvent interface, at the SourceBuffer videoTracks list.
                    videoTracks->remove(track);
                }
            }

            // 7.4 If the removed selected video track flag equals true, then queue a task to fire a simple event
            // named change at the HTMLMediaElement videoTracks list.
            if (removedSelectedVideoTrack) {
                ensureWeakOnHTMLMediaElementContext([](auto& mediaElement) {
                    mediaElement.ensureVideoTracks().scheduleChangeEvent();
                });
            }
        }

        // 8. Let SourceBuffer textTracks list equal the TextTrackList object returned by sourceBuffer.textTracks.
        RefPtr textTracks = buffer.textTracksIfExists();

        // 9. If the SourceBuffer textTracks list is not empty, then run the following steps:
        if (textTracks && textTracks->length()) {
            // 9.1 Let HTMLMediaElement textTracks list equal the TextTrackList object returned by the textTracks
            // attribute on the HTMLMediaElement.
            // 9.2 Let the removed enabled text track flag equal false.
            bool removedEnabledTextTrack = false;

            // 9.3 For each TextTrack object in the SourceBuffer textTracks list, run the following steps:
            for (ssize_t index = textTracks->length() - 1; index >= 0; index--) {
                auto& track = *textTracks->lastItem();

                if (withDestruction) {
                    // 9.3.1 Set the sourceBuffer attribute on the TextTrack object to null.
                    track.setSourceBuffer(nullptr);
                }

                // 9.3.2 If the mode attribute on the TextTrack object is set to "showing" or "hidden", then
                // set the removed enabled text track flag to true.
                if (track.mode() == TextTrack::Mode::Showing || track.mode() == TextTrack::Mode::Hidden)
                    removedEnabledTextTrack = true;

                // 9.3.3 Remove the TextTrack object from the HTMLMediaElement textTracks list.
                // 9.3.4 Queue a task to fire a trusted event named removetrack, that does not bubble and is not
                // cancelable, and that uses the TrackEvent interface, at the HTMLMediaElement textTracks list.
                if (isMainThread()) {
                    ensureWeakOnHTMLMediaElementContext([track = Ref { track }](HTMLMediaElement& mediaElement) mutable {
                        mediaElement.removeTextTrack(WTFMove(track));
                    });
                } else {
                    ensureWeakOnHTMLMediaElementContext([trackID = track.trackId()](auto& mediaElement) mutable {
                        mediaElement.removeTextTrack(trackID);
                    });
                }

                if (withDestruction) {
                    // 9.3.5 Remove the TextTrack object from the SourceBuffer textTracks list.
                    // 9.3.6 Queue a task to fire a trusted event named removetrack, that does not bubble and is not
                    // cancelable, and that uses the TrackEvent interface, at the SourceBuffer textTracks list.
                    textTracks->remove(track);
                }
            }

            // 9.4 If the removed enabled text track flag equals true, then queue a task to fire a simple event
            // named change at the HTMLMediaElement textTracks list.
            if (removedEnabledTextTrack) {
                ensureWeakOnHTMLMediaElementContext([](auto& mediaElement) {
                    mediaElement.ensureTextTracks().scheduleChangeEvent();
                });
            }
        }
    }

    notifyElementUpdateMediaState();
}

bool MediaSource::isTypeSupported(ScriptExecutionContext& context, const String& type)
{
    Vector<ContentType> mediaContentTypesRequiringHardwareSupport;
    if (RefPtr document = dynamicDowncast<Document>(context))
        mediaContentTypesRequiringHardwareSupport.appendVector(document->settings().mediaContentTypesRequiringHardwareSupport());

    return isTypeSupported(context, type, WTFMove(mediaContentTypesRequiringHardwareSupport));
}

bool MediaSource::isTypeSupported(ScriptExecutionContext& context, const String& type, Vector<ContentType>&& contentTypesRequiringHardwareSupport)
{
    // Section 2.2 isTypeSupported() method steps.
    // https://dvcs.w3.org/hg/html-media/raw-file/tip/media-source/media-source.html#widl-MediaSource-isTypeSupported-boolean-DOMString-type
    // 1. If type is an empty string, then return false.
    if (type.isNull() || type.isEmpty())
        return false;

    ContentType contentType(type);
    RefPtr document = dynamicDowncast<Document>(context);
    if (document && document->quirks().needsVP9FullRangeFlagQuirk())
        contentType = addVP9FullRangeVideoFlagToContentType(contentType);

    String codecs = contentType.parameter("codecs"_s);

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

    if (RefPtr document = dynamicDowncast<Document>(context)) {
        if (!contentTypeMeetsContainerAndCodecTypeRequirements(contentType, document->settings().allowedMediaContainerTypes(), document->settings().allowedMediaCodecTypes()))
            return false;

        parameters.allowedMediaContainerTypes = document->settings().allowedMediaContainerTypes();
        parameters.allowedMediaVideoCodecIDs = document->settings().allowedMediaVideoCodecIDs();
        parameters.allowedMediaAudioCodecIDs = document->settings().allowedMediaAudioCodecIDs();
        parameters.allowedMediaCaptionFormatTypes = document->settings().allowedMediaCaptionFormatTypes();
    }

    MediaPlayer::SupportsType supported;
    callOnMainThreadAndWait([&] {
        supported = MediaPlayer::supportsType(parameters);
    });

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

void MediaSource::elementIsShuttingDown()
{
    ALWAYS_LOG(LOGIDENTIFIER);
    m_mediaElement = nullptr;
    m_sourceopenPending = false;
    detachFromElement();
}

void MediaSource::detachFromElement()
{
    ALWAYS_LOG(LOGIDENTIFIER);

    // 2.4.2 Detaching from a media element
    // https://rawgit.com/w3c/media-source/45627646344eea0170dd1cbc5a3d508ca751abb8/media-source-respec.html#mediasource-detach

    if (detachable())
        m_readyStateBeforeDetached = readyState();

    // 1. Set the readyState attribute to "closed".
    // 7. Queue a task to fire a simple event named sourceclose at the MediaSource.
    setReadyState(ReadyState::Closed);
    elementDetached();

    // 2. Update duration to NaN.
    // Step is done in duration() method which will now always return invalidTime() if MediaSource is not detachable.

    // 3. Remove all the SourceBuffer objects from activeSourceBuffers.
    // 4. Queue a task to fire a simple event named removesourcebuffer at activeSourceBuffers.
    // can be called from the destructor, where we may no longer have a scriptExecutionContext.
    if (scriptExecutionContext()) {
        while (m_activeSourceBuffers->length()) {
            auto& buffer = *m_activeSourceBuffers->item(0);
            if (detachable()) {
                removeSourceBufferWithOptionalDestruction(buffer, false);
                m_activeSourceBuffers->remove(buffer);
            } else
                removeSourceBuffer(buffer);
        }
    } else
        m_activeSourceBuffers->replaceWith({ });

    if (detachable()) {
        for (auto& sourceBuffer : m_sourceBuffers.get())
            sourceBuffer->detach();

        m_mediaElement = nullptr;
        m_isAttached = false;

        return;
    }

    // 5. Remove all the SourceBuffer objects from sourceBuffers.
    // 6. Queue a task to fire a simple event named removesourcebuffer at sourceBuffers.
    // can be called from the destructor, where we may no longer have a scriptExecutionContext.
    if (scriptExecutionContext()) {
        while (m_sourceBuffers->length())
            removeSourceBuffer(*m_sourceBuffers->item(0));
    } else
        m_sourceBuffers->replaceWith({ });

    m_mediaElement = nullptr;
    m_isAttached = false;

    if (!m_private)
        return;

    m_private->shutdown();
    setPrivate(nullptr);
}

void MediaSource::sourceBufferDidChangeActiveState(SourceBuffer&, bool)
{
    regenerateActiveSourceBuffers();
}

bool MediaSource::attachToElement(WeakPtr<HTMLMediaElement>&& element)
{
    if (m_isAttached || !scriptExecutionContext())
        return false;

    ASSERT(isClosed());

    m_mediaElement = WTFMove(element);
    m_isAttached = true;

    return true;
}

void MediaSource::openIfInEndedState()
{
    if (readyState() != ReadyState::Ended)
        return;

    ALWAYS_LOG(LOGIDENTIFIER);

    setReadyState(ReadyState::Open);
    m_private->unmarkEndOfStream();
    for (auto& sourceBuffer : m_sourceBuffers.get())
        sourceBuffer->setMediaSourceEnded(false);
}

void MediaSource::openIfDeferredOpen()
{
    ALWAYS_LOG(LOGIDENTIFIER);

    ensureWeakOnHTMLMediaElementContext([client = m_client, this](auto& mediaElement) {
        if (!mediaElement.deferredMediaSourceOpenCanProgress())
            return;
        client->ensureWeakOnDispatcher([this](MediaSource&) {
            if (!m_openDeferred)
                return;
            m_openDeferred = false;
            onReadyStateChange(ReadyState::Closed, m_readyStateBeforeDetached.value_or(ReadyState::Open));
            m_readyStateBeforeDetached.reset();
        }, true);
    });
}

void MediaSource::setAsSrcObject(bool set)
{
    m_sourceopenPending = set;
}

bool MediaSource::virtualHasPendingActivity() const
{
    // Mark this object as hasPendingActivity if it has been set as the srcObject
    // of a HTMLMediaElement, but has not yet fired the "sourceopen" event.
    if (m_sourceopenPending)
        return true;

    return m_private || m_associatedRegistryCount;
}

void MediaSource::stop()
{
    ALWAYS_LOG(LOGIDENTIFIER);

    ensureWeakOnHTMLMediaElementContext([](auto& mediaElement) {
        mediaElement.detachMediaSource();
    });
    m_seekTargetPromise.reset();
    setPrivate(nullptr);
}

MediaSource::ReadyState MediaSource::readyState() const
{
    return (m_openDeferred || !m_private) ? ReadyState::Closed : m_private->readyState();
}

void MediaSource::onReadyStateChange(ReadyState oldState, ReadyState newState)
{
    ALWAYS_LOG(LOGIDENTIFIER, "old state = ", oldState, ", new state = ", newState);

    if (oldState == newState)
        return;

    if (oldState == ReadyState::Closed && newState >= ReadyState::Open)
        m_sourceopenPending = false;

    // MediaSource's readyState transitions from "closed" to "open" or from "ended" to "open".
    // If `detachable` attribute is true, from "closed" to "ended"
    if (oldState != ReadyState::Open && newState >= ReadyState::Open)
        scheduleEvent(eventNames().sourceopenEvent);

    // MediaSource's readyState transitions from "open" to "ended".
    // If `detachable` attribute is true, from "closed" to "ended"
    if (newState == ReadyState::Ended) {
        scheduleEvent(eventNames().sourceendedEvent);
        // We need to force the recalculation of the buffered range as its value depends
        // on the readyState.
        // https://w3c.github.io/media-source/#htmlmediaelement-extensions-buffered
        for (auto& sourceBuffer : m_sourceBuffers.get())
            sourceBuffer->setMediaSourceEnded(true);
        updateBufferedIfNeeded(true /* force */);
    }

    // MediaSource's readyState transitions from "open" to "closed" or "ended" to "closed".
    if (oldState > ReadyState::Closed && newState == ReadyState::Closed) {
        if (m_seekTargetPromise)
            m_seekTargetPromise->reject(PlatformMediaError::Cancelled);
        m_seekTargetPromise.reset();
        scheduleEvent(eventNames().sourcecloseEvent);
    }

    monitorSourceBuffers();
}

Vector<PlatformTimeRanges> MediaSource::activeRanges() const
{
    return WTF::map(m_activeSourceBuffers.get(), [](auto& sourceBuffer) {
        return sourceBuffer->bufferedInternal();
    });
}

ExceptionOr<Ref<SourceBufferPrivate>> MediaSource::createSourceBufferPrivate(const ContentType& incomingType)
{
    ContentType type { incomingType };

    RefPtr context = scriptExecutionContext();
    RefPtr document = dynamicDowncast<Document>(context);
    if (document && document->quirks().needsVP9FullRangeFlagQuirk())
        type = addVP9FullRangeVideoFlagToContentType(incomingType);

    RefPtr<SourceBufferPrivate> sourceBufferPrivate;
    switch (m_private->addSourceBuffer(type, DeprecatedGlobalSettings::webMParserEnabled(), sourceBufferPrivate)) {
    case MediaSourcePrivate::AddStatus::Ok:
        return sourceBufferPrivate.releaseNonNull();
    case MediaSourcePrivate::AddStatus::NotSupported:
        // 2.2 https://dvcs.w3.org/hg/html-media/raw-file/default/media-source/media-source.html#widl-MediaSource-addSourceBuffer-SourceBuffer-DOMString-type
        // Step 2: If type contains a MIME type ... that is not supported with the types
        // specified for the other SourceBuffer objects in sourceBuffers, then throw
        // a NotSupportedError exception and abort these steps.
        return Exception { ExceptionCode::NotSupportedError };
    case MediaSourcePrivate::AddStatus::ReachedIdLimit:
        // 2.2 https://dvcs.w3.org/hg/html-media/raw-file/default/media-source/media-source.html#widl-MediaSource-addSourceBuffer-SourceBuffer-DOMString-type
        // Step 3: If the user agent can't handle any more SourceBuffer objects then throw
        // a QuotaExceededError exception and abort these steps.
        return Exception { ExceptionCode::QuotaExceededError };
    }

    ASSERT_NOT_REACHED();
    return Exception { ExceptionCode::QuotaExceededError };
}

void MediaSource::scheduleEvent(const AtomString& eventName)
{
    DEBUG_LOG(LOGIDENTIFIER, "scheduling '", eventName, "'");

    queueTaskToDispatchEvent(*this, TaskSource::MediaElement, Event::create(eventName, Event::CanBubble::No, Event::IsCancelable::No));
}

ScriptExecutionContext* MediaSource::scriptExecutionContext() const
{
    return ActiveDOMObject::scriptExecutionContext();
}

enum EventTargetInterfaceType MediaSource::eventTargetInterface() const
{
    return EventTargetInterfaceType::MediaSource;
}

URLRegistry& MediaSource::registry() const
{
    return MediaSourceRegistry::registry();
}

void MediaSource::regenerateActiveSourceBuffers()
{
    Vector<RefPtr<SourceBuffer>> newList;
    for (auto& sourceBuffer : m_sourceBuffers.get()) {
        if (sourceBuffer->active())
            newList.append(sourceBuffer);
    }
    m_activeSourceBuffers->replaceWith(WTFMove(newList));
    for (auto& sourceBuffer : m_activeSourceBuffers.get())
        sourceBuffer->setBufferedDirty(true);

    notifyElementUpdateMediaState();

    updateBufferedIfNeeded();
}

void MediaSource::notifyElementUpdateMediaState() const
{
    ensureWeakOnHTMLMediaElementContext([](auto& mediaElement) {
        mediaElement.updateMediaState();
    });
}

void MediaSource::ensureWeakOnHTMLMediaElementContext(Function<void(HTMLMediaElement&)>&& task) const
{
    ensureOnMainThread([weakMediaElement = m_mediaElement, task = WTFMove(task)]() mutable {
        if (RefPtr<HTMLMediaElement> mediaElement = weakMediaElement.get())
            task(*mediaElement);
    });
}

void MediaSource::sourceBufferBufferedChanged()
{
    updateBufferedIfNeeded();
}

void MediaSource::updateBufferedIfNeeded(bool force)
{
    if (isClosed())
        return;

    if (!force && m_activeSourceBuffers->length() && std::all_of(m_activeSourceBuffers->begin(), m_activeSourceBuffers->end(), [](auto& buffer) { return !buffer->isBufferedDirty(); }))
        return;

    for (auto& sourceBuffer : m_activeSourceBuffers.get())
        sourceBuffer->setBufferedDirty(false);

    PlatformTimeRanges buffered;
    auto updatePrivate = makeScopeExit([&] {
        if (buffered == m_private->buffered())
            return;
        m_private->bufferedChanged(buffered);
        monitorSourceBuffers();
    });

    // Implements MediaSource algorithm for HTMLMediaElement.buffered.
    // https://dvcs.w3.org/hg/html-media/raw-file/default/media-source/media-source.html#htmlmediaelement-extensions
    Vector<PlatformTimeRanges> activeRanges = this->activeRanges();

    // 1. If activeSourceBuffers.length equals 0 then return an empty TimeRanges object and abort these steps.
    if (activeRanges.isEmpty())
        return;

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
        return;

    // 4. Let intersection ranges equal a TimeRange object containing a single range from 0 to highest end time.
    buffered.add(MediaTime::zeroTime(), highestEndTime);

    // 5. For each SourceBuffer object in activeSourceBuffers run the following steps:
    bool ended = readyState() == ReadyState::Ended;
    for (auto& sourceRanges : activeRanges) {
        // 5.1 Let source ranges equal the ranges returned by the buffered attribute on the current SourceBuffer.
        // 5.2 If readyState is "ended", then set the end time on the last range in source ranges to highest end time.
        if (ended && sourceRanges.length())
            sourceRanges.add(sourceRanges.start(sourceRanges.length() - 1), highestEndTime);

        // 5.3 Let new intersection ranges equal the intersection between the intersection ranges and the source ranges.
        // 5.4 Replace the ranges in intersection ranges with the new intersection ranges.
        buffered.intersectWith(sourceRanges);
    }
}

#if !RELEASE_LOG_DISABLED
void MediaSource::setLogIdentifier(uint64_t identifier)
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
        context->addConsoleMessage(MessageSource::JS, MessageLevel::Error, makeString("MediaSource "_s, type == RendererType::Video ? "video"_s : "audio"_s, " renderer creation failed."_s));
}

void MediaSource::sourceBufferReceivedFirstInitializationSegmentChanged()
{
    if (m_private && m_private->mediaPlayerReadyState() == MediaPlayer::ReadyState::HaveNothing) {
        // 6.1 If one or more objects in sourceBuffers have first initialization segment flag set to false, then abort these steps.
        for (auto& sourceBuffer : m_sourceBuffers.get()) {
            if (!sourceBuffer->receivedFirstInitializationSegment())
                return;
        }
        // 6.2 Set the HTMLMediaElement.readyState attribute to HAVE_METADATA.
        // 6.3 Queue a task to fire a simple event named loadedmetadata at the media element.
        m_private->setMediaPlayerReadyState(MediaPlayer::ReadyState::HaveMetadata);
    }
}

void MediaSource::sourceBufferActiveTrackFlagChanged(bool activeTrackFlag)
{
    if (!m_private)
        return;
    if (activeTrackFlag && m_private->mediaPlayerReadyState() > MediaPlayer::ReadyState::HaveCurrentData)
        setMediaPlayerReadyState(MediaPlayer::ReadyState::HaveMetadata);
}

void MediaSource::setMediaPlayerReadyState(MediaPlayer::ReadyState readyState)
{
    if (!m_private)
        return;
    m_private->setMediaPlayerReadyState(readyState);
}

void MediaSource::incrementDroppedFrameCount()
{
    ensureWeakOnHTMLMediaElementContext([](auto& mediaElement) {
        mediaElement.incrementDroppedFrameCount();
    });
}

void MediaSource::addAudioTrackToElement(Ref<AudioTrack>&& track)
{
    ensureWeakOnHTMLMediaElementContext([track = WTFMove(track)](auto& mediaElement) mutable {
        mediaElement.addAudioTrack(WTFMove(track));
    });
}

void MediaSource::addTextTrackToElement(Ref<TextTrack>&& track)
{
    ensureWeakOnHTMLMediaElementContext([track = WTFMove(track)](auto& mediaElement) mutable {
        mediaElement.addTextTrack(WTFMove(track));
    });
}

void MediaSource::addVideoTrackToElement(Ref<VideoTrack>&& track)
{
    ensureWeakOnHTMLMediaElementContext([track = WTFMove(track)](auto& mediaElement) mutable {
        mediaElement.addVideoTrack(WTFMove(track));
    });
}

void MediaSource::addAudioTrackMirrorToElement(Ref<AudioTrackPrivate>&& track, bool enabled)
{
    ensureWeakOnHTMLMediaElementContext([track = WTFMove(track), enabled](auto& mediaElement) mutable {
        auto audioTrack = AudioTrack::create(mediaElement.scriptExecutionContext(), track);
        audioTrack->setEnabled(enabled);
        mediaElement.addAudioTrack(WTFMove(audioTrack));
    });
}

void MediaSource::addTextTrackMirrorToElement(Ref<InbandTextTrackPrivate>&& track)
{
    ensureWeakOnHTMLMediaElementContext([track = WTFMove(track)](auto& mediaElement) mutable {
        if (!mediaElement.scriptExecutionContext())
            return;
        mediaElement.addTextTrack(InbandTextTrack::create(*mediaElement.scriptExecutionContext(), track));
    });
}

void MediaSource::addVideoTrackMirrorToElement(Ref<VideoTrackPrivate>&& track, bool selected)
{
    ensureWeakOnHTMLMediaElementContext([track = WTFMove(track), selected](auto& mediaElement) mutable {
        auto videoTrack = VideoTrack::create(mediaElement.scriptExecutionContext(), track);
        videoTrack->setSelected(selected);
        mediaElement.addVideoTrack(WTFMove(videoTrack));
    });
}

void MediaSource::memoryPressure()
{
    if (!isManaged())
        return;
    for (auto& sourceBuffer : m_sourceBuffers.get())
        sourceBuffer->memoryPressure();
}

Ref<MediaSourcePrivateClient> MediaSource::client() const
{
    return m_client;
}

bool MediaSource::enabledForContext(ScriptExecutionContext& context)
{
    UNUSED_PARAM(context);
#if ENABLE(MEDIA_SOURCE_IN_WORKERS)
    if (context.isWorkerGlobalScope())
        return context.settingsValues().mediaSourceInWorkerEnabled && platformStrategies()->mediaStrategy().hasThreadSafeMediaSourceSupport();
#endif

    ASSERT(context.isDocument());
    return true;
}

Ref<SourceBufferList> MediaSource::sourceBuffers() const
{
    return m_sourceBuffers;
}

Ref<SourceBufferList> MediaSource::activeSourceBuffers() const
{
    return m_activeSourceBuffers;
}

#if ENABLE(MEDIA_SOURCE_IN_WORKERS)

Ref<MediaSourceHandle> MediaSource::handle()
{
    if (!m_handle) {
        m_handle = MediaSourceHandle::create(*this, [weakClient = ThreadSafeWeakPtr { m_client.get() }](MediaSourceHandle::TaskType&& task, bool forceRunInWorker) {
            if (RefPtr protectedClient = weakClient.get())
                protectedClient->ensureWeakOnDispatcher(WTFMove(task), forceRunInWorker);
        }, detachable());
    }
    return *m_handle;
}

bool MediaSource::canConstructInDedicatedWorker(ScriptExecutionContext& context)
{
    return context.settingsValues().mediaSourceInWorkerEnabled && platformStrategies()->mediaStrategy().hasThreadSafeMediaSourceSupport();
}

#endif

} // namespace WebCore

#endif // ENABLE(MEDIA_SOURCE)
