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

#pragma once

#if ENABLE(MEDIA_SOURCE)

#include "ActiveDOMObject.h"
#include "AudioTrack.h"
#include "EventTarget.h"
#include "ExceptionOr.h"
#include "GenericEventQueue.h"
#include "SourceBufferPrivateClient.h"
#include "TextTrack.h"
#include "Timer.h"
#include "VideoTrack.h"
#include <wtf/LoggerHelper.h>

namespace WebCore {

class AudioTrackList;
class BufferSource;
class MediaSource;
class PlatformTimeRanges;
class SourceBufferPrivate;
class TextTrackList;
class TimeRanges;
class VideoTrackList;

class SourceBuffer final
    : public RefCounted<SourceBuffer>
    , public ActiveDOMObject
    , public EventTargetWithInlineData
    , private SourceBufferPrivateClient
    , private AudioTrackClient
    , private VideoTrackClient
    , private TextTrackClient
#if !RELEASE_LOG_DISABLED
    , private LoggerHelper
#endif
{
    WTF_MAKE_ISO_ALLOCATED(SourceBuffer);
public:
    static Ref<SourceBuffer> create(Ref<SourceBufferPrivate>&&, MediaSource*);
    virtual ~SourceBuffer();

    bool updating() const { return m_updating; }
    ExceptionOr<Ref<TimeRanges>> buffered() const;
    double timestampOffset() const;
    ExceptionOr<void> setTimestampOffset(double);

#if ENABLE(VIDEO_TRACK)
    VideoTrackList& videoTracks();
    AudioTrackList& audioTracks();
    TextTrackList& textTracks();
#endif

    double appendWindowStart() const;
    ExceptionOr<void> setAppendWindowStart(double);
    double appendWindowEnd() const;
    ExceptionOr<void> setAppendWindowEnd(double);

    ExceptionOr<void> appendBuffer(const BufferSource&);
    ExceptionOr<void> abort();
    ExceptionOr<void> remove(double start, double end);
    ExceptionOr<void> remove(const MediaTime&, const MediaTime&);
    ExceptionOr<void> changeType(const String&);

    const TimeRanges& bufferedInternal() const { ASSERT(m_buffered); return *m_buffered; }

    void abortIfUpdating();
    void removedFromMediaSource();
    void seekToTime(const MediaTime&);

    bool canPlayThroughRange(PlatformTimeRanges&);

    bool hasVideo() const;

    bool active() const { return m_active; }

    ScriptExecutionContext* scriptExecutionContext() const final { return ActiveDOMObject::scriptExecutionContext(); }

    using RefCounted::ref;
    using RefCounted::deref;

    struct TrackBuffer;

    Document& document() const;

    enum class AppendMode { Segments, Sequence };
    AppendMode mode() const { return m_mode; }
    ExceptionOr<void> setMode(AppendMode);

    void setShouldGenerateTimestamps(bool flag) { m_shouldGenerateTimestamps = flag; }

    bool isBufferedDirty() const { return m_bufferedDirty; }
    void setBufferedDirty(bool flag) { m_bufferedDirty = flag; }

    MediaTime highestPresentationTimestamp() const;
    void readyStateChanged();

    bool hasPendingActivity() const final;

    void trySignalAllSamplesEnqueued();

#if !RELEASE_LOG_DISABLED
    const Logger& logger() const final { return m_logger.get(); }
    const void* logIdentifier() const final { return m_logIdentifier; }
    const char* logClassName() const final { return "SourceBuffer"; }
    WTFLogChannel& logChannel() const final;
#endif

private:
    SourceBuffer(Ref<SourceBufferPrivate>&&, MediaSource*);

    void refEventTarget() final { ref(); }
    void derefEventTarget() final { deref(); }

    void suspend(ReasonForSuspension) final;
    void resume() final;
    void stop() final;
    const char* activeDOMObjectName() const final;
    bool canSuspendForDocumentSuspension() const final;

    void sourceBufferPrivateDidReceiveInitializationSegment(const InitializationSegment&) final;
    void sourceBufferPrivateDidReceiveSample(MediaSample&) final;
    bool sourceBufferPrivateHasAudio() const final;
    bool sourceBufferPrivateHasVideo() const final;
    void sourceBufferPrivateReenqueSamples(const AtomString& trackID) final;
    void sourceBufferPrivateDidBecomeReadyForMoreSamples(const AtomString& trackID) final;
    MediaTime sourceBufferPrivateFastSeekTimeForMediaTime(const MediaTime&, const MediaTime& negativeThreshold, const MediaTime& positiveThreshold) final;
    void sourceBufferPrivateAppendComplete(AppendResult) final;
    void sourceBufferPrivateDidReceiveRenderingError(int errorCode) final;

    void audioTrackEnabledChanged(AudioTrack&) final;
    void videoTrackSelectedChanged(VideoTrack&) final;

    void textTrackKindChanged(TextTrack&) final;
    void textTrackModeChanged(TextTrack&) final;
    void textTrackAddCues(TextTrack&, const TextTrackCueList&) final;
    void textTrackRemoveCues(TextTrack&, const TextTrackCueList&) final;
    void textTrackAddCue(TextTrack&, TextTrackCue&) final;
    void textTrackRemoveCue(TextTrack&, TextTrackCue&) final;

    EventTargetInterface eventTargetInterface() const final { return SourceBufferEventTargetInterfaceType; }

    bool isRemoved() const;
    void scheduleEvent(const AtomString& eventName);

    ExceptionOr<void> appendBufferInternal(const unsigned char*, unsigned);
    void appendBufferTimerFired();
    void resetParserState();

    void setActive(bool);

    bool validateInitializationSegment(const InitializationSegment&);

    void reenqueueMediaForTime(TrackBuffer&, const AtomString& trackID, const MediaTime&);
    void provideMediaData(TrackBuffer&, const AtomString& trackID);
    void didDropSample();
    void evictCodedFrames(size_t newDataSize);
    size_t maximumBufferSize() const;

    void monitorBufferingRate();

    void removeTimerFired();
    void removeCodedFrames(const MediaTime& start, const MediaTime& end);

    size_t extraMemoryCost() const;
    void reportExtraMemoryAllocated();

    void updateBufferedFromTrackBuffers();
    void updateMinimumUpcomingPresentationTime(TrackBuffer&, const AtomString& trackID);
    void resetMinimumUpcomingPresentationTime(TrackBuffer&, const AtomString& trackID);

    void appendError(bool);

    bool hasAudio() const;

    void rangeRemoval(const MediaTime&, const MediaTime&);

    void trySignalAllSamplesInTrackEnqueued(const AtomString&);

    friend class Internals;
    WEBCORE_EXPORT Vector<String> bufferedSamplesForTrackID(const AtomString&);
    WEBCORE_EXPORT Vector<String> enqueuedSamplesForTrackID(const AtomString&);
    WEBCORE_EXPORT MediaTime minimumUpcomingPresentationTimeForTrackID(const AtomString&);
    WEBCORE_EXPORT void setMaximumQueueDepthForTrackID(const AtomString&, size_t);

    Ref<SourceBufferPrivate> m_private;
    MediaSource* m_source;
    GenericEventQueue m_asyncEventQueue;
    AppendMode m_mode { AppendMode::Segments };

    Vector<unsigned char> m_pendingAppendData;
    Timer m_appendBufferTimer;

    RefPtr<VideoTrackList> m_videoTracks;
    RefPtr<AudioTrackList> m_audioTracks;
    RefPtr<TextTrackList> m_textTracks;

    Vector<AtomString> m_videoCodecs;
    Vector<AtomString> m_audioCodecs;
    Vector<AtomString> m_textCodecs;

    MediaTime m_timestampOffset;
    MediaTime m_appendWindowStart;
    MediaTime m_appendWindowEnd;

    MediaTime m_groupStartTimestamp;
    MediaTime m_groupEndTimestamp;

    HashMap<AtomString, TrackBuffer> m_trackBufferMap;
    RefPtr<TimeRanges> m_buffered;
    bool m_bufferedDirty { true };

    enum AppendStateType { WaitingForSegment, ParsingInitSegment, ParsingMediaSegment };
    AppendStateType m_appendState;

    MonotonicTime m_timeOfBufferingMonitor;
    double m_bufferedSinceLastMonitor { 0 };
    double m_averageBufferRate { 0 };

    size_t m_reportedExtraMemoryCost { 0 };

    MediaTime m_pendingRemoveStart;
    MediaTime m_pendingRemoveEnd;
    Timer m_removeTimer;

#if !RELEASE_LOG_DISABLED
    Ref<const Logger> m_logger;
    const void* m_logIdentifier;
#endif

    bool m_updating { false };
    bool m_receivedFirstInitializationSegment { false };
    bool m_active { false };
    bool m_bufferFull { false };
    bool m_shouldGenerateTimestamps { false };
    bool m_pendingInitializationSegmentForChangeType { false };
};

} // namespace WebCore

#endif
