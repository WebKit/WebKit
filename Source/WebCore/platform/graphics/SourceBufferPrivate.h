/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 * Copyright (C) 2013-2020 Apple Inc. All rights reserved.
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

#pragma once

#if ENABLE(MEDIA_SOURCE)

#include "InbandTextTrackPrivate.h"
#include "MediaDescription.h"
#include "MediaPlayer.h"
#include "MediaSample.h"
#include "PlatformTimeRanges.h"
#include "SampleMap.h"
#include "SourceBufferPrivateClient.h"
#include "TimeRanges.h"
#include <optional>
#include <wtf/Deque.h>
#include <wtf/Forward.h>
#include <wtf/HashMap.h>
#include <wtf/Logger.h>
#include <wtf/LoggerHelper.h>
#include <wtf/NativePromise.h>
#include <wtf/Ref.h>
#include <wtf/RobinHoodHashMap.h>
#include <wtf/ThreadSafeWeakPtr.h>
#include <wtf/UniqueRef.h>
#include <wtf/WeakPtr.h>
#include <wtf/WorkQueue.h>
#include <wtf/text/AtomStringHash.h>

namespace WebCore {

class MediaSourcePrivate;
class SharedBuffer;
class TrackBuffer;
class TimeRanges;

#if ENABLE(ENCRYPTED_MEDIA)
class CDMInstance;
#endif
#if ENABLE(LEGACY_ENCRYPTED_MEDIA)
class LegacyCDMSession;
#endif

enum class SourceBufferAppendMode : uint8_t {
    Segments,
    Sequence
};

class SourceBufferPrivate
    : public ThreadSafeRefCountedAndCanMakeThreadSafeWeakPtr<SourceBufferPrivate, WTF::DestructionThread::Main>
#if !RELEASE_LOG_DISABLED
    , public LoggerHelper
#endif
{
public:
    WEBCORE_EXPORT SourceBufferPrivate(MediaSourcePrivate&);
    WEBCORE_EXPORT virtual ~SourceBufferPrivate();

    virtual constexpr MediaPlatformType platformType() const = 0;

    WEBCORE_EXPORT virtual void setActive(bool);

    WEBCORE_EXPORT virtual Ref<MediaPromise> append(Ref<SharedBuffer>&&);

    virtual void abort();
    // Overrides must call the base class.
    virtual void resetParserState();
    virtual void removedFromMediaSource();

    virtual MediaPlayer::ReadyState readyState() const = 0;
    virtual void setReadyState(MediaPlayer::ReadyState) = 0;

    virtual bool canSwitchToType(const ContentType&) { return false; }

    WEBCORE_EXPORT virtual void setMediaSourceEnded(bool);
    virtual void setMode(SourceBufferAppendMode mode) { m_appendMode = mode; }
    WEBCORE_EXPORT virtual void reenqueueMediaIfNeeded(const MediaTime& currentMediaTime);
    WEBCORE_EXPORT virtual void addTrackBuffer(const AtomString& trackId, RefPtr<MediaDescription>&&);
    WEBCORE_EXPORT virtual void resetTrackBuffers();
    WEBCORE_EXPORT virtual void clearTrackBuffers(bool shouldReportToClient = false);
    WEBCORE_EXPORT virtual void setAllTrackBuffersNeedRandomAccess();
    virtual void setGroupStartTimestamp(const MediaTime& mediaTime) { m_groupStartTimestamp = mediaTime; }
    virtual void setGroupStartTimestampToEndTimestamp() { m_groupStartTimestamp = m_groupEndTimestamp; }
    virtual void setShouldGenerateTimestamps(bool flag) { m_shouldGenerateTimestamps = flag; }
    WEBCORE_EXPORT virtual Ref<MediaPromise> removeCodedFrames(const MediaTime& start, const MediaTime& end, const MediaTime& currentMediaTime);
    WEBCORE_EXPORT virtual void evictCodedFrames(uint64_t newDataSize, uint64_t maximumBufferSize, const MediaTime& currentTime);
    WEBCORE_EXPORT virtual size_t platformEvictionThreshold() const;
    WEBCORE_EXPORT virtual uint64_t totalTrackBufferSizeInBytes() const;
    WEBCORE_EXPORT virtual void resetTimestampOffsetInTrackBuffers();
    virtual void startChangingType() { m_pendingInitializationSegmentForChangeType = true; }
    virtual void setTimestampOffset(const MediaTime& timestampOffset) { m_timestampOffset = timestampOffset; }
    virtual void setAppendWindowStart(const MediaTime& appendWindowStart) { m_appendWindowStart = appendWindowStart;}
    virtual void setAppendWindowEnd(const MediaTime& appendWindowEnd) { m_appendWindowEnd = appendWindowEnd; }
    using ComputeSeekPromise = MediaTimePromise;
    WEBCORE_EXPORT virtual Ref<ComputeSeekPromise> computeSeekTime(const SeekTarget&);
    WEBCORE_EXPORT virtual void seekToTime(const MediaTime&);
    WEBCORE_EXPORT virtual void updateTrackIds(Vector<std::pair<AtomString, AtomString>>&& trackIdPairs);

    WEBCORE_EXPORT void setClient(SourceBufferPrivateClient&);
    WEBCORE_EXPORT void detach();

    const PlatformTimeRanges& buffered() const { return m_buffered; }

    bool isBufferFullFor(uint64_t requiredSize, uint64_t maximumBufferSize);

    // Methods used by MediaSourcePrivate
    bool hasAudio() const { return m_hasAudio; }
    bool hasVideo() const { return m_hasVideo; }
    bool hasReceivedFirstInitializationSegment() const { return m_receivedFirstInitializationSegment; }

    MediaTime timestampOffset() const { return m_timestampOffset; }

    virtual size_t platformMaximumBufferSize() const { return 0; }

    // Methods for ManagedSourceBuffer
    WEBCORE_EXPORT virtual void memoryPressure(uint64_t maximumBufferSize, const MediaTime& currentTime);

    // Internals Utility methods
    using SamplesPromise = NativePromise<Vector<String>, int>;
    WEBCORE_EXPORT virtual Ref<SamplesPromise> bufferedSamplesForTrackId(const AtomString&);
    WEBCORE_EXPORT virtual Ref<SamplesPromise> enqueuedSamplesForTrackID(const AtomString&);
    virtual MediaTime minimumUpcomingPresentationTimeForTrackID(const AtomString&) { return MediaTime::invalidTime(); }
    virtual void setMaximumQueueDepthForTrackID(const AtomString&, uint64_t) { }

#if !RELEASE_LOG_DISABLED
    virtual const Logger& sourceBufferLogger() const = 0;
    virtual const void* sourceBufferLogIdentifier() = 0;
#endif

#if ENABLE(LEGACY_ENCRYPTED_MEDIA)
    virtual void setCDMSession(LegacyCDMSession*) { }
#endif
#if ENABLE(ENCRYPTED_MEDIA)
    virtual void setCDMInstance(CDMInstance*) { }
    virtual bool waitingForKey() const { return false; }
    virtual void attemptToDecrypt() { }
#endif

    WorkQueue& queue() { return m_queue; }
protected:
    WEBCORE_EXPORT Ref<MediaPromise> updateBufferedFromTrackBuffers(const Vector<PlatformTimeRanges>&);

    MediaTime currentMediaTime() const;
    MediaTime duration() const;

    using InitializationSegment = SourceBufferPrivateClient::InitializationSegment;
    WEBCORE_EXPORT void didReceiveInitializationSegment(InitializationSegment&&);
    WEBCORE_EXPORT void didUpdateFormatDescriptionForTrackId(Ref<TrackInfo>&&, uint64_t);
    WEBCORE_EXPORT void didReceiveSample(Ref<MediaSample>&&);
    WEBCORE_EXPORT Ref<MediaPromise> setBufferedRanges(PlatformTimeRanges&&);

    virtual Ref<MediaPromise> appendInternal(Ref<SharedBuffer>&&) = 0;
    virtual void resetParserStateInternal() = 0;
    virtual MediaTime timeFudgeFactor() const { return PlatformTimeRanges::timeFudgeFactor(); }
    bool isActive() const { return m_isActive; }
    virtual bool isSeeking() const { return false; }
    virtual void flush(const AtomString&) { }
    virtual void enqueueSample(Ref<MediaSample>&&, const AtomString&) { }
    virtual void allSamplesInTrackEnqueued(const AtomString&) { }
    virtual bool isReadyForMoreSamples(const AtomString&) { return false; }
    virtual void notifyClientWhenReadyForMoreSamples(const AtomString&) { }

    virtual bool canSetMinimumUpcomingPresentationTime(const AtomString&) const { return false; }
    virtual void setMinimumUpcomingPresentationTime(const AtomString&, const MediaTime&) { }
    virtual void clearMinimumUpcomingPresentationTime(const AtomString&) { }

    void reenqueSamples(const AtomString& trackID);

    virtual bool precheckInitialisationSegment(const InitializationSegment&) { return true; }
    virtual void processInitialisationSegment(std::optional<InitializationSegment>&&) { }
    virtual void processFormatDescriptionForTrackId(Ref<TrackInfo>&&, uint64_t) { }

    void provideMediaData(const AtomString& trackID);

    virtual bool isMediaSampleAllowed(const MediaSample&) const { return true; }

    // Must be called once all samples have been processed.
    WEBCORE_EXPORT void appendCompleted(bool parsingSucceeded, Function<void()>&& = [] { });

    WEBCORE_EXPORT RefPtr<SourceBufferPrivateClient> client() const;

    ThreadSafeWeakPtr<MediaSourcePrivate> m_mediaSource { nullptr };

private:
    void updateHighestPresentationTimestamp();
    void updateMinimumUpcomingPresentationTime(TrackBuffer&, const AtomString& trackID);
    void reenqueueMediaForTime(TrackBuffer&, const AtomString& trackID, const MediaTime&);
    bool validateInitializationSegment(const InitializationSegment&);
    void provideMediaData(TrackBuffer&, const AtomString& trackID);
    void setBufferedDirty(bool);
    void trySignalAllSamplesInTrackEnqueued(TrackBuffer&, const AtomString& trackID);
    MediaTime findPreviousSyncSamplePresentationTime(const MediaTime&);
    Ref<MediaPromise> removeCodedFramesInternal(const MediaTime& start, const MediaTime& end, const MediaTime& currentMediaTime);
    bool evictFrames(uint64_t newDataSize, uint64_t maximumBufferSize, const MediaTime& currentTime);
    virtual Vector<PlatformTimeRanges> trackBuffersRanges() const;
    bool hasTooManySamples() const;

    bool m_hasAudio { false };
    bool m_hasVideo { false };
    bool m_isActive { false };

    ThreadSafeWeakPtr<SourceBufferPrivateClient> m_client;

    MemoryCompactRobinHoodHashMap<AtomString, UniqueRef<TrackBuffer>> m_trackBufferMap;

    SourceBufferAppendMode m_appendMode { SourceBufferAppendMode::Segments };

    using OperationPromise = NativePromise<void, PlatformMediaError, WTF::PromiseOption::Default | WTF::PromiseOption::NonExclusive>;
    Ref<OperationPromise> m_currentSourceBufferOperation { OperationPromise::createAndResolve() };

    bool m_shouldGenerateTimestamps { false };
    bool m_receivedFirstInitializationSegment { false };
    bool m_pendingInitializationSegmentForChangeType { false };
    size_t m_abortCount { 0 };

    void processPendingMediaSamples();
    bool processMediaSample(SourceBufferPrivateClient&, Ref<MediaSample>&&);

    using SamplesVector = Vector<Ref<MediaSample>>;
    SamplesVector m_pendingSamples;
    Ref<MediaPromise> m_currentAppendProcessing { MediaPromise::createAndResolve() };

    MediaTime m_timestampOffset;
    MediaTime m_appendWindowStart { MediaTime::zeroTime() };
    MediaTime m_appendWindowEnd { MediaTime::positiveInfiniteTime() };
    MediaTime m_highestPresentationTimestamp;

    MediaTime m_groupStartTimestamp { MediaTime::invalidTime() };
    MediaTime m_groupEndTimestamp { MediaTime::zeroTime() };

    bool m_isMediaSourceEnded { false };
    PlatformTimeRanges m_buffered;
    Ref<WorkQueue> m_queue { WorkQueue::create("SerialBufferPrivate Queue") };
};

} // namespace WebCore

#endif
