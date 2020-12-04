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
#include <wtf/text/AtomStringHash.h>

namespace WebCore {

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
    virtual ~SourceBufferPrivate() = default;

    virtual void append(Vector<unsigned char>&&) = 0;
    virtual void abort() = 0;
    virtual void resetParserState() = 0;
    virtual void removedFromMediaSource() = 0;
    virtual MediaPlayer::ReadyState readyState() const = 0;
    virtual void setReadyState(MediaPlayer::ReadyState) = 0;

    virtual void setActive(bool) { }
    virtual bool canSwitchToType(const ContentType&) { return false; }

    void setClient(SourceBufferPrivateClient* client) { m_client = client; }
    void setIsAttached(bool flag) { m_isAttached = flag; }
    void setCurrentTimeFudgeFactor(const MediaTime& time) { m_currentTimeFudgeFactor = time; }
    void setAppendWindowStart(const MediaTime& appendWindowStart) { m_appendWindowStart = appendWindowStart;}
    void setAppendWindowEnd(const MediaTime& appendWindowEnd) { m_appendWindowEnd = appendWindowEnd; }
    bool bufferFull() const { return m_bufferFull; }
    TimeRanges* buffered() const { return m_buffered.get(); }
    bool isBufferedDirty() const { return m_bufferedDirty; }
    void setBufferedDirty(bool flag) { m_bufferedDirty = flag; }
    void resetTrackBuffers();
    void clearTrackBuffers();
    void resetTimestampOffsetInTrackBuffers();
    MediaTime timestampOffset() const { return m_timestampOffset; }
    void setTimestampOffset(const MediaTime& timestampOffset) { m_timestampOffset = timestampOffset; }
    MediaTime highestPresentationTimestamp() const;
    void updateBufferedFromTrackBuffers(bool sourceIsEnded);
    void seekToTime(const MediaTime&);
    void trySignalAllSamplesInTrackEnqueued();
    void startChangingType() { m_pendingInitializationSegmentForChangeType = true; }

    void reenqueueMediaIfNeeded(const MediaTime& currentMediaTime, size_t pendingAppendDataCapacity, size_t maximumBufferSize);
    void removeCodedFrames(const MediaTime& start, const MediaTime& end, const MediaTime& currentMediaTime, bool isEnded);
    void evictCodedFrames(size_t newDataSize, size_t pendingAppendDataCapacity, size_t maximumBufferSize, const MediaTime& currentTime, const MediaTime& duration, bool isEnded);
    void addTrackBuffer(const AtomString& trackId, RefPtr<MediaDescription>);
    void updateTrackIds(Vector<std::pair<AtomString, AtomString>>&& trackIdPairs);
    void setAllTrackBuffersNeedRandomAccess();
    void setShouldGenerateTimestamps(bool flag) { m_shouldGenerateTimestamps = flag; }
    void setMode(SourceBufferAppendMode mode) { m_appendMode = mode; }
    void setGroupStartTimestamp(const MediaTime& mediaTime) { m_groupStartTimestamp = mediaTime; }
    void setGroupStartTimestampToEndTimestamp() { m_groupStartTimestamp = m_groupEndTimestamp; }
    size_t totalTrackBufferSizeInBytes() const;

    struct TrackBuffer {
        MediaTime lastDecodeTimestamp;
        MediaTime greatestDecodeDuration;
        MediaTime lastFrameDuration;
        MediaTime highestPresentationTimestamp;
        MediaTime highestEnqueuedPresentationTime;
        MediaTime minimumEnqueuedPresentationTime;
        DecodeOrderSampleMap::KeyType lastEnqueuedDecodeKey;
        MediaTime enqueueDiscontinuityBoundary { MediaTime::zeroTime() };
        MediaTime roundedTimestampOffset;
        uint32_t lastFrameTimescale { 0 };
        bool needRandomAccessFlag { true };
        bool enabled { false };
        bool needsReenqueueing { false };
        bool needsMinimumUpcomingPresentationTimeUpdating { false };
        SampleMap samples;
        DecodeOrderSampleMap::MapType decodeQueue;
        RefPtr<MediaDescription> description;
        PlatformTimeRanges buffered;

        TrackBuffer();
    };

    // Internals Utility methods
    virtual Vector<String> enqueuedSamplesForTrackID(const AtomString&) { return { }; }
    Vector<String> bufferedSamplesForTrackID(const AtomString&);
    virtual MediaTime minimumUpcomingPresentationTimeForTrackID(const AtomString&) { return MediaTime::invalidTime(); }
    virtual void setMaximumQueueDepthForTrackID(const AtomString&, size_t) { }
    MediaTime fastSeekTimeForMediaTime(const MediaTime& targetTime, const MediaTime& negativeThreshold, const MediaTime& positiveThreshold);

#if !RELEASE_LOG_DISABLED
    virtual const Logger& sourceBufferLogger() const = 0;
    virtual const void* sourceBufferLogIdentifier() = 0;
#endif

protected:
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

    void reenqueSamples(const AtomString& trackID);
    void didReceiveInitializationSegment(const SourceBufferPrivateClient::InitializationSegment&);
    void didReceiveSample(MediaSample&);
    void provideMediaData(const AtomString& trackID);

    SourceBufferPrivateClient* m_client { nullptr };

private:
    void updateMinimumUpcomingPresentationTime(TrackBuffer&, const AtomString& trackID);
    void reenqueueMediaForTime(TrackBuffer&, const AtomString& trackID, const MediaTime&);
    bool validateInitializationSegment(const SourceBufferPrivateClient::InitializationSegment&);
    void provideMediaData(TrackBuffer&, const AtomString& trackID);

    bool m_isAttached { false };

    HashMap<AtomString, TrackBuffer> m_trackBufferMap;
    MediaTime m_currentTimeFudgeFactor;

    SourceBufferAppendMode m_appendMode { SourceBufferAppendMode::Segments };

    bool m_shouldGenerateTimestamps { false };
    bool m_receivedFirstInitializationSegment { false };
    bool m_pendingInitializationSegmentForChangeType { false };

    MediaTime m_timestampOffset;
    MediaTime m_appendWindowStart { MediaTime::zeroTime() };
    MediaTime m_appendWindowEnd { MediaTime::positiveInfiniteTime() };

    MediaTime m_groupStartTimestamp { MediaTime::invalidTime() };
    MediaTime m_groupEndTimestamp { MediaTime::zeroTime() };

    bool m_bufferFull { false };
    RefPtr<TimeRanges> m_buffered;
    bool m_bufferedDirty { true };
};

} // namespace WebCore

#endif
