/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#if ENABLE(MEDIA_SOURCE)

#include "MediaDescription.h"
#include "MediaSample.h"
#include "PlatformTimeRanges.h"
#include "SampleMap.h"
#include <wtf/Logger.h>
#include <wtf/LoggerHelper.h>
#include <wtf/MediaTime.h>
#include <wtf/UniqueRef.h>

namespace WebCore {

class TrackBuffer final
#if !RELEASE_LOG_DISABLED
    : public LoggerHelper
#endif
{
    WTF_MAKE_FAST_ALLOCATED;
public:
    static UniqueRef<TrackBuffer> create(RefPtr<MediaDescription>&&);
    static UniqueRef<TrackBuffer> create(RefPtr<MediaDescription>&&, const MediaTime&);
    
    MediaTime maximumBufferedTime() const;
    void addBufferedRange(const MediaTime& start, const MediaTime& end, AddTimeRangeOption = AddTimeRangeOption::None);
    void addSample(MediaSample&);
    
    bool updateMinimumUpcomingPresentationTime();
    
    bool reenqueueMediaForTime(const MediaTime&, const MediaTime& timeFudgeFactor);
    MediaTime findSeekTimeForTargetTime(const MediaTime& targetTime, const MediaTime& negativeThreshold, const MediaTime& positiveThreshold);
    bool removeCodedFrames(const MediaTime& start, const MediaTime& end, const MediaTime& currentTime);
    PlatformTimeRanges removeSamples(const DecodeOrderSampleMap::MapType&, const char*);
    
    void resetTimestampOffset();
    void reset();
    void clearSamples();
    
    const MediaTime& lastDecodeTimestamp() const { return m_lastDecodeTimestamp; }
    void setLastDecodeTimestamp(MediaTime timestamp) { m_lastDecodeTimestamp = WTFMove(timestamp); }
    
    const MediaTime& greatestFrameDuration() const { return m_greatestFrameDuration; }
    void setGreatestFrameDuration(MediaTime duration) { m_greatestFrameDuration = WTFMove(duration); }
    const MediaTime& lastFrameDuration() const { return m_lastFrameDuration; }
    void setLastFrameDuration(MediaTime duration) { m_lastFrameDuration = WTFMove(duration); }
    
    const MediaTime& highestPresentationTimestamp() const { return m_highestPresentationTimestamp; }
    void setHighestPresentationTimestamp(MediaTime timestamp) { m_highestPresentationTimestamp = WTFMove(timestamp); }
    
    const MediaTime& highestEnqueuedPresentationTime() const { return m_highestEnqueuedPresentationTime; }
    void setHighestEnqueuedPresentationTime(MediaTime timestamp) { m_highestEnqueuedPresentationTime = WTFMove(timestamp); }
    const MediaTime& minimumEnqueuedPresentationTime() const { return m_minimumEnqueuedPresentationTime; }
    void setMinimumEnqueuedPresentationTime(MediaTime timestamp) { m_minimumEnqueuedPresentationTime = WTFMove(timestamp); }
    
    const DecodeOrderSampleMap::KeyType& lastEnqueuedDecodeKey() const { return m_lastEnqueuedDecodeKey; }
    void setLastEnqueuedDecodeKey(DecodeOrderSampleMap::KeyType key) { m_lastEnqueuedDecodeKey = WTFMove(key); }
    
    const MediaTime& enqueueDiscontinuityBoundary() const { return m_enqueueDiscontinuityBoundary; }
    void setEnqueueDiscontinuityBoundary(MediaTime boundary) { m_enqueueDiscontinuityBoundary = WTFMove(boundary); }
    
    const MediaTime& roundedTimestampOffset() const { return m_roundedTimestampOffset; }
    void setRoundedTimestampOffset(MediaTime offset) { m_roundedTimestampOffset = WTFMove(offset); }
    void setRoundedTimestampOffset(const MediaTime&, uint32_t, const MediaTime&);
    
    uint32_t lastFrameTimescale() const { return m_lastFrameTimescale; }
    void setLastFrameTimescale(uint32_t timescale) { m_lastFrameTimescale = timescale; }
    bool needRandomAccessFlag() const { return m_needRandomAccessFlag; }
    void setNeedRandomAccessFlag(bool flag) { m_needRandomAccessFlag = flag; }
    bool enabled() const { return m_enabled; }
    void setEnabled(bool enabled) { m_enabled = enabled; }
    bool needsReenqueueing() const { return m_needsReenqueueing; }
    void setNeedsReenqueueing(bool flag) { m_needsReenqueueing = flag; }
    bool needsMinimumUpcomingPresentationTimeUpdating() const { return m_needsMinimumUpcomingPresentationTimeUpdating; }
    void setNeedsMinimumUpcomingPresentationTimeUpdating(bool flag) { m_needsMinimumUpcomingPresentationTimeUpdating = flag; }
    
    const SampleMap& samples() const { return m_samples; }
    SampleMap& samples() { return m_samples; }
    const DecodeOrderSampleMap::MapType& decodeQueue() const { return m_decodeQueue; }
    DecodeOrderSampleMap::MapType& decodeQueue() { return m_decodeQueue; }
    const RefPtr<MediaDescription>& description() const { return m_description; }
    const PlatformTimeRanges& buffered() const { return m_buffered; }
    PlatformTimeRanges& buffered() { return m_buffered; }
    
#if !RELEASE_LOG_DISABLED
    void setLogger(const Logger&, const void*);
    const Logger& logger() const final { ASSERT(m_logger); return *m_logger.get(); }
    const void* logIdentifier() const final { return m_logIdentifier; }
    const char* logClassName() const final { return "TrackBuffer"; }
    WTFLogChannel& logChannel() const final;
#endif
    
private:
    friend UniqueRef<TrackBuffer> WTF::makeUniqueRefWithoutFastMallocCheck<TrackBuffer>(RefPtr<WebCore::MediaDescription>&&, const WTF::MediaTime&);
    TrackBuffer(RefPtr<MediaDescription>&&, const MediaTime&);
    
    SampleMap m_samples;
    DecodeOrderSampleMap::MapType m_decodeQueue;
    RefPtr<MediaDescription> m_description;
    PlatformTimeRanges m_buffered;
    
    MediaTime m_lastDecodeTimestamp { MediaTime::invalidTime() };
    
    MediaTime m_greatestFrameDuration { MediaTime::invalidTime() };
    MediaTime m_lastFrameDuration { MediaTime::invalidTime() };
    
    MediaTime m_highestPresentationTimestamp { MediaTime::invalidTime() };
    
    MediaTime m_highestEnqueuedPresentationTime { MediaTime::invalidTime() };
    MediaTime m_minimumEnqueuedPresentationTime { MediaTime::invalidTime() };
    
    DecodeOrderSampleMap::KeyType m_lastEnqueuedDecodeKey { MediaTime::invalidTime(), MediaTime::invalidTime() };
    
    MediaTime m_enqueueDiscontinuityBoundary;
    MediaTime m_discontinuityTolerance;
    
    MediaTime m_roundedTimestampOffset { MediaTime::invalidTime() };
    
#if !RELEASE_LOG_DISABLED
    RefPtr<const Logger> m_logger;
    const void* m_logIdentifier;
#endif
    
    uint32_t m_lastFrameTimescale { 0 };
    bool m_needRandomAccessFlag { true };
    bool m_enabled { false };
    bool m_needsReenqueueing { false };
    bool m_needsMinimumUpcomingPresentationTimeUpdating { false };
};

} // namespace WebCore

#endif // ENABLE(MEDIA_SOURCE)
