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

#include "MediaDescription.h"
#include "MediaPlayer.h"
#include "MediaSample.h"
#include "PlatformTimeRanges.h"
#include "SampleMap.h"
#include "SourceBufferPrivateClient.h"
#include "TimeRanges.h"
#include <wtf/HashMap.h>
#include <wtf/Logger.h>
#include <wtf/LoggerHelper.h>
#include <wtf/Ref.h>
#include <wtf/RobinHoodHashMap.h>
#include <wtf/UniqueRef.h>
#include <wtf/WeakPtr.h>
#include <wtf/text/AtomStringHash.h>

namespace WebCore {

class SharedBuffer;
class TrackBuffer;
class TimeRanges;

enum class SourceBufferAppendMode : uint8_t {
    Segments,
    Sequence
};

class SourceBufferPrivate
    : public RefCounted<SourceBufferPrivate>
#if !RELEASE_LOG_DISABLED
    , public LoggerHelper
#endif
{
public:
    WEBCORE_EXPORT SourceBufferPrivate();
    WEBCORE_EXPORT virtual ~SourceBufferPrivate();

    virtual void setActive(bool) = 0;
    WEBCORE_EXPORT virtual void append(Ref<SharedBuffer>&&);
    virtual void abort() = 0;
    virtual void resetParserState() = 0;
    virtual void removedFromMediaSource() = 0;
    virtual MediaPlayer::ReadyState readyState() const = 0;
    virtual void setReadyState(MediaPlayer::ReadyState) = 0;

    virtual bool canSwitchToType(const ContentType&) { return false; }

    WEBCORE_EXPORT virtual void setMediaSourceEnded(bool);
    virtual void setMode(SourceBufferAppendMode mode) { m_appendMode = mode; }
    WEBCORE_EXPORT virtual void reenqueueMediaIfNeeded(const MediaTime& currentMediaTime);
    WEBCORE_EXPORT virtual void addTrackBuffer(const AtomString& trackId, RefPtr<MediaDescription>&&);
    WEBCORE_EXPORT virtual void resetTrackBuffers();
    WEBCORE_EXPORT virtual void clearTrackBuffers();
    WEBCORE_EXPORT virtual void setAllTrackBuffersNeedRandomAccess();
    virtual void setGroupStartTimestamp(const MediaTime& mediaTime) { m_groupStartTimestamp = mediaTime; }
    virtual void setGroupStartTimestampToEndTimestamp() { m_groupStartTimestamp = m_groupEndTimestamp; }
    virtual void setShouldGenerateTimestamps(bool flag) { m_shouldGenerateTimestamps = flag; }
    WEBCORE_EXPORT virtual void updateBufferedFromTrackBuffers(bool sourceIsEnded);
    WEBCORE_EXPORT virtual void removeCodedFrames(const MediaTime& start, const MediaTime& end, const MediaTime& currentMediaTime, bool isEnded, CompletionHandler<void()>&& = [] { });
    WEBCORE_EXPORT virtual void evictCodedFrames(uint64_t newDataSize, uint64_t maximumBufferSize, const MediaTime& currentTime, bool isEnded);
    WEBCORE_EXPORT virtual uint64_t totalTrackBufferSizeInBytes() const;
    WEBCORE_EXPORT virtual void resetTimestampOffsetInTrackBuffers();
    virtual void startChangingType() { m_pendingInitializationSegmentForChangeType = true; }
    virtual void setTimestampOffset(const MediaTime& timestampOffset) { m_timestampOffset = timestampOffset; }
    virtual void setAppendWindowStart(const MediaTime& appendWindowStart) { m_appendWindowStart = appendWindowStart;}
    virtual void setAppendWindowEnd(const MediaTime& appendWindowEnd) { m_appendWindowEnd = appendWindowEnd; }
    WEBCORE_EXPORT virtual void seekToTime(const MediaTime&);
    WEBCORE_EXPORT virtual void updateTrackIds(Vector<std::pair<AtomString, AtomString>>&& trackIdPairs);

    WEBCORE_EXPORT void setClient(SourceBufferPrivateClient*);
    void setIsAttached(bool flag) { m_isAttached = flag; }

    const TimeRanges* buffered() const { return m_buffered.get(); }

    bool isBufferFullFor(uint64_t requiredSize, uint64_t maximumBufferSize);

    // Methods used by MediaSourcePrivate
    bool hasAudio() const { return m_hasAudio; }
    bool hasVideo() const { return m_hasVideo; }

    MediaTime timestampOffset() const { return m_timestampOffset; }

    virtual size_t platformMaximumBufferSize() const { return 0; }

    // Internals Utility methods
    WEBCORE_EXPORT virtual void bufferedSamplesForTrackId(const AtomString&, CompletionHandler<void(Vector<String>&&)>&&);
    WEBCORE_EXPORT virtual void enqueuedSamplesForTrackID(const AtomString&, CompletionHandler<void(Vector<String>&&)>&&);
    virtual MediaTime minimumUpcomingPresentationTimeForTrackID(const AtomString&) { return MediaTime::invalidTime(); }
    virtual void setMaximumQueueDepthForTrackID(const AtomString&, uint64_t) { }
    WEBCORE_EXPORT MediaTime fastSeekTimeForMediaTime(const MediaTime& targetTime, const MediaTime& negativeThreshold, const MediaTime& positiveThreshold);

#if !RELEASE_LOG_DISABLED
    virtual const Logger& sourceBufferLogger() const = 0;
    virtual const void* sourceBufferLogIdentifier() = 0;
#endif

protected:
    // The following method should never be called directly and be overridden instead.
    WEBCORE_EXPORT virtual void append(Vector<unsigned char>&&);
    virtual MediaTime timeFudgeFactor() const { return { 2002, 24000 }; }
    virtual bool isActive() const { return false; }
    virtual bool isSeeking() const { return false; }
    virtual MediaTime currentMediaTime() const { return { }; }
    virtual MediaTime duration() const { return { }; }
    virtual void flush(const AtomString&) { }
    virtual void enqueueSample(Ref<MediaSample>&&, const AtomString&) { }
    virtual void allSamplesInTrackEnqueued(const AtomString&) { }
    virtual bool isReadyForMoreSamples(const AtomString&) { return false; }
    virtual void notifyClientWhenReadyForMoreSamples(const AtomString&) { }

    virtual bool canSetMinimumUpcomingPresentationTime(const AtomString&) const { return false; }
    virtual void setMinimumUpcomingPresentationTime(const AtomString&, const MediaTime&) { }
    virtual void clearMinimumUpcomingPresentationTime(const AtomString&) { }

    void appendCompleted(bool parsingSucceeded, bool isEnded);
    void reenqueSamples(const AtomString& trackID);
    WEBCORE_EXPORT void didReceiveInitializationSegment(SourceBufferPrivateClient::InitializationSegment&&, CompletionHandler<void(SourceBufferPrivateClient::ReceiveResult)>&&);
    WEBCORE_EXPORT void didReceiveSample(Ref<MediaSample>&&);
    WEBCORE_EXPORT void setBufferedRanges(const PlatformTimeRanges&);
    void provideMediaData(const AtomString& trackID);

    WeakPtr<SourceBufferPrivateClient> m_client;

private:
    void updateHighestPresentationTimestamp();
    void updateMinimumUpcomingPresentationTime(TrackBuffer&, const AtomString& trackID);
    void reenqueueMediaForTime(TrackBuffer&, const AtomString& trackID, const MediaTime&);
    bool validateInitializationSegment(const SourceBufferPrivateClient::InitializationSegment&);
    void provideMediaData(TrackBuffer&, const AtomString& trackID);
    void setBufferedDirty(bool);
    void trySignalAllSamplesInTrackEnqueued(TrackBuffer&, const AtomString& trackID);
    MediaTime findPreviousSyncSamplePresentationTime(const MediaTime&);

    bool m_isAttached { false };
    bool m_hasAudio { false };
    bool m_hasVideo { false };

    MemoryCompactRobinHoodHashMap<AtomString, UniqueRef<TrackBuffer>> m_trackBufferMap;

    SourceBufferAppendMode m_appendMode { SourceBufferAppendMode::Segments };

    bool m_shouldGenerateTimestamps { false };
    bool m_receivedFirstInitializationSegment { false };
    bool m_pendingInitializationSegmentForChangeType { false };
    bool m_didReceiveSampleErrored { false };

    MediaTime m_timestampOffset;
    MediaTime m_appendWindowStart { MediaTime::zeroTime() };
    MediaTime m_appendWindowEnd { MediaTime::positiveInfiniteTime() };
    MediaTime m_highestPresentationTimestamp;

    MediaTime m_groupStartTimestamp { MediaTime::invalidTime() };
    MediaTime m_groupEndTimestamp { MediaTime::zeroTime() };

    bool m_isMediaSourceEnded { false };
    RefPtr<TimeRanges> m_buffered;
};

} // namespace WebCore

namespace WTF {

template<> struct EnumTraits<WebCore::SourceBufferAppendMode> {
    using values = EnumValues<
        WebCore::SourceBufferAppendMode,
        WebCore::SourceBufferAppendMode::Segments,
        WebCore::SourceBufferAppendMode::Sequence
    >;
};

}; // namespace WTF

#endif
